#include "SamplerSynthVoice.h"

#include "ClipAudioSourcePositionsModel.h"
#include "ClipAudioSourceSliceSettings.h"
#include "ClipAudioSourceSubvoiceSettings.h"
#include "ClipCommand.h"
#include "GainHandler.h"
#include "SamplerSynth.h"
#include "SamplerSynthSound.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QtMath>

#define DataRingSize 256
class SamplerSynthVoiceDataRing {
public:
    struct alignas(64) Entry {
        Entry *previous{nullptr};
        Entry *next{nullptr};
        jack_nframes_t time{0};
        float data{-1};
        int channel{-1};
        int note{-1};
        bool processed{true};
    };

    SamplerSynthVoiceDataRing () {
        Entry* entryPrevious{&ringData[DataRingSize - 1]};
        for (quint64 i = 0; i < DataRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~SamplerSynthVoiceDataRing() {}
    void write(jack_nframes_t time, float data, int midiChannel = -1, int midiNote = -1) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data stored at the write location:" << writeHead->data << "for time" << writeHead->time << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->time = time;
        entry->data = data;
        entry->channel = midiChannel;
        entry->note = midiNote;
        entry->processed = false;
    }
    float read(int *midiChannel, int *midiNote) {
        Entry *entry = readHead;
        readHead = readHead->next;
        float data = entry->data;
        if (midiChannel) {
            *midiChannel = entry->channel;
        }
        if (midiNote) {
            *midiNote = entry->note;
        }
        entry->processed = true;
        return data;
    }
    Entry ringData[DataRingSize];
    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
    QString name;
};

#define PlayheadCount 2
struct PlayheadData {
public:
    PlayheadData() {}
    double sourceSamplePosition{0};
    // Start, loop, stop, and fade positions are fetched:
    // - When a playhead is activated
    // - At an interval no longer than the sample rate (unless the playhead is already fading, at which point simply let it continue)
    int startPosition{0};
    int loopPosition{0};
    int stopPosition{0};
    // The fade positions are fetched directly from ClipAudioSource, and are calculated there
    // An "outie" fade for the loop start position begins with gain 0 at the position loopFadeAdjustment back from the loop start position, and reaches gain 1 at the position of the loop's start
    // An "innie" fade for the loop start position begins with gain 0 at the position of the loop's start, and fades up to gain 1 at loopFadeAdjustment samples forward from the loop's start
    int loopFadeAdjustment{0}; // Negative for an "outie" fade, positive for an "innie" fade, 0 for no fading
    // An "innie" fade for the loop stop position begins with gain 1 stopFadeAdjustment samples back from the loop's stop position, and reaches gain 0 at the loop's stop position
    // An "outie" fade for the loop stop position begins with gain 1 at the loop's stop position, and reaches gain 0 at stopFadeAdjustment samples forward of the loop's stop position
    int stopFadeAdjustment{0}; // Negative for an "innie" fade, positive for an "outie" fade, 0 for no fading
    // Pre-calculated sample positions to use for comparison and envelope calculations
    double attackStartSample{0}, attackEndSample{0}, attackDuration{0}, decayStartSample{0}, decayEndSample{0}, decayDuration{0};
    double playheadGain{1.0};
    bool startedNextPlayhead{false};
    bool active{false};
    int samplesSinceLastUpdate{0};
    int sampleRate{48000};
    const ClipAudioSource *clip{nullptr};
    const ClipAudioSourceSliceSettings *slice{nullptr};
    const ClipCommand *clipCommand{nullptr};
    const SamplerSynthSound *sound{nullptr};
    PlayheadData *nextPlayhead{nullptr};
    enum PlaybackStartPosition {
        StartPositionBeginning,
        StartPositionLoopPoint,
        StartPositionStopPoint,
    };
    PlaybackStartPosition playbackStartPosition{StartPositionBeginning};
    // Set startFromTheStart to true to begin at gain 1 instead start from the sample's start position (otherwise we start from the loop fade position and apply the attack logic)
    void start(const ClipAudioSource *theClip, const ClipAudioSourceSliceSettings *theSlice, const ClipCommand *theClipCommand, const SamplerSynthSound *theSound, const int &theSampleRate, const PlaybackStartPosition thePlaybackStartPosition) {
        startedNextPlayhead = false;
        clip = theClip;
        slice = theSlice;
        clipCommand = theClipCommand;
        sound = theSound;
        sampleRate = theSampleRate;
        playbackStartPosition = thePlaybackStartPosition;
        active = true;
        updatePositions(true);
        switch(playbackStartPosition) {
            case StartPositionBeginning:
                playheadGain = 1.0;
                sourceSamplePosition = startPosition;
                break;
            case StartPositionLoopPoint:
                playheadGain = 0.0;
                sourceSamplePosition = attackStartSample;
                break;
            case StartPositionStopPoint:
                playheadGain = 0.0;
                sourceSamplePosition = decayEndSample;
                break;
        }
    }
    // Call this at the *end* of the process loop, once current state has been handled
    // Doing it at the end ensures that, when we need to start new playheads, we will be in sync come next run
    void progress(double byHowManySamples) {
        sourceSamplePosition += byHowManySamples;
        bool startNextPlayhead{false};
        double nextPlayheadOffset{0.0};
        if (sourceSamplePosition < attackStartSample) {
            // Before the attack start position
            if (playbackStartPosition != StartPositionBeginning) {
                playheadGain = 0;
            }
            if (byHowManySamples < 0) {
                // If we are moving backward, stop this playhead
                if (clipCommand->looping) {
                    // If we're looping and got to here without having already started a playhead (that is, we're not crossfading), start the next playhead now
                    startNextPlayhead = true;
                    // Offset the next playhead by the exact amount we are ahead of the current position (to ensure we are phase correct)
                    nextPlayheadOffset = attackStartSample - sourceSamplePosition;
                }
                playheadGain = 0;
                stop();
            }
        } else if (sourceSamplePosition < attackEndSample) {
            // Between the attack start (inclusive) and attack end (exclusive)
            if (playbackStartPosition != StartPositionBeginning || byHowManySamples < 0) {
                // We only want to apply the envelope if we are running the loop, or we are moving backwards
                if (attackDuration > 0) {
                    playheadGain = (attackEndSample - sourceSamplePosition) / attackDuration;
                    // qDebug() << Q_FUNC_INFO << this << "attack" << playheadGain;
                } else {
                    playheadGain = 1.0;
                }
            } else {
                playheadGain = 1.0;
            }
            if (byHowManySamples < 0 && clipCommand->looping) {
                // If we're moving backwards and looping, start the playhead we're crossfading into now
                startNextPlayhead = true;
            }
        } else if (sourceSamplePosition < decayStartSample) {
            // Between the attack end (inclusive) and the decay start (exclusive)
            playheadGain = 1;
        } else if (sourceSamplePosition < decayEndSample) {
            // Between the decay start (inclusive) and decay end (exclusive)
            if (playbackStartPosition != StartPositionBeginning || byHowManySamples > 0) {
                // We only want to apply the envelope if we are running the loop, or we are moving forward
                if (decayDuration > 0) {
                    playheadGain = 1.0 - ((decayEndSample - sourceSamplePosition) / decayDuration);
                    // qDebug() << Q_FUNC_INFO << this << "decay" << playheadGain;
                } else {
                    playheadGain = 1.0;
                }
            } else {
                playheadGain = 1.0;
            }
            if (byHowManySamples > 0 && clipCommand->looping) {
                // If we are moving forward and looping, start the playhead we're crossfading into now
                startNextPlayhead = true;
            }
        } else {
            // On or after the decay end sample
            if (playbackStartPosition != StartPositionBeginning) {
                playheadGain = 0;
            }
            if (byHowManySamples > 0) {
                // If we are moving forward, stop this playhead
                if (clipCommand->looping) {
                    // If we're looping and got to here without having already started a playhead (that is, we're not crossfading), start the next playhead now
                    startNextPlayhead = true;
                    // Offset the next playhead by the exact amount we are ahead of the current position (to ensure we are phase correct)
                    nextPlayheadOffset = sourceSamplePosition - decayEndSample;
                }
                playheadGain = 0;
                stop();
            }
        }
        if (startNextPlayhead && startedNextPlayhead == false) {
            if (byHowManySamples > 0) {
                nextPlayhead->start(clip, slice, clipCommand, sound, sampleRate, StartPositionLoopPoint);
            } else {
                nextPlayhead->start(clip, slice, clipCommand, sound, sampleRate, StartPositionStopPoint);
            }
            // As we'll be progressing all the playheads immediately following starting them, let's ensure we're ready to be progressed
            nextPlayhead->sourceSamplePosition -= byHowManySamples;
            // Also, to ensure our phase is correct, we offset by how far the current step is now ahead
            nextPlayhead->sourceSamplePosition += nextPlayheadOffset;
            startedNextPlayhead = true;
        }
    }
    // Call this *after* each process loop
    void updateSamplesHandled(int numberOfSamples) {
        samplesSinceLastUpdate += numberOfSamples;
        if (samplesSinceLastUpdate >= sampleRate) {
            // qDebug() << Q_FUNC_INFO << "Updating positions at" << samplesSinceLastUpdate << "samples, higher or equal to" << sampleRate;
            updatePositions();
        }
    }
    void stop() {
        active = false;
    }
private:
    void updatePositions(bool initialFetch = false) {
        // If we're already performing a fade-out, don't update the positions for this playhead (we'll be gone shortly)
        if (sourceSamplePosition < decayStartSample || initialFetch) {
            startPosition = double((clipCommand->setStartPosition ? clipCommand->startPosition * clip->sampleRate() : slice->startPositionSamples())) / sound->stretchRate();
            stopPosition = double((clipCommand->setStopPosition ? clipCommand->stopPosition * clip->sampleRate() : slice->stopPositionSamples())) / sound->stretchRate();
            loopPosition = startPosition + (double(slice->loopDeltaSamples()) / sound->stretchRate());
            if (loopPosition >= stopPosition) {
                loopPosition = startPosition;
            }
            if (slice->playbackStyle() == ClipAudioSource::WavetableStyle) {
                loopFadeAdjustment = 0;
            } else {
                loopFadeAdjustment = double(slice->loopFadeAdjustment()) / sound->stretchRate();
            }
            if (loopFadeAdjustment < 0) {
                attackStartSample = loopPosition + loopFadeAdjustment;
                attackEndSample = loopPosition;
            } else {
                attackStartSample = loopPosition;
                attackEndSample = loopPosition + loopFadeAdjustment;
            }
            attackDuration = attackEndSample - attackStartSample;
            stopFadeAdjustment = double(slice->stopFadeAdjustment()) / sound->stretchRate();
            if (stopFadeAdjustment < 0) {
                decayStartSample = stopPosition + stopFadeAdjustment;
                decayEndSample = stopPosition;
            } else {
                decayStartSample = stopPosition;
                decayEndSample = stopPosition + stopFadeAdjustment;
            }
            decayDuration = decayEndSample - decayStartSample;
            // qDebug() << Q_FUNC_INFO << loopFadeAdjustment << attackStartSample << attackEndSample << attackDuration << decayStartSample << decayEndSample << decayDuration;
            samplesSinceLastUpdate = 0;
        }
    }
};

struct PlaybackData {
public:
    PlaybackData() {
        for (int playheadIndex = 0; playheadIndex < PlayheadCount - 1; ++ playheadIndex) {
            playheads[playheadIndex].nextPlayhead = &playheads[playheadIndex + 1];
        }
        playheads[PlayheadCount - 1].nextPlayhead = &playheads[0];
    }
    juce::AudioBuffer<float>* data{nullptr};
    const float *inL{nullptr};
    const float *inR{nullptr};
    double sourceSampleRate{0};
    double highpassCoefficient{0};
    double lowpassCoefficient{0};
    bool isLooping{false};
    bool snappedToBeat{false};
    float pan{0};
    int sampleDuration{0};
    int startPosition{0};
    int loopPosition{0};
    int stopPosition{0};
    double forwardTailingOffPosition{0};
    double backwardTailingOffPosition{0};
    double tempo{1.0};
    double pitch{1.0};
    PlayheadData playheads[PlayheadCount];
    // The sourceSamplePosition for the logical playback is separate from the playheads, as they will fade in and out independently of that position
};

class SamplerSynthVoicePrivate {
public:
    SamplerSynthVoicePrivate() {
        syncTimer = qobject_cast<SyncTimer*>(SyncTimer::instance());
        for (int i = 0; i < 128; ++i) {
            initialCC[i] = 0;
        }
        aftertouchRing.name = "aftertouchRing";
        pitchRing.name = "pitchRing";
        ccControlRing.name = "ccControlRing";
        ccValueRing.name = "ccValueRing";
    }

