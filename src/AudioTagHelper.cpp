#include "AudioTagHelper.h"

#include <QDebug>

#include <taglib/taglib.h>
#include <taglib/wavfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>

AudioTagHelper::AudioTagHelper(QObject* parent)
    : QObject(parent)
{
}

AudioTagHelper::~AudioTagHelper()
{
}

const QMap<QString, QString> AudioTagHelper::readWavMetadata(const QString& filepath)
{
    QMap<QString, QString> result;
    TagLib::PropertyMap tags;
    if (filepath.toLower().endsWith(".wav") || filepath.toLower().endsWith(".snd")) {
        TagLib::RIFF::WAV::File tagLibFile(qPrintable(filepath));
        tags = tagLibFile.properties();
    } else if (filepath.toLower().endsWith(".ogg")) {
        TagLib::Vorbis::File tagLibFile(qPrintable(filepath));
        tags = tagLibFile.properties();
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to read metadata - it was not a recognised filetype:" << filepath.split(".").last();
    }
    if(!tags.isEmpty()) {
        for(TagLib::PropertyMap::ConstIterator entry = tags.begin(); entry != tags.end(); ++entry) {
            result.insert(TStringToQString(entry->first), TStringToQString(entry->second.front()));
        }
    }
    return result;
}

void AudioTagHelper::saveWavMetadata(const QString &filepath, const QMap<QString, QString> &metadata)
{
    TagLib::PropertyMap tags;
    if (filepath.toLower().endsWith(".wav") || filepath.toLower().endsWith(".snd")) {
        TagLib::RIFF::WAV::File tagLibFile(qPrintable(filepath));
        tags = tagLibFile.properties();
        for (auto it = metadata.constKeyValueBegin(); it != metadata.constKeyValueEnd(); ++it) {
            tags.replace(QStringToTString(it->first), QStringToTString(it->second));
        }
        tagLibFile.setProperties(tags);
        tagLibFile.save();
    } else if (filepath.toLower().endsWith(".ogg")) {
        TagLib::Vorbis::File tagLibFile(qPrintable(filepath));
        tags = tagLibFile.properties();
        for (auto it = metadata.constKeyValueBegin(); it != metadata.constKeyValueEnd(); ++it) {
            tags.replace(QStringToTString(it->first), QStringToTString(it->second));
        }
        tagLibFile.setProperties(tags);
        tagLibFile.save();
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to write metadata - it was not a recognised filetype:" << filepath.split(".").last();
    }
}
