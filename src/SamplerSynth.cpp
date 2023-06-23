
#include "SamplerSynth.h"

#include "Helper.h"
#include "PlayGridManager.h"
#include "SamplerSynthSound.h"
#include "SamplerSynthVoice.h"
#include "ClipCommand.h"
#include "ClipAudioSourcePositionsModel.h"
#include "SyncTimer.h"
#include "JackThreadAffinitySetter.h"

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/statistics.h>

using namespace juce;

#define SAMPLER_CHANNEL_VOICE_COUNT 32

#define CommandQueueSize 256
class Grainerator;
class SamplerChannel
{
public:
    explicit SamplerChannel(SamplerSynth *samplerSynth, jack_client_t *client, const QString &clientName, const int &midiChannel);
    ~SamplerChannel();
    int process(jack_nframes_t nframes);
    double sampleRate() const;
    /**
     * \brief Actually handle the command
     * @param clipCommand The command to be handled
     * @param currentTick The absolute time position that this should be handled at (if this is in the past, it will be handled as soon as possible)
     */
    inline void handleCommand(ClipCommand *clipCommand, quint64 currentTick);
    ClipCommandRing commandRing;

    QString clientName;
    jack_client_t *jackClient{nullptr};
    jack_port_t *leftPort{nullptr};
    QString portNameLeft{"left_out"};
    jack_port_t *rightPort{nullptr};
    QString portNameRight{"right_out"};
    jack_port_t *midiInPort{nullptr};
    SamplerSynthVoice* voices[SAMPLER_CHANNEL_VOICE_COUNT];
    SamplerSynthPrivate* d{nullptr};
    int midiChannel{-1};
    float cpuLoad{0.0f};
    int modwheelValue{0};

    bool enabled{false};

    inline PlayGridManager* playGridManager() {
        if (!m_playGridManager) {
            m_playGridManager = PlayGridManager::instance();
        }
        return m_playGridManager;
    }
    PlayGridManager *m_playGridManager{nullptr};
    Grainerator *grainerator{nullptr};
};

// granular sampler synth stuff:
// * note on, start sending out clip commands based on the grain settings and the note and velocity, note off stops sending out notes (don't stop existing notes, just let the voice stop itself)
// * settings stored on clip
// * grain envelope (grainADSR)
// * grain selection area (existing start/end)
// * grain interval (minimum, maximum, value is ms)
// * grain size (minimum, maximum, value is ms)
// * pan variance (down from given pan, up from given pan, from -1.0 through 1.0)
// note on:
// get clipcommand for note
// start grain generator
    // pick grain based on clip data
    // start grain (create command based on clip's size, pan)
    // pick next grain time (interval + if variation is above 0, pick a time)
// note off:
// stop grain generator
// mark clipcommand for deletion