    // This is perhaps a little over-much, but it means we can handle
    // start/stop cycles so short that it fits inside a single process
    // run, as is needed for the granular playback mode
    ClipCommandRing commandRing;
    SamplerSynthVoiceDataRing aftertouchRing;
    SamplerSynthVoiceDataRing pitchRing;
    SamplerSynthVoiceDataRing ccControlRing;
    SamplerSynthVoiceDataRing ccValueRing;
    juce::ADSR adsr;
    SyncTimer *syncTimer{nullptr};
    SamplerSynth *samplerSynth{nullptr};
    ClipCommand *clipCommand{nullptr};
    ClipAudioSource *clip{nullptr};
    ClipAudioSourceSliceSettings *slice{nullptr};
    ClipAudioSourceSubvoiceSettings *subvoiceSettings{nullptr};
    SamplerSynthSound* sound{nullptr};
    double pitchRatio = 0;
    double sourceSamplePosition = 0;
    float targetGain = 0, lgain = 0, rgain = 0;
    // Used to make sure the first sample on looped playback is interpolated to an empty previous sample, rather than the previous sample in the loop
    bool firstRoll{true};

    float initialCC[128];
    int ccForHighpass{74};
    int ccForLowpass{1};
    float lowpassCutoff{0.0f};
    float highpassCutoff{0.0f};
    float allpassBufferL{0.0f};
    float allpassBufferR{0.0f};

