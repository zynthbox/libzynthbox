#include "MidiRouter.h"
#include "MidiRouterDeviceModel.h"

#include "ZynthboxBasics.h"
#include "JackConnectionHandler.h"
#include "JackPassthrough.h"
#include "PatternModel.h"
#include "PlayGridManager.h"
#include "MidiRecorder.h"
#include "MidiRouterDevice.h"
#include "MidiRouterFilter.h"
#include "MidiRouterFilterEntryRewriter.h"
#include "SyncTimer.h"
#include "TransportManager.h"
#include "JackThreadAffinitySetter.h"

#include <QDebug>
#include <QProcessEnvironment>
#include <QTimer>

#include <jack/jack.h>
#include <jack/midiport.h>

#include <chrono>
#include "MidiRouterFilterEntry.h"

// Set this to true to emit a bunch more debug output when the router is operating
#define DebugZLRouter false
// Set this to true to send out more debug information for the listener part of the class
#define DebugMidiListener false
// Set this to true to enable the watchdog
#define ZLROUTER_WATCHDOG false

#define MAX_LISTENER_MESSAGES 1024

// This is our translation from midi input channels to destinations. It contains
// information on what external output channel should be used if it's not a straight
// passthrough to the same channel the other side, and what channels should be
// targeted on the zynthian outputs.
struct SketchpadTrackInfo {
    SketchpadTrackInfo(int trackIndex)
        : trackIndex(trackIndex)
    {
        for (int i = 0; i < 16; ++i) {
            zynthianChannels[i] = -1;
        }
    }
    int zynthianChannels[16];
    MidiRouterDevice *routerDevice{nullptr};
    QString portName;
    int trackIndex{-1};
    int externalChannel{-1};
    MidiRouter::RoutingDestination destination{MidiRouter::ZynthianDestination};
    int currentlySelectedPatternIndex{-1};
    PatternModel *currentlySelectedPattern{nullptr};
    KeyScales::Octave octave{KeyScales::Octave4};
    KeyScales::Pitch pitch{KeyScales::PitchC};
    KeyScales::Scale scale{KeyScales::ScaleChromatic};
    PatternModel::KeyScaleLockStyle lockStyle{PatternModel::KeyScaleLockOff};
    KeyScales *keyScales{KeyScales::instance()};
    inline bool applyKeyScale(jack_midi_event_t& event) {
        // We only care about events...
        // - if we're supposed to be *some* kind of handling
        // - the scale is not chromatic (if it is, any given note will be on scale)
        // - it is a note related message (so note on or off, or poly aftertouch)
        if (lockStyle != PatternModel::KeyScaleLockOff && scale != KeyScales::ScaleChromatic && 0x79 < event.buffer[0] && event.buffer[0] < 0xB0) {
            // If we've got a pattern model defined, then we know what to do (otherwise we'll not have much idea)
            if (lockStyle == PatternModel::KeyScaleLockRewrite) {
                // Set the note value of the event to what it is supposed to be
                event.buffer[1] = keyScales->onScaleNote(event.buffer[1], scale, pitch, octave);
            } else if (lockStyle == PatternModel::KeyScaleLockBlock && keyScales->midiNoteOnScale(event.buffer[1], scale, pitch, octave) == false) {
                // We do not accept this event, as it is for a note which is not on scale
                return false;
            }
        }
        return true;
    }
};

struct MidiListenerPort {
    struct NoteMessage {
        bool fromInternal{false};
        bool isNoteMessage{false};
        unsigned char byte0{0};
        unsigned char byte1{0};
        unsigned char byte2{0};
        int size{0};
        int sketchpadTrack{0};
        MidiRouterDevice *eventDevice{nullptr};
        double timeStamp{0};
        NoteMessage *next{nullptr};
        NoteMessage *previous{nullptr};
        bool submitted{true};
    };
    MidiListenerPort() {
        NoteMessage *previous{&messages[MAX_LISTENER_MESSAGES - 1]};
        for (int i = 0; i < MAX_LISTENER_MESSAGES; ++i) {
            messages[i].previous = previous;
            previous->next = &messages[i];
            previous = &messages[i];
        }
        readHead = &messages[0];
        writeHead = &messages[0];
    }
    ~MidiListenerPort() { }
    inline void addMessage(const bool &fromInternal, const bool &isNoteMessage, const double &timeStampFrames, const double &timeStampUsecs, const jack_midi_event_t &event, const int rewriteChannel, const int sketchpadTrack, MidiRouterDevice *eventDevice)
    {
        if (eventDevice == nullptr) {
            qWarning() << Q_FUNC_INFO << "An extremely unlikely situation has occurred and we've ended up with a nullptr eventDevice - not adding message. The remaining data is" << fromInternal << isNoteMessage << timeStampFrames << timeStampUsecs << event.size << rewriteChannel << sketchpadTrack;
        } else {
            NoteMessage &message = *writeHead;
            writeHead = writeHead->next;
            message.timeStamp = timeStampFrames;
            const int eventChannel = (event.buffer[0] & 0xf);
            message.fromInternal = fromInternal;
            message.isNoteMessage = isNoteMessage;
            message.byte0 = event.buffer[0] - eventChannel + rewriteChannel;
            message.byte1 = event.size > 1 ? event.buffer[1] : 0;
            message.byte2 = event.size > 2 ? event.buffer[2] : 0;
            message.size = int(event.size);
            message.sketchpadTrack = sketchpadTrack;
            message.eventDevice = eventDevice;
            message.submitted = false;
            if (identifier == MidiRouter::PassthroughPort) {
                MidiRecorder::instance()->handleMidiMessage(message.byte0, message.byte1, message.byte2, event.size, timeStampUsecs, sketchpadTrack);
            }
        }
    }
    NoteMessage messages[MAX_LISTENER_MESSAGES];
    NoteMessage* writeHead{nullptr};
    NoteMessage* readHead{nullptr};
    MidiRouter::ListenerPort identifier{MidiRouter::UnknownPort};
    int waitTime{5};
};

// This class will watch what events ZynMidiRouter says it has handled, and just count them.
// The logic is then that we can compare that with what we think we wrote out during the most
// recent run in MidiRouter, and if they don't match, we can reissue the previous run's events
class MidiRouterWatchdog {
public:
    MidiRouterWatchdog();
    ~MidiRouterWatchdog() {
        if (client) {
            jack_client_close(client);
        }
    }
    jack_client_t *client{nullptr};
    jack_port_t *port{nullptr};

    uint32_t mostRecentEventCount{0};
    int process(jack_nframes_t nframes) {
        void *buffer = jack_port_get_buffer(port, nframes);
        mostRecentEventCount = jack_midi_get_event_count(buffer);
        return 0;
    }
};

int watchdog_process(jack_nframes_t nframes, void *arg) {
    return static_cast<MidiRouterWatchdog*>(arg)->process(nframes);
}