struct GraineratorVoice {
public:
    GraineratorVoice(SamplerChannel *channel)
        : channel(channel)
    {}
    ~GraineratorVoice() {}
    void start(ClipCommand *clipCommand, quint64 timestamp) {
        command = clipCommand;
        midiNote = clipCommand->midiNote;
        aftertouch = clipCommand->volume;
        envelope.reset();
        envelope.setSampleRate(clipCommand->clip->sampleRate());
        envelope.setParameters(clipCommand->clip->adsrParameters());
        startPosition = clipCommand->clip->getStartPosition(clipCommand->slice);
        stopPosition = clipCommand->clip->getStopPosition(clipCommand->slice);
        windowSize = (stopPosition - startPosition) * clipCommand->clip->grainSpray();
        position = startPosition + (clipCommand->clip->grainPosition() * (stopPosition - startPosition));
        if (clipCommand->clip->grainScan() != 0) {
            scan = 100.0f * clipCommand->clip->sampleRate() / channel->sampleRate();
        } else {
            scan = 0;
        }
        framesUntilNextGrain = timestamp;
    }
    void stop() {
        isTailingOff = true;
        envelope.noteOff();
    }
    ClipCommand *pickNextGrain() {
        const ClipAudioSource *clip = command->clip;
        ClipCommand *newGrain = ClipCommand::channelCommand(command->clip, command->midiChannel);
        if (newGrain == nullptr) {
            qWarning() << Q_FUNC_INFO << "Could not get a new grain, for some reason!";
        }
        newGrain->midiNote = command->midiNote;
        newGrain->startPlayback = true;
        newGrain->changeVolume = true;
        newGrain->volume = aftertouch * envelopeValue;
        newGrain->setStartPosition = true;
        newGrain->setStopPosition = true;
        newGrain->changePan = true;

        // We have two potential pitch ranges, with a weight that says which one of them to use more regularly
        // This might for example be used to make the majority of grains play at standard forward speed, and a
        // few occasional grains playing some variant of backward. To make that happen, you would use the settings
        // min1 = 1.0, max1 = 1.0, priority = 0.9, min2 = -1.2, max2 = -0.8
        // which then will result in the forward grains playing at normal pitch, backwards grains playing backward
        // at between 1.2 and 0.8 speed, and 90% of the generated grains being from the first set.
        if (clip->grainPitchMinimum1() == 1.0 && clip->grainPitchMaximum1() == 1.0 && clip->grainPitchMinimum2() == 1.0 && clip->grainPitchMaximum2() == 1.0) {
            // If all the pitch ranges are set to just play at normal pitch, don't do the random generation stuff below
            newGrain->changePitch = false;
            newGrain->pitchChange = 1.0f;
        } else {
            newGrain->changePitch = true;
            if (QRandomGenerator::global()->generateDouble() < clip->grainPitchPriority()) {
                // Lower range, use the first pitch range pair
                newGrain->pitchChange = clip->grainPitchMinimum1() + QRandomGenerator::global()->bounded(clip->grainPitchMaximum1() - clip->grainPitchMinimum1()) + pitch;
            } else {
                // Upper range, use the second pitch range pair
                newGrain->pitchChange = clip->grainPitchMinimum2() + QRandomGenerator::global()->bounded(clip->grainPitchMaximum2() - clip->grainPitchMinimum2()) + pitch;
            }
        }

        // grain duration (grain size start plus random from 0 through grain size additional, at most the size of the sample window)
        // (divided by 1000, because start and stop are expected to be in seconds, not milliseconds)
        const double duration = qMin((double(clip->grainSize()) + QRandomGenerator::global()->bounded(double(clip->grainSizeAdditional()))) / (abs(newGrain->pitchChange) * 1000.0f), double(clip->getDuration()));
        // grain start position
        if (windowSize < duration) {
            // If the duration is too long to fit inside the window, just start at the start - allow people to do it, since well, it'll work anyway
            newGrain->startPosition = position;
        } else {
            // Otherwise use the standard logic: from current position, to somewhere within the sample window, minus duration to ensure entire sample playback happens inside window
            newGrain->startPosition = position + QRandomGenerator::global()->bounded(double(windowSize - duration));
        }
        // Make sure we stick inside the window
        if (newGrain->startPosition > stopPosition) {
            newGrain->startPosition = startPosition + (newGrain->startPosition - stopPosition);
        }
        // grain stop position (start position plus duration - which has already been bounded by the above)
        newGrain->stopPosition = newGrain->startPosition + duration;
        // pan variance (random between pan minimum and pan maximum)
        newGrain->pan = clip->grainPanMinimum() + QRandomGenerator::global()->bounded(clip->grainPanMaximum() - clip->grainPanMinimum());
        return newGrain;
    }
    juce::ADSR envelope;
    SamplerChannel *channel{nullptr};
    ClipCommand *command{nullptr};
    float aftertouch{0};
    float pitch{0};
    float envelopeValue{0};
    float startPosition{0};
    float stopPosition{0};
    float windowSize{0};
    float position{0};
    float scan{0};
    jack_nframes_t framesUntilNextGrain{0};
    int midiNote{0};
    bool isActive{false};
    bool isTailingOff{false};
};

