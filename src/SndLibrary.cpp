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
#define DEBUG true

SndLibrary::SndLibrary(QObject *parent)
    : QObject(parent)
    , m_soundsModel(new SndLibraryModel(this))
    , m_soundsByOriginModel(new QSortFilterProxyModel(this))
    , m_soundsByCategoryModel(new QSortFilterProxyModel(this))
{
    m_soundsByOriginModel->setSourceModel(m_soundsModel);
    m_soundsByOriginModel->setFilterRole(SndLibraryModel::OriginRole);
    m_soundsByOriginModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_soundsByOriginModel->setFilterFixedString("my-sounds");

    m_soundsByCategoryModel->setSourceModel(m_soundsByOriginModel);
    m_soundsByCategoryModel->setFilterRole(SndLibraryModel::CategoryRole);
    m_soundsByCategoryModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void SndLibrary::serializeTo(const QString sourceDir, const QString outputFile)
{
    QDir dir(sourceDir);
    if (dir.exists()) {
        QString val;
        QFile pluginsFile("/zynthian/zynthbox-qml/config/plugins.json");
        pluginsFile.open(QIODevice::ReadOnly | QIODevice::Text);
        val = pluginsFile.readAll();
        pluginsFile.close();
        const auto pluginsObj = QJsonDocument::fromJson(val.toUtf8()).object();
        const QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.snd", QDir::Files);
        QMap<QString, QJsonObject> categoryFilesMap;
        int i = 0;
        if(DEBUG) qDebug() << "START Serialization";
        for (const QFileInfo &file: fileList) {
            if(DEBUG) qDebug() << QString("Extracting metadata from file #%1: %2").arg(++i).arg(file.fileName());
            // const auto metadata = AudioTagHelper::instance()->readWavMetadata(file.filePath());
            TagLib::RIFF::WAV::File tagLibFile(qPrintable(file.filePath()));
            TagLib::PropertyMap tags = tagLibFile.properties();
            if (tags.contains("ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT") &&
                tags.contains("ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT") &&
                tags.contains("ZYNTHBOX_SOUND_CATEGORY")
                ) {
                QJsonArray synthSlotsData = {"", "", "", "", ""};
                QJsonArray sampleSlotsData = {"", "", "", "", ""};
                QJsonArray fxSlotsData = {"", "", "", "", ""};
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
                            engineName.replace(pluginIdNameRegex, pluginsObj[match.captured(1)].toObject()["name"].toString());
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
                if (!categoryFilesMap.contains(category)) {
                    categoryFilesMap[category] = QJsonObject();
                }
                QJsonObject sndObj;
                sndObj["synthSlotsData"] = synthSlotsData;
                sndObj["sampleSlotsData"] = sampleSlotsData;
                sndObj["fxSlotsData"] = fxSlotsData;
                categoryFilesMap[category].insert(file.fileName(), sndObj);
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
        }
        const QJsonDocument result(resultObj);
        file.open(QFile::WriteOnly);
        file.write(result.toJson(QJsonDocument::Compact));
        file.close();
        if(DEBUG) qDebug() << "END Serialization";
    }
}

QSortFilterProxyModel* SndLibrary::model()
{
    return m_soundsByCategoryModel;
}

SndLibraryModel *SndLibrary::sourceModel()
{
    return m_soundsModel;
}

void SndLibrary::refresh()
{
    m_soundsModel->refresh();
}

void SndLibrary::setOriginFilter(QString origin)
{
    m_soundsByOriginModel->setFilterFixedString(origin);
}

void SndLibrary::setCategoryFilter(QString category)
{
    if (category == "*") {
        m_soundsByCategoryModel->setFilterRegExp("");
    } else {
        m_soundsByCategoryModel->setFilterFixedString(category);
    }
}
