#include "MidiRouterDevice.h"
#include "ZynthboxBasics.h"
#include "DeviceMessageTranslations.h"
#include "SyncTimer.h"

#include <QString>
#include <QList>
#include <QDebug>

#include <jack/jack.h>
#include <jack/midiport.h>

#define DebugRouterDevice false

class MidiRouterDevicePrivate {
public:
    MidiRouterDevicePrivate () {
        DeviceMessageTranslations::load();
        for (int channel = 0; channel < 16; ++channel) {
            receiveFromChannel[channel] = true;
            sendToChannel[channel] = true;
            masterChannel[channel] = -1;
            for (int note = 0; note < 128; ++note) {
                noteState[channel][note] = 0;
                noteActivationTrack[channel][note] = -1;
                acceptsNote[note] = true;
            }
        }
    }
    // Use this on any outgoing events, to ensure the event matches the device's master channel setup
    // Remember to call the function below after processing the event
    inline const void zynthboxToDevice(jack_midi_event_t *event) const;
    // Use this on any incoming events, to ensure the event matches zynthbox' internal master channel
    // Also use this after calling the above and processing outgoing events
    inline const void deviceToZynthbox(jack_midi_event_t *event) const;

    int acceptsNote[128];
    int noteState[16][128];
    int noteActivationTrack[16][128];
    int midiChannelTargetTrack[16];
    jack_client_t *jackClient{nullptr};
    QString hardwareId;
    QString zynthianId;
    QString humanReadableName;
    jack_midi_event_t *device_translations_cc{nullptr};
    // The string name which identifies this input device in jack
    QString inputPortName;
    // The string name which identifies this output device in jack
    QString outputPortName;
    // The master channel for the given channels' data (used for MPE upper/lower splits)
    int masterChannel[16];
    bool inputEnabled{false};
    bool outputEnabled{false};
    MidiRouterDevice::DeviceDirections direction{{}};
    MidiRouterDevice::DeviceTypes type{{}};
    bool receiveFromChannel[16];
    bool sendToChannel[16];
    bool sendTimecode{true};
    bool sendBeatClock{true};
    // Zynthbox' master channel
    int globalMaster{-1};

    jack_port_t *inputPort{nullptr};
    void *inputBuffer{nullptr};
    uint32_t inputEventCount{0};
    uint32_t nextInputEventIndex{0};
    jack_port_t *outputPort{nullptr};
    void *outputBuffer{nullptr};
    jack_nframes_t mostRecentOutputTime{0};
};

MidiRouterDevice::MidiRouterDevice(jack_client_t *jackClient, QObject *parent)
    : QObject(parent)
    , d(new MidiRouterDevicePrivate)
{
    DeviceMessageTranslations::load();
    d->jackClient = jackClient;
    setMidiChannelTargetTrack(-1, -1);
}

MidiRouterDevice::~MidiRouterDevice()
{
    // Pull down the ports when the device is removed
    setInputPortName(QString{});
    setOutputPortName(QString{});
    // Submit all the missing off events (which won't arrive any longer now the thing has been disconnected) into the schedule for their associated tracks
    for (int channel = 0; channel < 16; ++channel) {
        for (int note = 0; note < 128; ++note) {
            int currentActivations = d->noteState[channel][note];
            int sketchpadTrack = d->noteActivationTrack[channel][note];
            for (int activations = 0; activations < currentActivations; ++activations) {
                SyncTimer::instance()->sendNoteImmediately(note, channel, false, 0, sketchpadTrack);
            }
        }
    }
    delete d;
    DeviceMessageTranslations::unload();
}