#define GRAINERATOR_VOICES 16
class Grainerator {
public:
    explicit Grainerator(SamplerChannel *channel)
        : channel(channel)
    {
        for (int i = 0; i < GRAINERATOR_VOICES; ++i) {
            voices[i] = new GraineratorVoice(channel);
        }
    }
    ~Grainerator() {}
    void start(ClipCommand *command, quint64 timestamp) {
        bool voiceFound{false};
        for (GraineratorVoice *voice : qAsConst(voices)) {
            if (voice->command == nullptr) {
                voice->start(command, timestamp);
                voiceFound = true;
                break;
            }
        }
        if (!voiceFound) {
            qWarning() << Q_FUNC_INFO << "Failed to find a free voice - Grainerator has" << GRAINERATOR_VOICES << "voice available, i guess you've used too many, oh no!";
            SyncTimer::instance()->deleteClipCommand(command);
        }
    }
    void stop(ClipCommand *command) {
        for (GraineratorVoice *voice : qAsConst(voices)) {
            if (voice->command && voice->isTailingOff == false && voice->command->equivalentTo(command)) {
                voice->stop();
                SyncTimer::instance()->deleteClipCommand(command);
                break;
            }
        }
    }
    // TODO Handle these changes at event time (schedule into a ring the way voice does it and do actual handling in process)
    void handlePitchChange(int midiChannel, float value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        for (GraineratorVoice *voice : qAsConst(voices)) {
            if (voice->command && (voice->command->midiChannel == midiChannel || globalChannel == midiChannel)) {
                voice->pitch = value;
                break;
            }
        }
    }
    void handleAftertouch(int midiChannel, int value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        for (GraineratorVoice *voice : qAsConst(voices)) {
            if (voice->command && (voice->command->midiChannel == midiChannel || globalChannel == midiChannel)) {
                voice->aftertouch = float(value) / 127.0f;
                break;
            }
        }
    }
    void handlePolyphonicAftertouch(int midiChannel, int midiNote, int value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        for (GraineratorVoice *voice : qAsConst(voices)) {
            if (voice->command && voice->midiNote == midiNote && (voice->command->midiChannel == midiChannel || globalChannel == midiChannel)) {
                voice->aftertouch = float(value) / 127.0f;
                break;
            }
        }
    }
    void process(jack_nframes_t nframes, float framesPerMillisecond, jack_nframes_t current_frames) {
        for (jack_nframes_t frame = 0; frame < nframes; ++frame) {
            for (GraineratorVoice *voice : qAsConst(voices)) {
                ClipCommand *command = voice->command;
                if (command) {
                    if (voice->isActive) {
                        voice->envelopeValue = voice->envelope.getNextSample();
                    }
                    if (voice->framesUntilNextGrain == 0) {
                        if (voice->isActive == false) {
                            // If the envelope is not yet active, start it
                            voice->isActive = true;
                            voice->envelope.noteOn();
                            voice->envelopeValue = voice->envelope.getNextSample();
                        }
                        // pick the grain to play and schedule that at position `frame`
                        channel->handleCommand(voice->pickNextGrain(), current_frames + frame);
                        // work out how many frames we have until the next grain
                        // (grain interval minimum,
                        // plus random 0 through grain interval additional,
                        // multiplied by framesPerMillisecond)
                        const double additionalInterval = command->clip->grainIntervalAdditional() > 0 ? QRandomGenerator::global()->bounded(double(command->clip->grainIntervalAdditional())) : 0.0f;
                        if (command->clip->grainInterval() == 0) {
                            voice->framesUntilNextGrain = framesPerMillisecond * (double(voice->stopPosition - voice->startPosition) + additionalInterval);
                        } else {
                            voice->framesUntilNextGrain = framesPerMillisecond * (double(command->clip->grainInterval()) + additionalInterval);
                        }
                        // Only do this if we're actually supposed to be scanning through the playback, otherwise it just gets a little silly
                        if (voice->scan != 0) {
                            const float grainScan{command->clip->grainScan()};
                            voice->position += std::clamp((grainScan / voice->scan), -voice->windowSize, voice->windowSize);
                            if (grainScan < 0) {
                                // We're moving in reverse, check lower bound
                                if (voice->position < voice->startPosition) {
                                    voice->position = voice->stopPosition - (voice->startPosition - voice->position);
                                }
                            } else {
                                // We're playing forward, check upper bound
                                if (voice->position > voice->stopPosition) {
                                    voice->position = voice->startPosition + (voice->position - voice->stopPosition);
                                }
                            }
                        }
                    }
                    if (voice->isActive && !voice->envelope.isActive()) {
                        // Then we've reached the end of the note and need to do all the stopping things
                        SyncTimer::instance()->deleteClipCommand(command);
                        voice->command = nullptr;
                        voice->isActive = false;
                        voice->isTailingOff = false;
                    }
                    voice->framesUntilNextGrain--;
                }
            }
        }
    }
private:
    GraineratorVoice *voices[GRAINERATOR_VOICES];
    SamplerChannel *channel{nullptr};
};

