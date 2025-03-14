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
#include <taglib/taglib.h>
#include <taglib/wavfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>

/**
 * When DEBUG is set to true it will print a set of logs
 * which is not meant for production builds
 */
#ifndef DEBUG
#define DEBUG true
#endif

SndLibrary::SndLibrary(QObject *parent)
    : QObject(parent)
    , m_soundsModel(new SndLibraryModel(this))
    , m_soundsByOriginModel(new QSortFilterProxyModel(this))
    , m_soundsByCategoryModel(new QSortFilterProxyModel(this))
    , m_soundsByNameModel(new QSortFilterProxyModel(this))
    , m_updateAllFilesCountTimer(new QTimer(this))
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
    }

    // A timer for reducing overhead when updating all files count after a category filecount changes
    m_updateAllFilesCountTimer->setInterval(100);
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
    });

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

    // Populate sounds model when SndLibrary gets instantiated
    m_soundsModel->refresh();
}

void SndLibrary::serializeTo(const QString sourceDir, const QString origin, const QString outputFile)
{
    QDir dir(sourceDir);
    if (dir.exists()) {
        const QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.snd", QDir::Files);
        QMap<QString, QJsonObject> categoryFilesMap;
        int i = 0;
        if(DEBUG) qDebug() << "START Serialization";
        for (const QFileInfo &file: fileList) {
            if(DEBUG) qDebug() << QString("Extracting metadata from file #%1: %2").arg(++i).arg(file.fileName());
            SndFileInfo *soundInfo = extractSndFileInfo(file.filePath(), origin);
            if (soundInfo != nullptr) {
                if (!categoryFilesMap.contains(soundInfo->m_category)) {
                    categoryFilesMap[soundInfo->m_category] = QJsonObject();
                }
                QJsonObject sndObj;
                sndObj["synthSlotsData"] = QJsonArray::fromStringList(soundInfo->m_synthSlotsData);
                sndObj["sampleSlotsData"] = QJsonArray::fromStringList(soundInfo->m_sampleSlotsData);
                sndObj["fxSlotsData"] = QJsonArray::fromStringList(soundInfo->m_fxSlotsData);
                categoryFilesMap[soundInfo->m_category].insert(file.fileName(), sndObj);
                // if(DEBUG) qDebug() << QString("  Category : | %1 |").arg(category);
                // if(DEBUG) qDebug() << QString("  Synth    : | %1 |").arg(synthSlotsData.join(" | "));
                // if(DEBUG) qDebug() << QString("  Sample   : | %1 |").arg(sampleSlotsData.join(" | "));
                // if(DEBUG) qDebug() << QString("  Fx       : | %1 |").arg(fxSlotsData.join(" | "));
            }
        }
        QFile file(outputFile);
        QJsonObject resultObj = QJsonObject();
        for (auto key : categoryFilesMap.keys()) {
            auto categoryFiles = categoryFilesMap.value(key);
            QJsonObject categoryObj;
            categoryObj["count"] = categoryFiles.count();
            categoryObj["files"] = categoryFiles;
            resultObj[key] = categoryObj;
            QObject *obj = m_categories.value(key).value<QObject*>();
            if (obj != nullptr) {
                auto catObj = qobject_cast<SndCategoryInfo*>(obj);
                if (catObj != nullptr) {
                    catObj->setFileCount(categoryFiles.count());
                } else {
                    if (DEBUG) qDebug() << "Error updating fileCount for category" << key;
                }
            } else {
                if (DEBUG) qDebug() << "Error updating fileCount for category" << key;
            }
        }
        const QJsonDocument result(resultObj);
        file.open(QFile::WriteOnly);
        file.write(result.toJson(QJsonDocument::Compact));
        file.close();
        if(DEBUG) qDebug() << "END Serialization";
    }
}

void SndLibrary::refresh()
{
    m_soundsModel->refresh();
}

void SndLibrary::setOriginFilter(const QString origin)
{
    m_soundsByOriginModel->setFilterFixedString(origin);
    m_soundsByNameModel->setFilterRegExp("");
    m_soundsByNameModel->sort(0);
}

void SndLibrary::setCategoryFilter(const QString category)
{
    if (category == "*") {
        m_soundsByCategoryModel->setFilterRegExp("");
    } else {
        m_soundsByCategoryModel->setFilterFixedString(category);
    }
    m_soundsByNameModel->setFilterRegExp("");
    m_soundsByNameModel->sort(0);
}

