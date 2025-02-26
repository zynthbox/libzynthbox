#include "SndLibraryHelper.h"
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

/**
 * When DEBUG is set to true it will print a set of logs
 * which is not meant for production builds
 */
#define DEBUG true

/**
 * Use this macro to check if in debug mode
 */
#define IFDEBUG(x) if(DEBUG) x

SndLibraryHelper::SndLibraryHelper(QObject *parent) : QObject(parent)
{
}

void SndLibraryHelper::serializeTo(const QString sourceDir, const QString outputFile)
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
        IFDEBUG(qDebug() << "START Serialization");
        for (const QFileInfo &file: fileList) {
            IFDEBUG(qDebug() << QString("Extracting metadata from file #%1: %2").arg(++i).arg(file.fileName()));
            const auto metadata = AudioTagHelper::instance()->readWavMetadata(file.filePath());
            if (metadata.contains("ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT") &&
                metadata.contains("ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT") &&
                metadata.contains("ZYNTHBOX_SOUND_CATEGORY")
            ) {
                QJsonArray synthSlotsData = {"", "", "", "", ""};
                QJsonArray sampleSlotsData = {"", "", "", "", ""};
                QJsonArray fxSlotsData = {"", "", "", "", ""};
                const QString category = metadata["ZYNTHBOX_SOUND_CATEGORY"];
                const auto synthFxSnapshotJsonObj = QJsonDocument::fromJson(metadata["ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT"].toUtf8()).object();
                const auto sampleSnapshotJsonObj = QJsonDocument::fromJson(metadata["ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT"].toUtf8()).object();
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
                // IFDEBUG(qDebug() << QString("  Category : | %1 |").arg(category));
                // IFDEBUG(qDebug() << QString("  Synth    : | %1 |").arg(synthSlotsData.join(" | ")));
                // IFDEBUG(qDebug() << QString("  Sample   : | %1 |").arg(sampleSlotsData.join(" | ")));
                // IFDEBUG(qDebug() << QString("  Fx       : | %1 |").arg(fxSlotsData.join(" | ")));
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
        IFDEBUG(qDebug() << "END Serialization");
    }
}
