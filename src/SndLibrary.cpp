#include "SndLibrary.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QAbstractListModel>
#include <QDirIterator>
#include <QtGlobal>
#include <taglib/taglib.h>
#include <taglib/wavfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>


SndLibrary::SndLibrary(QObject *parent)
    : QObject(parent)
    , m_soundsModel(new SndLibraryModel(this))
    , m_soundsByOriginModel(new QSortFilterProxyModel(this))
    , m_soundsByCategoryModel(new QSortFilterProxyModel(this))
    , m_soundsByNameModel(new QSortFilterProxyModel(this))
    , m_updateAllFilesCountTimer(new QTimer(this))
    , m_sortModelByNameTimer(new QTimer(this))
    , m_sndIndexPath(qEnvironmentVariable("ZYNTHBOX_SND_INDEX_PATH", "/zynthian/zynthian-my-data/sounds/categories"))
    , m_sndIndexLookupTable(new QMap<QString, QStringList*>)
{
    m_soundsByOriginModel->setSourceModel(m_soundsModel);
    m_soundsByOriginModel->setFilterRole(SndLibraryModel::OriginRole);
    m_soundsByOriginModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByOriginModel->setFilterFixedString("my-sounds");
    m_soundsByOriginModel->setDynamicSortFilter(false);

    m_soundsByCategoryModel->setSourceModel(m_soundsByOriginModel);
    m_soundsByCategoryModel->setFilterRole(SndLibraryModel::CategoryRole);
    m_soundsByCategoryModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByCategoryModel->setDynamicSortFilter(false);

    m_soundsByNameModel->setSourceModel(m_soundsByCategoryModel);
    m_soundsByNameModel->setFilterRole(SndLibraryModel::NameRole);
    m_soundsByNameModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByNameModel->setSortRole(SndLibraryModel::NameRole);
    m_soundsByNameModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByNameModel->setDynamicSortFilter(false);

    QString val;
    QFile pluginsFile("/zynthian/zynthbox-qml/config/plugins.json");
    pluginsFile.open(QIODevice::ReadOnly | QIODevice::Text);
    val = pluginsFile.readAll();
    pluginsFile.close();
    m_pluginsObj = QJsonDocument::fromJson(val.toUtf8()).object();

    QFile categoriesFile("/zynthian/zynthbox-qml/config/snd_categories.json");
    categoriesFile.open(QIODevice::ReadOnly | QIODevice::Text);
    val = categoriesFile.readAll();
    categoriesFile.close();
    auto categoriesObj = QJsonDocument::fromJson(val.toUtf8()).object();
    for (auto category : categoriesObj.keys()) {
        m_categories.insert(category, QVariant::fromValue(new SndCategoryInfo(categoriesObj[category].toString(), category, this)));
        if (category != "*") {
            // * is a logical category and hence does not need a directory
            QDir().mkpath(m_sndIndexPath + "/" + category);
        }
    }

    // A timer for reducing overhead when updating all files count after a category filecount changes
    m_updateAllFilesCountTimer->setInterval(0);
    m_updateAllFilesCountTimer->setSingleShot(true);
    connect(m_updateAllFilesCountTimer, &QTimer::timeout, this, [=]() {
        int count = 0;
        for (auto entry = m_categories.begin(); entry != m_categories.end(); ++entry) {
            // Add up filecount for all categories except `*` which represents all categories
            if (entry.key() != "*") {
                QObject *obj = entry.value().value<QObject*>();
                if (obj != nullptr) {
                    auto catObj = qobject_cast<SndCategoryInfo*>(obj);
                    if (catObj != nullptr) {
                        count += catObj->m_fileCount;
                    }
                }
            }
        }
        QObject *obj = m_categories.value("*").value<QObject*>();
        if (obj != nullptr) {
            auto catObj = qobject_cast<SndCategoryInfo*>(obj);
            if (catObj != nullptr) {
                catObj->setFileCount(count);
            } else {
                if (DEBUG) qDebug() << "Error updating fileCount for category *";
            }
        } else {
            if (DEBUG) qDebug() << "Error updating fileCount for category *";
        }
    }, Qt::QueuedConnection);

    // A timer for reducing overhead when items need to be sorted after adding
    m_sortModelByNameTimer->setInterval(0);
    m_sortModelByNameTimer->setSingleShot(true);
    connect(m_sortModelByNameTimer, &QTimer::timeout, this, [=]() {
        m_soundsByNameModel->sort(0);
    }, Qt::QueuedConnection);

    // Update all files count when any category file count changes
    connect(m_soundsModel, &SndLibraryModel::categoryFilesCountChanged, this, [=](QString category, QString origin, int count) {
        // TODO : Check what to do with community-sounds
        if (origin == "my-sounds") {
            QObject *obj = m_categories.value(category).value<QObject*>();
            if (obj != nullptr) {
                auto catObj = qobject_cast<SndCategoryInfo*>(obj);
                if (catObj != nullptr) {
                    catObj->setFileCount(count);
                    // Start timer to update all files count
                    m_updateAllFilesCountTimer->start();
                } else {
                    if (DEBUG) qDebug() << "Error updating fileCount for category" << category;
                }
            } else {
                if (DEBUG) qDebug() << "Error updating fileCount for category" << category;
            }
        }
    }, Qt::QueuedConnection);

    // Sort sounds model by name when a new item is inserted
    connect(m_soundsModel, &QAbstractListModel::rowsInserted, this, [=](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);
        m_sortModelByNameTimer->start();
    }, Qt::QueuedConnection);

    // Populate sounds model when SndLibrary gets instantiated
    m_soundsModel->refresh();
}

