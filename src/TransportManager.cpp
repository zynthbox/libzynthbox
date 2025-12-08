#include "TransportManager.h"
#include "SyncTimer.h"
#include "TimerCommand.h"
#include "MidiRouterDevice.h"
#include "JackThreadAffinitySetter.h"

#include <QDebug>

#include <jack/jack.h>
#include <jack/midiport.h>

class TransportManagerPrivate {
public:
    TransportManagerPrivate(SyncTimer *syncTimerInstance);
    ~TransportManagerPrivate() {
        if (client) {
            jack_transport_stop(client);
            jack_release_timebase(client);
            jack_client_close(client);
        }
    }
    SyncTimer *syncTimer{nullptr};
    jack_client_t *client{nullptr};
    jack_port_t *inPort{nullptr};
    jack_port_t *outPort{nullptr};
    bool running{false};

    uint32_t mostRecentEventCount{0};
    jack_time_t nextMidiTick{0};
    const jack_midi_data_t midiTickEvent{0xf9};
    jack_time_t previousMidiClockFrame{0};
    double previousMidiClockDeltas[16]{0};
    double bpm{-1};
    jack_nframes_t jackFrameForLastRestart{0};
    jack_nframes_t midiTicksSinceLastRestart{0};
    jack_nframes_t mostRecentTickJackFrame{0};
    quint64 mostRecentlyClockedSyncTimerTick{0};
    jack_nframes_t jackFrameForLastSyncTimerTick{0};
    MidiRouterDevice *clockSourceDevice{nullptr};
    int clockSourcePPQN{-1};
    int internalPPQN{0};
    quint64 syncTimerIncrementPerTick{0};
    int process(jack_nframes_t nframes) {
        jack_nframes_t current_frames;
        jack_time_t current_usecs;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(client, &current_frames, &current_usecs, &next_usecs, &period_usecs);
        const double microsecondsPerFrame = double(next_usecs - current_usecs) / double(nframes);

        // Handle transport input as thrown at us by others
        void *inputBuffer = jack_port_get_buffer(inPort, nframes);
        jack_midi_event_t event;
        uint32_t eventIndex = 0;
        // Sniff for any midi start, stop, continue and so on messages, and react accordingly (unless we're already playing)
        while (true) {
            if (int err = jack_midi_event_get(&event, inputBuffer, eventIndex)) {
                if (err != -ENOBUFS) {
                    qWarning() << Q_FUNC_INFO << "jack_midi_event_get failed, received note lost! Attempted to fetch at index" << eventIndex << "and the error code is" << err;
                }
                break;
            } else {
                // switch (event.size) {
                //     case 1:
                //         qDebug() << Q_FUNC_INFO << "Event data (size 1)" << QString("0x%1").arg(event.buffer[0], 2, 16, QLatin1Char('0'));
                //         break;
                //     case 2:
                //         qDebug() << Q_FUNC_INFO << "Event data (size 2)" << QString("0x%1").arg(event.buffer[0], 2, 16, QLatin1Char('0')) << QString("0x%1").arg(event.buffer[1], 2, 16, QLatin1Char('0'));
                //         break;
                //     case 3:
                //         qDebug() << Q_FUNC_INFO << "Event data (size 3)" << QString("0x%1").arg(event.buffer[0], 2, 16, QLatin1Char('0')) << QString("0x%1").arg(event.buffer[1], 2, 16, QLatin1Char('0')) << QString("0x%1").arg(event.buffer[2], 2, 16, QLatin1Char('0'));
                //         break;
                //     default:
                //         qDebug() << Q_FUNC_INFO << "Weird event received we don't know anything about... apparently size" << event.size;
                // }
                switch(event.buffer[0]) {
                    case 0xf2: // position
                        // (event->buffer[2]<<7) | event->buffer[1];
                        break;
                    case 0xf8: // clock
                    {
                        // qDebug() << Q_FUNC_INFO << "Clock signal received";
                        if (clockSourcePPQN == -1) {
                            if (clockSourceDevice) {
                                clockSourcePPQN = clockSourceDevice->ppqn();
                                if (internalPPQN > clockSourcePPQN) {
                                    // If the internal PPQN is higher than the clock source, our increment is the number of SyncTimer ticks per each clock event
                                    syncTimerIncrementPerTick = quint64(internalPPQN / clockSourcePPQN);
                                } else {
                                    // If the external PPQN is the same or higher, then the increment is 1, but we use this variable to test when to update next
                                    syncTimerIncrementPerTick = quint64(clockSourcePPQN / internalPPQN);
                                }
                            } else {
                                // This is super extra bad, we apparently got a tick, but don't have a source...
                                // this isn't great. Let's just ignore that for now, we'll be clearing it up shortly
                                continue;
                            }
                        }
                        if (clockSourcePPQN < 1) {
                            // This is even more super extra bad, for some reason the ppqn of our source device is not a positive number?
                            // That should basically not be possible, and we can't really work with that
                            continue;
                        }
                        const jack_time_t currentMidiClockFrame{(current_frames + event.time)};
                        const double currentDelta{double((current_frames + event.time) - previousMidiClockFrame)};
                        // Logic is, have a rolling number of timestamp deltas up to 16, depending on a couple of things:
                        // - If there is only a small amount of change, assume this is not a jump in bpm, and instead that it is a smooth slide to another (or jitter), and allow that
                        // - If there is a larger jump in the timestamp delta between the previous and the incoming one, assume we are jumping to a new bpm, clear out previous timestamps, and start calculations from scratch
                        // If we have no previous deltas (that is, the first one is 0), use our current
                        // delta as the average so we at least have *something* to work with
                        double averageDelta{currentDelta};
                        for (int availableDeltas = 0; availableDeltas < 16; ++availableDeltas) {
                            if (previousMidiClockDeltas[availableDeltas] == 0) {
                                averageDelta /= double(availableDeltas + 1);
                                break;
                            } else {
                                averageDelta += previousMidiClockDeltas[availableDeltas];
                            }
                        }
                        if (abs(currentDelta - averageDelta) > 10) {
                            // If the delta is large, then we're being asked to jump immediately to another bpm
                            // Set all entries in our history to 0
                            memset(previousMidiClockDeltas, 0, sizeof(double)*16);
                        } else {
                            // If the delta change is tiny, assume we want either a smooth bpm transition, or that it's jitter
                            // Rotate all entries one step to the right (ignoring any overflow)
                            memmove(previousMidiClockDeltas + 1, previousMidiClockDeltas, sizeof(double)*15);
                        }
                        // Once rotated (or zeroed), set the first element to our new delta
                        previousMidiClockDeltas[0] = currentDelta;
                        previousMidiClockFrame = currentMidiClockFrame;
                        // Now we've got everything done, update our BPM to whatever is appropriate for the new average delta
                        const double newBpm{(1000.0 / (microsecondsPerFrame * averageDelta) / double(clockSourcePPQN)) * 60.0};
                        if (newBpm != bpm) {
                            bpm = newBpm;
                            QMetaObject::invokeMethod(syncTimer, &SyncTimer::effectiveBpmChanged, Qt::QueuedConnection);
                        }
                        ++midiTicksSinceLastRestart;
                        mostRecentTickJackFrame = currentMidiClockFrame;
                        static int throttleThing{0};
                        if (throttleThing++ > 10) {
                            qDebug() << Q_FUNC_INFO << "New BPM according to external clock is" << bpm;
                        }
                        // Update the SyncTimer tick data, so it can keep itself in sync
                        if (internalPPQN > clockSourcePPQN) {
                            // If the internal PPQN is higher than the clock source, make sure we're counting up the most recent time we got a clock event by by the number of sync timer steps that are in the clock source's clock period
                            mostRecentlyClockedSyncTimerTick += syncTimerIncrementPerTick;
                            jackFrameForLastSyncTimerTick = currentMidiClockFrame;
                        } else {
                            // If the external PPQN is the same or higher, update the ticks when we have an appropriate multiple of ticks since the last update
                            if (midiTicksSinceLastRestart == ((1 + mostRecentlyClockedSyncTimerTick) * syncTimerIncrementPerTick)) {
                                ++mostRecentlyClockedSyncTimerTick;
                                jackFrameForLastSyncTimerTick = currentMidiClockFrame;
                            }
                        }
                        // TODO Do we want to have a separate BPM value as defined by external clock, and then when in external clock mode, don't let the user set the thing, and instead display that value where-ever we display the BPM? Probably yes...
                        // TODO When we get a midi clock event, make sure we count up and push ticks to SyncTimer, so things can be scheduled as required.
                        // Optimally we want to also do some interpolation logic here, so we can intersperse ticks inside that timer... perhaps we can do
                        // that by synctimer knowing the BPM, and adjusting to each tick (either by pushing forward or backward) when it slips out of sync
                        // with the midi ticks, and otherwise "simply" operate entirely on BPM value the way we're doing already? That seems like it'd
                        // likely work reasonably well and scale both up and down...
                        break;
                    }
                    case 0xfa: // start
                    case 0xfb: // continue
                        // Spec says to ignore start messages if they arrive while playback is happening
                        qDebug() << Q_FUNC_INFO << "Received MIDI START message";
                        if (!syncTimer->timerRunning()) {
                            jackFrameForLastRestart = current_frames + event.time;
                            if (clockSourceDevice) {
                                // If we have a clock source, then *this* is when we're resetting things, not when the start command is actually handled
                                jack_transport_stop(client);
                                jack_transport_start(client);
                                midiTicksSinceLastRestart = 0;
                            }
                            TimerCommand *startCommand = syncTimer->getTimerCommand();
                            startCommand->operation = TimerCommand::StartPlaybackOperation;
                            syncTimer->scheduleTimerCommand(0, startCommand);
                        }
                        break;
                    case 0xfc: // stop
                        // Spec says to ignore stop messages if they arrive while playback is already stopped
                        qDebug() << Q_FUNC_INFO << "Received MIDI STOP message";
                        if (syncTimer->timerRunning()) {
                            TimerCommand *stopCommand = syncTimer->getTimerCommand();
                            stopCommand->operation = TimerCommand::StopPlaybackOperation;
                            syncTimer->scheduleTimerCommand(0, stopCommand);
                        }
                        break;
                    case 0xf9: // tick
                        // qDebug() << Q_FUNC_INFO << "Received MIDI Tick";
                        break;
                    default:
                        break;
                }
            }
            ++eventIndex;
        }
        void *outputBuffer = jack_port_get_buffer(outPort, nframes);
        jack_midi_clear_buffer(outputBuffer);
        // TODO These messages want to go onto the control channel (whatever is set in the zynthian settings), whenever appropriate... Tick is an rt message, so don't worry about that
        if (nextMidiTick == 0) {
            nextMidiTick = current_usecs;
        }
        int errorCode{0};
        while (nextMidiTick < next_usecs) {
            errorCode = jack_midi_event_write(outputBuffer, std::clamp<jack_nframes_t>(jack_time_to_frames(client, nextMidiTick) - current_frames, 0, nframes - 1), &midiTickEvent, 1);
            if (errorCode == ENOBUFS) {
                qWarning() << "Ran out of space while writing ticks to the buffer, how did this even happen?!";
            // Disabling this because it's a bit noisy on startup, and it basically just means "we had an xrun" anyway,
            // so... just ignore that here, we've got other things to worry about if that happens, and also it happens
            // on startup, when loading lots of things, which is a bit ugly, and also at that point irrelevant
            // } else if (errorCode != 0) {
                // qWarning() << Q_FUNC_INFO << "Error writing midi event at time point" << std::clamp<jack_nframes_t>(jack_time_to_frames(client, nextMidiTick) - current_frames, 0, nframes - 1) << "with error:" << -errorCode << strerror(-errorCode);
            }
            nextMidiTick += 10000;
        }
        return 0;
    }
    /**
     * @param state     current transport state.
     * @param nframes   number of frames in current period.
     * @param position  address of the position structure for the next cycle; pos->frame will be its frame number. If new_pos is FALSE, this structure contains extended position information from the current cycle. If TRUE, it contains whatever was set by the requester. The timebase_callback's task is to update the extended information here.
     * @param new_pos   TRUE (non-zero) for a newly requested pos, or for the first cycle after the timebase_callback is defined. 
     */
    void timebase_callback(jack_transport_state_t state, jack_nframes_t nframes, jack_position_t *position, int new_pos)  {
        /**
        * If new_pos is non-zero, calculate position->frame from Bar, Beat, Tick info
        * If new_pos is zero, calculate Bar, Beat, Tick from position->frame (the amount of frames this position is from the start of the playing song)
        */
        if (new_pos) {
            if (position->valid & JackPositionBBT) {
                qDebug() << Q_FUNC_INFO << "New position requested, based on bar/beat/tick" << state << nframes << position->bar << position->beat << position->tick;
            } else {
                // qDebug() << Q_FUNC_INFO << "New position and and bbt is not valid - is this the please-set-us-up-the-thing state?" << position->frame;
                syncTimer->setPosition(position);
            }
            position->valid = JackPositionBBT;
        } else {
            // qDebug() << Q_FUNC_INFO << "Calculate bar/beat/tick from position->frame - but actually we just pass in the values synctimer already calculated" << state << nframes << position->frame;
            syncTimer->setPosition(position);
            position->valid = JackPositionBBT;
        }
    }
};