    PlaybackData playbackData;
};

SamplerSynthVoice::SamplerSynthVoice(SamplerSynth *samplerSynth)
    : d(new SamplerSynthVoicePrivate)
{
    d->samplerSynth = samplerSynth;
}

SamplerSynthVoice::~SamplerSynthVoice()
{
    delete d;
}

// Instead of checking voice has a command, set an available-after timestamp when setting a clip
// - When adding a start command, set to INT_MAX
// - When adding a stop command, set to the given timestamp
// - Handle stop-all condition
// - When testing if a voice is available, check the voice's availableAfter against timestamp: if (timestamp > voice->availableAfter) { voice->handleCommand(clipCommand, timestamp); }
void SamplerSynthVoice::handleCommand(ClipCommand* clipCommand, jack_nframes_t timestamp)
{
    d->commandRing.write(clipCommand, timestamp);
    if (clipCommand->stopPlayback == true) {
        // Available after the tailoff period
        const ClipAudioSourceSliceSettings *slice{clipCommand->clip->sliceFromIndex(clipCommand->slice)};
        const double sourceSampleRate{clipCommand->clip->sampleRate()};
        const double release{slice->adsrRelease() * sourceSampleRate};
        availableAfter = timestamp + release;
        mostRecentStartCommand = nullptr;
    }
    // Not an else if, because we might both stop and start with the same command
    if (clipCommand->startPlayback == true) {
        if (clipCommand->looping == true) {
            availableAfter = UINT_MAX;
        } else {
            const double sourceSampleRate{clipCommand->clip->sampleRate()};
            const ClipAudioSourceSliceSettings *slice{clipCommand->clip->sliceFromIndex(clipCommand->slice)};
            const double startPosition = (int) ((clipCommand->setStartPosition ? clipCommand->startPosition * sourceSampleRate : slice->startPositionSamples()));
            const double stopPosition = (int) ((clipCommand->setStopPosition ? clipCommand->stopPosition * sourceSampleRate : slice->stopPositionSamples()));
            availableAfter = timestamp + (stopPosition - startPosition);
        }
        mostRecentStartCommand = clipCommand;
    }
}

void SamplerSynthVoice::checkExclusivity(ClipCommand* clipCommand, jack_nframes_t timestamp)
{
    // If the command calls for the same exclusivity group that we are in, stop playback of our current thing
    if (d->clipCommand->exclusivityGroup == clipCommand->exclusivityGroup) {
        ClipCommand* newCommand{d->syncTimer->getClipCommand()};
        newCommand->stopPlayback = true;
        newCommand->clip = d->clipCommand->clip;
        newCommand->slice = d->clipCommand->slice;
        newCommand->subvoice = d->clipCommand->subvoice;
        newCommand->volume = 1.0f;
        d->commandRing.write(newCommand, timestamp);
    }
}

void SamplerSynthVoice::setCurrentCommand(ClipCommand *clipCommand)
{
    if (d->clipCommand) {
        // This means we're changing what we should be doing in playback, and we need to update the old one
        if (clipCommand->changeLooping) {
            d->clipCommand->looping = clipCommand->looping;
            d->clipCommand->changeLooping = true;
        }
        if (clipCommand->changePitch) {
            d->clipCommand->pitchChange = clipCommand->pitchChange;
            d->clipCommand->changePitch = true;
        }
        if (clipCommand->changeSpeed) {
            d->clipCommand->speedRatio = clipCommand->speedRatio;
            d->clipCommand->changeSpeed = true;
        }
        if (clipCommand->changeGainDb) {
            d->clipCommand->gainDb = clipCommand->gainDb;
            d->clipCommand->changeGainDb = true;
        }
        if (clipCommand->changeVolume) {
            d->clipCommand->volume = clipCommand->volume;
            d->clipCommand->changeVolume = true;
            d->lgain = d->clipCommand->volume;
            d->rgain = d->clipCommand->volume;
            d->targetGain = d->clipCommand->volume;
        }
        if (clipCommand->startPlayback) {
            // This should be interpreted as "restart playback" in this case, so... reset the current position
            d->sourceSamplePosition = d->slice->startPositionSamples();
        }
        if (clipCommand->changePan) {
            d->clipCommand->pan = clipCommand->pan;
        }
        d->syncTimer->deleteClipCommand(clipCommand);
    } else {
        d->clipCommand = clipCommand;
    }
    for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
        PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
        if (playhead.active) {
            playhead.clipCommand = d->clipCommand;
        }
    }
    isPlaying = d->clipCommand;
}