void SndLibrary::processSndFiles(const QStringList sources)
{
    auto t_start = std::chrono::high_resolution_clock::now();
    refreshSndIndexLookupTable();
    for (auto source : sources) {
        const QFileInfo sourceInfo(source);

        if (sourceInfo.exists()) {
            if (sourceInfo.isDir()) {
                QDirIterator it(sourceInfo.filePath(), QStringList() << "*.snd", QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    processSndFile(it.next());
                }
            } else if (sourceInfo.isFile() && sourceInfo.absoluteFilePath().endsWith(".snd")) {
                processSndFile(sourceInfo.filePath());
            }
        } else {
            // TODO : Handle source file removed. Remove symlinks
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    if (DEBUG) qDebug() << "processSndFiles Time Taken :" << std::chrono::duration<double, std::chrono::seconds::period>(t_end-t_start).count();
    m_soundsModel->refresh();
}

void SndLibrary::processSndFile(const QString source)
{
    QDir baseSoundsDir("/zynthian/zynthian-my-data/sounds/");
    /**
     * @brief fileIdentifier is the unique string for a file that has the sound origin and username
     * For example, if a user named `user1` has a sound file named `sound1.snd` then the fileIdentifier
     * would be the relative path `community-sounds/user1/sound1.snd`. This fileIdentifier will be base64 encoded
     * and used as the symlink file name so when checking if a file is already processed, a snd file can be mapped
     * to its symlink file without keeping any database.
     */
    const QString fileIdentifier = baseSoundsDir.relativeFilePath(source);
    const QString fileIdentifierBase64Encoded = fileIdentifier.toUtf8().toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
    // const bool isAlreadyProcessed = m_sndIndexLookupTable->contains(fileIdentifierBase64Encoded);
    if (DEBUG) qDebug() << "Processing file" << fileIdentifier;
    // if (!isAlreadyProcessed) {
        TagLib::RIFF::WAV::File tagLibFile(qPrintable(source));
        TagLib::PropertyMap tags = tagLibFile.properties();
        const QString category = TStringToQString(tags["ZYNTHBOX_SOUND_CATEGORY"].front());
        const QString symlinkFilePath = m_sndIndexPath + "/" + category + "/" + fileIdentifierBase64Encoded;
        QFile(source).link(symlinkFilePath);
    // }
}

void SndLibrary::refreshSndIndexLookupTable()
{
    auto t_start = std::chrono::high_resolution_clock::now();
    m_sndIndexLookupTable->clear();
    QDirIterator it(m_sndIndexPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto fileInfo = QFileInfo(it.next());
        if (!m_sndIndexLookupTable->contains(fileInfo.baseName())) {
            m_sndIndexLookupTable->insert(fileInfo.baseName(), new QStringList());
        }
        m_sndIndexLookupTable->value(fileInfo.baseName())->append(fileInfo.dir().dirName());
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    if (DEBUG) qDebug() << "refreshSndIndexLookupTable Time Taken :" << std::chrono::duration<double, std::chrono::seconds::period>(t_end-t_start).count();
}

void SndLibrary::setOriginFilter(const QString origin)
{
    m_soundsByOriginModel->setFilterFixedString(origin);
    m_sortModelByNameTimer->start();
}

void SndLibrary::setCategoryFilter(const QString category)
{
    if (category == "*") {
        m_soundsByCategoryModel->setFilterRegExp("");
    } else {
        m_soundsByCategoryModel->setFilterFixedString(category);
    }
    m_sortModelByNameTimer->start();
}

QVariantMap SndLibrary::categories()
{
    return m_categories;
}

SndLibraryModel *SndLibrary::sourceModel()
{
    return m_soundsModel;
}

QString SndLibrary::sndIndexPath()
{
    return m_sndIndexPath;
}
