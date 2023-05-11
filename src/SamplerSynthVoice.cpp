#include "SamplerSynthVoice.h"

#include "ClipAudioSourcePositionsModel.h"
#include "ClipCommand.h"
#include "SamplerSynthSound.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QtMath>

static inline float velocityToGain(const float &velocity) {
//     static const float sensibleMinimum{log10(1.0f/127.0f)};
//     if (velocity == 0) {
//         return 0;
//     }
//     return (sensibleMinimum-log10(velocity))/sensibleMinimum;
    return velocity;
}

#define DataRingSize 128
class DataRing {
public:
    struct alignas(32) DataRingEntry {
        DataRingEntry *previous{nullptr};
        DataRingEntry *next{nullptr};
        jack_nframes_t time{0};
        float data{-1};
        bool processed{true};
    };

    DataRing () {
        DataRingEntry* entryPrevious{&ringData[DataRingSize - 1]};
        for (quint64 i = 0; i < DataRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~DataRing() {}
    void write(jack_nframes_t time, float data) {
        if (writeHead->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is already data stored at the write location, which likely means the buffer size is too small, which will require attention at the api level.";
        }
        writeHead->time = time;
        writeHead->data = data;
        writeHead->processed = false;
        writeHead = writeHead->next;
    }
    float read() {
        float data = readHead->data;
        readHead->processed = true;
        readHead = readHead->next;
        return data;
    }
    DataRingEntry ringData[DataRingSize];
    DataRingEntry *readHead{nullptr};
    DataRingEntry *writeHead{nullptr};
};

class SamplerSynthVoicePrivate {
public:
    SamplerSynthVoicePrivate() {
        syncTimer = qobject_cast<SyncTimer*>(SyncTimer::instance());
        for (int i = 0; i < 128; ++i) {
            initialCC[i] = -1;
        }
    }

    DataRing aftertouchRing;
    DataRing pitchRing;
    DataRing ccControlRing;
    DataRing ccValueRing;
    ADSR adsr;
    SyncTimer *syncTimer{nullptr};
    ClipCommand *clipCommand{nullptr};
    ClipAudioSource *clip{nullptr};
    qint64 clipPositionId{-1};
    quint64 startTick{0};
    quint64 nextLoopTick{0};
    quint64 nextLoopUsecs{0};
    double maxSampleDeviation{0.0};
    double pitchRatio = 0;
    double sourceSamplePosition = 0;
    double sourceSampleLength = 0;
    float lgain = 0, rgain = 0;
    // Used to make sure the first sample on looped playback is interpolated to an empty previous sample, rather than the previous sample in the loop
    bool firstRoll{false};

    float initialCC[128];
    int ccForHighpass{74};
    int ccForLowpass{1};
    float lowpassCutoff{0.0f};
    float highpassCutoff{0.0f};
    float allpassBufferL{0.0f};
    float allpassBufferR{0.0f};
};

SamplerSynthVoice::SamplerSynthVoice()
    : QObject()
    , juce::SamplerVoice()
    , d(new SamplerSynthVoicePrivate)
{
}

SamplerSynthVoice::~SamplerSynthVoice()
{
    delete d;
}

bool SamplerSynthVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SamplerSynthSound*> (sound) != nullptr;
}

void SamplerSynthVoice::setCurrentCommand(ClipCommand *clipCommand)
{
    if (d->clipCommand) {
        // This means we're changing what we should be doing in playback, and we need to delete the old one
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
            d->lgain = velocityToGain(d->clipCommand->volume);
            d->rgain = velocityToGain(d->clipCommand->volume);
        }
        if (clipCommand->changeSlice) {
            d->clipCommand->slice = clipCommand->slice;
        }
        if (clipCommand->startPlayback) {
            // This should be interpreted as "restart playback" in this case, so... reset the current position
            if (auto* playingSound = static_cast<SamplerSynthSound*> (getCurrentlyPlayingSound().get())) {
                d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate());
            }
        }
        d->syncTimer->deleteClipCommand(clipCommand);
    } else {
        d->clipCommand = clipCommand;
    }
    isPlaying = d->clipCommand;
}

