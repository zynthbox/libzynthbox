
#include "SamplerSynthSound.h"
#include "SamplerSynth.h"
#include "JUCEHeaders.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QString>
#include <QTimer>

class SamplerSynthSoundPrivate : public QObject {
Q_OBJECT
public:
    SamplerSynthSoundPrivate(SamplerSynthSound *q)
        : q(q)
    {
        soundLoader.moveToThread(qApp->thread());
        soundLoader.setInterval(1);
        soundLoader.setSingleShot(true);
        connect(&soundLoader, &QTimer::timeout, this, &SamplerSynthSoundPrivate::loadSoundData);
    }

    SamplerSynthSound *q{nullptr};
    QTimer soundLoader;
    std::unique_ptr<juce::AudioBuffer<float>> data;
    int length{0};
    double sourceSampleRate{0.0f};
    double sampleRateRatio{0.0f};
    size_t audioBufferLength{8192};

    ClipAudioSource *clip{nullptr};

    bool loadingSoundDataPostponed{false};
    void loadSoundData() {
        if (QFileInfo(clip->getPlaybackFile().getFile().getFullPathName().toRawUTF8()).exists()) {
            if (loadingSoundDataPostponed) {
                qDebug() << Q_FUNC_INFO << "Loading sound data for" << clip->getFilePath();
                loadingSoundDataPostponed = false;
            }
            AudioFormatReader *format{nullptr};
            juce::File file = clip->getPlaybackFile().getFile();
            tracktion_engine::AudioFileInfo fileInfo = clip->getPlaybackFile().getInfo();
            MemoryMappedAudioFormatReader *memoryFormat = fileInfo.format->createMemoryMappedReader(file);
            if (memoryFormat && memoryFormat->mapEntireFile()) {
                format = memoryFormat;
            }
            if (!format) {
                format = fileInfo.format->createReaderFor(file.createInputStream().release(), true);
            }
            if (format) {
                sourceSampleRate = format->sampleRate;
                if (sourceSampleRate > 0 && format->lengthInSamples > 0)
                {
                    sampleRateRatio = sourceSampleRate / SamplerSynth::instance()->sampleRate();
                    length = (int) format->lengthInSamples;
                    juce::AudioBuffer<float> *newBuffer = new juce::AudioBuffer<float>(jmin(2, int(format->numChannels)), length);
                    format->read(newBuffer, 0, length, 0, true, true);
                    data.reset(newBuffer);
                    q->isValid = true;
                }
                qDebug() << Q_FUNC_INFO << "Loaded data at sample rate" << sourceSampleRate << "from playback file" << clip->getPlaybackFile().getFile().getFullPathName().toRawUTF8();
                delete format;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to create a format reader for" << file.getFullPathName().toUTF8();
            }
        } else {
            qDebug() << Q_FUNC_INFO << "Postponing loading sound data for" << clip->getFilePath() << "100ms as the playback file is not there yet...";
            loadingSoundDataPostponed = true;
            soundLoader.start(100);
        }
    }
};

SamplerSynthSound::SamplerSynthSound(ClipAudioSource *clip)
    : d(new SamplerSynthSoundPrivate(this))
{
    leftBuffer = new float[d->audioBufferLength]();
    rightBuffer = new float[d->audioBufferLength]();
    d->clip = clip;
    d->loadSoundData();
    QObject::connect(clip, &ClipAudioSource::speedRatioChanged, &d->soundLoader, [this](){ isValid = false; if (d->data) { d->length = 0; d->data.release(); } }, Qt::QueuedConnection);
    QObject::connect(clip, &ClipAudioSource::playbackFileChanged, &d->soundLoader, [this](){ isValid = false; d->soundLoader.start(1); }, Qt::QueuedConnection);
}

SamplerSynthSound::~SamplerSynthSound()
{
    delete d;
    delete leftBuffer;
    delete rightBuffer;
}

ClipAudioSource *SamplerSynthSound::clip() const
{
    return d->clip;
}

juce::AudioBuffer<float> *SamplerSynthSound::audioData() const noexcept
{
    return d->data.get();
}

int SamplerSynthSound::length() const
{
    return d->length;
}

int SamplerSynthSound::startPosition(int slice) const
{
    return d->clip->getStartPosition(slice) * d->sourceSampleRate;
}

int SamplerSynthSound::stopPosition(int slice) const
{
    return d->clip->getStopPosition(slice) * d->sourceSampleRate;
}

int SamplerSynthSound::rootMidiNote() const
{
    return d->clip->rootNote();
}

double SamplerSynthSound::sourceSampleRate() const
{
    return d->sourceSampleRate;
}

double SamplerSynthSound::sampleRateRatio() const
{
    return d->sampleRateRatio;
}

// Since our pimpl is a qobject, let's make sure we do it properly
#include "SamplerSynthSound.moc"
