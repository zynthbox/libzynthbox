#include "SndLibrary.h"
#include "AudioTagHelper.h"

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
#include <QRegularExpression>
#include <QAbstractProxyModel>
#include <taglib/taglib.h>
#include <taglib/wavfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>


SndLibrary::SndLibrary(QObject *parent)
    : QObject(parent)
    , m_soundsModel(new SndLibraryModel(this))
    , m_soundsByOriginModel(new QSortFilterProxyModel(this))
    , m_soundsByCategoryModel(new CategoryFilterProxyModel(this))
    , m_soundsByNameModel(new QSortFilterProxyModel(this))
    , m_updateAllFilesCountTimer(new QTimer(this))
    , m_sortModelByNameTimer(new QTimer(this))
    , m_sndIndexPath(qEnvironmentVariable("ZYNTHBOX_SND_INDEX_PATH", "/zynthian/zynthian-my-data/sounds/categories"))
    , m_sndIndexLookupTable(new QMap<QString, QStringList*>)
{
    m_soundsByOriginModel->setSourceModel(m_soundsModel);
    m_soundsByOriginModel->setFilterRole(SndLibraryModel::OriginRole);
    m_soundsByOriginModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByOriginModel->setDynamicSortFilter(false);
    setOriginFilter(m_originFilter); // Set default origin filter

    m_soundsByCategoryModel->setSourceModel(m_soundsByOriginModel);
    m_soundsByCategoryModel->setFilterRole(SndLibraryModel::CategoryRole);
    m_soundsByCategoryModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByCategoryModel->setDynamicSortFilter(false);
    setCategoryFilter(m_categoryFilter); // Set default category filter

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
        QString categoryDisplayName{"Unnamed"};
        if (categoriesObj[category].isObject()) {
            const QJsonObject categoryObject{categoriesObj[category].toObject()};
            static const QLatin1String displayNameKey{"displayName"};
            if (categoryObject.contains(displayNameKey) && categoryObject[displayNameKey].isString()) {
                categoryDisplayName = categoryObject[displayNameKey].toString();
            }
        }
        m_categories.insert(category, QVariant::fromValue(new SndCategoryInfo(categoryDisplayName, category, this)));
        if (category != "*") {
            // * is a logical category and hence does not need a directory
            QDir().mkpath(m_sndIndexPath + "/" + category);
        }
    }

    // A timer for reducing overhead when updating all files count after a category filecount changes
    m_updateAllFilesCountTimer->setInterval(0);
    m_updateAllFilesCountTimer->setSingleShot(true);
    connect(m_updateAllFilesCountTimer, &QTimer::timeout, this, [=]() {
        int myCount = 0;
        int communityCount = 0;
        for (auto entry = m_categories.begin(); entry != m_categories.end(); ++entry) {
            // Add up filecount for all categories except `*` which represents all categories and except `100` which represents "Best Of" category
            if (entry.key() != "*" && entry.key() != "100") {
                QObject *obj = entry.value().value<QObject*>();
                if (obj != nullptr) {
                    auto catObj = qobject_cast<SndCategoryInfo*>(obj);
                    if (catObj != nullptr) {
                        myCount += catObj->m_myFileCount;
                        communityCount += catObj->m_communityFileCount;
                    }
                }
            }
        }
        QObject *obj = m_categories.value("*").value<QObject*>();
        if (obj != nullptr) {
            auto catObj = qobject_cast<SndCategoryInfo*>(obj);
            if (catObj != nullptr) {
                catObj->setMyFileCount(myCount);
                catObj->setCommunityFileCount(communityCount);
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
        QObject *obj = m_categories.value(category).value<QObject*>();
        if (obj != nullptr) {
            auto catObj = qobject_cast<SndCategoryInfo*>(obj);
            if (catObj != nullptr) {
                if (origin == "my-sounds") {
                    catObj->setMyFileCount(count);
                } else if (origin == "community-sounds") {
                    catObj->setCommunityFileCount(count);
                }
                // Start timer to update all files count
                m_updateAllFilesCountTimer->start();
            } else {
                if (DEBUG) qDebug() << "Error updating fileCount for category" << category;
            }
        } else {
            if (DEBUG) qDebug() << "Error updating fileCount for category" << category;
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
            // Source file removed. Remove symlinks
            const QString fileIdentifier = m_baseSoundsDir.relativeFilePath(source);
            const QString fileIdentifierBase64Encoded = fileIdentifier.toUtf8().toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
            if (DEBUG) qDebug() << "Snd file removed :" << fileIdentifier << fileIdentifierBase64Encoded;
            if (m_sndIndexLookupTable->contains(fileIdentifierBase64Encoded)) {
                const auto categories = m_sndIndexLookupTable->value(fileIdentifierBase64Encoded);
                if (DEBUG) qDebug() << "  symlink had categories :" << categories->join(",");
                for (const auto& cat : *categories) {
                    if (DEBUG) qDebug() << "  Removing symlink :" << m_sndIndexPath + "/" + cat + "/" + fileIdentifierBase64Encoded;
                    QFile::remove(m_sndIndexPath + "/" + cat + "/" + fileIdentifierBase64Encoded);
                }
            }
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    if (DEBUG) qDebug() << "processSndFiles Time Taken :" << std::chrono::duration<double, std::chrono::seconds::period>(t_end-t_start).count();
    m_soundsModel->refresh();
}

void SndLibrary::processSndFile(const QString absolutePath)
{    
    TagLib::RIFF::WAV::File tagLibFile(qPrintable(absolutePath));
    TagLib::PropertyMap tags = tagLibFile.properties();
    const QString category = TStringToQString(tags["ZYNTHBOX_SOUND_CATEGORY"].front());
    processSndFile(absolutePath, category);
}

void SndLibrary::processSndFile(const QString absolutePath, const QString category)
{
    /**
     * @brief fileIdentifier is the unique string for a file that has the sound origin and username
     * For example, if a user named `user1` has a sound file named `sound1.snd` then the fileIdentifier
     * would be the relative path `community-sounds/user1/sound1.snd`. This fileIdentifier will be base64 encoded
     * and used as the symlink file name so when checking if a file is already processed, a snd file can be mapped
     * to its symlink file without keeping any database.
     */
    const QString fileIdentifier = m_baseSoundsDir.relativeFilePath(absolutePath);
    const QString fileIdentifierBase64Encoded = fileIdentifier.toUtf8().toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
    // const bool isAlreadyProcessed = m_sndIndexLookupTable->contains(fileIdentifierBase64Encoded);
    if (DEBUG) qDebug() << "Processing file" << fileIdentifier;
    // if (!isAlreadyProcessed) {
    const QString symlinkFilePath = m_sndIndexPath + "/" + category + "/" + fileIdentifierBase64Encoded;
    QFile(absolutePath).link(symlinkFilePath);
    // }
    Q_EMIT sndFileAdded(fileIdentifier);
}

void SndLibrary::refreshSndIndexLookupTable()
{
    auto t_start = std::chrono::high_resolution_clock::now();
    m_sndIndexLookupTable->clear();
    QDirIterator it(m_sndIndexPath, QDir::Files | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto fileInfo = QFileInfo(it.next());
        if (fileInfo.isSymbolicLink() && fileInfo.symLinkTarget().endsWith(".snd")) {
            if (!m_sndIndexLookupTable->contains(fileInfo.baseName())) {
                m_sndIndexLookupTable->insert(fileInfo.baseName(), new QStringList());
            }
            m_sndIndexLookupTable->value(fileInfo.baseName())->append(fileInfo.dir().dirName());
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    if (DEBUG) qDebug() << "refreshSndIndexLookupTable Time Taken :" << std::chrono::duration<double, std::chrono::seconds::period>(t_end-t_start).count();
}

void SndLibrary::setOriginFilter(const QString origin)
{
    if (m_originFilter != origin) {
        m_originFilter = origin;
        m_soundsByOriginModel->setFilterFixedString(origin);
        m_sortModelByNameTimer->start();
        Q_EMIT originFilterChanged();
    }
}

void SndLibrary::setCategoryFilter(const QString category)
{
    if (m_categoryFilter != category) {
        m_categoryFilter = category;
        if (category == "*") {
            m_soundsByCategoryModel->setFilterRegularExpression(".*");
        } else {
            m_soundsByCategoryModel->setFilterRegularExpression(category);
        }
        m_sortModelByNameTimer->start();
        Q_EMIT categoryFilterChanged();
    }
}

QVariantMap SndLibrary::categories()
{
    return m_categories;
}

QObject *SndLibrary::model() const
{
    return m_soundsByNameModel;
}

SndLibraryModel *SndLibrary::sourceModel()
{
    return m_soundsModel;
}

QString SndLibrary::sndIndexPath()
{
    return m_sndIndexPath;
}

QString SndLibrary::originFilter()
{
    return m_originFilter;
}

QString SndLibrary::categoryFilter()
{
    return m_categoryFilter;
}

void SndLibrary::updateSndFileCategory(SndFileInfo *sndFile, QString newCategory)
{
    QString oldCategory = sndFile->category();
    m_soundsModel->removeSndFileInfo(sndFile);

    // Update metadata in snd file
    auto tags = AudioTagHelper::instance()->readWavMetadata(sndFile->filePath());
    tags["ZYNTHBOX_SOUND_CATEGORY"] = newCategory;
    AudioTagHelper::instance()->saveWavMetadata(sndFile->filePath(), tags);

    // Update sndfile category property
    sndFile->setCategory(newCategory);

    // Remove symlink from old category
    QFile(m_sndIndexPath + "/" + oldCategory + "/" + sndFile->fileIdentifierBase64Encoded()).remove();
    // Create symlink to new category
    QFile(sndFile->filePath()).link(m_sndIndexPath + "/" + newCategory + "/" + sndFile->fileIdentifierBase64Encoded());

    // Decrease old category file count by 1
    QObject *obj = m_categories.value(oldCategory).value<QObject*>();
    if (obj != nullptr) {
        auto catObj = qobject_cast<SndCategoryInfo*>(obj);
        if (catObj != nullptr) {
            catObj->setMyFileCount(catObj->m_myFileCount - 1);
        }
    }

    // Increase new category file count by 1
    obj = m_categories.value(newCategory).value<QObject*>();
    if (obj != nullptr) {
        auto catObj = qobject_cast<SndCategoryInfo*>(obj);
        if (catObj != nullptr) {
            catObj->setMyFileCount(catObj->m_myFileCount + 1);
        }
    }

    m_soundsModel->addSndFileInfo(sndFile);
}

void SndLibrary::addToBestOf(QString absolutePath)
{
    addToBestOf(qobject_cast<SndFileInfo*>(sourceModel()->getSound(absolutePath)));
}

void SndLibrary::addToBestOf(SndFileInfo *sndFileInfo)
{
    if (sndFileInfo != nullptr) {
        processSndFile(sndFileInfo->filePath(), "100");
        sourceModel()->addSndFileInfo(new SndFileInfo(sndFileInfo->fileIdentifier(), sndFileInfo->name(), sndFileInfo->origin(), "100", this));
        QObject *obj = m_categories.value("100").value<QObject*>();
        if (obj != nullptr) {
            auto catObj = qobject_cast<SndCategoryInfo*>(obj);
            if (catObj != nullptr) {
                if (sndFileInfo->origin() == "my-sounds") {
                    catObj->setMyFileCount(catObj->m_myFileCount + 1);
                } else if (sndFileInfo->origin() == "community-sounds") {
                    catObj->setCommunityFileCount(catObj->m_communityFileCount + 1);
                }
            }
        }
    }
}

void SndLibrary::removeFromBestOf(QString absolutePath)
{
    removeFromBestOf(qobject_cast<SndFileInfo*>(sourceModel()->getSound(absolutePath)));
}

void SndLibrary::removeFromBestOf(SndFileInfo *sndFileInfo)
{
    if (sndFileInfo != nullptr) {
        QFile::remove(m_sndIndexPath + "/100/" + sndFileInfo->fileIdentifierBase64Encoded());
        sourceModel()->removeSndFileInfo(sndFileInfo);
        QObject *obj = m_categories.value("100").value<QObject*>();
        if (obj != nullptr) {
            auto catObj = qobject_cast<SndCategoryInfo*>(obj);
            if (catObj != nullptr) {
                if (sndFileInfo->origin() == "my-sounds") {
                    catObj->setMyFileCount(catObj->m_myFileCount - 1);
                } else if (sndFileInfo->origin() == "community-sounds") {
                    catObj->setCommunityFileCount(catObj->m_communityFileCount - 1);
                }
            }
        }
    }
}

CategoryFilterProxyModel::CategoryFilterProxyModel(SndLibrary *parent)
    : QSortFilterProxyModel(parent)
    , m_sndLibrary(parent)
{}

bool CategoryFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    const QString category = sourceModel()->data(sourceModel()->index(source_row, 0, source_parent), SndLibraryModel::CategoryRole).toString();
    if (m_sndLibrary->categoryFilter() == "*") {
        // If category filter is set to "*", filter out any sounds from "Best Of" category. "Best Of" will be displayed when "Best Of" button is checked
        // For other categories, it will get filtered implicitly
        return category != "100";
    } else {
        return filterRegularExpression().match(category).hasMatch();
    }
}