void jackConnect(jack_client_t* jackClient, const QString &from, const QString &to) {
    int result = jack_connect(jackClient, from.toUtf8(), to.toUtf8());
    if (result == 0 || result == EEXIST) {
//         qDebug() << "SamplerSynth:" << (result == EEXIST ? "Retaining existing connection from" : "Successfully created new connection from" ) << from << "to" << to;
    } else {
        qWarning() << "SamplerSynth: Failed to connect" << from << "with" << to << "with error code" << result;
        // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
    }
}

SamplerChannel::SamplerChannel(SamplerSynth *samplerSynth, jack_client_t *client, const QString &clientName, const int &midiChannel)
    : clientName(clientName)
    , midiChannel(midiChannel)
{
    grainerator = new Grainerator(this);
    jackClient = client;
    for (int voiceIndex = 0; voiceIndex < SAMPLER_CHANNEL_VOICE_COUNT; ++voiceIndex) {
        SamplerSynthVoice *voice = new SamplerSynthVoice(samplerSynth);
        voices[voiceIndex] = voice;
    }
    midiInPort = jack_port_register(jackClient, QString("%1-midiIn").arg(clientName).toUtf8(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    leftPort = jack_port_register(jackClient, QString("%1-%2").arg(clientName).arg(portNameLeft).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    rightPort = jack_port_register(jackClient, QString("%1-%2").arg(clientName).arg(portNameRight).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    jackConnect(jackClient, QString("SamplerSynth:%1-%2").arg(clientName).arg(portNameLeft).toUtf8(), QLatin1String{"system:playback_1"});
    jackConnect(jackClient, QString("SamplerSynth:%1-%2").arg(clientName).arg(portNameRight).toUtf8(), QLatin1String{"system:playback_2"});
    if (midiChannel < 0) {
        jackConnect(jackClient, QLatin1String("ZLRouter:PassthroughOut"), QString("SamplerSynth:%1-midiIn").arg(clientName));
    } else {
        jackConnect(jackClient, QLatin1String("ZLRouter:Channel%1").arg(QString::number(midiChannel)), QString("SamplerSynth:%1-midiIn").arg(clientName));
    }
    qInfo() << Q_FUNC_INFO << "Successfully created and set up" << clientName;
}

SamplerChannel::~SamplerChannel() {
    delete grainerator;
}

int SamplerChannel::process(jack_nframes_t nframes) {
    // First handle any queued up commands (starting, stopping, changes to voice state, that sort of stuff)
    quint64 timestamp{0};
    while (commandRing.readHead->clipCommand) {
        ClipCommand *command = commandRing.read(&timestamp);
        handleCommand(command, timestamp);
    }
    if (enabled) {
        jack_nframes_t current_frames;
        jack_time_t current_usecs;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(jackClient, &current_frames, &current_usecs, &next_usecs, &period_usecs);

        // First, let's handle ourselves some midi input
        void* inputBuffer = jack_port_get_buffer(midiInPort, nframes);
        jack_midi_event_t event;
        int eventChannel{-1};
        uint32_t eventIndex = 0;
        while (true) {
            if (int err = jack_midi_event_get(&event, inputBuffer, eventIndex)) {
                if (err != -ENOBUFS) {
                    qWarning() << clientName << "jack_midi_event_get failed, received note lost! Attempted to fetch at index" << eventIndex << "and the error code is" << err;
                }
                // Otherwise we can be reasonably certain that it's just the end of the buffer
                break;
            } else {
                const unsigned char &byte1 = event.buffer[0];
                const int globalChannel{0};
                if (0x7F < byte1 &&  byte1 < 0xf0) {
                    // TODO handle all-off message (so we can make the thing shut up when things like e.g. the pewpew app on a roli light block doesn't send out off notes when cleared)
                    // TODO Handle MPE global-channel instructions and upper/lower split...
                    eventChannel = (byte1 & 0xf);
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        // Note Off or On message
                        const int note{event.buffer[1]};
                        const int velocity{event.buffer[2]};
                        playGridManager()->midiMessageToClipCommands(&commandRing, midiChannel, byte1, note, velocity);
                        while (commandRing.readHead->clipCommand) {
                            ClipCommand *command = commandRing.read();
                            if (command->clip->granular()) {
                                if (command->stopPlayback) {
                                    grainerator->stop(command);
                                }
                                if (command->startPlayback) {
                                    grainerator->start(command, event.time);
                                }
                            } else {
                                handleCommand(command, current_frames + event.time);
                            }
                        }
                    } else if (0x9F < byte1 && byte1 < 0xB0) {
                        // Polyphonic Aftertouch
                        const int note{event.buffer[1]};
                        const int pressure{event.buffer[2]};
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->mostRecentStartCommand && voice->mostRecentStartCommand->midiNote == note && (voice->mostRecentStartCommand->midiChannel == eventChannel || eventChannel == globalChannel)) {
                                voice->handleAftertouch(event.time, pressure);
                            }
                        }
                        grainerator->handlePolyphonicAftertouch(eventChannel, note, pressure, event.time);
                    } else if (0xAF < byte1 && byte1 < 0xC0) {
                        // Control/mode change
                        const int control{event.buffer[1]};
                        const int value{event.buffer[2]};
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->mostRecentStartCommand && voice->mostRecentStartCommand->midiChannel == eventChannel) {
                                voice->handleControlChange(event.time, control, value);
                            }
                        }
                        if (control == 1) {
                            // Mod wheel - just storing this so we can pass it to new voices when we start them, so initial values make sense
                            modwheelValue = value;
                        }
                    } else if (0xBF < byte1 && byte1 < 0xD0) {
                        // Program change
                    } else if (0xCF < byte1 && byte1 < 0xE0) {
                        // Non-polyphonic aftertouch
                        const int pressure{event.buffer[1]};
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->mostRecentStartCommand && voice->mostRecentStartCommand->midiChannel == eventChannel) {
                                voice->handleAftertouch(event.time, pressure);
                            }
                        }
                        grainerator->handleAftertouch(eventChannel, pressure, event.time);
                    } else if (0xDF < byte1 && byte1 < 0xF0) {
                        // Pitch bend
                        // Going the other way:
                        // char byte3 = pitchValue >> 7;
                        // byte byte2 = pitchValue & 0x7F;
                        // Per-note pitch bend should be +/- 48 semitones by default
                        // Master-channel pitch bend should be +/- 2 semitones by default
                        // Change either to +/- 96 using Registered Parameter Number 0
                        const float bendMax{2.0};
                        const float pitchValue = bendMax * (float((event.buffer[2] * 128) + event.buffer[1]) - 8192) / 16383.0;
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->mostRecentStartCommand && voice->mostRecentStartCommand->midiChannel == eventChannel) {
                                voice->handlePitchChange(event.time, pitchValue);
                            }
                        }
                        grainerator->handlePitchChange(eventChannel, pitchValue, event.time);
                    }
                }
            }
            ++eventIndex;
        }
        const double framesPerMicrosecond = double(nframes) / double(next_usecs - current_usecs);
        grainerator->process(nframes, framesPerMicrosecond * 1000.0f, current_frames);

        // Then, if we've actually got our ports set up, let's play whatever voices are active
        jack_default_audio_sample_t *leftBuffer{nullptr}, *rightBuffer{nullptr};
        if (leftPort && rightPort) {
            leftBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(leftPort, nframes);
            rightBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(rightPort, nframes);
            memset(leftBuffer, 0, nframes * sizeof (jack_default_audio_sample_t));
            memset(rightBuffer, 0, nframes * sizeof (jack_default_audio_sample_t));
            for (SamplerSynthVoice *voice : qAsConst(voices)) {
                voice->process(leftBuffer, rightBuffer, nframes, current_frames, current_usecs, next_usecs, period_usecs);
            }
        }
        // Micro-hackery - -2 is the first item in the list of channels, so might as well just go with that
        if (midiChannel == -2) {
            cpuLoad = jack_cpu_load(jackClient);
        }
    }
    return 0;
}

