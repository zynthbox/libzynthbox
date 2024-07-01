#include "SamplerSynthVoice.h"

#include "ClipAudioSourcePositionsModel.h"
#include "ClipCommand.h"
#include "SamplerSynth.h"
#include "SamplerSynthSound.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QtMath>

namespace tracktion_engine {
#include <tracktion_engine/3rd_party/soundtouch/include/SoundTouch.h>
};

#define DataRingSize 128
class DataRing {
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

    DataRing () {
        Entry* entryPrevious{&ringData[DataRingSize - 1]};
        for (quint64 i = 0; i < DataRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~DataRing() {}
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

struct PlaybackData {
public:
    PlaybackData() {}
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
    DataRing aftertouchRing;
    DataRing pitchRing;
    DataRing ccControlRing;
    DataRing ccValueRing;
    juce::ADSR adsr;
    SyncTimer *syncTimer{nullptr};
    SamplerSynth *samplerSynth{nullptr};
    ClipCommand *clipCommand{nullptr};
    ClipAudioSource *clip{nullptr};
    SamplerSynthSound* sound{nullptr};
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

    float timeStretchingInput[2];
    float timeStretchingOutput[2];
    tracktion_engine::soundtouch::SoundTouch soundTouch;

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
        const double sourceSampleRate{clipCommand->clip->sampleRate()};
        const double release{clipCommand->clip->adsrRelease() * sourceSampleRate};
        availableAfter = timestamp + release;
        mostRecentStartCommand = nullptr;
    }
    // Not an else if, because we might both stop and start with the same command
    if (clipCommand->startPlayback == true) {
        if (clipCommand->looping == true) {
            availableAfter = UINT_MAX;
        } else {
            const double sourceSampleRate{clipCommand->clip->sampleRate()};
            const double startPosition = (int) ((clipCommand->setStartPosition ? clipCommand->startPosition : clipCommand->clip->getStartPosition(clipCommand->slice)) * sourceSampleRate);
            const double stopPosition = (int) ((clipCommand->setStopPosition ? clipCommand->stopPosition : clipCommand->clip->getStopPosition(clipCommand->slice)) * sourceSampleRate);
            availableAfter = timestamp + (stopPosition - startPosition);
        }
        mostRecentStartCommand = clipCommand;
    }
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
            d->lgain = d->clipCommand->volume;
            d->rgain = d->clipCommand->volume;
        }
        if (clipCommand->changeSlice) {
            d->clipCommand->slice = clipCommand->slice;
        }
        if (clipCommand->startPlayback) {
            // This should be interpreted as "restart playback" in this case, so... reset the current position
            d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * d->clip->sampleRate());
        }
        if (clipCommand->changePan) {
            d->clipCommand->pan = clipCommand->pan;
        }
        d->syncTimer->deleteClipCommand(clipCommand);
    } else {
        d->clipCommand = clipCommand;
    }
    isPlaying = d->clipCommand;
}

void SamplerSynthVoice::setModwheel(int modwheelValue)
{
    d->initialCC[1] = modwheelValue;
}

