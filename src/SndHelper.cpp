#include "SndHelper.h"
#include "AudioTagHelper.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFileInfoList>

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
            // IFDEBUG(qDebug() << "  Category : " << metadata["ZYNTHBOX_SOUND_CATEGORY"]);
        }
    }
    IFDEBUG(qDebug() << "End Serialize");
}