class SamplerSynthPrivate {
public:
    SamplerSynthPrivate() {
    }
    ~SamplerSynthPrivate() {
        if (jackClient) {
            jack_client_close(jackClient);
        }
        qDeleteAll(channels);
    }
    int process(jack_nframes_t nframes);
    jack_client_t *jackClient{nullptr};
    bool initialized{false};
    QMutex synthMutex;
    bool syncLocked{false};
    static const int numVoices{128};
    jack_nframes_t sampleRate{0};

    QHash<ClipAudioSource*, SamplerSynthSound*> clipSounds;
    QList<ClipAudioSourcePositionsModel*> positionModels;
    te::Engine *engine{nullptr};

    // An ordered list of Jack clients, one each for...
    // Global audio (midi "channel" -2, for e.g. the metronome and sample previews)
    // Global effects targeted audio (midi "channel" -1)
    // Channel 1 (midi channel 0, and the logical music channel called Channel 1 in a sketchpad)
    // Channel 2 (midi channel 1)
    // ...
    // Channel 10 (midi channel 9)
    QList<SamplerChannel *> channels;
};

int SamplerSynthPrivate::process(jack_nframes_t nframes)
{
    if (initialized) {
        for(SamplerChannel *channel : qAsConst(channels)) {
            channel->process(nframes);
        }
        jack_nframes_t current_frames;
        jack_time_t current_usecs;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(jackClient, &current_frames, &current_usecs, &next_usecs, &period_usecs);
        for (ClipAudioSourcePositionsModel *model : qAsConst(positionModels)) {
            model->setMostRecentPositionUpdate(current_frames + nframes);
        }
    }
    return 0;
}

