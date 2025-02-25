#include "SndHelper.h"
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

/**
 * When DEBUG is set to true it will print a set of logs
 * which is not meant for production builds
 */
#define DEBUG true

/**
 * Use this macro to check if in debug mode
 */
#define IFDEBUG(x) if(DEBUG) x

SndHelper::SndHelper(QObject *parent) : QObject(parent)
{
}

void SndHelper::serializeTo(const QString sourceDir, const QString outputFile)
{
    IFDEBUG(qDebug() << "Start Serialize");
    QDir dir(sourceDir);
    if (dir.exists()) {
        QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.snd", QDir::Files);
        int i = 0;
        for (const QFileInfo &file: fileList) {
            IFDEBUG(qDebug() << QString("Extracting metadata from file #%1: %2").arg(++i).arg(file.fileName()));
            const auto metadata = AudioTagHelper::instance()->readWavMetadata(file.filePath());
            if (metadata.contains("ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT") &&
                metadata.contains("ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT") &&
                metadata.contains("ZYNTHBOX_SOUND_CATEGORY")
            ) {
                QStringList synthSlotsData = {"", "", "", "", ""};
                QStringList sampleSlotsData = {"", "", "", "", ""};
                QStringList fxSlotsData = {"", "", "", "", ""};
                const QString category = metadata["ZYNTHBOX_SOUND_CATEGORY"];
                const auto synthFxSnapshotJsonObj = QJsonDocument::fromJson(metadata["ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT"].toUtf8()).object();
                const auto sampleSnapshotJsonObj = QJsonDocument::fromJson(metadata["ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT"].toUtf8()).object();
                for (auto layerData : synthFxSnapshotJsonObj["layers"].toArray()) {
                    const auto layerDataObj = layerData.toObject();
                    const QString engineType = layerDataObj["engine_type"].toString();
                    if (engineType == "MIDI Synth") {
                        synthSlotsData.replace(layerDataObj["slot_index"].toInt(), QString("%1 > %2").arg(layerDataObj["engine_name"].toString().split("/").last()).arg(layerDataObj["preset_name"].toString()));
                    } else if (engineType == "Audio Effect") {
                        fxSlotsData.replace(layerDataObj["slot_index"].toInt(), QString("%1 > %2").arg(layerDataObj["engine_name"].toString().split("/").last()).arg(layerDataObj["preset_name"].toString()));
                    }
                }
                for (int i = 0; i < sampleSnapshotJsonObj.keys().length(); ++i) {
                    sampleSlotsData.replace(i, sampleSnapshotJsonObj[QString("%1").arg(i)].toObject()["filename"].toString());
                }
                IFDEBUG(qDebug() << QString("  Category : %1").arg(category));
                IFDEBUG(qDebug() << QString("  Synth    : %1").arg(synthSlotsData.join(", ")));
                IFDEBUG(qDebug() << QString("  Sample   : %1").arg(sampleSlotsData.join(", ")));
                IFDEBUG(qDebug() << QString("  Fx       : %1").arg(fxSlotsData.join(", ")));
            }
        }
    }
    IFDEBUG(qDebug() << "End Serialize");
}