void SamplerSynthVoice::startNote(ClipCommand *clipCommand, jack_nframes_t timestamp)
{
    if (auto sound = d->samplerSynth->clipToSound(clipCommand->clip)) {
        d->sound = sound;
        d->clip = sound->clip();

        d->pitchRatio = std::pow(2.0, (clipCommand->midiNote - sound->rootMidiNote()) / 12.0) * sound->sourceSampleRate() / d->clip->sampleRate();
        d->sourceSampleLength = d->clip->getDuration() * sound->sourceSampleRate() / d->clip->speedRatio();
        if (d->clipCommand->changePitch && d->clipCommand->pitchChange < 0) {
            d->sourceSamplePosition = (int) ((d->clipCommand->setStopPosition ? d->clipCommand->stopPosition : d->clip->getStopPosition(d->clipCommand->slice)) * sound->sourceSampleRate());
        } else {
            d->sourceSamplePosition = (int) ((d->clipCommand->setStartPosition ? d->clipCommand->startPosition : d->clip->getStartPosition(d->clipCommand->slice)) * sound->sourceSampleRate());
        }

        d->playbackData.snappedToBeat = (trunc(d->clip->getLengthInBeats()) == d->clip->getLengthInBeats());
        d->playbackData.isLooping = d->clipCommand->looping;

        d->lgain = clipCommand->volume;
        d->rgain = clipCommand->volume;

        d->adsr.reset();
        d->adsr.setSampleRate(sound->sourceSampleRate());
        d->adsr.setParameters(d->clip->granular() ? d->clip->grainADSR().getParameters() : d->clip->adsrParameters());
        isTailingOff = false;
        d->adsr.noteOn();

        d->playbackData.data = d->sound->audioData();
        if (d->playbackData.data) {
            d->playbackData.inL = d->playbackData.data->getReadPointer(0);
            d->playbackData.inR = d->playbackData.data->getNumChannels() > 1 ? d->playbackData.data->getReadPointer(1) : nullptr;
        } else {
            d->playbackData.inL = nullptr;
            d->playbackData.inR = nullptr;
        }
        d->playbackData.sourceSampleRate = d->sound->sourceSampleRate();
        d->playbackData.sampleDuration = sound->length() - 1;

        // this bit is basically mtof - that is, converts a midi note to its equivalent expected frequency (here given a 440Hz concert tone), and it's going to be somewhere within a reasonable amount along the audible scale
        const float highpassAdjustmentInHz = pow(2, ((127.0f * d->highpassCutoff) - 69) / 12) * 440;
        const double highpassTan = std::tan(M_PI * highpassAdjustmentInHz / d->playbackData.sourceSampleRate);
        d->playbackData.highpassCoefficient = (highpassTan - 1.f) / (highpassTan + 1.f);
        const float lowpassAdjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
        const double lowpassTan = std::tan(M_PI * lowpassAdjustmentInHz / d->playbackData.sourceSampleRate);
        d->playbackData.lowpassCoefficient = (lowpassTan - 1.f) / (lowpassTan + 1.f);

        d->playbackData.pan = std::clamp(float(d->clip->pan()) + d->clipCommand->pan, -1.0f, 1.0f);
        d->playbackData.startPosition = (int) ((d->clipCommand->setStartPosition ? d->clipCommand->startPosition : d->clip->getStartPosition(d->clipCommand->slice)) * d->playbackData.sourceSampleRate);
        d->playbackData.stopPosition = (int) ((d->clipCommand->setStopPosition ? d->clipCommand->stopPosition : d->clip->getStopPosition(d->clipCommand->slice)) * d->playbackData.sourceSampleRate);
        d->playbackData.loopPosition = int((d->clip->getStartPosition(d->clipCommand->slice) + (d->clip->loopDelta() / d->clip->speedRatio())) * d->playbackData.sourceSampleRate);
        if (d->playbackData.loopPosition >= d->playbackData.stopPosition) {
            d->playbackData.loopPosition = d->playbackData.startPosition;
        }
        d->playbackData.forwardTailingOffPosition = (d->playbackData.stopPosition - (d->adsr.getParameters().release * d->playbackData.sourceSampleRate));
        d->playbackData.backwardTailingOffPosition = (d->playbackData.startPosition + (d->adsr.getParameters().release * d->playbackData.sourceSampleRate));

        if (d->clip->timeStretchLive()) {
            d->soundTouch.setChannels(2);
            d->soundTouch.setSampleRate(d->sound->sourceSampleRate());
        }

        if (clipCommand->looping == true) {
            availableAfter = UINT_MAX;
        } else {
            availableAfter = timestamp + jack_nframes_t(d->playbackData.stopPosition - d->playbackData.startPosition);
        }
    } else {
        jassertfalse; // this object can only play SamplerSynthSounds!
    }
}