int transport_process(jack_nframes_t nframes, void *arg) {
    return static_cast<TransportManagerPrivate*>(arg)->process(nframes);
}

void transport_timebase_callback(jack_transport_state_t state, jack_nframes_t nframes, jack_position_t *pos, int new_pos, void *arg) {
    return static_cast<TransportManagerPrivate*>(arg)->timebase_callback(state, nframes, pos, new_pos);
}

TransportManagerPrivate::TransportManagerPrivate(SyncTimer *syncTimerInstance)
{
    syncTimer = syncTimerInstance;
    internalPPQN = syncTimer->getMultiplier();
}


TransportManager::TransportManager(SyncTimer *parent)
    : QObject(parent)
    , d(new TransportManagerPrivate(parent))
{
}

TransportManager::~TransportManager()
{
    delete d;
}

void TransportManager::initialize()
{
    jack_status_t real_jack_status{};
    d->client = jack_client_open("TransportManager", JackNullOption, &real_jack_status);
    if (d->client) {
        d->inPort = jack_port_register(d->client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
        d->outPort = jack_port_register(d->client, "midi_out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);
        if (d->inPort && d->outPort) {
            if (jack_set_timebase_callback(d->client, 0, transport_timebase_callback, static_cast<void*>(d)) == 0) {
                // Set the process callback.
                if (jack_set_process_callback(d->client, transport_process, static_cast<void*>(d)) == 0) {
                    if (jack_activate(d->client) == 0) {
                        qDebug() << Q_FUNC_INFO << "Set up the transport manager, which lets us handle midi sync messages, and function as a Jack timebase master";
                        jack_transport_start(d->client);
                        zl_set_jack_client_affinity(d->client);
                    } else {
                        qWarning() << Q_FUNC_INFO << "Failed to activate the Jack client";
                    }
                } else {
                    qWarning() << Q_FUNC_INFO << "Failed to set Jack processing callback";
                }
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to register as transport master";
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to register one or both of the transport manager's ports";
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to create Jack client";
    }
}

void TransportManager::restartTransport()
{
    if (d->clockSourceDevice) {
        jack_transport_stop(d->client);
        jack_transport_start(d->client);
        d->midiTicksSinceLastRestart = 0;
    }
}

double TransportManager::bpm() const
{
    return d->bpm;
}

uint32_t TransportManager::midiTicksSinceLastRestart() const
{
    return d->midiTicksSinceLastRestart;
}

uint32_t TransportManager::mostRecentTickJackFrame() const
{
    return d->mostRecentTickJackFrame;
}

uint32_t TransportManager::jackFrameForLastRestart() const
{
    return d->jackFrameForLastRestart;
}

quint64 TransportManager::mostRecentlyClockedSyncTimerTick() const
{
    return d->mostRecentlyClockedSyncTimerTick;
}

int TransportManager::externalPPQN() const
{
    return d->clockSourcePPQN;
}

void TransportManager::setClockSource(MidiRouterDevice* device)
{
    if (d->clockSourceDevice) {
        d->clockSourceDevice->disconnect(this);
    }
    d->clockSourceDevice = device;
    if (d->clockSourceDevice) {
        connect(d->clockSourceDevice, &MidiRouterDevice::ppqnChanged, this, [this](){
            d->clockSourcePPQN = d->clockSourceDevice->ppqn();
            if (d->internalPPQN > d->clockSourcePPQN) {
                // If the internal PPQN is higher than the clock source, our increment is the number of SyncTimer ticks per each clock event
                d->syncTimerIncrementPerTick = quint64(d->internalPPQN / d->clockSourcePPQN);
            } else {
                // If the external PPQN is the same or higher, then the increment is 1, but we use this variable to test when to update next
                d->syncTimerIncrementPerTick = quint64(d->clockSourcePPQN / d->internalPPQN);
            }
        });
        connect(d->clockSourceDevice, &QObject::destroyed, this, [this](){
            d->clockSourceDevice = nullptr;
            d->clockSourcePPQN = -1;
            d->syncTimerIncrementPerTick = 0;
            d->bpm = -1;
            Q_EMIT d->syncTimer->effectiveBpmChanged();
        });
    }
    Q_EMIT d->syncTimer->effectiveBpmChanged();
}