void MidiRouterDevice::processBegin(const jack_nframes_t &nframes)
{
    // Set up the output buffer
    if (d->outputPort) {
        d->outputBuffer = jack_port_get_buffer(d->outputPort, nframes);
        jack_midi_clear_buffer(d->outputBuffer);
    } else {
        d->outputBuffer = nullptr;
    }
    d->mostRecentOutputTime = 0;
    // Set up the input buffer and fetch the first event (if there are any)
    d->nextInputEventIndex = 0;
    currentInputEvent.size = 0;
    if (d->inputPort) {
        d->inputBuffer = jack_port_get_buffer(d->inputPort, nframes);
        d->inputEventCount = jack_midi_get_event_count(d->inputBuffer);
        nextInputEvent();
    } else {
        d->inputBuffer = nullptr;
        d->inputEventCount = 0;
    }
}

void MidiRouterDevice::writeEventToOutput(jack_midi_event_t& event, const int &outputChannel)
{
    const bool isNoteMessage = event.buffer[0] > 0x7F && event.buffer[0] < 0xA0;
    if (isNoteMessage == false || d->acceptsNote[event.buffer[1]]) {
        d->zynthboxToDevice(&event);
        const int eventChannel = (event.buffer[0] & 0xf);
        if (outputChannel > -1) {
            event.buffer[0] = event.buffer[0] - eventChannel + outputChannel;
        }
        int errorCode = jack_midi_event_write(d->outputBuffer, event.time, event.buffer, event.size);
        if (errorCode == -EINVAL) {
            // If the error invalid happens, we should likely assume the event was out of order for whatever reason, and just schedule it at the same time as the most recently scheduled event
            #if DebugRouterDevice
                qWarning() << Q_FUNC_INFO << "Attempted to write out-of-order event for time" << event.time << "so writing to most recent instead:" << d->mostRecentOutputTime;
            #endif
            errorCode = jack_midi_event_write(d->outputBuffer, d->mostRecentOutputTime, event.buffer, event.size);
        }
        if (errorCode != 0) {
            if (errorCode == -ENOBUFS) {
                qWarning() << Q_FUNC_INFO << "Ran out of space while writing events!";
            } else {
                qWarning() << Q_FUNC_INFO << "Error writing midi event:" << -errorCode << strerror(-errorCode) << "for event at time" << event.time << "of size" << event.size;
            }
        #if DebugRouterDevice
        } else {
            if (DebugRouterDevice) { qDebug() << Q_FUNC_INFO << "Wrote event to buffer at time" << QString::number(event.time).rightJustified(4, ' ') << "on channel" << currentChannel << "for port" << (output ? output->portName : "no-port-details") << "with data" << event.buffer[0] << event.buffer[1]; }
        #endif
        }
        if (d->mostRecentOutputTime < event.time) {
            d->mostRecentOutputTime = event.time;
        }
        if (outputChannel > -1) {
            event.buffer[0] = event.buffer[0] + eventChannel - outputChannel;
        }
        d->deviceToZynthbox(&event);
    }
}

void MidiRouterDevice::nextInputEvent()
{
    if (d->inputBuffer != nullptr && d->nextInputEventIndex < d->inputEventCount) {
        if (int error = jack_midi_event_get(&currentInputEvent, d->inputBuffer, d->nextInputEventIndex)) {
            currentInputEvent.size = 0;
            qWarning() << Q_FUNC_INFO << "jack_midi_event_get, received event lost! We were supposed to have" << d->inputEventCount << "events, attempted to fetch at index" << d->nextInputEventIndex << "and the error code is" << error << strerror(-error);
        } else {
            // Let's make sure the event is going to be at least reasonably valid
            d->deviceToZynthbox(&currentInputEvent);
            if (currentInputEvent.buffer[0] > 0xAF && currentInputEvent.buffer[0] < 0xC0) {
                // Then it's a CC message, and maybe we want to do a thing?
                const jack_midi_event_t &otherEvent = d->device_translations_cc[currentInputEvent.buffer[1]];
                if (otherEvent.size > 0) {
                    currentInputEvent.size = otherEvent.size;
                    currentInputEvent.buffer = otherEvent.buffer;
                    // leave the time code intact
                }
            }
            // if (d->type.testFlag(MidiRouterDevice::HardwareDeviceType)) {
            //     qDebug() << Q_FUNC_INFO << "Retrieved jack midi event on device" << d->humanReadableName << "with data size" << d->currentInputEvent->size << "at time" << d->currentInputEvent->time << "event" << d->nextInputEventIndex + 1 << "of" << d->inputEventCount;
            // }
        }
    } else {
        currentInputEvent.size = 0;
    }
    d->nextInputEventIndex++;
}