MidiRouterWatchdog::MidiRouterWatchdog()
{
#if ZLROUTER_WATCHDOG
    jack_status_t real_jack_status{};
    client = jack_client_open("ZLRouterWatchdog", JackNullOption, &real_jack_status);
    if (client) {
        port = jack_port_register(client, "ZynMidiRouterIn", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
        if (port) {
            // Set the process callback.
            if (jack_set_process_callback(client, watchdog_process, static_cast<void*>(this)) == 0) {
                if (jack_activate(client) == 0) {
                    int result = jack_connect(client, "ZynMidiRouter:midi_out", "ZLRouterWatchdog:ZynMidiRouterIn");
                    if (result == 0 || result == EEXIST) {
                        qDebug() << "ZLRouter Watchdog: Set up the watchdog for ZynMidiRouter, which lets us keep a track of what events are going through";
                        zl_set_jack_client_affinity(client);
                    } else {
                        qWarning() << "ZLRouter Watchdog: Failed to connect to ZynMidiRouter's midi output port";
                    }
                } else {
                    qWarning() << "ZLRouter Watchdog: Failed to activate the Jack client";
                }
            } else {
                qWarning() << "ZLRouter Watchdog: Failed to set Jack processing callback";
            }
        } else {
            qWarning() << "ZLRouter Watchdog: Failed to register watchdog port";
        }
    } else {
        qWarning() << "ZLRouter Watchdog: Failed to create Jack client";
    }
#endif
}

jack_time_t expected_next_usecs{0};
class MidiRouterPrivate {
public:
    MidiRouterPrivate(MidiRouter *q)
        : q(q)
    {
        for (int channel = 0; channel < 16; ++channel) {
            masterChannels.append(15);
        }
        for (int i = 0; i < ZynthboxTrackCount; ++i) {
            sketchpadTracks[i] = nullptr;
        }
        passthroughListener.identifier = MidiRouter::PassthroughPort;
        passthroughListener.waitTime = 1;
        listenerPorts[0] = &passthroughListener;
        internalPassthroughListener.identifier = MidiRouter::InternalPassthroughPort;
        internalPassthroughListener.waitTime = 5;
        listenerPorts[1] = &internalPassthroughListener;
        internalControllerPassthroughListener.identifier = MidiRouter::InternalControllerPassthroughPort;
        internalControllerPassthroughListener.waitTime = 5;
        listenerPorts[2] = &internalControllerPassthroughListener;
        hardwareInListener.identifier = MidiRouter::HardwareInPassthroughPort;
        hardwareInListener.waitTime = 5;
        listenerPorts[3] = &hardwareInListener;
        externalOutListener.identifier = MidiRouter::ExternalOutPort;
        externalOutListener.waitTime = 5;
        listenerPorts[4] = &externalOutListener;
        syncTimer = SyncTimer::instance();
    };
    ~MidiRouterPrivate() {
        if (jackClient) {
            jack_client_close(jackClient);
        }
        for (SketchpadTrackInfo *track : sketchpadTracks) {
            delete track;
        }
        delete watchdog;
    };

    MidiRouter *q;
    MidiRouterDeviceModel *devicesModel{nullptr};
    MidiRouterWatchdog *watchdog{new MidiRouterWatchdog};
    SyncTimer *syncTimer{nullptr};
    bool done{false};
    bool constructing{true};
    bool filterMidiOut{false};
    QStringList disabledMidiInPorts;
    QStringList enabledMidiOutPorts;
    QStringList enabledMidiFbPorts;
    // By default, let's just do an all-Upper zone setup
    int expressiveSplitPoint{-1};
    int masterChannel{15};
    QList<int> masterChannels;
    float processingLoad{0.0f};

    int currentSketchpadTrack{0};
    jack_client_t* jackClient{nullptr};

    // This is a list of devices that always exist (specifically, the SyncTimer input devices, and TimeCode's bi-directional device)
    QList<MidiRouterDevice*> internalDevices;
    QList<MidiRouterDevice*> devices;
    QList<MidiRouterDevice*> allEnabledInputs;
    QList<MidiRouterDevice*> allEnabledOutputs;

    MidiRouterDevice *zynthianOutputs[16];
    SketchpadTrackInfo *sketchpadTracks[ZynthboxTrackCount];
    SketchpadTrackInfo *passthroughOutputPort{nullptr};
    MidiRouterDevice *currentTrackMirror{nullptr};

    MidiListenerPort passthroughListener;
    MidiListenerPort internalPassthroughListener;
    MidiListenerPort internalControllerPassthroughListener;
    MidiListenerPort hardwareInListener;
    MidiListenerPort externalOutListener;
    MidiListenerPort* listenerPorts[5];

    void connectPorts(const QString &from, const QString &to) {
        int result = jack_connect(jackClient, from.toUtf8(), to.toUtf8());
        if (result == 0 || result == EEXIST) {
            if (DebugZLRouter) { qDebug() << "ZLRouter:" << (result == EEXIST ? "Retaining existing connection from" : "Successfully created new connection from" ) << from << "to" << to; }
        } else {
            qWarning() << "ZLRouter: Failed to connect" << from << "with" << to << "with error code" << result;
            // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
        }
    }
    void disconnectPorts(const QString &from, const QString &to) {
        // Don't attempt to connect already connected ports
        int result = jack_disconnect(jackClient, from.toUtf8(), to.toUtf8());
        if (result == 0) {
            if (DebugZLRouter) { qDebug() << "ZLRouter: Successfully disconnected" << from << "from" << to; }
        } else {
            qWarning() << "ZLRouter: Failed to disconnect" << from << "from" << to << "with error code" << result;
        }
    }

    uint32_t mostRecentEventsForZynthian{0};
    QAtomicInt jack_xrun_count{0};
    int process(jack_nframes_t nframes) {
        // auto t1 = std::chrono::high_resolution_clock::now();
        jack_nframes_t current_frames;
        jack_time_t current_usecs;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(jackClient, &current_frames, &current_usecs, &next_usecs, &period_usecs);
        const double microsecondsPerFrame = double(next_usecs - current_usecs) / double(nframes);

#if ZLROUTER_WATCHDOG
        uint32_t unclearedMessages{0};
        if (watchdog->mostRecentEventCount < mostRecentEventsForZynthian) {
            if (DebugZLRouter) { qWarning() << "ZLRouter: Apparently the last run lost events in Zynthian (received" << watchdog->mostRecentEventCount << "events, we sent out" << mostRecentEventsForZynthian << "events) - let's assume that it broke super badly and not clear our output buffers so things can catch back up"; }
            unclearedMessages = watchdog->mostRecentEventCount;
        }
#endif
        passthroughOutputPort->routerDevice->processBegin(nframes);
        currentTrackMirror->processBegin(nframes);
        for (SketchpadTrackInfo *track : qAsConst(sketchpadTracks)) {
            track->routerDevice->processBegin(nframes);
        }
        for (int channel = 0; channel < 16; ++channel) {
            zynthianOutputs[channel]->processBegin(nframes);
        }
        int eventChannel{-1};

        // Handle input coming from our SyncTimer
        quint64 subbeatLengthInMicroseconds{0};
        quint64 currentJackPlayhead{0};
        syncTimer->process(nframes, nullptr, &currentJackPlayhead, &subbeatLengthInMicroseconds);

        // A quick bit of sanity checking - usually everything's fine, but occasionally we might get events while
        // starting up, and we kind of need to settle down before then, and a good indicator something went wrong
        // is that the subbeatLengthInMicroseconds variable is zero, and so we can use that to make sure things are
        // reasonably sane before trying to do anything.
        if (subbeatLengthInMicroseconds > 0) {
            jack_midi_event_t *event{nullptr};
            MidiRouterDevice *eventDevice{nullptr};
            SketchpadTrackInfo *currentTrack{nullptr};
            bool inputDeviceIsHardware{false};
            bool inputDeviceIsSequencer{false};
            for (MidiRouterDevice *device : qAsConst(devices)) {
                device->processBegin(nframes);
            }
            while(true) {
                for (MidiRouterDevice *device : qAsConst(allEnabledInputs)) {
                    jack_midi_event_t *deviceEvent = &device->currentInputEvent;
                    // If there either is not currently an event picked for comparison, or the device's event
                    // is older than what is currently picked, that should be the next one to get processed
                    if (deviceEvent->size > 0 && (event == nullptr || deviceEvent->time < event->time)) {
                        event = deviceEvent;
                        eventDevice = device;
                        inputDeviceIsHardware = device->deviceType(MidiRouterDevice::HardwareDeviceType);
                        inputDeviceIsSequencer = device->deviceType(MidiRouterDevice::SequencerType);
                    }
                }
                // If we no longer have any incoming events to process, scamper
                if (event == nullptr) {
                    break;
                }
                const MidiRouterFilterEntry *eventDeviceFilterEntry = eventDevice->inputEventFilter()->match(*event);
                // Now process the event we picked
                const unsigned char &byte0 = event->buffer[0];
                if (byte0 == 0xf0) {
                    for (MidiRouterDevice *device : qAsConst(allEnabledOutputs)) {
                        device->writeEventToOutput(*event, eventDeviceFilterEntry);
                    }
                    passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                    // qDebug() << Q_FUNC_INFO << "SysEx message received and passed through to everywhere that wants to listen";
                } else {
                    if (0x7F < byte0 && byte0 < 0xF0) {
                        eventChannel = (byte0 & 0xf);
                    } else {
                        eventChannel = -1;
                    }
                    if (-1 < eventChannel && eventChannel < 16) {
                        const double timestamp = current_frames + event->time;
                        const double timestampUsecs = current_usecs + (microsecondsPerFrame * double(event->time));
                        int sketchpadTrack{-1};
                        if (eventDeviceFilterEntry) {
                            sketchpadTrack = eventDeviceFilterEntry->targetTrack();
                        } else {
                            sketchpadTrack = eventDevice->targetTrackForMidiChannel(eventChannel);
                        }
                        if (sketchpadTrack == -1 || sketchpadTrack == -2) {
                            sketchpadTrack = currentSketchpadTrack;
                        }
                        // Make sure we're using the correct output
                        // This is done to ensure that if we have any note-on events happen on some
                        // output, then all the following commands associated with that note should
                        // go to the same output (so any further ons, and any matching offs)
                        bool isNoteMessage{false};
                        bool isCCMessage{0xAF < byte0 && byte0 < 0xC0};
                        if (0x7F < byte0 && byte0 < 0xA0) {
                            const int &midiNote = event->buffer[1];
                            isNoteMessage = true;
                            // either any note off message, or a note on message with velocity 0 should be considered a note off by convention
                            if (byte0 >= 0x90 && event->buffer[2] > 0) { // this is a note on message
                                eventDevice->setNoteActive(sketchpadTrack, eventChannel, midiNote, true);
                                sketchpadTrack = eventDevice->noteActivationTrack(eventChannel, midiNote);
                            } else {
                                sketchpadTrack = eventDevice->noteActivationTrack(eventChannel, midiNote);
                                eventDevice->setNoteActive(sketchpadTrack, eventChannel, midiNote, false);
                            }
                        }
                        if (sketchpadTrack == -1 || sketchpadTrack == -2) {
                            sketchpadTrack = currentSketchpadTrack;
                        }
                        if (sketchpadTrack == -3) {
                            // We should not be handling sketchpad track -3, which is the "no track" destination
                        } else {
                            if (inputDeviceIsHardware) {
                                // qDebug() << Q_FUNC_INFO << "Hardware input message received for channel" << eventChannel << "of size" << event->size;
                                hardwareInListener.addMessage(false, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                            } else {
                                if (inputDeviceIsSequencer) {
                                    internalPassthroughListener.addMessage(true, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                } else {
                                    internalControllerPassthroughListener.addMessage(true, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                }
                            }
                            if (inputDeviceIsHardware == false && eventChannel == masterChannel) {
                                // qDebug() << Q_FUNC_INFO << "Event comes from internal device " << eventDevice << "and is on the master channel, send to all enabled outputs:" << allEnabledOutputs;
                                for (MidiRouterDevice *device : qAsConst(allEnabledOutputs)) {
                                    device->writeEventToOutput(*event, eventDeviceFilterEntry);
                                }
                            }
                            if (eventDevice->filterZynthianOutputByChannel()) {
                                passthroughListener.addMessage(!inputDeviceIsHardware, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                zynthianOutputs[eventChannel]->writeEventToOutput(*event, eventDeviceFilterEntry);
                                // Since we've already sent out all the master channel messages anyway, don't write them again
                                if (isCCMessage && eventChannel != masterChannel) {
                                    // qDebug() << Q_FUNC_INFO << "SyncTimer master track event is CC and is NOT on the master channel, send to all enabled outputs:" << allEnabledOutputs;
                                    for (MidiRouterDevice *device : qAsConst(allEnabledOutputs)) {
                                        // TODO We'll likely want to filter this on device type ControllerType at some point, but for now everything gets it
                                        device->writeEventToOutput(*event, eventDeviceFilterEntry);
                                    }
                                }
                                passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                            } else {
                                currentTrack = sketchpadTracks[sketchpadTrack];
                                // Before we send things out, ensure that any note message is rewritten or blocked as required for the destination track's currently selected clip
                                if (currentTrack->applyKeyScale(*event)) {
                                    currentTrack->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                                    switch (currentTrack->destination) {
                                        case MidiRouter::ZynthianDestination:
                                            passthroughListener.addMessage(!inputDeviceIsHardware, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                            for (const int &zynthianChannel : currentTrack->zynthianChannels) {
                                                if (zynthianChannel == -1) {
                                                    continue;
                                                }
                                                zynthianOutputs[zynthianChannel]->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            }
                                            currentTrackMirror->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            break;
                                        case MidiRouter::SamplerDestination:
                                            passthroughListener.addMessage(!inputDeviceIsHardware, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                            currentTrackMirror->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            break;
                                        case MidiRouter::ExternalDestination:
                                        {
                                            int externalChannel = (currentTrack->externalChannel == -1) ? currentTrack->trackIndex : currentTrack->externalChannel;
                                            passthroughListener.addMessage(!inputDeviceIsHardware, isNoteMessage, timestamp, timestampUsecs, *event, eventChannel, sketchpadTrack, eventDevice);
                                            externalOutListener.addMessage(!inputDeviceIsHardware, isNoteMessage, timestamp, timestampUsecs, *event, externalChannel, sketchpadTrack, eventDevice);
                                            if (!(inputDeviceIsHardware == false && eventChannel == masterChannel)) {
                                                // Since we've already done this above for master-channel events, don't write them again
                                                for (MidiRouterDevice *device : qAsConst(allEnabledOutputs)) {
                                                    device->writeEventToOutput(*event, eventDeviceFilterEntry, externalChannel);
                                                }
                                            }
                                            currentTrackMirror->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                                            break;
                                        }
                                        case MidiRouter::NoDestination:
                                        default:
                                            // Do nothing here
                                            break;
                                    }
                                }
                            }
                        }
                    } else if (event->size == 1 || event->size == 2) {
                        const double timestamp = current_frames + event->time;
                        const double timestampUsecs = current_usecs + (microsecondsPerFrame * double(event->time));
                        const bool isBeatClock = (byte0 == 0xf2 || byte0 == 0xf8 || byte0 == 0xfa || byte0 == 0xfb || byte0 == 0xfc);
                        const bool isTimecode = (byte0 == 0xf9);
                        currentTrackMirror->writeEventToOutput(*event, eventDeviceFilterEntry);
                        if (inputDeviceIsHardware) {
                            hardwareInListener.addMessage(false, false, timestamp, timestampUsecs, *event, eventChannel, currentSketchpadTrack, eventDevice);
                        }
                        for (MidiRouterDevice *device : qAsConst(allEnabledOutputs)) {
                            if (isBeatClock && device->sendBeatClock() == false) {
                                continue;
                            } else if (isTimecode && device->sendTimecode() == false) {
                                continue;
                            }
                            device->writeEventToOutput(*event, eventDeviceFilterEntry);
                        }
                        if (isBeatClock || isTimecode) {
                            for (MidiRouterDevice *zynthianDevice : qAsConst(zynthianOutputs)) {
                                zynthianDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                            }
                        } else {
                            currentTrack = sketchpadTracks[currentSketchpadTrack];
                            for (const int &zynthianChannel : currentTrack->zynthianChannels) {
                                if (zynthianChannel == -1) {
                                    continue;
                                }
                                zynthianOutputs[zynthianChannel]->writeEventToOutput(*event, eventDeviceFilterEntry);
                            }
                        }
                        passthroughOutputPort->routerDevice->writeEventToOutput(*event, eventDeviceFilterEntry);
                    } else {
                        qWarning() << "ZLRouter: Something's badly wrong and we've ended up with a message supposedly on channel" << eventChannel;
                    }
                }
                // Set us back up for the next run
                eventDevice->nextInputEvent();
                eventDevice = nullptr;
                event = nullptr;
            }
            for (MidiRouterDevice *device : qAsConst(devices)) {
                device->processEnd();
            }
            processingLoad = jack_cpu_load(jackClient);

#if ZLROUTER_WATCHDOG
            mostRecentEventsForZynthian = jack_midi_get_event_count(zynthianOutputBuffer);
#endif
        }
        for (int channel = 0; channel < 16; ++channel) {
            zynthianOutputs[channel]->processEnd();
        }
        for (SketchpadTrackInfo *track : qAsConst(sketchpadTracks)) {
            track->routerDevice->processEnd();
        }
        currentTrackMirror->processEnd();
        passthroughOutputPort->routerDevice->processEnd();

        // std::chrono::duration<double, std::milli> ms_double = std::chrono::high_resolution_clock::now() - t1;
        // if (ms_double.count() > 0.2) {
        //     qDebug() << Q_FUNC_INFO << ms_double.count() << "ms after" << belowThreshold << "runs under 0.2ms";
        //     belowThreshold = 0;
        // } else {
        //     ++belowThreshold;
        // }

        return 0;
    }
    int belowThreshold{0};
    int xrun() {
        ++jack_xrun_count;
        return 0;
    }

    QTimer *hardwareDeviceConnector{nullptr};
    void refreshDevices() {
        const char **ports = jack_get_ports(jackClient, nullptr, JACK_DEFAULT_MIDI_TYPE, JackPortIsPhysical);
        QList<MidiRouterDevice*> connectedDevices{internalDevices};
        QList<MidiRouterDevice*> newDevices;
        if (ports == nullptr) {
            qDebug() << Q_FUNC_INFO << "No physical ports found";
        } else {
            for (const char **p = ports; *p; p++) {
                const QString portName{QString::fromLocal8Bit(*p)};
                MidiRouterDevice *device{nullptr};
                jack_port_t *hardwarePort = jack_port_by_name(jackClient, *p);
                if (hardwarePort) {
                    QString humanReadableName, zynthianId, hardwareId;
                    int num_aliases;
                    char *aliases[2];
                    aliases[0] = (char *)malloc(size_t(jack_port_name_size()));
                    aliases[1] = (char *)malloc(size_t(jack_port_name_size()));
                    num_aliases = jack_port_get_aliases(hardwarePort, aliases);
                    static const QString ttyMidiPortName{"ttymidi:MIDI_"};
                    if (portName.startsWith(ttyMidiPortName)) {
                        humanReadableName = QString{"Midi 5-Pin"};
                        hardwareId = zynthianId = ttyMidiPortName.left(ttyMidiPortName.length() - 1);
                    } else if (num_aliases > 0) {
                        int i;
                        for (i = 0; i < num_aliases; i++) {
                            QStringList hardwareIdSplit;
                            QStringList splitAlias = QString::fromUtf8(aliases[i]).split('-');
                            if (splitAlias.length() > 5) {
                                for (int i = 0; i < 5; ++i) {
                                    if (i > 0) {
                                        hardwareIdSplit.append(splitAlias.takeFirst());
                                    } else {
                                        splitAlias.removeFirst();
                                    }
                                }
                                humanReadableName = splitAlias.join(" ");
                                zynthianId = splitAlias.join("_");
                                hardwareId = hardwareIdSplit.join("-");
                                break;
                            }
                        }
                    } else {
                        QStringList splitAlias = portName.split('-');
                        humanReadableName = splitAlias.join(" ");
                        zynthianId = splitAlias.join("_");
                        hardwareId = zynthianId;
                    }
                    free(aliases[0]);
                    free(aliases[1]);
                    int jackPortFlags = jack_port_flags(hardwarePort);
                    QString inputPortName = QString("input-%1").arg(portName);
                    QString outputPortName = QString("output-%1").arg(portName);
                    for (MidiRouterDevice *needle : qAsConst(devices)) {
                        if (needle->hardwareId() == hardwareId && needle->zynthianId() == zynthianId) {
                            device = needle;
                            break;
                        }
                    }
                    if (!device) {
                        for (MidiRouterDevice *needle : qAsConst(newDevices)) {
                            if (needle->hardwareId() == hardwareId && needle->zynthianId() == zynthianId) {
                                device = needle;
                                break;
                            }
                        }
                        if (!device) {
                            device = new MidiRouterDevice(jackClient, q);
                            newDevices << device;
                            device->setDeviceType(MidiRouterDevice::HardwareDeviceType);
                            device->setZynthianMasterChannel(masterChannel);
                            device->setZynthianId(zynthianId);
                            device->setHardwareId(hardwareId);
                            device->setHumanReadableName(humanReadableName);
                        }
                    }
                    if (jackPortFlags & JackPortIsOutput) {
                        bool currentState{device->inputEnabled()};
                        bool portIsUnknown{device->inputPortName().isEmpty()};
                        device->setInputPortName(inputPortName);
                        device->setInputEnabled(!disabledMidiInPorts.contains(device->zynthianId()));
                        connectPorts(portName, QString("ZLRouter:%1").arg(inputPortName));
                        if (portIsUnknown || currentState != device->inputEnabled()) {
                            // Only debug this out if there's a change or the device is new
                            qDebug() << Q_FUNC_INFO << "Updated" << device << device->humanReadableName() << "input port" << device->inputPortName() << "enabled state to" << device->inputEnabled();
                        }
                    } else if (jackPortFlags & JackPortIsInput) {
                        bool currentState{device->outputEnabled()};
                        bool portIsUnknown{device->outputPortName().isEmpty()};
                        device->setOutputPortName(outputPortName);
                        device->setOutputEnabled(enabledMidiOutPorts.contains(device->zynthianId()));
                        connectPorts(QString("ZLRouter:%1").arg(outputPortName), portName);
                        if (portIsUnknown || currentState != device->outputEnabled()) {
                            // Only debug this out if there's a change or the device is new
                            qDebug() << Q_FUNC_INFO << "Updated" << device << device->humanReadableName() << "output port" << device->outputPortName() << "enabled state to" << device->outputEnabled();
                        }
                    }
                    if (connectedDevices.contains(device) == false) {
                        connectedDevices << device;
                    }
                } else {
                    qWarning() << Q_FUNC_INFO << "Failed to open hardware port for identification:" << portName;
                }
            }
            free(ports);
        }
        for (MidiRouterDevice *device : qAsConst(devices)) {
            if (connectedDevices.contains(device) == false) {
                // A device has been removed, notify people about that
                Q_EMIT q->removedHardwareDevice(device->zynthianId(), device->humanReadableName());
                // And then we should get rid of it, because it'd all done and stuff (doing this through a timer delay, to make sure we don't end up deleting a device part way through a process run loop)
                QTimer::singleShot(1000, device, &QObject::deleteLater);
            }
        }
        devices = connectedDevices;
        for (MidiRouterDevice *device : qAsConst(newDevices)) {
            // A new device was discovered, notify people about that
            Q_EMIT q->addedHardwareDevice(device->zynthianId(), device->humanReadableName());
        }
        QList<MidiRouterDevice*> enabledInputs;
        QList<MidiRouterDevice*> enabledOutputs;
        for (MidiRouterDevice *device : qAsConst(devices)) {
            device->setZynthianMasterChannel(masterChannel);
            if (device->inputEnabled()) {
                enabledInputs << device;
            }
            if (device->outputEnabled()) {
                enabledOutputs << device;
            }
        }
        allEnabledInputs = enabledInputs;
        allEnabledOutputs = enabledOutputs;
        for (SketchpadTrackInfo *track : qAsConst(sketchpadTracks)) {
            disconnectFromOutputs(track);
            connectToOutputs(track);
        }
    }

    void disconnectFromOutputs(SketchpadTrackInfo *track) {
        const QString portName = QString("ZLRouter:%1").arg(track->portName);
        if (track->destination == MidiRouter::ZynthianDestination) {
            // Nothing to be done to unhook things here
        } else if (track->destination == MidiRouter::ExternalDestination) {
            for (const QString &externalPort : enabledMidiOutPorts) {
                disconnectPorts(portName, externalPort);
            }
        }
    }

    void connectToOutputs(SketchpadTrackInfo *track) {
        const QString portName = QString("ZLRouter:%1").arg(track->portName);
        if (track->destination == MidiRouter::ZynthianDestination) {
            // Nothing to be done to hook things up here
        } else if (track->destination == MidiRouter::ExternalDestination) {
            for (const QString &externalPort : enabledMidiOutPorts) {
                connectPorts(portName, externalPort);
            }
        }
    }

    QTimer *allTrackKeyScaleInfoUpdater{nullptr};
    void allTrackKeyScaleInfoUpdaterActual() {
        SequenceModel *sequence = qobject_cast<SequenceModel*>(PlayGridManager::instance()->getSequenceModel("", false));
        // a simple heuristic for ensuring we've got some patterns loaded (the event might happen too early)
        if (sequence->activePatternObject()) {
            // qDebug() << Q_FUNC_INFO << "There is an active pattern object, let's get this stuff updated, yo!";
            for (int trackIndex = 0; trackIndex < ZynthboxTrackCount; ++trackIndex) {
                SketchpadTrackInfo *trackInfo{sketchpadTracks[trackIndex]};
                PatternModel *patternModel = qobject_cast<PatternModel*>(sequence->getByClipId(trackIndex, trackInfo->currentlySelectedPatternIndex));
                if (trackInfo->currentlySelectedPattern != patternModel) {
                    if (trackInfo->currentlySelectedPattern) {
                        trackInfo->currentlySelectedPattern->disconnect(q);
                    }
                    trackInfo->currentlySelectedPattern = patternModel;
                    QObject::connect(patternModel, &PatternModel::scaleChanged, q, [this](){ updateAllTrackKeyScaleInfo(); });
                    QObject::connect(patternModel, &PatternModel::pitchChanged, q, [this](){ updateAllTrackKeyScaleInfo(); });
                    QObject::connect(patternModel, &PatternModel::octaveChanged, q, [this](){ updateAllTrackKeyScaleInfo(); });
                    QObject::connect(patternModel, &PatternModel::lockToKeyAndScaleChanged, q, [this](){ updateAllTrackKeyScaleInfo(); });
                    // Clean up, essentially just in case...
                    QObject::connect(patternModel, &QObject::destroyed, q, [trackInfo, patternModel](){
                        if (trackInfo->currentlySelectedPattern == patternModel) {
                            trackInfo->currentlySelectedPattern = nullptr;
                            trackInfo->lockStyle = PatternModel::KeyScaleLockOff;
                        }
                    });
                }
                if (trackInfo->currentlySelectedPattern) {
                    trackInfo->scale = trackInfo->currentlySelectedPattern->scaleKey();
                    trackInfo->octave = trackInfo->currentlySelectedPattern->octaveKey();
                    trackInfo->pitch = trackInfo->currentlySelectedPattern->pitchKey();
                    trackInfo->lockStyle = trackInfo->currentlySelectedPattern->lockToKeyAndScale();
                } else {
                    trackInfo->lockStyle = PatternModel::KeyScaleLockOff;
                }
            }
        } else {
            // If indeed we have been asked to run things too early, let's postpone and try again in a little bit
            // qDebug() << Q_FUNC_INFO << "Asking for a retry...";
            allTrackKeyScaleInfoUpdater->start(1000);
        }
    }
    void updateAllTrackKeyScaleInfo() {
        if (allTrackKeyScaleInfoUpdater == nullptr) {
            allTrackKeyScaleInfoUpdater = new QTimer(q);
            allTrackKeyScaleInfoUpdater->setInterval(0);
            allTrackKeyScaleInfoUpdater->setSingleShot(true);
            QObject::connect(allTrackKeyScaleInfoUpdater, &QTimer::timeout, q, [this](){ allTrackKeyScaleInfoUpdaterActual(); });
        }
        allTrackKeyScaleInfoUpdater->start();
    }
};

static int client_process(jack_nframes_t nframes, void* arg) {
    return static_cast<MidiRouterPrivate*>(arg)->process(nframes);
}
static int client_xrun(void* arg) {
    return static_cast<MidiRouterPrivate*>(arg)->xrun();
}
static void client_port_registration(jack_port_id_t /*port*/, int /*registering*/, void *arg) {
    QMetaObject::invokeMethod(static_cast<MidiRouterPrivate*>(arg)->hardwareDeviceConnector, "start");
}
static void client_registration(const char */*name*/, int /*registering*/, void *arg) {
    QMetaObject::invokeMethod(static_cast<MidiRouterPrivate*>(arg)->hardwareDeviceConnector, "start");
}

MidiRouter::MidiRouter(QObject *parent)
    : QThread(parent)
    , d(new MidiRouterPrivate(this))
{
    qRegisterMetaType<MidiRouter::ListenerPort>();
    qRegisterMetaType<MidiRouterFilterEntryRewriter::RuleType>();
    qRegisterMetaType<MidiRouterFilterEntryRewriter::EventSize>();
    qRegisterMetaType<MidiRouterFilterEntryRewriter::EventByte>();
    qRegisterMetaType<MidiRouterFilterEntryRewriter::ValueSpecifier>();
    reloadConfiguration();
    TransportManager::instance(d->syncTimer)->initialize();
    // Open the client.
    jack_status_t real_jack_status{};
    d->jackClient = jack_client_open("ZLRouter", JackNullOption, &real_jack_status);
    d->devicesModel = new MidiRouterDeviceModel(d->jackClient, this);
    JackConnectionHandler::instance()->setJackClient(d->jackClient);
    if (d->jackClient) {
        if (jack_set_process_callback(d->jackClient, client_process, static_cast<void*>(d)) == 0) {
            jack_set_xrun_callback(d->jackClient, client_xrun, static_cast<void*>(d));
            d->hardwareDeviceConnector = new QTimer(this);
            d->hardwareDeviceConnector->setSingleShot(true);
            d->hardwareDeviceConnector->setInterval(300);
            connect(d->hardwareDeviceConnector, &QTimer::timeout, this, [this](){
                d->refreshDevices();
            });
            // Sketchpad has a concept of 10 channels, and we want an output for each of those
            for (int track = 0; track < ZynthboxTrackCount; ++track) {
                SketchpadTrackInfo *trackInfo = new SketchpadTrackInfo(track);
                trackInfo->portName = QString("Channel%1").arg(QString::number(track));
                trackInfo->routerDevice = new MidiRouterDevice(d->jackClient, this);
                trackInfo->routerDevice->setOutputPortName(trackInfo->portName.toUtf8());
                trackInfo->routerDevice->setOutputEnabled(true);
                trackInfo->routerDevice->setZynthianMasterChannel(d->masterChannel);
                d->sketchpadTracks[track] = trackInfo;
            }
            // Set up the 16 channels for Zynthian-controlled synths
            for (int channel = 0; channel < 16; ++channel) {
                MidiRouterDevice *device = new MidiRouterDevice(d->jackClient, this);
                const QString channelName = QString("Zynthian-Channel%1").arg(QString::number(channel)).toUtf8();
                device->setZynthianId(channelName);
                device->setHumanReadableName(channelName);
                device->setOutputPortName(channelName);
                device->setOutputEnabled(true);
                device->setZynthianMasterChannel(d->masterChannel);
                d->zynthianOutputs[channel] = device;
            }
            // Set up the passthrough output port
            d->passthroughOutputPort = new SketchpadTrackInfo(0);
            d->passthroughOutputPort->portName = QString("PassthroughOut");
            d->passthroughOutputPort->routerDevice = new MidiRouterDevice(d->jackClient, this);
            d->passthroughOutputPort->routerDevice->setOutputPortName(d->passthroughOutputPort->portName.toUtf8());
            d->passthroughOutputPort->routerDevice->setOutputEnabled(true);
            // Set up the current-track mirroring port
            d->currentTrackMirror = new MidiRouterDevice(d->jackClient, this);
            d->currentTrackMirror->setOutputPortName(QString{"CurrentTrackMirror"});
            d->currentTrackMirror->setOutputEnabled(true);

            jack_set_port_registration_callback(d->jackClient, client_port_registration, static_cast<void*>(d));
            jack_set_client_registration_callback(d->jackClient, client_registration, static_cast<void*>(d));
            // Activate the client.
            if (jack_activate(d->jackClient) == 0) {
                qInfo() << "ZLRouter: Successfully created and set up the ZLRouter's Jack client";
                zl_set_jack_client_affinity(d->jackClient);
                // Set up the timecode generator thing as a router device
                MidiRouterDevice *timecodeDevice = new MidiRouterDevice(d->jackClient, this);
                timecodeDevice->setDeviceType(MidiRouterDevice::TimeCodeGeneratorType);
                timecodeDevice->setZynthianId("TransportManager");
                timecodeDevice->setHumanReadableName("Zynthbox TransportManager");
                    // This does not want to actually receive any timecode signals, otherwise it gets weird
                timecodeDevice->setSendTimecode(false);
                timecodeDevice->setSendBeatClock(false);
                timecodeDevice->setInputPortName("TransportManager-in");
                timecodeDevice->setInputEnabled(true);
                timecodeDevice->setOutputPortName("TransportManager-out");
                timecodeDevice->setOutputEnabled(true);
                timecodeDevice->setZynthianMasterChannel(d->masterChannel);
                d->internalDevices << timecodeDevice;
                d->connectPorts("TransportManager:midi_out", "ZLRouter:TransportManager-in");
                d->connectPorts("ZLRouter:TransportManager-out", "TransportManager:midi_in");
                // Set up SyncTimer as a router device
                for (int track = 0; track < ZynthboxTrackCount; ++track) {
                    // First the sequencer device
                    MidiRouterDevice *syncTimerDevice = new MidiRouterDevice(d->jackClient, this);
                    syncTimerDevice->setZynthianId(QLatin1String("SyncTimer-Track%1-Sequencer").arg(QString::number(track)));
                    syncTimerDevice->setHumanReadableName(QLatin1String("SyncTimer Track%1-Sequencer").arg(QString::number(track)));
                    syncTimerDevice->setDeviceType(MidiRouterDevice::ControllerType);
                    syncTimerDevice->setDeviceType(MidiRouterDevice::SequencerType);
                    syncTimerDevice->setInputPortName(QLatin1String("SyncTimer-Track%1-Sequencer").arg(QString::number(track)).toUtf8());
                    syncTimerDevice->setInputEnabled(true);
                    syncTimerDevice->setMidiChannelTargetTrack(-1, track);
                    syncTimerDevice->setZynthianMasterChannel(d->masterChannel);
                    d->internalDevices << syncTimerDevice;
                    d->connectPorts(QLatin1String("SyncTimer:Track%1-Sequencer").arg(QString::number(track)).toUtf8(), QLatin1String("ZLRouter:SyncTimer-Track%1-Sequencer").arg(QString::number(track)).toUtf8());
                    // Then the controller device
                    syncTimerDevice = new MidiRouterDevice(d->jackClient, this);
                    syncTimerDevice->setZynthianId(QLatin1String("SyncTimer-Track%1-Controller").arg(QString::number(track)));
                    syncTimerDevice->setHumanReadableName(QLatin1String("SyncTimer Track%1-Controller").arg(QString::number(track)));
                    syncTimerDevice->setDeviceType(MidiRouterDevice::ControllerType);
                    syncTimerDevice->setInputPortName(QLatin1String("SyncTimer-Track%1-Controller").arg(QString::number(track)).toUtf8());
                    syncTimerDevice->setInputEnabled(true);
                    syncTimerDevice->setMidiChannelTargetTrack(-1, track);
                    syncTimerDevice->setZynthianMasterChannel(d->masterChannel);
                    d->internalDevices << syncTimerDevice;
                    d->connectPorts(QLatin1String("SyncTimer:Track%1-Controller").arg(QString::number(track)).toUtf8(), QLatin1String("ZLRouter:SyncTimer-Track%1-Controller").arg(QString::number(track)).toUtf8());
                }
                // SyncTimer also has a master track for controlling things that are not directly tied to
                // a track (such as, for example, cc values for specific zynthian-controlled
                // synths, or things which are actually global, like the bpm)
                MidiRouterDevice *masterDevice = new MidiRouterDevice(d->jackClient, this);
                masterDevice->setZynthianId("SyncTimer-MasterTrack-Sequencer");
                masterDevice->setHumanReadableName("SyncTimer Master Track Sequencer");
                masterDevice->setDeviceType(MidiRouterDevice::MasterTrackType);
                masterDevice->setDeviceType(MidiRouterDevice::SequencerType);
                masterDevice->setInputPortName("SyncTimer-MasterTrack-Sequencer");
                masterDevice->setInputEnabled(true);
                masterDevice->setZynthianMasterChannel(d->masterChannel);
                masterDevice->setFilterZynthianOutputByChannel(true);
                d->internalDevices << masterDevice;
                d->connectPorts("SyncTimer:MasterTrack-Sequencer", "ZLRouter:SyncTimer-MasterTrack-Sequencer");
                masterDevice = new MidiRouterDevice(d->jackClient, this);
                masterDevice->setZynthianId("SyncTimer-MasterTrack-Controller");
                masterDevice->setHumanReadableName("SyncTimer Master Track Controller");
                masterDevice->setDeviceType(MidiRouterDevice::MasterTrackType);
                masterDevice->setInputPortName("SyncTimer-MasterTrack-Controller");
                masterDevice->setInputEnabled(true);
                masterDevice->setZynthianMasterChannel(d->masterChannel);
                masterDevice->setFilterZynthianOutputByChannel(true);
                d->internalDevices << masterDevice;
                d->connectPorts("SyncTimer:MasterTrack-Controller", "ZLRouter:SyncTimer-MasterTrack-Controller");
                // Set the devices to just be the internal devices, and then connect any hardware
                d->devices = d->internalDevices;
                // Now hook up the hardware inputs
                d->hardwareDeviceConnector->start();
            } else {
                qWarning() << "ZLRouter: Failed to activate ZLRouter Jack client";
            }
        } else {
            qWarning() << "ZLRouter: Failed to set the ZLRouter Jack processing callback";
        }
    } else {
        qWarning() << "ZLRouter: Could not create the ZLRouter Jack client.";
    }
    d->constructing = false;
    start();
}

MidiRouter::~MidiRouter()
{
    delete d;
}

void MidiRouter::run() {
    int cuiaOriginId{0};
    ZynthboxBasics::Track cuiaTrack{ZynthboxBasics::CurrentTrack};
    ZynthboxBasics::Slot cuiaSlot{ZynthboxBasics::CurrentSlot};
    static const QString emptyString{};
    int cuiaValue{0};
    while (true) {
        if (d->done) {
            break;
        }
        for (int i = 0; i < 5; ++i) {
            MidiListenerPort *listenerPort = d->listenerPorts[i];
            MidiListenerPort::NoteMessage *message = listenerPort->readHead;
            while (!message->submitted) {
                // FIXME Somehow eventDevice might end up as a nullptr - that seems extremely weird and will need some proper investigating...
                if (message->isNoteMessage && message->eventDevice) {
                    const bool setOn = (message->byte0 >= 0x90 && message->byte2 > 0);
                    const int midiChannel = (message->byte0 & 0xf);
                    const int &midiNote = message->byte1;
                    const int &velocity = message->byte2;
                    Q_EMIT noteChanged(listenerPort->identifier, midiNote, midiChannel, velocity, setOn, message->timeStamp, message->byte0, message->byte1, message->byte2, message->sketchpadTrack, message->eventDevice->hardwareId());
                }
                Q_EMIT midiMessage(listenerPort->identifier, message->size, message->byte0, message->byte1, message->byte2, message->sketchpadTrack, message->fromInternal, message->eventDevice ? message->eventDevice->hardwareId() : emptyString);
                message->submitted = true;
                listenerPort->readHead = listenerPort->readHead->next;
                message = listenerPort->readHead;
            }
        }
        for (MidiRouterDevice* device : d->devices) {
            while (device->cuiaRing.readHead->processed == false) {
                CUIAHelper::Event event = device->cuiaRing.read(&cuiaOriginId, &cuiaTrack, &cuiaSlot, &cuiaValue);
                Q_EMIT cuiaEvent(CUIAHelper::instance()->cuiaCommand(event), cuiaOriginId, cuiaTrack, cuiaSlot, cuiaValue);
            }
        }
        Q_EMIT processingLoadChanged();
        msleep(5);
    }
}

void MidiRouter::cuiaEventFeedback(const QString& cuiaCommand, const int& originId, const ZynthboxBasics::Track& track, const ZynthboxBasics::Slot& slot, const int& value)
{
    const CUIAHelper::Event cuiaEvent{CUIAHelper::instance()->cuiaEvent(cuiaCommand)};
    if (cuiaEvent != CUIAHelper::NoCuiaEvent) {
        if (cuiaEvent == CUIAHelper::SetClipCurrentEvent) {
            int trackIndex{track};
            if (trackIndex < 0) {
                trackIndex = d->currentSketchpadTrack;
            }
            const int slotIndex{slot};
            // This really shouldn't happen, but in case it does...
            if (slotIndex > -1) {
                SketchpadTrackInfo *trackInfo{d->sketchpadTracks[trackIndex]};
                trackInfo->currentlySelectedPatternIndex = slotIndex;
                d->updateAllTrackKeyScaleInfo();
            }
        }
        for (MidiRouterDevice * device : qAsConst(d->allEnabledOutputs)) {
            device->cuiaEventFeedback(cuiaEvent, originId, track, slot, value);
        }
    }
}

float MidiRouter::processingLoad() const
{
    // Jack returns a floating-point percent value, but we want it as a 0-1 floating point value
    return d->processingLoad / 100;
}

QObject * MidiRouter::model() const
{
    return d->devicesModel;
}

void MidiRouter::markAsDone() {
    d->done = true;
}

void MidiRouter::setSkechpadTrackDestination(int sketchpadTrack, MidiRouter::RoutingDestination destination, int externalChannel)
{
    if (sketchpadTrack > -1 && sketchpadTrack < ZynthboxTrackCount) {
        SketchpadTrackInfo *trackInfo = d->sketchpadTracks[sketchpadTrack];
        trackInfo->externalChannel = externalChannel;
        if (trackInfo->destination != destination) {
            d->disconnectFromOutputs(trackInfo);
            trackInfo->destination = destination;
            d->connectToOutputs(trackInfo);
        }
    }
}

void MidiRouter::setCurrentSketchpadTrack(int sketchpadTrack)
{
    if (d->currentSketchpadTrack != sketchpadTrack) {
        d->currentSketchpadTrack = qBound(0, sketchpadTrack, ZynthboxTrackCount - 1);
        Q_EMIT currentSketchpadTrackChanged();
    }
}

int MidiRouter::currentSketchpadTrack() const
{
    return d->currentSketchpadTrack;
}

void MidiRouter::setZynthianChannels(int sketchpadTrack, const QList<int> &zynthianChannels)
{
    if (sketchpadTrack > -1 && sketchpadTrack < ZynthboxTrackCount) {
        SketchpadTrackInfo *trackInfo = d->sketchpadTracks[sketchpadTrack];
        bool hasChanged{false};
        for (int i = 0; i < 16; ++i) {
            int original = trackInfo->zynthianChannels[i];
            trackInfo->zynthianChannels[i] = zynthianChannels.value(i, -1);
            if (original != trackInfo->zynthianChannels[i]) {
                hasChanged = true;
            }
        }
        if (hasChanged) {
            if (DebugZLRouter) { qDebug() << "ZLRouter: Updating zynthian channels for" << trackInfo->portName << "from" << trackInfo->zynthianChannels << "to" << zynthianChannels; }
        }
    }
}

void MidiRouter::setZynthianSynthAcceptedChannels(int zynthianChannel, const QList<int> &acceptedMidiChannels)
{
    if (-1 < zynthianChannel && zynthianChannel < 16) {
        d->zynthianOutputs[zynthianChannel]->setAcceptedMidiChannels(acceptedMidiChannels);
    }
}

void MidiRouter::setZynthianSynthKeyzones(int zynthianChannel, int keyZoneStart, int keyZoneEnd, int rootNote)
{
    if (-1 < zynthianChannel && zynthianChannel < 16) {
        QList<int> notes;
        for (int note = keyZoneStart; note < keyZoneEnd + 1; ++note) {
            // This might look a bit weird - but, we use -1;-1 as the range for "just don't do anything for this slot"
            if (-1 < note && note < 128) {
                notes << note;
            }
        }
        d->zynthianOutputs[zynthianChannel]->setAcceptedNotes(notes);
        d->zynthianOutputs[zynthianChannel]->setTransposeAmount(rootNote - 60);
    }
}

void MidiRouter::setExpressiveSplitPoint(const int& splitPoint)
{
    if (d->expressiveSplitPoint != std::clamp(splitPoint, -1, 15)) {
        d->expressiveSplitPoint = std::clamp(splitPoint,  -1, 15);
        Q_EMIT expressiveSplitPointChanged();
    }
}

const int & MidiRouter::expressiveSplitPoint() const
{
    return d->expressiveSplitPoint;
}

const QList<int> &MidiRouter::masterChannels() const
{
    return d->masterChannels;
}

const int & MidiRouter::masterChannel() const
{
    return d->masterChannel;
}

void MidiRouter::reloadConfiguration()
{
    // TODO Make the fb stuff work as well... (also, note to self, work out what that stuff actually is?)
    // If 0, zynthian expects no midi to be routed externally, and if 1 it expects everything to go out
    // So, in our parlance, that means that 1 means route events external for anything on a Zynthian channel, and for non-Zynthian channels, use our own rules
    QString envVar = qgetenv("ZYNTHIAN_MIDI_FILTER_OUTPUT");
    if (envVar.isEmpty()) {
        if (DebugZLRouter) { qDebug() << "No env var data for output filtering, setting default"; }
        envVar = "0";
    }
    d->filterMidiOut = envVar.toInt();
    envVar = qgetenv("ZYNTHIAN_MIDI_PORTS");
    if (envVar.isEmpty()) {
        if (DebugZLRouter) { qDebug() << "No env var data for midi ports, setting default"; }
        envVar = "DISABLED_IN=\\nENABLED_OUT=ttymidi:MIDI_out\\nENABLED_FB=";
    }
    const QStringList midiPorts = envVar.split("\\n");
    for (const QString &portOptions : midiPorts) {
        const QStringList splitOptions{portOptions.split("=")};
        if (splitOptions.length() == 2) {
            if (splitOptions[0] == "DISABLED_IN") {
                d->disabledMidiInPorts = splitOptions[1].split(",");
            } else if (splitOptions[0] == "ENABLED_OUT") {
                d->enabledMidiOutPorts = splitOptions[1].split(",");
            } else if (splitOptions[0] == "ENABLED_FB") {
                d->enabledMidiFbPorts = splitOptions[1].split(",");
            }
        } else {
            qWarning() << "ZLRouter: Malformed option in the midi ports variable - we expected a single = in the following string, and encountered two:" << portOptions;
        }
    }
    envVar = qgetenv("ZYNTHIAN_MIDI_MASTER_CHANNEL");
    if (envVar.isEmpty()) {
        if (DebugZLRouter) { qDebug() << "No env var data for midi master channel, setting default"; }
        envVar = "16";
    }
    d->masterChannel = std::clamp(envVar.toInt() - 1, 0, 15);
    for (int channel = 0; channel < 16; ++channel) {
        if (d->expressiveSplitPoint == -1) {
            // Set to all-Upper, we interpret this as our "standard" layout, and assign the master channel according to what's set in webconf
            d->masterChannels[channel] = d->masterChannel;
        } else {
            if (channel > d->expressiveSplitPoint) {
                // Upper zone
                d->masterChannels[channel] = 15;
            } else {
                // Lower zone
                d->masterChannels[channel] = 0;
            }
        }
    }
    Q_EMIT masterChannelsChanged();

    // TODO Implement layer keyzone splitting for the zynthian outputs

    if (DebugZLRouter) {
        qDebug() << "ZLRouter: Loaded settings, which are now:";
        qDebug() << "Filter midi out?" << d->filterMidiOut;
        qDebug() << "Disabled midi input devices:" << d->disabledMidiInPorts;
        qDebug() << "Enabled midi output devices:" << d->enabledMidiOutPorts;
        qDebug() << "Enabled midi fb devices:" << d->enabledMidiFbPorts;
        qDebug() << "Midi Master Channel:" << d->masterChannel;
    }
    if (!d->constructing) {
        // Reconnect out outputs after reloading
        for (SketchpadTrackInfo *track : qAsConst(d->sketchpadTracks)) {
            track->routerDevice->setZynthianMasterChannel(d->masterChannel);
        }
        d->passthroughOutputPort->routerDevice->setZynthianMasterChannel(d->masterChannel);
        for (int channel = 0; channel < 16; ++channel) {
            d->zynthianOutputs[channel]->setZynthianMasterChannel(d->masterChannel);
        }
        d->refreshDevices();
    }
}
