
#include "SamplerSynthSound.h"
#include "AudioLevels.h"
#include "SamplerSynth.h"
#include "JUCEHeaders.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRunnable>
#include <QString>
#include <QTimer>
#include <QThreadPool>

namespace tracktion_engine {
#include <tracktion_engine/3rd_party/soundtouch/include/SoundTouch.h>
};

class SamplerSynthSoundTimestretcher : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit SamplerSynthSoundTimestretcher(ClipAudioSource *clip, const juce::AudioBuffer<float> *inputData, SamplerSynthSoundPrivate *parent = nullptr);
    ~SamplerSynthSoundTimestretcher() override;

    void run() override;
    Q_SLOT void abort();
    Q_SIGNAL void done();
    juce::AudioBuffer<float> data;
    int sampleLength{0};
    double stretchRate{1.0f};
private:
    class Private;
    Private* d{nullptr};
};

class SamplerSynthSoundPrivate : public QObject {
Q_OBJECT
public:
    SamplerSynthSoundPrivate(SamplerSynthSound *q)
        : q(q)
        , thumbnail(512, AudioLevels::instance()->m_formatManager, AudioLevels::instance()->m_thumbnailsCache)
    {
        soundLoader.moveToThread(qApp->thread());
        soundLoader.setInterval(1);
        soundLoader.setSingleShot(true);
        connect(&soundLoader, &QTimer::timeout, this, &SamplerSynthSoundPrivate::loadSoundData);
        playbackDataUpdater.moveToThread(qApp->thread());
        playbackDataUpdater.setInterval(0);
        playbackDataUpdater.setSingleShot(true);
        connect(&playbackDataUpdater, &QTimer::timeout, this, &SamplerSynthSoundPrivate::updatePlaybackDataActual);
    }
    ~SamplerSynthSoundPrivate() {
        if (thumbnailSource) {
            delete thumbnailSource;
        }
    }

    SamplerSynthSound *q{nullptr};
    QTimer soundLoader;
    std::unique_ptr<juce::AudioBuffer<float>> data;
    int length{0};
    double sourceSampleRate{0.0f};
    double sampleRateRatio{0.0f};
    size_t audioBufferLength{8192};

    ClipAudioSource *clip{nullptr};
    juce::FileInputSource *thumbnailSource{nullptr};
    tracktion::TracktionThumbnail thumbnail;