void MidiRouterDevice::processEnd()
{
    d->outputBuffer = nullptr;
    d->inputBuffer = nullptr;
    d->nextInputEventIndex = 0;
    d->inputEventCount = 0;
    currentInputEvent.size = 0;
}

void MidiRouterDevice::resetNoteActivation()
{
    for (int channel = 0; channel < 16; ++channel) {
        for (int note = 0; note < 128; ++note) {
            d->noteState[channel][note] = 0;
        }
    }
}

void MidiRouterDevice::setNoteActive(const int &sketchpadTrack, const int& channel, const int& note, const bool& active)
{
    if (-1 < channel && channel < 16 && -1 < note && note < 128) {
        if (active) {
            d->noteState[channel][note] += 1;
            if (d->noteState[channel][note] == 1) {
                d->noteActivationTrack[channel][note] = sketchpadTrack;
            }
        } else {
            d->noteState[channel][note] -= 1;
            if (d->noteState[channel][note] == 0) {
                d->noteActivationTrack[channel][note] = -1;
            }
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Attempted to set note activation state for note" << note << "on channel" << channel << "to" << active;
    }
}

const int & MidiRouterDevice::noteActivationState(const int& channel, const int& note) const
{
    return d->noteState[std::clamp(channel, 0, 15)][std::clamp(note, 0, 127)];
}

const int & MidiRouterDevice::noteActivationTrack(const int& channel, const int& note) const
{
    return d->noteActivationTrack[channel][note];
}

void MidiRouterDevice::setHardwareId(const QString& hardwareId)
{
    d->hardwareId = hardwareId;
    setObjectName(QString("%1/%2").arg(d->hardwareId).arg(d->zynthianId));
}

const QString & MidiRouterDevice::hardwareId() const
{
    return d->hardwareId;
}

void MidiRouterDevice::setZynthianId(const QString& zynthianId)
{
    d->zynthianId = zynthianId;
    setObjectName(QString("%1/%2").arg(d->hardwareId).arg(d->zynthianId));
}

const QString & MidiRouterDevice::zynthianId() const
{
    return d->zynthianId;
}

void MidiRouterDevice::setHumanReadableName(const QString& humanReadableName)
{
    if (d->humanReadableName != humanReadableName) {
        d->humanReadableName = humanReadableName;
        DeviceMessageTranslations::apply(d->humanReadableName, &d->device_translations_cc);
        const int masterChannel = DeviceMessageTranslations::deviceMasterChannel(humanReadableName);
        for (int channel = 0; channel < 16; ++channel) {
            d->masterChannel[channel] = masterChannel;
        }
    }
}

const QString & MidiRouterDevice::humanReadableName() const
{
    return d->humanReadableName;
}

void MidiRouterDevice::setInputPortName(const QString& portName)
{
    if (d->inputPortName != portName) {
        d->inputPortName = portName;
        d->direction.setFlag(MidiRouterDevice::InDevice, true);
        if (d->inputPort) {
            if (int error = jack_port_unregister(d->jackClient, d->inputPort)) {
                qDebug() << Q_FUNC_INFO << "Failed to unregister input port even though there's one registered. We'll ignore that and keep going, but this seems not quite right. Reported error was:" << error << strerror(-error);
            }
            d->inputPort = nullptr;
        }
        if (portName.isEmpty() == false) {
            d->inputPort = jack_port_register(d->jackClient, d->inputPortName.toUtf8(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
        }
        if (d->inputPort == nullptr) {
            d->inputEnabled = false;
        }
    }
}

const QString & MidiRouterDevice::inputPortName() const
{
    return d->inputPortName;
}

void MidiRouterDevice::setInputEnabled(const bool& enabled)
{
    d->inputEnabled = enabled;
}

const bool & MidiRouterDevice::inputEnabled() const
{
    return d->inputEnabled;
}

void MidiRouterDevice::setOutputPortName(const QString& portName)
{
    if (d->outputPortName != portName) {
        d->outputPortName = portName;
        d->direction.setFlag(MidiRouterDevice::OutDevice, true);
        if (d->outputPort) {
            if (int error = jack_port_unregister(d->jackClient, d->outputPort)) {
                qDebug() << Q_FUNC_INFO << "Failed to unregister output port even though there's one registered. We'll ignore that and keep going, but this seems not quite right. Reported error was:" << error << strerror(-error);
            }
            d->outputPort = nullptr;
        }
        if (portName.isEmpty() == false) {
            d->outputPort = jack_port_register(d->jackClient, d->outputPortName.toUtf8(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
        }
        if (d->outputPort == nullptr) {
            d->outputEnabled = false;
        }
    }
}

const QString & MidiRouterDevice::outputPortName() const
{
    return d->outputPortName;
}

void MidiRouterDevice::setOutputEnabled(const bool& enabled)
{
    d->outputEnabled = enabled;
}

const bool & MidiRouterDevice::outputEnabled() const
{
    return d->outputEnabled;
}

void MidiRouterDevice::setAcceptedNotes(const QList<int> notes, bool accepted, bool setOthersOpposite)
{
    if (setOthersOpposite) {
        for (int note = 0; note < 128; ++note) {
            d->acceptsNote[note] = !accepted;
        }
    }
    for (const int &note : qAsConst(notes)) {
        d->acceptsNote[std::clamp(note, 0, 127)] = accepted;
    }
}

void MidiRouterDevice::setAcceptsNote(const int& note, bool accepted)
{
    d->acceptsNote[std::clamp(note, 0, 127)] = accepted;
}

void MidiRouterDevice::setDeviceDirection(const DeviceDirection& direction, const bool& supportsDirection)
{
    d->direction.setFlag(direction, supportsDirection);
}

bool MidiRouterDevice::supportsDirection(const DeviceDirection& direction) const
{
    return d->direction.testFlag(direction);
}

void MidiRouterDevice::setDeviceType(const DeviceType& type, const bool& isType)
{
    d->type.setFlag(type, isType);
}

bool MidiRouterDevice::deviceType(const DeviceType& type) const
{
    return d->type.testFlag(type);
}

void MidiRouterDevice::setZynthianMasterChannel(int globalMaster)
{
    d->globalMaster = globalMaster;
}

void MidiRouterDevice::setReceiveChannels(const QList<int>& channels, const bool& receive)
{
    for (const int &channel : qAsConst(channels)) {
        if (channel > -1 && channel < 16) {
            d->receiveFromChannel[channel] = receive;
        }
    }
}

const bool & MidiRouterDevice::receiveChannel(const int& channel) const
{
    return d->receiveFromChannel[channel];
}

void MidiRouterDevice::setSendToChannels(const QList<int>& channels, const bool& sendTo)
{
    for (const int &channel : qAsConst(channels)) {
        if (channel > -1 && channel < 16) {
            d->sendToChannel[channel] = sendTo;
        }
    }
}

const bool & MidiRouterDevice::sendToChannel(const int& channel) const
{
    return d->sendToChannel[channel];
}

void MidiRouterDevice::setSendTimecode(const bool& sendTimecode)
{
    d->sendTimecode = sendTimecode;
}

const bool & MidiRouterDevice::sendTimecode() const
{
    return d->sendTimecode;
}

void MidiRouterDevice::setSendBeatClock(const bool& sendBeatClock)
{
    d->sendBeatClock = sendBeatClock;
}

const bool & MidiRouterDevice::sendBeatClock() const
{
    return d->sendBeatClock;
}

const void MidiRouterDevicePrivate::zynthboxToDevice(jack_midi_event_t* event) const
{
    // Doesn't make sense to change things for events which aren't channel events
    const jack_midi_data_t &byte0 = event->buffer[0];
    if (0x7F < byte0 && byte0 < 0xF0) {
        const jack_midi_data_t eventChannel = (byte0 & 0xf);
        const int &masterChannel = this->masterChannel[eventChannel];
        // Only apply if there's a given master channel, and it's not the same as the global one
        if (masterChannel > -1 && masterChannel != globalMaster) {
            if ((
                (eventChannel > globalMaster && eventChannel > masterChannel) ||
                (eventChannel < globalMaster && eventChannel < masterChannel)
            ) == false) {
                // Only move the event if it isn't already outside the range of the two master channels
                if (eventChannel > globalMaster) {
                    // Then it's between device master and global, so we move it down one channel
                    event->buffer[0] = byte0 - 1;
                    // qDebug() << Q_FUNC_INFO << "Moving event down from" << eventChannel << "to" << eventChannel - 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel < globalMaster) {
                    // Then it's between global and device master, so we move it up one channel
                    event->buffer[0] = byte0 + 1;
                    // qDebug() << Q_FUNC_INFO << "Moving event up from" << eventChannel << "to" << eventChannel + 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel == globalMaster) {
                    // Then it's on the device master, and should be on the global master channel
                    event->buffer[0] = byte0 - globalMaster + masterChannel;
                    // qDebug() << Q_FUNC_INFO << "Moving event from" << globalMaster << "to device master" << masterChannel << "for byte0" << event->buffer[0];
                }
            }
        }
    }
}

const void MidiRouterDevicePrivate::deviceToZynthbox(jack_midi_event_t* event) const
{
    // Doesn't make sense to change things for events which aren't channel events
    const jack_midi_data_t &byte0 = event->buffer[0];
    if (0x7F < byte0 && byte0 < 0xF0) {
        const jack_midi_data_t eventChannel = (byte0 & 0xf);
        const int &masterChannel = this->masterChannel[eventChannel];
        if (masterChannel > -1 && masterChannel != globalMaster) {
            if ((
                (eventChannel > masterChannel && eventChannel > globalMaster) ||
                (eventChannel < masterChannel && eventChannel < globalMaster)
            ) == false) {
                // Only move the event if it isn't already outside the range of the two master channels
                if (eventChannel > masterChannel) {
                    // Then it's between device master and global, so we move it down one channel
                    event->buffer[0] = byte0 - 1;
                    // qDebug() << Q_FUNC_INFO << "Moving event down from" << eventChannel << "to" << eventChannel - 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel < masterChannel) {
                    // Then it's between global and device master, so we move it up one channel
                    event->buffer[0] = byte0 + 1;
                    // qDebug() << Q_FUNC_INFO << "Moving event up from" << eventChannel << "to" << eventChannel + 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel == masterChannel) {
                    // Then it's on the device master, and should be on the global master channel
                    event->buffer[0] = byte0 - masterChannel + globalMaster;
                    // qDebug() << Q_FUNC_INFO << "Moving event from" << eventChannel << "to global master" << globalMaster << "for byte0" << event->buffer[0];
                }
            }
        }
    }
}

void MidiRouterDevice::setMidiChannelTargetTrack(const int& midiChannel, const int& sketchpadTrack)
{
    if (midiChannel == -1) {
        for (int channel = 0; channel < 16; ++channel) {
            d->midiChannelTargetTrack[channel] = sketchpadTrack;
        }
    } else {
        d->midiChannelTargetTrack[std::clamp(midiChannel, 0, 15)] = sketchpadTrack;
    }
}

int MidiRouterDevice::targetTrackForMidiChannel(const int& midiChannel) const
{
    return d->midiChannelTargetTrack[std::clamp(midiChannel, 0, 15)];
}