void SamplerSynthVoice::setModwheel(int modwheelValue)
{
    d->initialCC[1] = modwheelValue;
}

static inline float nextSample(const float *source, const int &sourceLength, double *currentPosition, float *increment, const float &firstSample, const float &lastSample, const float &loopPosition, const ClipAudioSource::LoopStyle &loopStyle) {
    *currentPosition += *increment;
    if (*increment > 0) {
        // Currently moving forward
        if (*currentPosition > lastSample) {
            switch (loopStyle) {
                case ClipAudioSource::PingPongLoop:
                    // Invert the direction of the loop (so next time we'll be moving backwards)
                    *increment *= -1;
                    *currentPosition = lastSample + 1 - (*currentPosition - lastSample);
                    break;
                case ClipAudioSource::BackwardLoop:
                    // this condition will never happen (increment must always be negative when running loop backwards)
                    qWarning() << Q_FUNC_INFO << "Error in loop logic - somehow we've got a positive increment, but are supposed to be moving backwards";
                    break;
                case ClipAudioSource::ForwardLoop:
                default:
                    *currentPosition = loopPosition + (*currentPosition - lastSample);
                    break;
            }
        }
    } else {
        if (*currentPosition < firstSample) {
            switch (loopStyle) {
                case ClipAudioSource::PingPongLoop:
                    // Invert the direction of the loop (so next time we'll be moving forward)
                    *increment *= -1;
                    *currentPosition = firstSample + (firstSample - *currentPosition);
                    break;
                case ClipAudioSource::BackwardLoop:
                    *currentPosition = loopPosition + 1 - (firstSample - *currentPosition);
                    break;
                case ClipAudioSource::ForwardLoop:
                default:
                    // this condition will never happen (increment must always be positive when running loop forwards)
                    qWarning() << Q_FUNC_INFO << "Error in loop logic - somehow we've got a negative increment, but are supposed to be moving forwards";
                    break;
            }
        }
    }
    return (source != nullptr && int(*currentPosition) < sourceLength) ? source[int(*currentPosition)] : 0;
}

void SamplerSynthVoice::startNote(ClipCommand *clipCommand, jack_nframes_t timestamp)
{
    if (auto sound = d->samplerSynth->clipToSound(clipCommand->clip)) {
        d->sound = sound;
        d->clip = sound->clip();
        d->slice = d->clip->sliceFromIndex(clipCommand->slice);
        d->subvoiceSettings = clipCommand->subvoice == -1 ? nullptr : d->slice->subvoiceSettingsPlayback()[clipCommand->subvoice];
        d->playbackData.sourceSampleRate = d->sound->sourceSampleRate();

        d->playbackData.snappedToBeat = (trunc(d->slice->lengthBeats()) == d->slice->lengthBeats());
        d->playbackData.isLooping = d->clipCommand->looping;

        d->targetGain = clipCommand->volume;
        d->lgain = 0;
        d->rgain = 0;
        d->clipCommand->volume = 0;

        d->adsr.reset();
        d->adsr.setSampleRate(d->playbackData.sourceSampleRate);
        d->adsr.setParameters(d->slice->granular() ? d->slice->grainADSR().getParameters() : d->slice->adsrParameters());
        isTailingOff = false;
        d->adsr.noteOn();

        d->playbackData.data = d->sound->audioData();
        if (d->playbackData.data) {
            d->playbackData.inL = d->playbackData.data->getReadPointer(0);
            d->playbackData.inR = d->playbackData.data->getNumChannels() > 1 ? d->playbackData.data->getReadPointer(1) : d->playbackData.inL;
        } else {
            d->playbackData.inL = nullptr;
            d->playbackData.inR = nullptr;
        }
        d->playbackData.sampleDuration = sound->length();

        // this bit is basically mtof - that is, converts a midi note to its equivalent expected frequency (here given a 440Hz concert tone), and it's going to be somewhere within a reasonable amount along the audible scale
        const float highpassAdjustmentInHz = pow(2, ((127.0f * d->highpassCutoff) - 69) / 12) * 440;
        const double highpassTan = std::tan(M_PI * highpassAdjustmentInHz / d->playbackData.sourceSampleRate);
        d->playbackData.highpassCoefficient = (highpassTan - 1.f) / (highpassTan + 1.f);
        const float lowpassAdjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
        const double lowpassTan = std::tan(M_PI * lowpassAdjustmentInHz / d->playbackData.sourceSampleRate);
        d->playbackData.lowpassCoefficient = (lowpassTan - 1.f) / (lowpassTan + 1.f);

        d->playbackData.pan = std::clamp(float(d->slice->pan()) + d->clipCommand->pan + (d->subvoiceSettings ? d->subvoiceSettings->pan() : 0.0f), -1.0f, 1.0f);
        d->playbackData.startPosition = double((d->clipCommand->setStartPosition ? d->clipCommand->startPosition * d->playbackData.sourceSampleRate : d->slice->startPositionSamples())) / d->sound->stretchRate();
        d->playbackData.stopPosition = double((d->clipCommand->setStopPosition ? d->clipCommand->stopPosition * d->playbackData.sourceSampleRate : d->slice->stopPositionSamples())) / d->sound->stretchRate();
        d->playbackData.loopPosition = d->playbackData.startPosition + (double(d->slice->loopDeltaSamples()) / d->sound->stretchRate());
        if (d->playbackData.loopPosition >= d->playbackData.stopPosition) {
            d->playbackData.loopPosition = d->playbackData.startPosition;
        }
        d->playbackData.forwardTailingOffPosition = d->playbackData.stopPosition - (double(d->adsr.getParameters().release * d->playbackData.sourceSampleRate) / d->sound->stretchRate());
        d->playbackData.backwardTailingOffPosition = d->playbackData.startPosition + (double(d->adsr.getParameters().release * d->playbackData.sourceSampleRate) / d->sound->stretchRate());

        d->pitchRatio = std::pow(2.0, (clipCommand->midiNote - d->slice->rootNote()) / 12.0);
        if (d->clipCommand->changePitch && d->clipCommand->pitchChange < 0) {
            d->sourceSamplePosition = d->playbackData.stopPosition;
        } else {
            d->sourceSamplePosition = d->playbackData.startPosition;
        }

        if (clipCommand->looping == true) {
            availableAfter = UINT_MAX;
        } else {
            availableAfter = timestamp + jack_nframes_t(d->playbackData.stopPosition - d->playbackData.startPosition);
        }
        d->playbackData.playheads[0].start(d->clip, d->slice, d->clipCommand, d->sound, d->samplerSynth->sampleRate(), PlayheadData::StartPositionBeginning);
    } else {
        jassertfalse; // this object can only play SamplerSynthSounds!
    }
}