    bool loadingSoundDataPostponed{false};
    void loadSoundData() {
        clip->startProcessing("Loading...");
        thumbnail.clear();
        if (QFileInfo(clip->getPlaybackFile().getFile().getFullPathName().toRawUTF8()).exists()) {
            if (loadingSoundDataPostponed) {
                qDebug() << Q_FUNC_INFO << "Loading sound data for" << clip->getFilePath();
                loadingSoundDataPostponed = false;
            }
            AudioFormatReader *format{nullptr};
            juce::File file = clip->getPlaybackFile().getFile();
            tracktion::AudioFileInfo fileInfo = clip->getPlaybackFile().getInfo();
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
                juce::FileInputSource *newSource{new juce::FileInputSource(file)};
                thumbnail.setSource(newSource);
                if (thumbnailSource) {
                    delete thumbnailSource;
                }
                thumbnailSource = newSource;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to create a format reader for" << file.getFullPathName().toUTF8();
            }
            clip->endProcessing();
        } else {
            qDebug() << Q_FUNC_INFO << "Postponing loading sound data for" << clip->getFilePath() << "100ms as the playback file is not there yet...";
            loadingSoundDataPostponed = true;
            soundLoader.start(100);
            clip->setProcessingDescription("Waiting For File...");
        }
    }

    QTimer playbackDataUpdater;
    void updatePlaybackData() {
        playbackDataUpdater.start();
    }
    SamplerSynthSoundTimestretcher *activeTimeStretcher{nullptr};
    void updatePlaybackDataActual() {
        if (activeTimeStretcher) {
            // qDebug() << Q_FUNC_INFO << "Existing timestretcher active, disconnecting, aborting, and removing" << activeTimeStretcher;
            activeTimeStretcher->disconnect();
            connect(activeTimeStretcher, &SamplerSynthSoundTimestretcher::done, activeTimeStretcher, &QObject::deleteLater, Qt::QueuedConnection);
            activeTimeStretcher->abort();
        }
        if (clip->timeStretchStyle() == ClipAudioSource::TimeStretchOff) {
            // qDebug() << Q_FUNC_INFO << "No time stretching required for clip" << clip->getFileName();
            completedTimeStretcher = nullptr;
            timeStretcherNeedsChanging = true;
        } else {
            clip->startProcessing("Stretching Time...");
            activeTimeStretcher = new SamplerSynthSoundTimestretcher(clip, data.get(), this);
            // qDebug() << Q_FUNC_INFO << "Creating new timestretcher for clip" << clip->getFileName() << "with style" << clip->timeStretchStyle() << "pitch" << clip->pitch() << "and speed ratio" << clip->speedRatio() << activeTimeStretcher;
            connect(activeTimeStretcher, &SamplerSynthSoundTimestretcher::done, this, &SamplerSynthSoundPrivate::timeStretcherCompleted, Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(activeTimeStretcher);
        }
    }
    SamplerSynthSoundTimestretcher *completedTimeStretcher{nullptr};
    bool timeStretcherNeedsChanging{false};
    Q_SLOT void timeStretcherCompleted() {
        // Swapping the playback timestretcher instance out needs to be done in the jack process loop, to ensure it doesn't end up getting swapped half way through a run
        if (completedTimeStretcher) {
            // qDebug() << Q_FUNC_INFO << "We've got an old completed timestretcher, so let's get rid of that";
            SamplerSynthSoundTimestretcher *oldCompleted = completedTimeStretcher;
            completedTimeStretcher = nullptr;
            oldCompleted->deleteLater();
        }
        completedTimeStretcher = activeTimeStretcher;
        activeTimeStretcher = nullptr;
        completedTimeStretcher->disconnect();
        timeStretcherNeedsChanging = true;
        clip->endProcessing();
    }
    SamplerSynthSoundTimestretcher *playbackTimeStretcher{nullptr};
};

SamplerSynthSound::SamplerSynthSound(ClipAudioSource *clip)
    : d(new SamplerSynthSoundPrivate(this))
{
    leftBuffer = new float[d->audioBufferLength]();
    rightBuffer = new float[d->audioBufferLength]();
    d->clip = clip;
    d->loadSoundData();
    QObject::connect(clip, &ClipAudioSource::playbackFileChanged, &d->soundLoader, [this](){ isValid = false; d->soundLoader.start(1); }, Qt::QueuedConnection);
    QObject::connect(clip, &ClipAudioSource::timeStretchStyleChanged, d, [this](){ d->updatePlaybackData(); });
    QObject::connect(clip, &ClipAudioSource::speedRatioChanged, d, [this](){ d->updatePlaybackData(); });
    QObject::connect(clip, &ClipAudioSource::pitchChanged, d, [this](){ d->updatePlaybackData(); });
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
    if (d->timeStretcherNeedsChanging) {
        if (d->playbackTimeStretcher) {
            // As this posts an event, it doesn't actually do any major memory type stuff, so we can get away with doing it in the process loop
            d->playbackTimeStretcher->deleteLater();
        }
        d->playbackTimeStretcher = d->completedTimeStretcher;
        d->completedTimeStretcher = nullptr;
        d->timeStretcherNeedsChanging = false;
    }
    if (d->playbackTimeStretcher && d->clip->timeStretchStyle() != ClipAudioSource::TimeStretchOff) {
        return &d->playbackTimeStretcher->data;
    }
    return d->data.get();
}

