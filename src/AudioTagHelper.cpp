#include "AudioTagHelper.h"

#include <taglib/taglib.h>
#include <taglib/wavfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>

AudioTagHelper::AudioTagHelper(QObject* parent)
    : QObject(parent)
{
}

AudioTagHelper::~AudioTagHelper()
{
}

QMap<QString, QString> AudioTagHelper::readWavMetadata(const QString& filepath)
{
    QMap<QString, QString> result;
    TagLib::RIFF::WAV::File tagLibFile(qPrintable(filepath));
    TagLib::PropertyMap tags = tagLibFile.properties();
    if(!tags.isEmpty()) {
        for(TagLib::PropertyMap::ConstIterator entry = tags.begin(); entry != tags.end(); ++entry) {
            result.insert(TStringToQString(entry->first), TStringToQString(entry->second.front()));
        }
    }
    return result;
}

void AudioTagHelper::saveWavMetadata(const QString &filepath, const QMap<QString, QString> &metadata)
{
    TagLib::RIFF::WAV::File tagLibFile(qPrintable(filepath));
    TagLib::PropertyMap tags = tagLibFile.properties();
    for (auto it = metadata.constKeyValueBegin(); it != metadata.constKeyValueEnd(); ++it) {
        tags.replace(QStringToTString(it->first), QStringToTString(it->second));
    }
    tagLibFile.setProperties(tags);
    tagLibFile.save();
}