void SamplerSynthVoice::stopNote(float velocity, bool allowTailOff, jack_nframes_t timestamp)
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
        d->soundTouch.clear();
        d->adsr.reset();
        if (d->clip) {
            d->clip = nullptr;
            d->sound = nullptr;
        }
        if (d->clipCommand) {
            d->syncTimer->deleteClipCommand(d->clipCommand);
            d->clipCommand = nullptr;
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
    float peakGain{0.0f};
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
        d->playbackData.pan = std::clamp(float(d->clip->pan()) + d->clipCommand->pan, -1.0f, 1.0f);
        d->playbackData.startPosition = (int) ((d->clipCommand->setStartPosition ? d->clipCommand->startPosition : d->clip->getStartPosition(d->clipCommand->slice)) * d->playbackData.sourceSampleRate);
        d->playbackData.stopPosition = (int) ((d->clipCommand->setStopPosition ? d->clipCommand->stopPosition : d->clip->getStopPosition(d->clipCommand->slice)) * d->playbackData.sourceSampleRate);
        d->playbackData.loopPosition = int((d->clip->getStartPosition(d->clipCommand->slice) + (d->clip->loopDelta() / d->clip->speedRatio())) * d->playbackData.sourceSampleRate);
        if (d->playbackData.loopPosition >= d->playbackData.stopPosition) {
            d->playbackData.loopPosition = d->playbackData.startPosition;
        }
        d->playbackData.forwardTailingOffPosition = (d->playbackData.stopPosition - (d->adsr.getParameters().release * d->playbackData.sourceSampleRate));
        d->playbackData.backwardTailingOffPosition = (d->playbackData.startPosition + (d->adsr.getParameters().release * d->playbackData.sourceSampleRate));
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
                    if (control == d->ccForLowpass) {
                        // Brightness control
                        value = std::clamp(value, 0.0f, 127.0f);
                        d->lowpassCutoff = (127.0f - value) / 127.0f;
                        // Update the coefficient etc (see above for this hz number)
                        const float adjustmentInHz = pow(2, ((127.0f * d->lowpassCutoff) - 69) / 12) * 440;
                        const double tan = std::tan(M_PI * adjustmentInHz / d->playbackData.sourceSampleRate);
                        d->playbackData.lowpassCoefficient = (tan - 1.f) / (tan + 1.f);
                    }
                    if (control == d->ccForHighpass) {
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
            if (d->clipCommand && (dataChannel == -1 || (d->clipCommand && dataChannel == d->clipCommand->midiChannel))) {
                d->pitchRatio = std::pow(2.0, (std::clamp(pitch + double(d->clipCommand->midiNote), 0.0, 127.0) - double(d->sound->rootMidiNote())) / 12.0)
                    * d->playbackData.sourceSampleRate / d->clip->sampleRate();
            }
        }
        while (d->aftertouchRing.readHead->processed == false && d->aftertouchRing.readHead->time == frame) {
            const float aftertouch = d->aftertouchRing.read(&dataChannel, &dataNote);
            if (d->clipCommand && (dataChannel == -1 || (d->clipCommand && dataChannel == d->clipCommand->midiChannel)) && (dataNote == -1 || (d->clipCommand && dataNote == d->clipCommand->midiNote))) {
                d->lgain = d->rgain = d->clipCommand->volume = (aftertouch/127.0f);
            }
        }
        // Don't actually perform playback operations unless we've got something to play
        if (d->clip) {
            const float clipPitchChange = d->clipCommand->changePitch ? d->clipCommand->pitchChange * d->clip->pitchChangePrecalc() : d->clip->pitchChangePrecalc();
            const float clipVolume = d->clip->volumeAbsolute() * d->clip->getGain();
            const float lPan = 2 * (1.0 + d->playbackData.pan); // Used for m/s panning, to ensure the signal is proper, we need to multiply it by 2 eventually, so might as well pre-do that calculation here
            const float rPan = 2 * (1.0 - d->playbackData.pan); // Used for m/s panning, to ensure the signal is proper, we need to multiply it by 2 eventually, so might as well pre-do that calculation here

            const float envelopeValue = d->adsr.getNextSample();
            float l{0};
            float r{0};
            const int sampleIndex{int(d->sourceSamplePosition)};
            float modIntegral{0};
            const float fraction = std::modf(d->sourceSamplePosition, &modIntegral);
            const double pitchRatio{d->pitchRatio * clipPitchChange};
            if ((fraction < 0.0001f && pitchRatio == 1.0f) || d->clip->timeStretchLive()) {
                // If we're just doing un-pitch-shifted playback, don't bother interpolating,
                // just grab the sample as given and adjust according to the requests, might
                // as well save a bit of processing (it's a very common case, and used for
                // e.g. the metronome ticks, and we do want that stuff to be as low impact
                // as we can reasonably make it).
                l = sampleIndex < d->playbackData.sampleDuration ? d->playbackData.inL[sampleIndex] * d->lgain * envelopeValue * clipVolume : 0;
                r = d->playbackData.inR != nullptr && sampleIndex < d->playbackData.sampleDuration ? d->playbackData.inR[sampleIndex] * d->rgain * envelopeValue * clipVolume : l;
                if (d->clip->timeStretchLive()) {
                    d->soundTouch.setPitch(pitchRatio);
                    d->timeStretchingInput[0] = l;
                    d->timeStretchingInput[1] = r;
                    d->soundTouch.putSamples(d->timeStretchingInput, 1);
                    if (d->soundTouch.numUnprocessedSamples() > 0) {
                        d->soundTouch.receiveSamples(d->timeStretchingOutput, 1);
                        l = d->timeStretchingOutput[0];
                        r = d->timeStretchingOutput[1];
                    } else {
                        l = r = 0;
                    }
                }
            } else {
                // Use Hermite interpolation to ensure out sound data is reasonably on the expected
                // curve. We could use linear interpolation, but Hermite is cheap enough that it's
                // worth it for the improvements in sound quality. Any more and we'll need to do some
                // precalc work and do sample stretching per octave/note/whatnot ahead of time...
                // Maybe that's something we could offer an option for, if people really really want it?
                int previousSampleIndex{sampleIndex - 1};
                int nextSampleIndex{sampleIndex + 1};
                int nextNextSampleIndex{sampleIndex + 2};
                if (d->playbackData.isLooping) {
                    if (d->firstRoll) {
                        previousSampleIndex = previousSampleIndex < d->playbackData.startPosition ? -1 : previousSampleIndex;
                        d->firstRoll = false;
                    } else {
                        previousSampleIndex = previousSampleIndex < d->playbackData.startPosition ? d->playbackData.stopPosition - 1 : previousSampleIndex;
                    }
                    if (nextSampleIndex > d->playbackData.stopPosition) {
                        nextSampleIndex = d->playbackData.startPosition;
                        nextNextSampleIndex = nextSampleIndex + 1;
                    } else if (nextNextSampleIndex > d->playbackData.stopPosition) {
                        nextSampleIndex = d->playbackData.startPosition;
                    }
                } else {
                    previousSampleIndex = previousSampleIndex < d->playbackData.startPosition ? -1 : previousSampleIndex;
                    nextSampleIndex = nextSampleIndex > d->playbackData.stopPosition ? -1 : nextSampleIndex;
                    nextNextSampleIndex = nextNextSampleIndex > d->playbackData.stopPosition ? -1 : nextNextSampleIndex;
                }
                // If the various other sample positions are outside the sample area, the sample value is 0 and we should be treating it like there's no sample data
                const float l0 = d->playbackData.sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : d->playbackData.inL[(int)previousSampleIndex];
                const float l1 = d->playbackData.sampleDuration < sampleIndex ? 0 : d->playbackData.inL[(int)sampleIndex];
                const float l2 = d->playbackData.sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : d->playbackData.inL[(int)nextSampleIndex];
                const float l3 = d->playbackData.sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : d->playbackData.inL[(int)nextNextSampleIndex];
                l = interpolateHermite4pt3oX(l0, l1, l2, l3, fraction) * d->lgain * envelopeValue * clipVolume;
                if (d->playbackData.inR == nullptr) {
                    r = l;
                } else {
                    const float r0 = d->playbackData.sampleDuration < previousSampleIndex || previousSampleIndex == -1 ? 0 : d->playbackData.inR[(int)previousSampleIndex];
                    const float r1 = d->playbackData.sampleDuration < sampleIndex ? 0 : d->playbackData.inR[(int)sampleIndex];
                    const float r2 = d->playbackData.sampleDuration < nextSampleIndex || nextSampleIndex == -1 ? 0 : d->playbackData.inR[(int)nextSampleIndex];
                    const float r3 = d->playbackData.sampleDuration < nextNextSampleIndex || nextNextSampleIndex == -1 ? 0 : d->playbackData.inR[(int)nextNextSampleIndex];
                    r = interpolateHermite4pt3oX(r0, r1, r2, r3, fraction) * d->rgain * envelopeValue * clipVolume;
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
            const float sSignal = l - r;
            l = lPan * mSignal + sSignal;
            r = rPan * mSignal - sSignal;

            // Apply allpass filter effect
            // Roughly based on https://thewolfsound.com/lowpass-highpass-filter-plugin-with-juce/
            if (d->highpassCutoff > 0) {
                // highpass filtering left channel
                const float allpassFilteredSampleL = d->playbackData.highpassCoefficient * l + d->allpassBufferL;
                d->allpassBufferL = l - d->playbackData.highpassCoefficient * allpassFilteredSampleL;
                l = 0.5f * (l + highpassSign * allpassFilteredSampleL);
                // highpass filtering right channel
                const float allpassFilteredSampleR = d->playbackData.highpassCoefficient * r + d->allpassBufferR;
                d->allpassBufferR = r - d->playbackData.highpassCoefficient * allpassFilteredSampleR;
                r = 0.5f * (r + highpassSign * allpassFilteredSampleR);
            }
            if (d->lowpassCutoff > 0) {
                // lowpass filtering left channel
                const float allpassFilteredSampleL = d->playbackData.lowpassCoefficient * l + d->allpassBufferL;
                d->allpassBufferL = l - d->playbackData.lowpassCoefficient * allpassFilteredSampleL;
                l = 0.5f * (l + allpassFilteredSampleL);
                // lowpass filtering right channel
                const float allpassFilteredSampleR = d->playbackData.lowpassCoefficient * r + d->allpassBufferR;
                d->allpassBufferR = r - d->playbackData.lowpassCoefficient * allpassFilteredSampleR;
                r = 0.5f * (r + allpassFilteredSampleR);
            }

            const float newGain{l + r};
            if (newGain > peakGain) {
                peakGain = newGain;
            }

            // Add the playback data into the current sound's playback buffer at the current frame position
            // static uint throttler{0}; ++throttler; if (throttler > 200 * nframes) { throttler = 0; };
            // if (throttler == 0) { qDebug() << Q_FUNC_INFO << d->sound; }
            *(d->sound->leftBuffer + int(frame)) += l;
            *(d->sound->rightBuffer + int(frame)) += r;

            if (d->clip->timeStretchLive()) {
                d->sourceSamplePosition += 1.0f;
            } else {
                d->sourceSamplePosition += pitchRatio;
            }

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
                            stopNote(d->clipCommand->volume, false, currentFrame);
                            // Before we stop, send out one last update for this command
                            if (d->clip && d->clip->playbackPositionsModel()) {
                                d->clip->playbackPositionsModel()->setPositionData(current_frames + frame, d->clipCommand, peakGain * 0.5f, d->sourceSamplePosition / d->sourceSampleLength, d->playbackData.pan);
                            }
                        } else if (isTailingOff == false && d->sourceSamplePosition >= d->playbackData.forwardTailingOffPosition) {
                            stopNote(d->clipCommand->volume, true, currentFrame);
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
                            stopNote(d->clipCommand->volume, false, currentFrame);
                            // Before we stop, send out one last update for this command
                            if (d->clip && d->clip->playbackPositionsModel()) {
                                d->clip->playbackPositionsModel()->setPositionData(current_frames + frame, d->clipCommand, peakGain * 0.5f, d->sourceSamplePosition / d->sourceSampleLength, d->playbackData.pan);
                            }
                        } else if (isTailingOff == false && d->sourceSamplePosition <= d->playbackData.backwardTailingOffPosition) {
                            stopNote(d->clipCommand->volume, true, currentFrame);
                        }
                    }
                }
            } else {
                stopNote(d->clipCommand->volume, false, currentFrame);
            }
        }
    }

    // TODO setPositionData should be more of a call and forget thing (so, update on end of run, and only remove automatically, use a ring to store the data to be interpreted in the model, interpret that data (very) regularly, and update it there, so that lifting is in the ui...)
    // - Send clipCommand (don't actually read, just use the address of it as an ID), along with peakGian, position, and pan
    // - Read ring in UI thread, update all data entries, and... we probably need a lot more potential positions (like maybe 32 voices times about ten? Call it 256 for no particular reason?)
    // Because it might have gone away after being stopped above, so let's try and not crash
    if (d->clip && d->clip->playbackPositionsModel()) {
        d->clip->playbackPositionsModel()->setPositionData(current_frames + nframes, d->clipCommand, peakGain * 0.5f, d->sourceSamplePosition / d->sourceSampleLength, d->playbackData.pan);
    }
}