const int & SamplerSynthSound::length() const
{
    if (d->playbackTimeStretcher && d->clip->timeStretchStyle() != ClipAudioSource::TimeStretchOff) {
        return d->playbackTimeStretcher->sampleLength;
    }
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

const double & SamplerSynthSound::sourceSampleRate() const
{
    return d->sourceSampleRate;
}

const double & SamplerSynthSound::stretchRate() const
{
    static const double noStretch{1.0};
    if (d->playbackTimeStretcher && d->clip->timeStretchStyle() != ClipAudioSource::TimeStretchOff) {
        return d->playbackTimeStretcher->stretchRate;
    }
    return noStretch;
}

const double & SamplerSynthSound::sampleRateRatio() const
{
    return d->sampleRateRatio;
}

tracktion::TracktionThumbnail * SamplerSynthSound::thumbnail()
{
    return &d->thumbnail;
}

class SamplerSynthSoundTimestretcher::Private {
public:
    Private() {}
    ~Private() {}
    SamplerSynthSoundPrivate *parent{nullptr};
    ClipAudioSource *clip{nullptr};
    const juce::AudioBuffer<float> *inputData{nullptr};
    tracktion::soundtouch::SoundTouch soundTouch;

    bool abort{false};
    QMutex abortMutex;
    bool isAborted() {
        QMutexLocker locker(&abortMutex);
        return abort;
    }
};

SamplerSynthSoundTimestretcher::SamplerSynthSoundTimestretcher(ClipAudioSource *clip, const juce::AudioBuffer<float> *inputData, SamplerSynthSoundPrivate *parent)
    : QObject(nullptr)
    , d(new Private)
{
    static int consequtive{0};
    ++consequtive;
    setObjectName(QString("TimeStretcher-%1").arg(consequtive));
    d->parent = parent;
    d->clip = clip;
    d->inputData = inputData;
    setAutoDelete(false);
}

SamplerSynthSoundTimestretcher::~SamplerSynthSoundTimestretcher()
{
    qDebug() << Q_FUNC_INFO << this;
    delete d;
}

void SamplerSynthSoundTimestretcher::abort()
{
    QMutexLocker locker(&d->abortMutex);
    d->abort = true;
}

void SamplerSynthSoundTimestretcher::run()
{
    // Our system's based around stereo playback, so... no reason to try and do more than that
    const int numChannels{d->inputData->getNumChannels() == 1 ? 1 : 2};
    const int numSamples{d->inputData->getNumSamples()};

    d->soundTouch.setChannels(uint(numChannels));
    d->soundTouch.setSampleRate(d->parent->sourceSampleRate);
    if (d->clip->timeStretchStyle() == ClipAudioSource::TimeStretchStandard) {
        d->soundTouch.setSetting(SETTING_USE_AA_FILTER, 1); // Default when SOUNDTOUCH_PREVENT_CLICK_AT_RATE_CROSSOVER is not defined
        d->soundTouch.setSetting(SETTING_AA_FILTER_LENGTH, 64); // Default value set in the RateTransposer ctor
        d->soundTouch.setSetting(SETTING_USE_QUICKSEEK, 0); // Default value set in TDStretch ctor
        d->soundTouch.setSetting(SETTING_SEQUENCE_MS, 0); // Default value - defined as DEFAULT_SEQUENCE_MS USE_AUTO_SEQUENCE_LEN ( = 0)
        d->soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 0); // Default value - defined as DEFAULT_SEEKWINDOW_MS USE_AUTO_SEEKWINDOW_LEN ( = 0)
    } else if (d->clip->timeStretchStyle() == ClipAudioSource::TimeStretchBetter) {
        // The settings used by the tracktion timestretcher's SoundTouchBetter setting
        d->soundTouch.setSetting(SETTING_USE_AA_FILTER, 1);
        d->soundTouch.setSetting(SETTING_AA_FILTER_LENGTH, 64);
        d->soundTouch.setSetting(SETTING_USE_QUICKSEEK, 0);
        d->soundTouch.setSetting(SETTING_SEQUENCE_MS, 60);
        d->soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 25);
    }
    d->soundTouch.setTempo(d->clip->speedRatio());
    d->soundTouch.setPitch(d->clip->pitchChangePrecalc());

    // Resize the buffer to be able to fit the new samples (setting it just a little higher than we
    // actually need, to make sure we have enough space for all the samples, as flushing SoundTouch
    // will produce potentially some blank samples at the end to fill the output buffer up)
    const int initialFeedSize = d->soundTouch.getSetting(SETTING_INITIAL_LATENCY);
    data.setSize(numChannels, numSamples * d->soundTouch.getInputOutputSampleRatio() + initialFeedSize, true);
    // qDebug() << Q_FUNC_INFO << "Set the size of our output buffer to" << data.getNumSamples() << "based on" << numSamples << "input samples, an output ratio of" << d->soundTouch.getInputOutputSampleRatio() << "and initial feed size of" << initialFeedSize;

    const size_t blockSize{512};
    const size_t stereoBlockSize{1024};
    float readBuffer[stereoBlockSize];
    int sampleWritePosition{0};
    auto fetchReadySamples = [this, &readBuffer, &sampleWritePosition, numChannels](){
        int retrievedSamplesCount{0};
        do {
            if (d->isAborted()) {
                break;
            }
            retrievedSamplesCount = int(d->soundTouch.receiveSamples(readBuffer, blockSize));
            // Write the interleaved data into the buffer
            for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex) {
                const float* channelSource = readBuffer + channelIndex;
                for (int sampleIndex = 0; sampleIndex < retrievedSamplesCount; ++sampleIndex) {
                    if (sampleWritePosition + sampleIndex > data.getNumSamples()) {
                        qWarning() << Q_FUNC_INFO << "The write position is now larger than the amount of space we've got for samples. We've got space for" << data.getNumSamples() << "and were asked to write into at least position" << sampleWritePosition + sampleIndex;
                    }
                    data.setSample(channelIndex, sampleWritePosition + sampleIndex, *channelSource);
                    channelSource += numChannels;
                }
            }
            sampleWritePosition += retrievedSamplesCount;
        } while (retrievedSamplesCount != 0);
    };

    int startSample{0};
    qint64 numLeft{numSamples};
    // Create a scratch buffer to contain the interleaved samples to send to SoundTouch
    float interleaveBuffer[stereoBlockSize];
    while (numLeft > 0) {
        if (d->isAborted()) {
            break;
        }
        d->clip->setProcessingProgress(float(numSamples - numLeft) / float(numSamples));
        // Either read our desired block size, or whatever is left, whichever is shorter
        const int numThisTime{int(qMin(numLeft, qint64(blockSize)))};
        // qDebug() << Q_FUNC_INFO << "Operating on" << numThisTime << "samples, with" << numLeft << "remaining";
        // Now feed stuff into SoundTouch
        if (numChannels == 1) {
            // For a single channel, we can just pass that single channel's read pointer
            d->soundTouch.putSamples(d->inputData->getReadPointer(0, startSample), uint(numThisTime));
        } else {
            // For stereo content, create an interleaved selection of samples as SoundTouch wants them
            const float *inputArray[2]{d->inputData->getReadPointer(0, startSample), d->inputData->getReadPointer(1, startSample)};
            juce::AudioDataConverters::interleaveSamples(inputArray, interleaveBuffer, numThisTime, numChannels);
            // Now feed stuff into SoundTouch
            d->soundTouch.putSamples(interleaveBuffer, uint(numThisTime));
        }
        // If there are any samples ready, pull them out
        fetchReadySamples();
        // Next run...
        startSample += numThisTime;
        numLeft -= numThisTime;
    }
    if (d->isAborted() == false) {
        // Make sure that we're blushed out whatever's left in the algorithm (note there will likely be empty samples at the end)
        d->soundTouch.flush();
        if (d->isAborted() == false) {
            // Retrieve whatever remaining samples might exist
            fetchReadySamples();
            if (d->isAborted() == false) {
                // const int previousSize{data.getNumSamples()};
                // Resize the buffer to be the exact size we're supposed to have been given
                data.setSize(numChannels, numSamples * d->soundTouch.getInputOutputSampleRatio(), true);
                sampleLength = data.getNumSamples();
                stretchRate = d->clip->speedRatio();
                // qDebug() << Q_FUNC_INFO << "Sample has been stretched and whatnot, and the rate by which that is a thing is" << stretchRate << "after reducing the buffer size by" << previousSize - sampleLength;
            }
        }
    }
    // And finally, tell anybody who cares that we're done
    Q_EMIT done();
}

// Since our pimpl is a qobject, let's make sure we do it properly
#include "SamplerSynthSound.moc"