ClipCommand *SamplerSynthVoice::currentCommand() const
{
    return d->clipCommand;
}

void SamplerSynthVoice::setModwheel(int modwheelValue)
{
    d->initialCC[1] = modwheelValue;
}

void SamplerSynthVoice::setStartTick(quint64 startTick)
{
    d->startTick = startTick;
}

void SamplerSynthVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    if (auto* sound = dynamic_cast<const SamplerSynthSound*> (s))
    {
        if (sound->isValid() && sound->clip()) {
            d->pitchRatio = std::pow (2.0, (midiNoteNumber - sound->rootMidiNote()) / 12.0)
                            * sound->sourceSampleRate() / getSampleRate();

            d->maxSampleDeviation = d->syncTimer->subbeatCountToSeconds(d->syncTimer->getBpm(), 1) * sound->sourceSampleRate();
            d->clip = sound->clip();
            d->sourceSampleLength = d->clip->getDuration() * sound->sourceSampleRate();
            d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * sound->sourceSampleRate());

            d->nextLoopTick = d->startTick + d->clip->getLengthInBeats() * d->syncTimer->getMultiplier();
            d->nextLoopUsecs = 0;

            if (d->clipPositionId > -1) {
                d->clip->playbackPositionsModel()->removePosition(d->clipPositionId);
            }
            d->clipPositionId = d->clip->playbackPositionsModel()->createPositionID();

            d->lgain = velocityToGain(velocity);
            d->rgain = velocityToGain(velocity);
            if (d->initialCC[d->ccForLowpass] > -1) {
                d->lowpassCutoff = (127.0f - d->initialCC[d->ccForLowpass]) / 127.0f;
            }
            if (d->initialCC[d->ccForHighpass] > -1) {
                d->highpassCutoff = d->initialCC[d->ccForHighpass] / 127.0f;
            }

            d->adsr.reset();
            d->adsr.setSampleRate(sound->sourceSampleRate());
            d->adsr.setParameters(d->clip->adsrParameters());
            isTailingOff = false;
            d->adsr.noteOn();
        }
    }
    else
    {
        jassertfalse; // this object can only play SamplerSynthSounds!
    }
}

void SamplerSynthVoice::stopNote (float velocity, bool allowTailOff)
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
        clearCurrentNote();
        d->adsr.reset();
        if (d->clip) {
            d->clip->playbackPositionsModel()->removePosition(d->clipPositionId);
            d->clip = nullptr;
            d->clipPositionId = -1;
        }
        if (d->clipCommand) {
            d->syncTimer->deleteClipCommand(d->clipCommand);
            d->clipCommand = nullptr;
        }
        isPlaying = false;
        d->nextLoopTick = 0;
        d->nextLoopUsecs = 0;
        isTailingOff = false;
        d->firstRoll = true;
        d->allpassBufferL = d->allpassBufferR = 0.0f;
        for (int i = 0; i < 128; ++i) {
            d->initialCC[i] = -1;
        }
    }
}

void SamplerSynthVoice::pitchWheelMoved (int /*newValue*/) {}
void SamplerSynthVoice::controllerMoved (int /*controllerNumber*/, int /*newValue*/) {}

void SamplerSynthVoice::handleControlChange(jack_nframes_t time, int control, int value)
{
    if (d->clipCommand && !isTailingOff) {
        d->ccControlRing.write(time, control);
        d->ccValueRing.write(time, value);
    }
}

void SamplerSynthVoice::handleAftertouch(jack_nframes_t time, int pressure)
{
    if (d->clipCommand && !isTailingOff) {
        d->aftertouchRing.write(time, pressure);
    }
}