static int sampler_process(jack_nframes_t nframes, void* arg) {
    return static_cast<SamplerSynthPrivate*>(arg)->process(nframes);
}

double SamplerChannel::sampleRate() const
{
    return d->sampleRate;
}

void SamplerChannel::handleCommand(ClipCommand *clipCommand, quint64 currentTick)
{
    if (clipCommand->stopPlayback || clipCommand->startPlayback) {
        if (clipCommand->stopPlayback) {
            for (SamplerSynthVoice * voice : qAsConst(voices)) {
                const ClipCommand *currentVoiceCommand = voice->mostRecentStartCommand;
                if (voice->isTailingOff == false && currentVoiceCommand && currentVoiceCommand->equivalentTo(clipCommand)) {
                    voice->handleCommand(clipCommand, currentTick);
                    // Since we may have more than one going at the same time (since we allow long releases), just stop the first one
                    break;
                }
            }
        }
        if (clipCommand->startPlayback) {
            for (SamplerSynthVoice *voice : qAsConst(voices)) {
                if (voice->availableAfter < currentTick) {
                    voice->handleCommand(clipCommand, currentTick);
                    break;
                }
            }
        }
    } else {
        for (SamplerSynthVoice * voice : qAsConst(voices)) {
            const ClipCommand *currentVoiceCommand = voice->mostRecentStartCommand;
            if (currentVoiceCommand && currentVoiceCommand->equivalentTo(clipCommand)) {
                // Update the voice with the new command
                voice->handleCommand(clipCommand, currentTick);
            }
        }
    }
}

SamplerSynth *SamplerSynth::instance()  {
    static SamplerSynth *instance{nullptr};
    if (!instance) {
        instance = new SamplerSynth(qApp);
    }
    return instance;
}

