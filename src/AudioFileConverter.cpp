
#include "AudioFileConverter.h"
#include "AudioTagHelper.h"
#include "Plugin.h"

#include <QDir>
#include <QFile>

class AudioFileConverterPrivate {
public:
    AudioFileConverterPrivate() {}
    double progress{0.0};
    QString taskDescription;
    AudioFileConverter::ConversionState state{AudioFileConverter::IdleState};
    QString origin;
    QString destination;
    AudioFileConverter::AudioFormat destinationFormat{AudioFileConverter::OggVorbisAudioFormat};
    bool abort{false};
};

AudioFileConverter::AudioFileConverter(QObject* parent)
    : QObject(parent)
    , d(new AudioFileConverterPrivate)
{
}

AudioFileConverter::~AudioFileConverter()
{
    delete d;
}

bool AudioFileConverter::convert(const QString& origin, const QString& destination, const AudioFormat& destinationFormat)
{
    qDebug() << Q_FUNC_INFO << origin << destination << destinationFormat;
    bool success{true};
    if (d->state != ConvertingState) {
        // If we are not already performing a conversion, let's begin this one
        d->state = ConvertingState;
        Q_EMIT stateChanged();
        d->progress = -1;
        Q_EMIT progressChanged();
        d->taskDescription = QString{"Initializing conversion..."};
        Q_EMIT taskDescriptionChanged();
        d->origin = origin;
        Q_EMIT originChanged();
        d->destination = destination;
        Q_EMIT destinationChanged();
        d->destinationFormat = destinationFormat;
        Q_EMIT destinationFormatChanged();
        qApp->processEvents();
        if (QFile::exists(origin)) {
            // The origin file exists, let's keep going
            if (QFile::exists(destination) == false) {
                QFileInfo destinationInfo{destination};
                QDir destinationPath{destinationInfo.path()};
                if (destinationPath.exists() == false) {
                    // If the path doesn't exist, create it (and if that fails, abort hard and describe why)
                    if (destinationPath.mkpath(destinationPath.path()) == false) {
                        d->taskDescription = QString{"Could not create the path intended to hold the destination file: %1"}.arg(destinationPath.path());
                        d->state = FailureState;
                        Q_EMIT taskDescriptionChanged();
                        Q_EMIT stateChanged();
                        success = false;
                    }
                }
                if (success) {
                    tracktion_engine::Engine *engine{Plugin::instance()->getTracktionEngine()};
                    juce::File originFile = juce::File(origin.toStdString());
                    tracktion_engine::AudioFile *originAudioFile = new tracktion_engine::AudioFile(*engine, originFile);
                    if (originAudioFile) {
                        AudioFormatReader *reader{originAudioFile->getFormat()->createReaderFor(originFile.createInputStream().release(), true)};
                        if (reader) {
                            int numChannels{originAudioFile->getNumChannels()};
                            juce::File destinationFile = juce::File(destination.toStdString());
                            AudioFormatWriter *writer{nullptr};
                            switch(d->destinationFormat) {
                                case OggVorbisAudioFormat:
                                {
                                    juce::OggVorbisAudioFormat format;
                                    writer = format.createWriterFor(destinationFile.createOutputStream().release(), originAudioFile->getSampleRate(), uint(numChannels), originAudioFile->getBitsPerSample(), {}, 10);
                                    break;
                                }
                                case OggVorbisLowQualityAudioFormat:
                                {
                                    juce::OggVorbisAudioFormat format;
                                    writer = format.createWriterFor(destinationFile.createOutputStream().release(), originAudioFile->getSampleRate(), uint(numChannels), originAudioFile->getBitsPerSample(), {}, 5);
                                    break;
                                }
                                case WaveAudioFormat:
                                {
                                    juce::WavAudioFormat format;
                                    writer = format.createWriterFor(destinationFile.createOutputStream().release(), originAudioFile->getSampleRate(), uint(numChannels), originAudioFile->getBitsPerSample(), {}, 1);
                                    break;
                                }
                            }
                            static int chunkSize{8192};
                            juce::int64 totalSampleCount{originAudioFile->getLengthInSamples()};
                            juce::int64 currentSamplePosition{0};
                            d->taskDescription = QString{"Performing audio conversion..."};
                            while (currentSamplePosition < totalSampleCount) {
                                bool readWriteSuccessful = writer->writeFromAudioReader(*reader, currentSamplePosition, chunkSize);
                                if (readWriteSuccessful == false) {
                                    qDebug() << Q_FUNC_INFO << "Failed to read/write the audio files...";
                                }
                                currentSamplePosition += chunkSize;
                                d->progress = qMin(1.0, double(currentSamplePosition) / double(totalSampleCount));
                                Q_EMIT progressChanged();
                                qApp->processEvents();
                                if (d->abort) {
                                    d->taskDescription = QString{"Audio file conversion aborted"};
                                    Q_EMIT taskDescriptionChanged();
                                    d->state = AbortedState;
                                    Q_EMIT stateChanged();
                                }
                            }
                            // Delete the writer to inform it that we're done (this causes
                            // the writer to flush what's in the buffer and finish up writing the file)
                            delete writer;
                            if (d->abort == false) {
                                d->taskDescription = QString{"Copying tags to new file..."};
                                d->progress = -1;
                                Q_EMIT taskDescriptionChanged();
                                Q_EMIT progressChanged();
                                qApp->processEvents();
                                AudioTagHelper *tagHelper{AudioTagHelper::instance()};
                                const QMap<QString, QString> tags = tagHelper->readWavMetadata(origin);
                                if (tags.count() > 0) {
                                    tagHelper->saveWavMetadata(destination, tags);
                                }
                                d->taskDescription = QString{"Completed audio file conversion"};
                                Q_EMIT taskDescriptionChanged();
                                d->state = SuccessState;
                                Q_EMIT stateChanged();
                                qApp->processEvents();
                            }
                            // Finally, remember to clean up...
                            d->progress = 0;
                            Q_EMIT progressChanged();
                            delete reader;
                            delete originAudioFile;
                        } else {
                            d->taskDescription = QString{"The origin file is not audio: %1"}.arg(origin);
                            d->state = FailureState;
                            Q_EMIT taskDescriptionChanged();
                            Q_EMIT stateChanged();
                            success = false;
                        }
                    } else {
                        d->taskDescription = QString{"The origin file could not be opened: %1"}.arg(origin);
                        d->state = FailureState;
                        Q_EMIT taskDescriptionChanged();
                        Q_EMIT stateChanged();
                        success = false;
                    }
                }
            } else {
                d->taskDescription = QString{"The destination already exists, and will need removing before you can perform the conversion: %1"}.arg(origin);
                d->state = FailureState;
                Q_EMIT taskDescriptionChanged();
                Q_EMIT stateChanged();
                success = false;
            }
        } else {
            d->taskDescription = QString{"Cannot convert a file which doesn't exist: %1"}.arg(origin);
            d->state = FailureState;
            Q_EMIT taskDescriptionChanged();
            Q_EMIT stateChanged();
            success = false;
        }
    } else {
        success = false;
    }
    return success;
}

bool AudioFileConverter::abort(bool deleteLeftovers)
{
    bool success{true};
    d->abort = true;
    if (d->state == ConvertingState) {
        while (d->state != AbortedState) {
            qApp->processEvents();
        }
        if (deleteLeftovers) {
            // If the destination file exists, get rid of it
            QFile destinationFile{d->destination};
            if (destinationFile.exists()) {
                destinationFile.remove();
            }
        }
    } else {
        success = false;
    }
    d->abort = false;
    return success;
}

double AudioFileConverter::progress() const
{
    return d->progress;
}

QString AudioFileConverter::taskDescription() const
{
    return d->taskDescription;
}

QString AudioFileConverter::origin() const
{
    return d->origin;
}

QString AudioFileConverter::destination() const
{
    return d->destination;
}

AudioFileConverter::AudioFormat AudioFileConverter::destinationFormat() const
{
    return d->destinationFormat;
}

AudioFileConverter::ConversionState AudioFileConverter::state() const
{
    return d->state;
}