void SndLibrary::addSndFiles(const QStringList sndFilepaths, const QString origin, const QString statsFilepath)
{
    QFile file(statsFilepath);
    QJsonObject resultObj;
    QMap<QString, QJsonObject> categoryFilesMap;

    // Read statistics file if exists otherwise create empty object
    if (file.exists()) {
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            resultObj = QJsonDocument::fromJson(file.readAll()).object();
            file.close();
        } else {
            qCritical() << "Cannot open statistics file" << file.fileName();
        }
    }

    // Extract sound information from all snd files and add them to model
    for (auto sndFilepath : sndFilepaths) {
        if(DEBUG) qDebug() << "Extracting sound information from" << sndFilepath;
        SndFileInfo *soundInfo = extractSndFileInfo(sndFilepath, origin);
        if (!categoryFilesMap.contains(soundInfo->m_category)) {
            if(DEBUG) qDebug() << "categoryFilesMap do not have entry for category" << soundInfo->m_category;
            if (resultObj.contains(soundInfo->m_category)) {
                if(DEBUG) qDebug() << "  Copying category from statsFile";
                // If stats file already has a category entry, copy it and add new files to that category
                categoryFilesMap[soundInfo->m_category] = resultObj[soundInfo->m_category].toObject()["files"].toObject();
            } else {
                if(DEBUG) qDebug() << "  Creating empty category";
                // If stats do not have the category entry, create new empty object
                categoryFilesMap[soundInfo->m_category] = QJsonObject();
            }
        }
        if (soundInfo != nullptr) {
            m_soundsModel->addSndFileInfo(soundInfo);
            QJsonObject sndObj;
            sndObj["synthSlotsData"] = QJsonArray::fromStringList(soundInfo->m_synthSlotsData);
            sndObj["sampleSlotsData"] = QJsonArray::fromStringList(soundInfo->m_sampleSlotsData);
            sndObj["fxSlotsData"] = QJsonArray::fromStringList(soundInfo->m_fxSlotsData);
            categoryFilesMap[soundInfo->m_category].insert(soundInfo->m_name, sndObj);
        }
    }
    m_soundsByNameModel->sort(0);

    // Write updated json to stats file
    if (file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        for (auto key : categoryFilesMap.keys()) {
            auto categoryFiles = categoryFilesMap.value(key);
            QJsonObject categoryObj;
            categoryObj["count"] = categoryFiles.count();
            categoryObj["files"] = categoryFiles;
            resultObj[key] = categoryObj;
            QObject *obj = m_categories.value(key).value<QObject*>();
            if (obj != nullptr) {
                auto catObj = qobject_cast<SndCategoryInfo*>(obj);
                if (catObj != nullptr) {
                    catObj->setFileCount(categoryFiles.count());
                }
            } else {
                if (DEBUG) qDebug() << "Error updating fileCount for category" << key;
            }
        }
        const QJsonDocument result(resultObj);
        file.write(result.toJson(QJsonDocument::Compact));
        file.close();
    }
}

bool SndLibrary::removeSndFile(const QString filepath, const QString origin)
{
    Q_UNUSED(filepath);
    Q_UNUSED(origin);
    return false;
}

SndFileInfo* SndLibrary::extractSndFileInfo(const QString filepath, const QString origin)
{
    const QFileInfo sourceFileInfo(filepath);
    SndFileInfo* soundInfo{nullptr};

    // const auto metadata = AudioTagHelper::instance()->readWavMetadata(file.filePath());
    TagLib::RIFF::WAV::File tagLibFile(qPrintable(filepath));
    TagLib::PropertyMap tags = tagLibFile.properties();
    if (tags.contains("ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT") &&
        tags.contains("ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT") &&
        tags.contains("ZYNTHBOX_SOUND_CATEGORY")
        ) {
        QStringList synthSlotsData = {"", "", "", "", ""};
        QStringList sampleSlotsData = {"", "", "", "", ""};
        QStringList fxSlotsData = {"", "", "", "", ""};
        const QString category = TStringToQString(tags["ZYNTHBOX_SOUND_CATEGORY"].front());
        const auto synthFxSnapshotJsonObj = QJsonDocument::fromJson(TStringToQString(tags["ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT"].front()).toUtf8()).object();
        const auto sampleSnapshotJsonObj = QJsonDocument::fromJson(TStringToQString(tags["ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT"].front()).toUtf8()).object();
        for (auto layerData : synthFxSnapshotJsonObj["layers"].toArray()) {
            const auto layerDataObj = layerData.toObject();
            const QString engineType = layerDataObj["engine_type"].toString();
            QString engineName = layerDataObj["engine_name"].toString().split("/").last();
            if (!engineName.isEmpty()) {
                /**
                 *  A regex to filter out plugin name variables like `${ZBP_00158_name}`
                 *  The regex matches the format `${`, captures the plugin id `ZBP_\\d*` and matches the plugin name variable `_name}`
                 */
                const QRegularExpression pluginIdNameRegex("\\$\\{(ZBP_\\d*)_name\\}");
                // Find out the plugin id from engine name if any
                const QRegularExpressionMatch match = pluginIdNameRegex.match(engineName);
                // Replace the variable with actual plugin name
                if (match.hasMatch()) {
                    engineName.replace(pluginIdNameRegex, m_pluginsObj[match.captured(1)].toObject()["name"].toString());
                }
            }
            if (engineType == "MIDI Synth") {
                synthSlotsData[layerDataObj["slot_index"].toInt()] = QString("%1 > %2").arg(engineName).arg(layerDataObj["preset_name"].toString());
            } else if (engineType == "Audio Effect") {
                fxSlotsData[layerDataObj["slot_index"].toInt()] = QString("%1 > %2").arg(engineName).arg(layerDataObj["preset_name"].toString());
            }
        }
        for (int i = 0; i < sampleSnapshotJsonObj.keys().length(); ++i) {
            sampleSlotsData[i] = sampleSnapshotJsonObj[QString("%1").arg(i)].toObject()["filename"].toString();
        }
        soundInfo = new SndFileInfo(sourceFileInfo.fileName(), origin, category, synthSlotsData, sampleSlotsData, fxSlotsData, this);
    }

    return soundInfo;
}

QVariantMap SndLibrary::categories()
{
    return m_categories;
}
