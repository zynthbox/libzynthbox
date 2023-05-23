
#include "SamplerSynth.h"

#include "JUCEHeaders.h"
#include "Helper.h"
#include "PlayGridManager.h"
#include "SamplerSynthSound.h"
#include "SamplerSynthVoice.h"
#include "ClipCommand.h"
#include "SyncTimer.h"

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

#define SAMPLER_CHANNEL_VOICE_COUNT 16

#define CommandQueueSize 256
class Grainerator;
class SamplerChannel
{
public:
    explicit SamplerChannel(const QString &clientName, const int &midiChannel);
    ~SamplerChannel();
    int process(jack_nframes_t nframes);
    double sampleRate() const;
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

    bool enabled{true};

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

class Grainerator {
public:
    explicit Grainerator(SamplerChannel *channel)
        : channel(channel)
    {
        for (int i = 0; i < 128; ++i) {
            clipCommands[i] = nullptr;
            framesUntilNextGrain[i] = 0;
            aftertouch[i] = 1.0f;
            pitch[i] = 0;
            envelopeValue[i] = 0;
            position[i] = 0;
        }
    }
    ~Grainerator() {}
    // TODO handle multi-sample triggers for the same note... woops, forgot we can do that ;)
    void start(ClipCommand *clipCommand, quint64 timestamp) {
        const int midiNote{clipCommand->midiNote};
        aftertouch[midiNote] = clipCommand->volume;
        envelope[midiNote].reset();
        envelope[midiNote].setSampleRate(clipCommand->clip->sampleRate());
        envelope[midiNote].setParameters(clipCommand->clip->adsrParameters());
        startPosition[midiNote] = clipCommand->clip->getStartPosition(clipCommand->slice);
        stopPosition[midiNote] = clipCommand->clip->getStopPosition(clipCommand->slice);
        windowSize[midiNote] = (stopPosition[midiNote] - startPosition[midiNote]) * clipCommand->clip->grainSpray();
        position[midiNote] = startPosition[midiNote] + (clipCommand->clip->grainPosition() * (stopPosition[midiNote] - startPosition[midiNote]));
        if (clipCommand->clip->grainScan() != 0) {
            scan[midiNote] = 100.0f * clipCommand->clip->sampleRate() / channel->sampleRate();
        } else {
            scan[midiNote] = 0;
        }
        framesUntilNextGrain[midiNote] = timestamp;
        clipCommands[midiNote] = clipCommand;
    }
    void stop(ClipCommand *clipCommand) {
        envelope[clipCommand->midiNote].noteOff();
    }
    // TODO Handle these changes at event time (schedule into a ring the way voice does it and do actual handling in process)
    void handlePitchChange(int midiChannel, float value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        for (int midiNote = 0; midiNote < 128; ++midiNote) {
            if (clipCommands[midiNote] && (clipCommands[midiNote]->midiChannel == midiChannel || globalChannel == midiChannel)) {
                pitch[midiNote] = value;
            }
        }
    }
    void handleAftertouch(int midiChannel, int value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        for (int midiNote = 0; midiNote < 128; ++midiNote) {
            if (clipCommands[midiNote] && (clipCommands[midiNote]->midiChannel == midiChannel || globalChannel == midiChannel)) {
                aftertouch[midiNote] = float(value) / 127.0f;
            }
        }
    }
    void handlePolyphonicAftertouch(int midiChannel, int midiNote, int value, jack_nframes_t /*eventTime*/) {
        const int globalChannel{0};
        if (clipCommands[midiNote] && (clipCommands[midiNote]->midiChannel == midiChannel || globalChannel == midiChannel)) {
            aftertouch[midiNote] = float(value) / 127.0f;
        }
    }
    void process(jack_nframes_t nframes, float framesPerMillisecond) {
        for (jack_nframes_t frame = 0; frame < nframes; ++frame) {
            for (int midiNote = 0; midiNote < 128; ++midiNote) {
                ClipCommand *command = clipCommands[midiNote];
                if (command) {
                    // If the envelope is not yet active, start it
                    if (!envelope[midiNote].isActive()) {
                        envelope[midiNote].noteOn();
                    }
                    envelopeValue[midiNote] = envelope[midiNote].getNextSample();
                    if (framesUntilNextGrain[midiNote] == 0) {
                        // pick the grain to play and schedule that at position `frame`
                        ClipCommand *command = pickNextGrain(midiNote);
                        channel->handleCommand(command, frame);
                        // work out how many frames we have until the next grain
                        // (grain interval minimum,
                        // plus random 0 through grain interval additional,
                        // multiplied by framesPerMillisecond)
                        const double additionalInterval = command->clip->grainIntervalAdditional() > 0 ? QRandomGenerator::global()->bounded(double(command->clip->grainIntervalAdditional())) : 0.0f;
                        if (command->clip->grainInterval() == 0) {
                            framesUntilNextGrain[midiNote] = framesPerMillisecond * (double(command->clip->getStopPosition(command->slice) - command->clip->getStartPosition(command->slice)) + additionalInterval);
                        } else {
                            framesUntilNextGrain[midiNote] = framesPerMillisecond * (double(command->clip->grainInterval()) + additionalInterval);
                        }
                        // Only do this if we're actually supposed to be scanning through the playback, otherwise it just gets a little silly
                        if (scan[midiNote] != 0) {
                            const float grainScan{command->clip->grainScan()};
                            position[midiNote] += std::clamp((grainScan / scan[midiNote]), -windowSize[midiNote], windowSize[midiNote]);
                            if (grainScan < 0) {
                                // We're moving in reverse, check lower bound
                                if (position[midiNote] < startPosition[midiNote]) {
                                    position[midiNote] = stopPosition[midiNote] - (startPosition[midiNote] - position[midiNote]);
                                }
                            } else {
                                // We're playing forward, check upper bound
                                if (position[midiNote] > stopPosition[midiNote]) {
                                    position[midiNote] = startPosition[midiNote] + (position[midiNote] - stopPosition[midiNote]);
                                }
                            }
                        }
                    }
                    if (!envelope[midiNote].isActive()) {
                        // Then we've reached the end of the note and need to do all the stopping things
                        SyncTimer::instance()->deleteClipCommand(command);
                        SyncTimer::instance()->deleteClipCommand(clipCommands[command->midiNote]);
                        clipCommands[command->midiNote] = nullptr;
                        framesUntilNextGrain[command->midiNote] = 0;
                        aftertouch[command->midiNote] = 0;
                        pitch[command->midiNote] = 0;
                    }
                    framesUntilNextGrain[midiNote]--;
                }
            }
        }
    }
    ClipCommand *pickNextGrain(int midiNote) {
        const ClipCommand *command = clipCommands[midiNote];
        ClipCommand *newGrain = ClipCommand::channelCommand(command->clip, command->midiChannel);
        newGrain->startPlayback = true;
        newGrain->midiNote = command->midiNote;
        newGrain->changeVolume = true;
        newGrain->volume = aftertouch[midiNote] * envelopeValue[midiNote];
        newGrain->setStartPosition = true;
        newGrain->setStopPosition = true;
        newGrain->changePan = true;
        // grain duration (grain size start plus random from 0 through grain size additional, at most the size of the sample window)
        // (divided by 1000, because start and stop are expected to be in seconds, not milliseconds)
        const double duration = qMin((double(newGrain->clip->grainSize()) + QRandomGenerator::global()->bounded(double(newGrain->clip->grainSizeAdditional()))) / 1000.0f, double(newGrain->clip->getDuration()));
        // grain start position
        if (windowSize[midiNote] < duration) {
            // If the duration is too long to fit inside the window, just start at the start - allow people to do it, since well, it'll work anyway
            newGrain->startPosition = position[midiNote];
        } else {
            // Otherwise use the standard logic: from current position, to somewhere within the sample window, minus duration to ensure entire sample playback happens inside window
            newGrain->startPosition = position[midiNote] + QRandomGenerator::global()->bounded(double(windowSize[midiNote] - duration));
        }
        // Make sure we stick inside the window
        if (newGrain->startPosition > stopPosition[midiNote]) {
            newGrain->startPosition = startPosition[midiNote] + (newGrain->startPosition - stopPosition[midiNote]);
        }
        // grain stop position (start position plus duration - which has already been bounded by the above)
        newGrain->stopPosition = newGrain->startPosition + duration;
        // pan variance (random between pan minimum and pan maximum)
        newGrain->pan = newGrain->clip->grainPanMinimum() + QRandomGenerator::global()->bounded(newGrain->clip->grainPanMaximum() - newGrain->clip->grainPanMinimum());
        return newGrain;
    }
private:
    SamplerChannel *channel{nullptr};
    ClipCommand *clipCommands[128];
    jack_nframes_t framesUntilNextGrain[128];
    float aftertouch[128];
    float pitch[128];
    juce::ADSR envelope[128];
    float envelopeValue[128];
    float startPosition[128];
    float stopPosition[128];
    float windowSize[128];
    float position[128];
    float scan[128];
};

static int client_process(jack_nframes_t nframes, void* arg) {
    return static_cast<SamplerChannel*>(arg)->process(nframes);
}

void jackConnect(jack_client_t* jackClient, const QString &from, const QString &to) {
    int result = jack_connect(jackClient, from.toUtf8(), to.toUtf8());
    if (result == 0 || result == EEXIST) {
//         qDebug() << "SamplerSynth:" << (result == EEXIST ? "Retaining existing connection from" : "Successfully created new connection from" ) << from << "to" << to;
    } else {
        qWarning() << "SamplerSynth: Failed to connect" << from << "with" << to << "with error code" << result;
        // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
    }
}

SamplerChannel::SamplerChannel(const QString &clientName, const int &midiChannel)
    : clientName(clientName)
    , midiChannel(midiChannel)
{
    grainerator = new Grainerator(this);
    jack_status_t real_jack_status{};
    jackClient = jack_client_open(clientName.toUtf8(), JackNullOption, &real_jack_status);
    if (jackClient) {
        // Set the process callback.
        if (jack_set_process_callback(jackClient, client_process, this) == 0) {
            for (int voiceIndex = 0; voiceIndex < SAMPLER_CHANNEL_VOICE_COUNT; ++voiceIndex) {
                SamplerSynthVoice *voice = new SamplerSynthVoice();
                voices[voiceIndex] = voice;
            }
            midiInPort = jack_port_register(jackClient, "midiIn", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
            leftPort = jack_port_register(jackClient, portNameLeft.toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            rightPort = jack_port_register(jackClient, portNameRight.toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            // Activate the client.
            if (jack_activate(jackClient) == 0) {
                jackConnect(jackClient, QString("%1:%2").arg(clientName).arg(portNameLeft).toUtf8(), QLatin1String{"system:playback_1"});
                jackConnect(jackClient, QString("%1:%2").arg(clientName).arg(portNameRight).toUtf8(), QLatin1String{"system:playback_2"});
                if (midiChannel < 0) {
                    jackConnect(jackClient, QLatin1String("ZLRouter:PassthroughOut"), QString("%1:midiIn").arg(clientName));
                } else {
                    jackConnect(jackClient, QLatin1String("ZLRouter:Channel%1").arg(QString::number(midiChannel)), QString("%1:midiIn").arg(clientName));
                }
                qInfo() << Q_FUNC_INFO << "Successfully created and set up" << clientName;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to activate SamplerSynth Jack client" << clientName;
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to set the SamplerSynth Jack processing callback";
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to set up SamplerSynth Jack client" << clientName;
    }
}

SamplerChannel::~SamplerChannel() {
    if (jackClient) {
        jack_client_close(jackClient);
    }
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
                                handleCommand(command, event.time);
                            }
                        }
                    } else if (0x9F < byte1 && byte1 < 0xB0) {
                        // Polyphonic Aftertouch
                        const int note{event.buffer[1]};
                        const int pressure{event.buffer[2]};
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->isPlaying && voice->currentCommand()->midiNote == note && (voice->currentCommand()->midiChannel == eventChannel || eventChannel == globalChannel)) {
                                voice->handleAftertouch(event.time, pressure);
                            }
                        }
                        grainerator->handlePolyphonicAftertouch(eventChannel, note, pressure, event.time);
                    } else if (0xAF < byte1 && byte1 < 0xC0) {
                        // Control/mode change
                        const int control{event.buffer[1]};
                        const int value{event.buffer[2]};
                        for (SamplerSynthVoice *voice : qAsConst(voices)) {
                            if (voice->isPlaying && voice->currentCommand()->midiChannel == eventChannel) {
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
                            if (voice->isPlaying && voice->currentCommand()->midiChannel == eventChannel) {
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
                            if (voice->isPlaying && voice->currentCommand()->midiChannel == eventChannel) {
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
        grainerator->process(nframes, framesPerMicrosecond * 1000.0f);

        // Then, if we've actually got our ports set up, let's play whatever voices are active
        jack_default_audio_sample_t *leftBuffer{nullptr}, *rightBuffer{nullptr};
        if (leftPort && rightPort) {
            leftBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(leftPort, nframes);
            rightBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(rightPort, nframes);
            memset(leftBuffer, 0, nframes * sizeof (jack_default_audio_sample_t));
            memset(rightBuffer, 0, nframes * sizeof (jack_default_audio_sample_t));
            for (SamplerSynthVoice *voice : qAsConst(voices)) {
                if (voice->isPlaying) {
                    voice->process(leftBuffer, rightBuffer, nframes, current_frames, current_usecs, next_usecs, period_usecs);
                }
            }
        }
        // Micro-hackery - -2 is the first item in the list of channels, so might as well just go with that
        if (midiChannel == -2) {
            cpuLoad = jack_cpu_load(jackClient);
        }
    }
    return 0;
}

class SamplerSynthImpl;
class SamplerSynthPrivate {
public:
    SamplerSynthPrivate() {
    }
    ~SamplerSynthPrivate() {
        qDeleteAll(channels);
    }
    QMutex synthMutex;
    bool syncLocked{false};
    SamplerSynthImpl *synth{nullptr};
    static const int numVoices{128};

    QHash<ClipAudioSource*, SamplerSynthSound*> clipSounds;
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

class SamplerSynthImpl : public juce::Synthesiser {
public:
    void startVoiceImpl(juce::SynthesiserVoice* voice, juce::SynthesiserSound* sound, int midiChannel, int midiNoteNumber, float velocity)
    {
        startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
    }
    SamplerSynthPrivate *d{nullptr};
};

double SamplerChannel::sampleRate() const
{
    return d->synth->getSampleRate();
}

void SamplerChannel::handleCommand(ClipCommand *clipCommand, quint64 currentTick)
{
    SamplerSynthSound *sound = d->clipSounds[clipCommand->clip];
    if (clipCommand->stopPlayback || clipCommand->startPlayback) {
        if (clipCommand->stopPlayback) {
            for (SamplerSynthVoice * voice : qAsConst(voices)) {
                const ClipCommand *currentVoiceCommand = voice->currentCommand();
                if (voice->isTailingOff == false && voice->getCurrentlyPlayingSound().get() == sound && currentVoiceCommand->equivalentTo(clipCommand)) {
                    voice->setCurrentCommand(clipCommand);
                    voice->stopNote(clipCommand->volume, true);
                    // Since we may have more than one going at the same time (since we allow long releases), just stop the first one
                    break;
                }
            }
        }
        if (clipCommand->startPlayback) {
            for (SamplerSynthVoice *voice : qAsConst(voices)) {
                if (!voice->isPlaying) {
                    voice->setCurrentCommand(clipCommand);
                    voice->setModwheel(modwheelValue);
                    voice->setStartTick(currentTick);
                    d->synth->startVoiceImpl(voice, sound, clipCommand->midiChannel, clipCommand->midiNote, clipCommand->volume);
                    break;
                }
            }
        }
    } else {
        for (SamplerSynthVoice * voice : qAsConst(voices)) {
            const ClipCommand *currentVoiceCommand = voice->currentCommand();
            if (voice->getCurrentlyPlayingSound().get() == sound && currentVoiceCommand->equivalentTo(clipCommand)) {
                // Update the voice with the new command
                voice->setCurrentCommand(clipCommand);
                // We may have more than one thing going for the same sound on the same note, which... shouldn't
                // really happen, but it's ugly and we just need to deal with that when stopping, so, update /all/
                // the voices where both the sound and the command match.
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
    d->synth = new SamplerSynthImpl();
    d->synth->d = d;
}

SamplerSynth::~SamplerSynth()
{
    delete d->synth;
    delete d;
}

void SamplerSynth::initialize(tracktion_engine::Engine *engine)
{
    d->engine = engine;
    qInfo() << Q_FUNC_INFO << "Registering ten (plus two global) channels, with" << SAMPLER_CHANNEL_VOICE_COUNT << "voices each";
    for (int channelIndex = 0; channelIndex < 12; ++channelIndex) {
        QString channelName;
        if (channelIndex == 0) {
            channelName = QString("SamplerSynth-global-uneffected");
        } else if (channelIndex == 1) {
            channelName = QString("SamplerSynth-global-effected");
        } else {
            channelName = QString("SamplerSynth-channel_%1").arg(QString::number(channelIndex -1));
        }
        // Funny story, the actual channels have midi channels equivalent to their name, minus one. The others we can cheat with
        SamplerChannel *channel = new SamplerChannel(channelName, channelIndex - 2);
        channel->d = d;
        jack_nframes_t sampleRate = jack_get_sample_rate(channel->jackClient);
        d->synth->setCurrentPlaybackSampleRate(sampleRate);
        for (SamplerSynthVoice *voice : qAsConst(channel->voices)) {
            d->synth->addVoice(voice);
        }
        d->channels << channel;
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
        d->synth->addSound(sound);
    } else {
        qDebug() << "Clip list already contains the clip up for registration" << clip << clip->getFilePath();
    }
}

void SamplerSynth::unregisterClip(ClipAudioSource *clip)
{
    QMutexLocker locker(&d->synthMutex);
    if (d->clipSounds.contains(clip)) {
        d->clipSounds.remove(clip);
        for (int i = 0; i < d->synth->getNumSounds(); ++i) {
            SynthesiserSound::Ptr sound = d->synth->getSound(i);
            if (auto *samplerSound = static_cast<SamplerSynthSound*> (sound.get())) {
                if (samplerSound->clip() == clip) {
                    d->synth->removeSound(i);
                    break;
                }
            }
        }
    }
}

void SamplerSynth::handleClipCommand(ClipCommand *clipCommand)
{
    qWarning() << Q_FUNC_INFO << "This function is not sufficiently safe - schedule notes using SyncTimer::scheduleClipCommand instead!";
    handleClipCommand(clipCommand, SyncTimer::instance()->jackPlayhead());
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