SamplerSynth::SamplerSynth(QObject *parent)
    : QObject(parent)
    , d(new SamplerSynthPrivate)
{
}

SamplerSynth::~SamplerSynth()
{
    delete d;
}

void SamplerSynth::initialize(tracktion_engine::Engine *engine)
{
    d->engine = engine;
    jack_status_t real_jack_status{};
    d->jackClient = jack_client_open("SamplerSynth", JackNullOption, &real_jack_status);
    if (d->jackClient) {
        // Set the process callback.
        if (jack_set_process_callback(d->jackClient, sampler_process, d) == 0) {
            // Activate the client.
            if (jack_activate(d->jackClient) == 0) {
                d->sampleRate = jack_get_sample_rate(d->jackClient);
                qInfo() << Q_FUNC_INFO << "Successfully created and set up SamplerSynth client";
                zl_set_jack_client_affinity(d->jackClient);
                qInfo() << Q_FUNC_INFO << "Registering ten (plus two global) channels, with" << SAMPLER_CHANNEL_VOICE_COUNT << "voices each";
                for (int channelIndex = 0; channelIndex < 12; ++channelIndex) {
                    QString channelName;
                    if (channelIndex == 0) {
                        channelName = QString("global-uneffected");
                    } else if (channelIndex == 1) {
                        channelName = QString("global-effected");
                    } else {
                        channelName = QString("channel_%1").arg(QString::number(channelIndex -1));
                    }
                    // Funny story, the actual channels have midi channels equivalent to their name, minus one. The others we can cheat with
                    SamplerChannel *channel = new SamplerChannel(this, d->jackClient, channelName, channelIndex - 2);
                    channel->d = d;
                    d->channels << channel;
                }
                d->initialized = true;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to activate SamplerSynth Jack client";
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to set the SamplerSynth Jack processing callback";
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to set up SamplerSynth Jack client";
    }
}

tracktion_engine::Engine *SamplerSynth::engine() const
{
    return d->engine;
}

void SamplerSynth::registerClip(ClipAudioSource *clip)
{
    QMutexLocker locker(&d->synthMutex);
    if (!d->clipSounds.contains(clip)) {
        SamplerSynthSound *sound = new SamplerSynthSound(clip);
        d->clipSounds[clip] = sound;
        d->positionModels << clip->playbackPositionsModel();
    } else {
        qDebug() << "Clip list already contains the clip up for registration" << clip << clip->getFilePath();
    }
}

void SamplerSynth::unregisterClip(ClipAudioSource *clip)
{
    QMutexLocker locker(&d->synthMutex);
    if (d->clipSounds.contains(clip)) {
        d->clipSounds.remove(clip);
        d->positionModels.removeAll(clip->playbackPositionsModel());
    }
}

SamplerSynthSound * SamplerSynth::clipToSound(ClipAudioSource* clip) const
{
    return d->clipSounds[clip];
}

float SamplerSynth::cpuLoad() const
{
    if (d->channels.count() == 0) {
        return 0;
    }
    return d->channels[0]->cpuLoad;
}

void SamplerSynth::handleClipCommand(ClipCommand *clipCommand, quint64 currentTick)
{
    if (d->clipSounds.contains(clipCommand->clip) && clipCommand->midiChannel + 2 < d->channels.count()) {
        SamplerChannel *channel = d->channels[clipCommand->midiChannel + 2];
        if (channel->commandRing.writeHead->clipCommand) {
            qWarning() << Q_FUNC_INFO << "Big problem! Attempted to add a clip command to the queue, but we've not handled the one that's already in the queue.";
        } else {
            channel->commandRing.write(clipCommand, currentTick);
        }
    }
}

void SamplerSynth::setChannelEnabled(const int &channel, const bool &enabled) const
{
    if (channel > -3 && channel < 10) {
        if (d->channels[channel + 2]->enabled != enabled) {
            qDebug() << "Setting SamplerSynth channel" << channel << "to" << enabled;
            d->channels[channel + 2]->enabled = enabled;
        }
    }
}