void SamplerSynthVoice::handlePitchChange(jack_nframes_t time, float pitchValue) {
    if (d->clipCommand && !isTailingOff) {
        d->pitchRing.write(time, pitchValue);
    }
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
void SamplerSynthVoice::process(jack_default_audio_sample_t *leftBuffer, jack_default_audio_sample_t *rightBuffer, jack_nframes_t nframes, jack_nframes_t /*current_frames*/, jack_time_t current_usecs, jack_time_t next_usecs, float /*period_usecs*/)
{
    if (auto* playingSound = static_cast<SamplerSynthSound*> (getCurrentlyPlayingSound().get()))
    {
        if (playingSound->isValid() && d->clipCommand) {
            if (d->nextLoopUsecs == 0) {
                const quint64 differenceToPlayhead = d->nextLoopTick - d->syncTimer->jackPlayhead();
                d->nextLoopUsecs = d->syncTimer->jackPlayheadUsecs() + (differenceToPlayhead * d->syncTimer->jackSubbeatLengthInMicroseconds());
            }
            const double microsecondsPerFrame = (next_usecs - current_usecs) / nframes;
            float peakGain{0.0f};
            auto& data = *playingSound->audioData();
            const float* const inL = data.getReadPointer (0);
            const float* const inR = data.getNumChannels() > 1 ? data.getReadPointer (1) : nullptr;
            const double sourceSampleRate = playingSound->sourceSampleRate();

            // this bit is basically mtof - that is, converts a midi note to its equivalent expected frequency (here given a 440Hz concert tone), and it's going to be somewhere within a reasonable amount along the audible scale
            const float highpassAdjustmentInHz = pow(2, ((127.0f * d->highpassCutoff) - 69) / 12) * 440;
            const double highpassTan = std::tan(M_PI * highpassAdjustmentInHz / sourceSampleRate);
            double highpassCoefficient = (highpassTan - 1.f) / (highpassTan + 1.f);
            const float lowpassAdjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
            const double lowpassTan = std::tan(M_PI * lowpassAdjustmentInHz / sourceSampleRate);
            double lowpassCoefficient = (lowpassTan - 1.f) / (lowpassTan + 1.f);

            const float clipVolume = d->clip->volumeAbsolute();
            const int startPosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * sourceSampleRate);
            const int stopPosition = playingSound->stopPosition(d->clipCommand->slice);
            const int sampleDuration = playingSound->length() - 1;
            const float pan = (float) d->clip->pan();
            const float lPan = 0.5 * (1.0 + pan);
            const float rPan = 0.5 * (1.0 - pan);
            const bool isLooping = d->clipCommand->looping;
            for(jack_nframes_t frame = 0; frame < nframes; ++frame) {
                while (d->ccControlRing.readHead->processed == false && d->ccControlRing.readHead->time <= frame) {
                    // Consume the control change values, but... we don't really have anything to properly use them for
                    const float control = d->ccControlRing.read();
                    float value = d->ccValueRing.read();
                    if (d->initialCC[int(control)] == -1) {
                        d->initialCC[int(control)] = value;
                    }
                    if (control == d->ccForLowpass) {
                        // Brightness control
                        value = std::clamp(d->initialCC[d->ccForLowpass] + value, 0.0f, 127.0f);
                        d->lowpassCutoff = (127.0f - value) / 127.0f;
                        // Update the coefficient etc (see above for this hz number)
                        const float adjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
                        const double tan = std::tan(M_PI * adjustmentInHz / sourceSampleRate);
                        lowpassCoefficient = (tan - 1.f) / (tan + 1.f);
                    }
                    if (control == d->ccForHighpass) {
                        value = std::clamp(d->initialCC[d->ccForHighpass] + value, 0.0f, 127.0f);
                        d->highpassCutoff = value / 127.0f;
                        // Update the coefficient etc (see above for this hz number)
                        const float adjustmentInHz = pow(2, ((127.0f * d->highpassCutoff) - 69) / 12) * 440;
                        const double tan = std::tan(M_PI * adjustmentInHz / sourceSampleRate);
                        highpassCoefficient = (tan - 1.f) / (tan + 1.f);
                    }
                }
                while (d->pitchRing.readHead->processed == false && d->pitchRing.readHead->time <= frame) {
                    d->pitchRatio = std::pow(2.0, (std::clamp(d->pitchRing.read() + double(d->clipCommand->midiNote), 0.0, 127.0) - double(playingSound->rootMidiNote())) / 12.0)
                            * playingSound->sourceSampleRate() / getSampleRate();
                }
                while (d->aftertouchRing.readHead->processed == false && d->aftertouchRing.readHead->time <= frame) {
                    d->lgain = d->rgain = d->clipCommand->volume = (float(d->aftertouchRing.read())/127.0f);
                }

                const float envelopeValue = d->adsr.getNextSample();
                float l{0};
                float r{0};
                const int sampleIndex{int(d->sourceSamplePosition)};
                float modIntegral{0};
                const float fraction = std::modf(d->sourceSamplePosition, &modIntegral);
                if (fraction < 0.0001f && d->pitchRatio == 1.0f) {
                    // If we're just doing un-pitch-shifted playback, don't bother interpolating,
                    // just grab the sample as given and adjust according to the requests, might
                    // as well save a bit of processing (it's a very common case, and used for
                    // e.g. the metronome ticks, and we do want that stuff to be as low impact
                    // as we can reasonably make it).
                    l = sampleIndex < stopPosition ? inL[sampleIndex] * d->lgain * envelopeValue * clipVolume: 0;
                    r = inR != nullptr && sampleIndex < stopPosition ? inR[sampleIndex] * d->rgain * envelopeValue * clipVolume : 0;
                } else {
                    // Use Hermite interpolation to ensure out sound data is reasonably on the expected
                    // curve. We could use linear interpolation, but Hermite is cheap enough that it's
                    // worth it for the improvements in sound quality. Any more and we'll need to do some
                    // precalc work and do sample stretching per octave/note/whatnot ahead of time...
                    // Maybe that's something we could offer an option for, if people really really want it?
                    int previousSampleIndex{sampleIndex - 1};
                    int nextSampleIndex{sampleIndex + 1};
                    int nextNextSampleIndex{sampleIndex + 2};
                    if (isLooping) {
                        if (d->firstRoll) {
                            previousSampleIndex = previousSampleIndex < startPosition ? -1 : previousSampleIndex;
                            d->firstRoll = false;
                        } else {
                            previousSampleIndex = previousSampleIndex < startPosition ? stopPosition - 1 : previousSampleIndex;
                        }
                        if (nextSampleIndex > stopPosition) {
                            nextSampleIndex = startPosition;
                            nextNextSampleIndex = nextSampleIndex + 1;
                        } else if (nextNextSampleIndex > stopPosition) {
                            nextSampleIndex = startPosition;
                        }
                    } else {
                        previousSampleIndex = previousSampleIndex < startPosition ? -1 : previousSampleIndex;
                        nextSampleIndex = nextSampleIndex > stopPosition ? -1 : nextSampleIndex;
                        nextNextSampleIndex = nextNextSampleIndex > stopPosition ? -1 : nextNextSampleIndex;
                    }
                    // If the various other sample positions are outside the sample area, the sample value is 0 and we should be treating it like there's no sample data
                    const float l0 = sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : inL[(int)previousSampleIndex];
                    const float l1 = sampleDuration < sampleIndex ? 0 : inL[(int)sampleIndex];
                    const float l2 = sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : inL[(int)nextSampleIndex];
                    const float l3 = sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : inL[(int)nextNextSampleIndex];
                    l = interpolateHermite4pt3oX(l0, l1, l2, l3, fraction) * d->lgain * envelopeValue * clipVolume;
                    if (inR == nullptr) {
                        r = l;
                    } else {
                        const float r0 = sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : inR[(int)previousSampleIndex];
                        const float r1 = sampleDuration < sampleIndex ? 0 : inR[(int)sampleIndex];
                        const float r2 = sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : inR[(int)nextSampleIndex];
                        const float r3 = sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : inR[(int)nextNextSampleIndex];
                        r = interpolateHermite4pt3oX(r0, r1, r2, r3, fraction) * d->rgain * envelopeValue * clipVolume;
                    }
                }

                // Implement M/S Panning
                const float mSignal = 0.5 * (l + r);
                const float sSignal = l - r;
                l = lPan * mSignal + sSignal;
                r = rPan * mSignal - sSignal;

                // Apply allpass filter effect
                // Roughly based on https://thewolfsound.com/lowpass-highpass-filter-plugin-with-juce/
                if (d->highpassCutoff > 0) {
                    // highpass filtering left channel
                    const float allpassFilteredSampleL = highpassCoefficient * l + d->allpassBufferL;
                    d->allpassBufferL = l - highpassCoefficient * allpassFilteredSampleL;
                    l = 0.5f * (l + highpassSign * allpassFilteredSampleL);
                    // highpass filtering right channel
                    const float allpassFilteredSampleR = highpassCoefficient * r + d->allpassBufferR;
                    d->allpassBufferR = r - highpassCoefficient * allpassFilteredSampleR;
                    r = 0.5f * (r + highpassSign * allpassFilteredSampleR);
                }
                if (d->lowpassCutoff > 0) {
                    // lowpass filtering left channel
                    const float allpassFilteredSampleL = lowpassCoefficient * l + d->allpassBufferL;
                    d->allpassBufferL = l - lowpassCoefficient * allpassFilteredSampleL;
                    l = 0.5f * (l + allpassFilteredSampleL);
                    // lowpass filtering right channel
                    const float allpassFilteredSampleR = lowpassCoefficient * r + d->allpassBufferR;
                    d->allpassBufferR = r - lowpassCoefficient * allpassFilteredSampleR;
                    r = 0.5f * (r + allpassFilteredSampleR);
                }

                const float newGain{l + r};
                if (newGain > peakGain) {
                    peakGain = newGain;
                }

                *leftBuffer += l;
                ++leftBuffer;
                *rightBuffer += r;
                ++rightBuffer;

                d->sourceSamplePosition += d->pitchRatio;

                if (!d->adsr.isActive()) {
                    stopNote(d->clipCommand->volume, false);
                    break;
                }
                if (isLooping) {
                    // beat align samples by reading clip duration in beats from clip, saving synctimer's jack playback positions in voice on startNote, and adjust in if (looping) section of process, and make sure the loop is restarted on that beat if deviation is sufficiently large (like... one timer tick is too much maybe?)
                    if (trunc(d->clip->getLengthInBeats()) == d->clip->getLengthInBeats()) {
                        // If the clip is actually a clean multiple of a number of beats, let's make sure it loops matching that beat position
                        // Work out next loop start point in usecs
                        // Once we hit the frame for that number of usecs after the most recent start, reset playback position to match.
                        // nb: Don't try and be clever, actually make sure to play the first sample in the sound - play past the end rather than before the start
                        if (current_usecs + jack_time_t(frame * microsecondsPerFrame) >= d->nextLoopUsecs) {
                            // Work out the position of the next loop, based on the most recent beat tick position, not the current position, as that might be slightly incorrect
                            const quint64 lengthInTicks = d->clip->getLengthInBeats() * d->syncTimer->getMultiplier();
                            d->nextLoopTick = d->nextLoopTick + lengthInTicks;
                            const quint64 differenceToPlayhead = d->nextLoopTick - d->syncTimer->jackPlayhead();
                            d->nextLoopUsecs = d->syncTimer->jackPlayheadUsecs() + (differenceToPlayhead * d->syncTimer->jackSubbeatLengthInMicroseconds());
//                             qDebug() << "Resetting - next tick" << d->nextLoopTick << "next usecs" << d->nextLoopUsecs << "difference to playhead" << differenceToPlayhead;

                            // Reset the sample playback position back to the start point
                            d->sourceSamplePosition = startPosition;
                        }
                    } else if (d->sourceSamplePosition >= stopPosition) {
                        // If we're not beat-matched, just loop "normally"
                        // TODO Switch start position for the loop position here
                        d->sourceSamplePosition = startPosition;
                    }
                } else {
                    if (d->sourceSamplePosition >= stopPosition)
                    {
                        stopNote(d->clipCommand->volume, false);
                        break;
                    } else if (isTailingOff == false && d->sourceSamplePosition >= (stopPosition - (d->adsr.getParameters().release * sourceSampleRate))) {
                        stopNote(d->clipCommand->volume, true);
                    }
                }
            }

            // Because it might have gone away after being stopped above, so let's try and not crash
            if (d->clip && d->clipPositionId > -1) {
                d->clip->playbackPositionsModel()->setPositionGainAndProgress(d->clipPositionId, peakGain * 0.5f, d->sourceSamplePosition / d->sourceSampleLength);
            }
        }
    }
}