void SamplerSynthVoice::stopNote(float velocity, bool allowTailOff, jack_nframes_t timestamp, float peakGainLeft, float peakGainRight)
{
    if (velocity > 0) {
        // Note off velocity (aka "lift" for mpe) is going to need thought...
    }
    if (allowTailOff)
    {
        d->adsr.noteOff();
        isTailingOff = true;
    }
    else
    {
        d->adsr.reset();
        if (d->clip) {
            // Before we stop, send out one last update for this command
            if (d->clip->playbackPositionsModel()) {
                for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
                    const PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
                    if (playhead.active) {
                        if (peakGainLeft > -1 || peakGainRight > -1) {
                            d->clip->playbackPositionsModel()->setPositionData(timestamp, d->clipCommand, playheadIndex, peakGainLeft * playhead.playheadGain, peakGainRight * playhead.playheadGain, playhead.sourceSamplePosition / d->playbackData.sampleDuration, d->playbackData.pan);
                        } else {
                            d->clip->playbackPositionsModel()->setPositionData(timestamp, d->clipCommand, playheadIndex, 0.0, 0.0, playhead.sourceSamplePosition / d->playbackData.sampleDuration, d->playbackData.pan);
                        }
                    }
                }
            }
            d->clip = nullptr;
            d->slice = nullptr;
            d->sound = nullptr;
        }
        if (d->clipCommand) {
            d->syncTimer->deleteClipCommand(d->clipCommand);
            d->clipCommand = nullptr;
        }
        for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
            d->playbackData.playheads[playheadIndex].stop();
        }
        isPlaying = false;
        isTailingOff = false;
        d->firstRoll = true;
        d->allpassBufferL = d->allpassBufferR = 0.0f;
        availableAfter = timestamp;
    }
}

void SamplerSynthVoice::handleControlChange(jack_nframes_t time, int channel, int control, int value)
{
    d->ccControlRing.write(time, control, channel);
    d->ccValueRing.write(time, value, channel);
}

void SamplerSynthVoice::handleAftertouch(jack_nframes_t time, int channel, int note, int pressure)
{
    d->aftertouchRing.write(time, pressure, channel, note);
}

void SamplerSynthVoice::handlePitchChange(jack_nframes_t time, int channel, int note, float pitchValue) {
    d->pitchRing.write(time, pitchValue, channel, note);
}

inline float interpolateHermite4pt3oX(float x0, float x1, float x2, float x3, float t)
{
    float c0 = x1;
    float c1 = .5F * (x2 - x0);
    float c2 = x0 - (2.5F * x1) + (2 * x2) - (.5F * x3);
    float c3 = (.5F * (x3 - x0)) + (1.5F * (x1 - x2));
    return (((((c3 * t) + c2) * t) + c1) * t) + c0;
}

// if we perform highpass filtering, we need to invert the output of the allpass (multiply it by -1)
static const double highpassSign{-1.f};
void SamplerSynthVoice::process(jack_default_audio_sample_t */*leftBuffer*/, jack_default_audio_sample_t */*rightBuffer*/, jack_nframes_t nframes, jack_nframes_t current_frames, jack_time_t /*current_usecs*/, jack_time_t /*next_usecs*/, float /*period_usecs*/)
{
    float peakGainLeft{0.0f}, peakGainRight{0.0f};
    int dataChannel{0}, dataNote{0};

    // First, a quick sanity check, just to be on the safe side:
    // Ensure that the clip we're operating on is still known to the sampler
    if (d->clip && d->samplerSynth->clipToSound(d->clip) == nullptr) {
        stopNote(0, false, current_frames);
    }

    // We don't want to have super-high precision on this, as it's user control, but we
    // do want to be able to change the various sound settings at play-time (for
    // controlling loops and such), so let's make sure we do that once per process call
    // for any playing voice, in addition to when it starts
    if (d->clip && d->clipCommand) {
        d->playbackData.data = d->sound->audioData();
        if (d->playbackData.data) {
            d->playbackData.inL = d->playbackData.data->getReadPointer(0);
            d->playbackData.inR = d->playbackData.data->getNumChannels() > 1 ? d->playbackData.data->getReadPointer(1) : d->playbackData.inL;
        } else {
            d->playbackData.inL = nullptr;
            d->playbackData.inR = nullptr;
        }
        d->playbackData.sampleDuration = d->sound->length();
        d->playbackData.pan = std::clamp(float(d->slice->pan()) + d->clipCommand->pan + (d->subvoiceSettings ? d->subvoiceSettings->pan() : 0.0f), -1.0f, 1.0f);
        d->playbackData.startPosition = double((d->clipCommand->setStartPosition ? d->clipCommand->startPosition * d->playbackData.sourceSampleRate : d->slice->startPositionSamples())) / d->sound->stretchRate();
        d->playbackData.stopPosition = double((d->clipCommand->setStopPosition ? d->clipCommand->stopPosition * d->playbackData.sourceSampleRate : d->slice->stopPositionSamples())) / d->sound->stretchRate();
        d->playbackData.loopPosition = d->playbackData.startPosition + (double(d->slice->loopDeltaSamples()) / d->sound->stretchRate());
        if (d->playbackData.loopPosition >= d->playbackData.stopPosition) {
            d->playbackData.loopPosition = d->playbackData.startPosition;
        }
        d->playbackData.forwardTailingOffPosition = d->playbackData.stopPosition - (double(d->adsr.getParameters().release * d->playbackData.sourceSampleRate) / d->sound->stretchRate());
        d->playbackData.backwardTailingOffPosition = d->playbackData.startPosition + (double(d->adsr.getParameters().release * d->playbackData.sourceSampleRate) / d->sound->stretchRate());
    }

    // Process each frame in turn (any commands that want handling for a given frame, control changes, that sort of thing, and finally the audio itself)
    for(jack_nframes_t frame = 0; frame < nframes; ++frame) {
        // Check if we've got any commands that need handling for this frame
        const jack_nframes_t currentFrame{current_frames + frame};
        while (d->commandRing.readHead->processed == false && d->commandRing.readHead->timestamp <= currentFrame) {
            ClipCommand *newCommand = d->commandRing.read();
            // We only want to delete the command if it's only a stop command, since then nothing else will be handling it
            bool shouldDelete{false};
            if (newCommand->stopPlayback) {
                // If the command is also requesting that we start playback, then we're
                // actually wanting to restart playback and should stop the current playback
                // first, with no tailoff
                stopNote(newCommand->volume, newCommand->startPlayback == false, currentFrame);
                shouldDelete = true;
            }
            if (newCommand->startPlayback) {
                setCurrentCommand(newCommand);
                startNote(newCommand, currentFrame);
                shouldDelete = false;
            }
            if (shouldDelete) {
                d->syncTimer->deleteClipCommand(newCommand);
            }
        }

        while (d->ccControlRing.readHead->processed == false && d->ccControlRing.readHead->time == frame) {
            // Consume the control change values, but... we don't really have anything to properly use them for
            const float control = d->ccControlRing.read(&dataChannel, &dataNote);
            float value = d->ccValueRing.read(&dataChannel, &dataNote);
            if (dataChannel == -1 || (d->clipCommand && dataChannel == d->clipCommand->midiChannel)) {
                if (control == 0x7B) {
                    // All Notes Off
                    stopNote(0, false, currentFrame);
                } else {
                    if (isTailingOff == false && control == d->ccForLowpass) {
                        // Brightness control
                        value = std::clamp(value, 0.0f, 127.0f);
                        d->lowpassCutoff = (127.0f - value) / 127.0f;
                        // Update the coefficient etc (see above for this hz number)
                        const float adjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
                        const double tan = std::tan(M_PI * adjustmentInHz / d->playbackData.sourceSampleRate);
                        d->playbackData.lowpassCoefficient = (tan - 1.f) / (tan + 1.f);
                    }
                    if (isTailingOff == false && control == d->ccForHighpass) {
                        value = std::clamp(value, 0.0f, 127.0f);
                        d->highpassCutoff = value / 127.0f;
                        // Update the coefficient etc (see above for this hz number)
                        const float adjustmentInHz = pow(2, ((127.0f * d->highpassCutoff) - 69) / 12) * 440;
                        const double tan = std::tan(M_PI * adjustmentInHz / d->playbackData.sourceSampleRate);
                        d->playbackData.highpassCoefficient = (tan - 1.f) / (tan + 1.f);
                    }
                }
            }
        }
        while (d->pitchRing.readHead->processed == false && d->pitchRing.readHead->time == frame) {
            const float pitch = d->pitchRing.read(&dataChannel, &dataNote);
            if (isTailingOff == false && (d->clipCommand && (dataChannel == -1 || (d->clipCommand && dataChannel == d->clipCommand->midiChannel)))) {
                d->pitchRatio = std::pow(2.0, (std::clamp(pitch + double(d->clipCommand->midiNote), 0.0, 127.0) - double(d->slice->rootNote())) / 12.0);
            }
        }
        while (d->aftertouchRing.readHead->processed == false && d->aftertouchRing.readHead->time == frame) {
            const float aftertouch = d->aftertouchRing.read(&dataChannel, &dataNote);
            if (isTailingOff == false && (d->clipCommand && (dataChannel == -1 || (d->clipCommand && dataChannel == d->clipCommand->midiChannel)) && (dataNote == -1 || (d->clipCommand && dataNote == d->clipCommand->midiNote)))) {
                // const float previousGain = d->targetGain;
                static constexpr float minGainDB{-24.f};
                static constexpr float maxGainDB{0.0f};
                d->targetGain = juce::Decibels::decibelsToGain(juce::jmap((aftertouch/127.0f), 0.0f, 1.0f, minGainDB, maxGainDB), minGainDB);
                // if (d->subvoiceSettings == nullptr) { qDebug() << d->clip << "On frame" << currentFrame << "target gain changed by" << d->targetGain - previousGain << "from" << previousGain << "to" << d->targetGain << "with current gain at" << d->lgain; }
            }
        }
        if (d->clipCommand) {
            float targetGainDelta{abs(d->targetGain - d->lgain)};
            if (targetGainDelta > 0.000001) {
                static const float maxGainChangePerFrame{0.0001f};
                float newGain{0.0f};
                if (d->targetGain > d->lgain) {
                    newGain = d->lgain + qMin(targetGainDelta, maxGainChangePerFrame);
                } else {
                    newGain = d->lgain - qMin(targetGainDelta, maxGainChangePerFrame);
                }
                d->lgain = d->rgain = d->clipCommand->volume = newGain;
            } else {
                d->lgain = d->rgain = d->clipCommand->volume = d->targetGain;
            }
        }
        // Don't actually perform playback operations unless we've got something to play
        if (d->clip) {
            // If we're using timestretching for our clip shifting, then we should not also be applying the clip's pitch shifting here
            const float clipPitchChange = d->clip->rootSliceActual()->timeStretchStyle() == ClipAudioSource::TimeStretchOff
                ? (d->clipCommand->changePitch ? d->clipCommand->pitchChange * d->clip->rootSliceActual()->pitchChangePrecalc() : d->clip->rootSliceActual()->pitchChangePrecalc()) * (d->subvoiceSettings ? d->subvoiceSettings->pitchChangePrecalc() : 1.0f)
                : (d->clipCommand->changePitch ? d->clipCommand->pitchChange : 1.0f) * (d->subvoiceSettings ? d->subvoiceSettings->pitchChangePrecalc() : 1.0f);
            // For the root slice, don't apply the gain twice, that's just silly, and for everything else, apply both the root slice gain, and the current slice
            const float clipGain = (d->slice == d->clip->rootSliceActual() ? 1 : d->clip->rootSliceActual()->gainHandlerActual()->operationalGain()) * d->slice->gainHandlerActual()->operationalGain() * (d->subvoiceSettings ? d->subvoiceSettings->gain() : 1.0f);
            const float lPan = 0.5f * (1.0f + qMax(-1.0f, d->playbackData.pan));
            const float rPan = 0.5f * (1.0f - qMax(0.0f, d->playbackData.pan));

            const float envelopeValue = d->adsr.getNextSample();
            float l{0};
            float r{0};
            // If we're using timestretching for our clip's pitch shifting, then we also should not be applying the speed ratio here
            const double pitchRatio{d->pitchRatio * clipPitchChange * (d->clip->rootSliceActual()->timeStretchStyle() == ClipAudioSource::TimeStretchOff ? d->clip->speedRatio() : 1.0f) * d->sound->sampleRateRatio()};
            for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
                const PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
                if (playhead.active) {
                    float playheadL{0};
                    float playheadR{0};
                    const int sampleIndex{int(playhead.sourceSamplePosition)};
                    float modIntegral{0};
                    const float fraction = std::modf(playhead.sourceSamplePosition, &modIntegral);
                    if ((fraction < 0.0001f && pitchRatio == 1.0f)) {
                        // If we're just doing un-pitch-shifted playback, don't bother interpolating,
                        // just grab the sample as given and adjust according to the requests, might
                        // as well save a bit of processing (it's a very common case, and used for
                        // e.g. the metronome ticks and sketches, and we do want that stuff to be as
                        // low impact as we can reasonably make it).
                        playheadL = sampleIndex < d->playbackData.sampleDuration ? d->playbackData.inL[sampleIndex] * d->lgain * envelopeValue * clipGain : 0;
                        playheadR = d->playbackData.inR != nullptr && sampleIndex < d->playbackData.sampleDuration ? d->playbackData.inR[sampleIndex] * d->rgain * envelopeValue * clipGain : l;
                    } else {
                        // Use Hermite interpolation to ensure out sound data is reasonably on the expected
                        // curve. We could use linear interpolation, but Hermite is cheap enough that it's
                        // worth it for the improvements in sound quality. Any more and we'll need to do some
                        // precalc work and do sample stretching per octave/note/whatnot ahead of time...
                        // Maybe that's something we could offer an option for, if people really really want it?
                        int previousSampleIndex{sampleIndex - 1};
                        int nextSampleIndex{sampleIndex + 1};
                        int nextNextSampleIndex{sampleIndex + 2};
                        if (d->playbackData.isLooping && d->slice->loopCrossfadeAmount() == 0) {
                            // If we are looping, we'll need to wrap our data stream to match the loop
                            // But, don't do this if we're crossfading (at which point the loop stream interpolation is done by the playheads, not here)
                            if (d->firstRoll) {
                                previousSampleIndex = previousSampleIndex < playhead.startPosition ? -1 : previousSampleIndex;
                                d->firstRoll = false;
                            } else {
                                previousSampleIndex = previousSampleIndex < playhead.startPosition ? playhead.stopPosition - 1 : previousSampleIndex;
                            }
                            if (nextSampleIndex > playhead.stopPosition) {
                                nextSampleIndex = playhead.startPosition;
                                nextNextSampleIndex = nextSampleIndex + 1;
                            } else if (nextNextSampleIndex > playhead.stopPosition) {
                                nextSampleIndex = playhead.startPosition;
                            }
                        } else {
                            previousSampleIndex = previousSampleIndex < playhead.startPosition ? -1 : previousSampleIndex;
                            nextSampleIndex = nextSampleIndex > playhead.stopPosition ? -1 : nextSampleIndex;
                            nextNextSampleIndex = nextNextSampleIndex > playhead.stopPosition ? -1 : nextNextSampleIndex;
                        }
                        // If the various other sample positions are outside the sample area, the sample value is 0 and we should be treating it like there's no sample data
                        const float l0 = d->playbackData.sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : d->playbackData.inL[(int)previousSampleIndex];
                        const float l1 = d->playbackData.sampleDuration < sampleIndex ? 0 : d->playbackData.inL[(int)sampleIndex];
                        const float l2 = d->playbackData.sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : d->playbackData.inL[(int)nextSampleIndex];
                        const float l3 = d->playbackData.sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : d->playbackData.inL[(int)nextNextSampleIndex];
                        playheadL = interpolateHermite4pt3oX(l0, l1, l2, l3, fraction) * d->lgain * envelopeValue * clipGain;
                        if (d->playbackData.inR == nullptr) {
                            playheadR = playheadL;
                        } else {
                            const float r0 = d->playbackData.sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : d->playbackData.inR[(int)previousSampleIndex];
                            const float r1 = d->playbackData.sampleDuration < sampleIndex ? 0 : d->playbackData.inR[(int)sampleIndex];
                            const float r2 = d->playbackData.sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : d->playbackData.inR[(int)nextSampleIndex];
                            const float r3 = d->playbackData.sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : d->playbackData.inR[(int)nextNextSampleIndex];
                            playheadR = interpolateHermite4pt3oX(r0, r1, r2, r3, fraction) * d->rgain * envelopeValue * clipGain;
                        }
                    }
                    l += (playheadL * playhead.playheadGain);
                    r += (playheadR * playhead.playheadGain);
                }
            }
            // Progress the playheads (so that when we try and check next sample, they will be at the proper position)
            for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
                PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
                if (playhead.active) {
                    playhead.progress(pitchRatio);
                }
            }

            // The sound data might possibly disappear while we're attempting to play,
            // and if that happens, we really need to not try and use it. If it does
            // happen, zero out the inputs to avoid terrible noises and an angry jackd
            // which will just mute the heck out of everything and give up.
            // Specifically, this will invariably happen when doing offline pitch
            // shifting or speed ratio adjustments
            if (d->sound->isValid == false) {
                l = r = 0;
            }

            // Implement M/S Panning
            const float mSignal = 0.5 * (l + r);
            const float sSignal = 0.5 * (l - r);
            l = lPan * mSignal + sSignal;
            r = rPan * mSignal - sSignal;

            // FIXME: Sort out the filter situation...
            // Alright... allpass filter is clearly the wrong thing here, we really want to leave things alone unless
            // explicitly applying a filter, and... an allpass may well have a flat response, but isn't phase correct,
            // so, let's avoid that and get back to that later on
            //
            // // Apply allpass filter effect
            // // Roughly based on https://thewolfsound.com/lowpass-highpass-filter-plugin-with-juce/
            // if (d->highpassCutoff > 0) {
            //     // highpass filtering left channel
            //     const float allpassFilteredSampleL = d->playbackData.highpassCoefficient * l + d->allpassBufferL;
            //     d->allpassBufferL = l - d->playbackData.highpassCoefficient * allpassFilteredSampleL;
            //     l = 0.5f * (l + highpassSign * allpassFilteredSampleL);
            //     // highpass filtering right channel
            //     const float allpassFilteredSampleR = d->playbackData.highpassCoefficient * r + d->allpassBufferR;
            //     d->allpassBufferR = r - d->playbackData.highpassCoefficient * allpassFilteredSampleR;
            //     r = 0.5f * (r + highpassSign * allpassFilteredSampleR);
            // }
            // if (d->lowpassCutoff > 0) {
            //     // lowpass filtering left channel
            //     const float allpassFilteredSampleL = d->playbackData.lowpassCoefficient * l + d->allpassBufferL;
            //     d->allpassBufferL = l - d->playbackData.lowpassCoefficient * allpassFilteredSampleL;
            //     l = 0.5f * (l + allpassFilteredSampleL);
            //     // lowpass filtering right channel
            //     const float allpassFilteredSampleR = d->playbackData.lowpassCoefficient * r + d->allpassBufferR;
            //     d->allpassBufferR = r - d->playbackData.lowpassCoefficient * allpassFilteredSampleR;
            //     r = 0.5f * (r + allpassFilteredSampleR);
            // }

            if (l > peakGainLeft) {
                peakGainLeft = l;
            }
            if (r > peakGainRight) {
                peakGainRight = r;
            }

            // Add the playback data into the current sound's playback buffer at the current frame position
            // static uint throttler{0}; ++throttler; if (throttler > 200 * nframes) { throttler = 0; };
            // if (throttler == 0) { qDebug() << Q_FUNC_INFO << d->sound; }
            *(d->sound->leftBuffer + int(frame)) += l;
            *(d->sound->rightBuffer + int(frame)) += r;

            d->sourceSamplePosition += pitchRatio;

            if (d->adsr.isActive()) {
                if (pitchRatio > 0) {
                    // We're playing the sample forwards, so let's handle things with that direction in mind
                    if (d->playbackData.isLooping) {
                        if (d->sourceSamplePosition >= d->playbackData.stopPosition) {
                            d->sourceSamplePosition = d->playbackData.loopPosition;
                        }
                    } else {
                        if (d->sourceSamplePosition >= d->playbackData.stopPosition)
                        {
                            stopNote(d->targetGain, false, currentFrame, peakGainLeft, peakGainRight);
                        } else if (isTailingOff == false && d->sourceSamplePosition >= d->playbackData.forwardTailingOffPosition) {
                            stopNote(d->targetGain, true, currentFrame, peakGainLeft, peakGainRight);
                        }
                    }
                } else {
                    // We're playing the sample backwards, so let's handle things with that direction in mind
                    // That is, start position is used for the stop location and vice versa
                    if (d->playbackData.isLooping) {
                        if (d->sourceSamplePosition <= d->playbackData.stopPosition) {
                            // TODO Switch start position for the loop position here - this'll likely need that second loop position to make sense... or will it?! thought needed at any rate.
                            d->sourceSamplePosition = d->playbackData.stopPosition;
                        }
                    } else {
                        if (d->sourceSamplePosition <= d->playbackData.startPosition)
                        {
                            stopNote(d->targetGain, false, currentFrame, peakGainLeft, peakGainRight);
                        } else if (isTailingOff == false && d->sourceSamplePosition <= d->playbackData.backwardTailingOffPosition) {
                            stopNote(d->targetGain, true, currentFrame, peakGainLeft, peakGainRight);
                        }
                    }
                }
            } else {
                stopNote(d->targetGain, false, currentFrame, peakGainLeft, peakGainRight);
            }
        }
    }
    for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
        PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
        if (playhead.active) {
            playhead.updateSamplesHandled(int(nframes));
        }
    }

    // And finally, end of the process run, if we're doing some playbackery, update the playback positions
    if (d->clip && d->clip->playbackPositionsModel()) {
        for (int playheadIndex = 0; playheadIndex < PlayheadCount; ++playheadIndex) {
            const PlayheadData &playhead = d->playbackData.playheads[playheadIndex];
            if (playhead.active) {
                d->clip->playbackPositionsModel()->setPositionData(current_frames + nframes, d->clipCommand, playheadIndex, peakGainLeft * playhead.playheadGain, peakGainRight * playhead.playheadGain, playhead.sourceSamplePosition / d->playbackData.sampleDuration, d->playbackData.pan);
            }
        }
    }
}
