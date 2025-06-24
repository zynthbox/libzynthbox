#include "MidiRouterDevice.h"
#include "ZynthboxBasics.h"
#include "DeviceMessageTranslations.h"
#include "MidiRouter.h"
#include "MidiRouterDeviceModel.h"
#include "MidiRouterFilter.h"
#include "MidiRouterFilterEntryRewriter.h"
#include "SysexHelper.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <jack/jack.h>
#include <jack/midiport.h>
#include "MidiRouterFilterEntry.h"

#define DebugRouterDevice false

#define MidiRouterDeviceCCValueRingSize 512
class MidiRouterDeviceCCValueRing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        bool processed{true};
        jack_midi_event_t event;
    };
    explicit MidiRouterDeviceCCValueRing() {
        Entry* entryPrevious{&ringData[MidiRouterDeviceCCValueRingSize - 1]};
        for (quint64 i = 0; i < MidiRouterDeviceCCValueRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~MidiRouterDeviceCCValueRing() {
    }

    void write(const jack_midi_event_t &event) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location: midi event of size" << entry->event.size << ". This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->event = event;
        entry->processed = false;
    }
    // This ring does not have a read-and-clear function, as it is likely to be called from the jack process loop and we want to avoid that doing memory type things
    void markAsRead() {
        Entry *entry = readHead;
        readHead = readHead->next;
        entry->processed = true;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[MidiRouterDeviceCCValueRingSize];
};

class MidiRouterDevicePrivate {
public:
    MidiRouterDevicePrivate (MidiRouterDevice *q)
        : q(q)
    {
        static int consequtiveId{-1};
        ++consequtiveId;
        id = consequtiveId;
        DeviceMessageTranslations::load();
        for (int channel = 0; channel < 16; ++channel) {
            acceptsChannel[channel] = true;
            receiveFromChannel[channel] = true;
            sendToChannel[channel] = true;
            masterChannel[channel] = -1;
            for (int sketchpadTrack = 0; sketchpadTrack < ZynthboxTrackCount; ++sketchpadTrack) {
                trackActivationRewrites[sketchpadTrack][channel] = 0;
                trackActivationRewrites[sketchpadTrack][channel] = -1;
            }
            for (int note = 0; note < 128; ++note) {
                noteState[channel][note] = 0;
                noteActivationTrack[channel][note] = -1;
                acceptsNote[note] = true;
                // Not technically notes, but like... same range
                ccValues[channel][note] = 0;
            }
        }
        sysexHelper = new SysexHelper(q);
        inputEventFilter = new MidiRouterFilter(q);
        inputEventFilter->setDirection(MidiRouterFilter::InputDirection);
        outputEventFilter = new MidiRouterFilter(q);
        outputEventFilter->setDirection(MidiRouterFilter::OutputDirection);
    }
    MidiRouterDevice *q{nullptr};
    int id{-1};
    MidiRouter *router{nullptr};
    SysexHelper *sysexHelper{nullptr};
    MidiRouterFilter *inputEventFilter{nullptr};
    MidiRouterFilter *outputEventFilter{nullptr};
    // Use this on any outgoing events, to ensure the event matches the device's master channel setup
    // Remember to call the function below after processing the event
    inline const void zynthboxToDevice(jack_midi_event_t *event) const;
    // Use this on any incoming events, to ensure the event matches zynthbox' internal master channel
    // Also use this after calling the above and processing outgoing events
    inline const void deviceToZynthbox(jack_midi_event_t *event) const;

    int transposeAmount{0};
    int acceptsNote[128];
    int acceptsChannel[16];
    int lastAcceptedChannel{15};
    int noteState[16][128];
    int noteActivationTrack[16][128];
    int trackActivationRewriteCount[ZynthboxTrackCount][16];
    int trackActivationRewrites[ZynthboxTrackCount][16];
    int midiChannelTargetTrack[16];
    int ccValues[16][128];
    MidiRouterDeviceCCValueRing ccValueUpdates;
    jack_client_t *jackClient{nullptr};
    bool visible{true};
    QString hardwareId{"no-hardware-id"};
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
    bool writeMidiEvents{true};
    // Zynthbox' master channel
    int globalMaster{-1};
    bool filterZynthianByChannel{false};
    int lowerMasterChannel{0};
    int upperMasterChannel{15};
    int noteSplitPoint{127};
    int lastLowerZoneMemberChannel{7};
    int lowerZoneMasterPitchBendRange{2};
    int upperZoneMasterPitchBendRange{2};
    int lowerZoneMemberPitchBendRange{48};
    int upperZoneMemberPitchBendRange{48};

    jack_port_t *inputPort{nullptr};
    void *inputBuffer{nullptr};
    uint32_t inputEventCount{0};
    uint32_t nextInputEventIndex{0};
    jack_port_t *outputPort{nullptr};
    void *outputBuffer{nullptr};
    jack_nframes_t mostRecentOutputTime{0};

    void updateMasterChannel() {
        for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
            if (channelIndex > lastLowerZoneMemberChannel && noteSplitPoint < 127) {
                masterChannel[channelIndex] = upperMasterChannel;
            } else {
                masterChannel[channelIndex] = lowerMasterChannel;
            }
        }
    }

    bool doingSettingsHandling{false};
    void loadDeviceSettings() {
        if (doingSettingsHandling == false) {
            doingSettingsHandling = true;
            // qDebug() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Loading device settings";
            QSettings settings;
            settings.beginGroup("MIDIDeviceSettings");
            settings.beginGroup(q->zynthianId());
            // Fetch the basics for the device itself
            QVariantList receiveFromChannelVariant = settings.value("receiveFromChannel", {}).toList();
            if (receiveFromChannelVariant.count() == 16) {
                for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                    receiveFromChannel[channelIndex] = std::clamp(receiveFromChannelVariant[channelIndex].toInt(), 0, 16);
                }
                Q_EMIT q->midiChannelTargetTracksChanged();
            } else if (receiveFromChannelVariant.count() != 0) {
                qWarning() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Fetched the receiveFromChannel values - we've ended up with an unacceptable number of entries, and the retrieved value was" << receiveFromChannelVariant;
            }
            QVariantList sendToChannelVariant = settings.value("sendToChannel", {}).toList();
            if (sendToChannelVariant.count() == 16) {
                for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                    sendToChannel[channelIndex] = std::clamp(sendToChannelVariant[channelIndex].toInt(), 0, 16);
                }
                Q_EMIT q->channelsToSendToChanged();
            } else if (sendToChannelVariant.count() != 0) {
                qWarning() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Fetched the sendToChannel values - we've ended up with an unacceptable number of entries, and the retrieved value was" << sendToChannelVariant;
            }
            q->setSendTimecode(settings.value("sendTimecode", sendTimecode).toBool());
            q->setSendBeatClock(settings.value("sendBeatClock", sendBeatClock).toBool());
            // Fetch the MPE settings
            settings.beginGroup("MPESettings");
            q->setLowerMasterChannel(settings.value("lowerMasterChannel", 0).toInt());
            q->setUpperMasterChannel(settings.value("upperMasterChannel", 15).toInt());
            q->setNoteSplitPoint(settings.value("noteSplitPoint", 127).toInt());
            q->setLastLowerZoneMemberChannel(settings.value("lastLowerZoneMemberChannel", 7).toInt());
            q->setLowerMasterPitchBendRange(settings.value("lowerMasterPitchBendRange", 2).toInt());
            q->setLowerMemberPitchBendRange(settings.value("lowerMemberPitchBendRange", 48).toInt());
            q->setUpperMemberPitchBendRange(settings.value("upperMemberPitchBendRange", 48).toInt());
            q->setUpperMasterPitchBendRange(settings.value("upperMasterPitchBendRange", 2).toInt());
            settings.endGroup();
            // Fetch the two event filters
            const QString storedInputEventFilter{settings.value("inputEventFilter", "").toString()};
            if (inputEventFilter->deserialize(storedInputEventFilter) == false) {
                qWarning() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Failed to deserialize the input event filter settings from the stored value" << storedInputEventFilter;
            }
            const QString storedOutputEventFilter{settings.value("outputEventFilter", "").toString()};
            if (outputEventFilter->deserialize(storedOutputEventFilter) == false) {
                qWarning() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Failed to deserialise the output event filter settings from the stored value" << storedOutputEventFilter;
            }
            settings.endGroup();
            settings.endGroup();
            doingSettingsHandling = false;
        }
    }
    void saveDeviceSettings() {
        if (doingSettingsHandling == false) {
            doingSettingsHandling = true;
            // qDebug() << Q_FUNC_INFO << humanReadableName << q->objectName() << "Saving device settings";
            QSettings settings;
            settings.beginGroup("MIDIDeviceSettings");
            settings.beginGroup(q->zynthianId());
            // Store the basics for the device itself
            QVariantList receiveFromChannelVariant, sendToChannelVariant;
            for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                receiveFromChannelVariant << receiveFromChannel[channelIndex];
                sendToChannelVariant << sendToChannel[channelIndex];
            }
            settings.setValue("receiveFromChannel", receiveFromChannelVariant);
            settings.setValue("sendToChannel", sendToChannelVariant);
            settings.setValue("sendTimecode", sendTimecode);
            settings.setValue("sendBeatClock", sendBeatClock);
            // Save the MPE settings in their own sub-group
            settings.beginGroup("MPESettings");
            settings.setValue("lowerMasterChannel", lowerMasterChannel);
            settings.setValue("upperMasterChannel", upperMasterChannel);
            settings.setValue("noteSplitPoint", noteSplitPoint);
            settings.setValue("lastLowerZoneMemberChannel", lastLowerZoneMemberChannel);
            settings.setValue("lowerZoneMasterPitchBendRange", lowerZoneMasterPitchBendRange);
            settings.setValue("lowerZoneMemberPitchBendRange", lowerZoneMemberPitchBendRange);
            settings.setValue("upperZoneMemberPitchBendRange", upperZoneMemberPitchBendRange);
            settings.setValue("upperZoneMasterPitchBendRange", upperZoneMasterPitchBendRange);
            settings.endGroup();
            // Store each of the two event filters
            settings.setValue("inputEventFilter", inputEventFilter->serialize());
            settings.setValue("outputEventFilter", outputEventFilter->serialize());
            settings.endGroup();
            settings.endGroup();
            doingSettingsHandling = false;
        }
    }
};

MidiRouterDevice::MidiRouterDevice(jack_client_t *jackClient, MidiRouter *parent)
    : QObject(parent)
    , d(new MidiRouterDevicePrivate(this))
{
    d->router = parent;
    DeviceMessageTranslations::load();
    d->jackClient = jackClient;
    setMidiChannelTargetTrack(-1, -1);
    // Make sure that we save the settings when things change
    QTimer *deviceSettingsSaverThrottle{new QTimer(this)};
    deviceSettingsSaverThrottle->setSingleShot(true);
    deviceSettingsSaverThrottle->setInterval(0);
    connect(deviceSettingsSaverThrottle, &QTimer::timeout, this, [this](){ d->saveDeviceSettings(); });
    connect(this, &MidiRouterDevice::midiChannelTargetTracksChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::sendTimecodeChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::sendBeatClockChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::channelsToSendToChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::lowerMasterChannelChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::upperMasterChannelChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::noteSplitPointChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::lastLowerZoneMemberChannelChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::lowerMasterPitchBendRangeChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::lowerMemberPitchBendRangeChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::upperMemberPitchBendRangeChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(this, &MidiRouterDevice::upperMasterPitchBendRangeChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(d->inputEventFilter, &MidiRouterFilter::entriesDataChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
    connect(d->outputEventFilter, &MidiRouterFilter::entriesDataChanged, this, [this, deviceSettingsSaverThrottle](){ if (d->doingSettingsHandling == false) { deviceSettingsSaverThrottle->start(); } });
}

MidiRouterDevice::~MidiRouterDevice()
{
    qobject_cast<MidiRouterDeviceModel*>(d->router->model())->removeDevice(this);
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

void MidiRouterDevice::completeInitialisation()
{
    d->loadDeviceSettings();
    qobject_cast<MidiRouterDeviceModel*>(d->router->model())->addDevice(this);
    // As one of the first things, ask the device what sort of device it is
    SysexMessage *identityRequestMessage{d->sysexHelper->createKnownMessage(SysexHelper::IdentityRequestMessage)};
    identityRequestMessage->setDeleteOnSend(true);
    d->sysexHelper->send(identityRequestMessage);
}

const int &MidiRouterDevice::id() const
{
    return d->id;
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
    // Fire off any events that might be in the output ring for immediate dispatch
    while (midiOutputRing.readHead->processed == false) {
        const juce::MidiBuffer &buffer = midiOutputRing.readHead->buffer;
        for (const juce::MidiMessageMetadata &juceMessage : buffer) {
            // These want to be written raw onto the output buffer (they will have already gone through filters etc)
            jack_midi_event_write(d->outputBuffer, 0,
                const_cast<jack_midi_data_t*>(juceMessage.data), // this might seems odd, but it's really only because juce's internal store is const here, and the data types are otherwise the same
                size_t(juceMessage.numBytes) // this changes signedness, but from a lesser space (int) to a larger one (unsigned long)
            );
        }
        midiOutputRing.markAsRead();
    }
    // Send out any queued up sysex messages
    d->sysexHelper->process(d->outputBuffer);
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

void MidiRouterDevice::writeEventToOutput(jack_midi_event_t& event, const MidiRouterFilterEntry *eventFilter, int outputChannel)
{
    if (eventFilter) {
        eventFilter->writeEventToDevice(this);
    } else {
        const MidiRouterFilterEntry *filterEntry = d->outputEventFilter->match(event);
        if (filterEntry) {
            filterEntry->writeEventToDevice(this);
        } else {
            d->zynthboxToDevice(&event);
            const int eventChannel{event.buffer[0] & 0xf};
            if (event.size == 3 && 0xAF < event.buffer[0] && event.buffer[0] < 0xC0 && event.buffer[1] == 0x78) {
                for (int sketchpadTrack = 0; sketchpadTrack < ZynthboxTrackCount; ++sketchpadTrack) {
                    d->trackActivationRewriteCount[sketchpadTrack][eventChannel] = 0;
                    d->trackActivationRewrites[sketchpadTrack][eventChannel] = -1;
                }
                for (int note = 0; note < 128; ++note) {
                    d->noteState[eventChannel][note] = 0;
                }
            }
            if (outputChannel > -1) {
                if (d->acceptsChannel[outputChannel] == false) {
                    outputChannel = d->lastAcceptedChannel;
                }
                event.buffer[0] = event.buffer[0] - eventChannel + outputChannel;
            } else if (d->acceptsChannel[eventChannel] == false) {
                outputChannel = d->lastAcceptedChannel;
                event.buffer[0] = event.buffer[0] - eventChannel + outputChannel;
            }
            writeEventToOutputActual(event);
            if (outputChannel > -1) {
                event.buffer[0] = event.buffer[0] + eventChannel - outputChannel;
            }
            d->deviceToZynthbox(&event);
        }
    }
}

void MidiRouterDevice::writeEventToOutputActual(jack_midi_event_t& event)
{
    const bool isNoteMessage = event.buffer[0] > 0x7F && event.buffer[0] < 0xA0;
    if (isNoteMessage == false || d->acceptsNote[event.buffer[1]]) {
        const jack_midi_data_t untransposedNote{event.buffer[1]};
        if (isNoteMessage) {
            event.buffer[1] = std::clamp(int(event.buffer[1]) + d->transposeAmount, 0, 127);
        }
        int errorCode{0};
        // If we have been asked to not write midi events to this output device, don't write the event (and just report no error)
        if (d->writeMidiEvents) {
            errorCode = jack_midi_event_write(d->outputBuffer, event.time, event.buffer, event.size);
            if (errorCode == -EINVAL) {
                // If the error invalid happens, we should likely assume the event was out of order for whatever reason, and just schedule it at the same time as the most recently scheduled event
                #if DebugRouterDevice
                    qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Attempted to write out-of-order event for time" << event.time << "so writing to most recent instead:" << d->mostRecentOutputTime;
                #endif
                errorCode = jack_midi_event_write(d->outputBuffer, d->mostRecentOutputTime, event.buffer, event.size);
            }
        }
        event.buffer[1] = untransposedNote;
        if (errorCode != 0) {
            if (errorCode == -ENOBUFS) {
                // FIXME Super-massive hack hiding a bunch of trouble with the usb gadget midi thing and jack... it's still noisy, but... less
                static const QString usbHumanName{"USB MIDI "};
                if (d->humanReadableName.startsWith(usbHumanName)) {
                    qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Ran out of space while writing events!";
                }
            } else {
                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Error writing midi event:" << -errorCode << strerror(-errorCode) << "for event at time" << event.time << "of size" << event.size;
            }
        #if DebugRouterDevice
        } else {
            if (DebugRouterDevice) { qDebug() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Wrote event to buffer at time" << QString::number(event.time).rightJustified(4, ' ') << "on channel" << currentChannel << "for port" << (output ? output->portName : "no-port-details") << "with data" << event.buffer[0] << event.buffer[1]; }
        #endif
        }
        if (d->mostRecentOutputTime < event.time) {
            d->mostRecentOutputTime = event.time;
        }
    }
}

void MidiRouterDevice::nextInputEvent()
{
    if (d->inputBuffer != nullptr && d->nextInputEventIndex < d->inputEventCount) {
        if (int error = jack_midi_event_get(&currentInputEvent, d->inputBuffer, d->nextInputEventIndex)) {
            currentInputEvent.size = 0;
            qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "jack_midi_event_get, received event lost! We were supposed to have" << d->inputEventCount << "events, attempted to fetch at index" << d->nextInputEventIndex << "and the error code is" << error << strerror(-error);
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
                d->ccValueUpdates.write(currentInputEvent);
                d->ccValues[currentInputEvent.buffer[0] & 0xf][currentInputEvent.buffer[1]] = currentInputEvent.buffer[2];
            } else if (currentInputEvent.buffer[0] == 0xF0) {
                // This is a sysex message, so pass it to the helper for handling
                d->sysexHelper->handleInputEvent(currentInputEvent);
            }
            // if (d->type.testFlag(MidiRouterDevice::HardwareDeviceType)) {
            //     qDebug() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Retrieved jack midi event on device" << d->humanReadableName << "with data size" << d->currentInputEvent->size << "at time" << d->currentInputEvent->time << "event" << d->nextInputEventIndex + 1 << "of" << d->inputEventCount;
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
            } else if (d->noteState[channel][note] < 0) {
                d->noteState[channel][note] = 0;
            }
        }
    } else {
        qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Attempted to set note activation state for note" << note << "on channel" << channel << "to" << active;
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

const void MidiRouterDevice::setTrackActivationRewriteChannel(const int& sketchpadTrack, const int& eventChannel, const int& rewriteChannel) const
{
    if (rewriteChannel == -1) {
        // When asked to set the rewrite channel to -1, it means we're logically identifying an off note, so count down our activations
        d->trackActivationRewriteCount[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)]--;
        if (d->trackActivationRewriteCount[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)] < 0) {
            // This may happen if we're being given off notes after having also been given an all-notes-off, so handle that gracefully
            d->trackActivationRewriteCount[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)] = 0;
        }
    } else {
        // When asked to set the rewrite channel to other than -1, it means we're logically identifying an on note, so count down our activations
        d->trackActivationRewriteCount[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)]++;
    }
    if (d->trackActivationRewriteCount[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)] == 0) {
        d->trackActivationRewrites[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)] = -1;
    } else {
        d->trackActivationRewrites[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)] = std::clamp(rewriteChannel, -1, 15);
    }
}

const int & MidiRouterDevice::trackActivationRewriteChannel(const int& sketchpadTrack, const int& eventChannel) const
{
    return d->trackActivationRewrites[std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)][std::clamp(eventChannel, 0, 15)];
}


int MidiRouterDevice::ccValue(const int& midiChannel, const int& ccControl) const
{
    return d->ccValues[std::clamp(midiChannel, 0, 15)][std::clamp(ccControl, 0, 127)];
}

void MidiRouterDevice::handlePostponedEvents()
{
    while (d->ccValueUpdates.readHead->processed == false) {
        const jack_midi_event_t &event{d->ccValueUpdates.readHead->event};
        Q_EMIT ccValueChanged(event.buffer[0] & 0xf, event.buffer[1], event.buffer[2]);
        d->ccValueUpdates.markAsRead();
    }
    d->sysexHelper->handlePostponedEvents();
}

void MidiRouterDevice::forceSetCCValue(const int& midiChannel, const int& ccControl, const int& ccValue) const
{
    d->ccValues[std::clamp(midiChannel, 0, 15)][std::clamp(ccControl, 0, 127)] = std::clamp(ccValue, 0, 127);
}

void MidiRouterDevice::setVisible(const bool& visible)
{
    if (d->visible != visible) {
        d->visible = visible;
        Q_EMIT visibleChanged();
    }
}

bool MidiRouterDevice::visible() const
{
    return d->visible;
}

void MidiRouterDevice::setHardwareId(const QString& hardwareId)
{
    d->hardwareId = hardwareId;
    setObjectName(QString("%1/%2").arg(d->hardwareId).arg(d->zynthianId));
    Q_EMIT hardwareIdChanged();
}

const QString & MidiRouterDevice::hardwareId() const
{
    return d->hardwareId;
}

void MidiRouterDevice::setZynthianId(const QString& zynthianId)
{
    d->zynthianId = zynthianId;
    setObjectName(QString("%1/%2").arg(d->hardwareId).arg(d->zynthianId));
    Q_EMIT zynthianIdChanged();
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
        Q_EMIT humanReadableNameChanged();
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
                qDebug() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Failed to unregister input port even though there's one registered. We'll ignore that and keep going, but this seems not quite right. Reported error was:" << error << strerror(-error);
            }
            d->inputPort = nullptr;
        }
        if (portName.isEmpty() == false) {
            d->inputPort = jack_port_register(d->jackClient, d->inputPortName.toUtf8(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
        }
        if (d->inputPort == nullptr) {
            d->inputEnabled = false;
        }
        Q_EMIT inputPortNameChanged();
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
                qDebug() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Failed to unregister output port even though there's one registered. We'll ignore that and keep going, but this seems not quite right. Reported error was:" << error << strerror(-error);
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

void MidiRouterDevice::setTransposeAmount(int transposeAmount)
{
    d->transposeAmount = transposeAmount;
}

void MidiRouterDevice::setAcceptedMidiChannels(const QList<int>& acceptedMidiChannels)
{
    for (int channel = 0; channel < 16; ++channel) {
        d->acceptsChannel[channel] = acceptedMidiChannels.contains(channel);
        if (d->acceptsChannel[channel]) {
            d->lastAcceptedChannel = channel;
        }
    }
}

void MidiRouterDevice::setFilterZynthianOutputByChannel(const bool& filterZynthianByChannel)
{
    d->filterZynthianByChannel = filterZynthianByChannel;
}

bool MidiRouterDevice::filterZynthianOutputByChannel() const
{
    return d->filterZynthianByChannel;
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
    Q_EMIT channelsToSendToChanged();
}

const bool & MidiRouterDevice::sendToChannel(const int& channel) const
{
    return d->sendToChannel[channel];
}

QVariantList MidiRouterDevice::channelsToSendTo() const
{
    QVariantList list;
    for (const bool &value : d->sendToChannel) {
        list.append(value);
    }
    return list;
}

void MidiRouterDevice::setSendTimecode(const bool& sendTimecode)
{
    if (d->sendTimecode != sendTimecode) {
        d->sendTimecode = sendTimecode;
        Q_EMIT sendTimecodeChanged();
    }
}

const bool & MidiRouterDevice::sendTimecode() const
{
    return d->sendTimecode;
}

void MidiRouterDevice::setSendBeatClock(const bool& sendBeatClock)
{
    if (d->sendBeatClock != sendBeatClock) {
        d->sendBeatClock = sendBeatClock;
        Q_EMIT sendBeatClockChanged();
    }
}

const bool & MidiRouterDevice::sendBeatClock() const
{
    return d->sendBeatClock;
}

void MidiRouterDevice::setWriteMidiEvents(const bool& writeMidiEvents)
{
    if (d->writeMidiEvents != writeMidiEvents) {
        d->writeMidiEvents = writeMidiEvents;
        Q_EMIT writeMidiEventsChanged();
    }
}

const bool & MidiRouterDevice::writeMidiEvents() const
{
    return d->writeMidiEvents;
}

int MidiRouterDevice::lowerMasterChannel() const
{
    return d->lowerMasterChannel;
}

void MidiRouterDevice::setLowerMasterChannel(const int& lowerMasterChannel)
{
    if (d->lowerMasterChannel != lowerMasterChannel) {
        d->lowerMasterChannel = std::clamp(lowerMasterChannel, 0, 15);
        Q_EMIT lowerMasterChannelChanged();
        d->updateMasterChannel();
    }
}

int MidiRouterDevice::upperMasterChannel() const
{
    return d->upperMasterChannel;
}

void MidiRouterDevice::setUpperMasterChannel(const int& upperMasterChannel)
{
    if (d->upperMasterChannel != upperMasterChannel) {
        d->upperMasterChannel = std::clamp(upperMasterChannel, 0, 15);
        Q_EMIT upperMasterChannelChanged();
        d->updateMasterChannel();
    }
}

int MidiRouterDevice::noteSplitPoint() const
{
    return d->noteSplitPoint;
}

void MidiRouterDevice::setNoteSplitPoint(const int& noteSplitPoint)
{
    if (d->noteSplitPoint != noteSplitPoint) {
        d->noteSplitPoint = std::clamp(noteSplitPoint, 0, 127);
        Q_EMIT noteSplitPointChanged();
    }
}

int MidiRouterDevice::lastLowerZoneMemberChannel() const
{
    return d->lastLowerZoneMemberChannel;
}

void MidiRouterDevice::setLastLowerZoneMemberChannel(const int& lastLowerZoneMemberChannel)
{
    if (d->lastLowerZoneMemberChannel != lastLowerZoneMemberChannel) {
        d->lastLowerZoneMemberChannel = std::clamp(lastLowerZoneMemberChannel, 0, 15);
        Q_EMIT lastLowerZoneMemberChannelChanged();
        d->updateMasterChannel();
    }
}

int MidiRouterDevice::lowerMasterPitchBendRange() const
{
    return d->lowerZoneMasterPitchBendRange;
}

void MidiRouterDevice::setLowerMasterPitchBendRange(const int& lowerMasterPitchBendRange)
{
    if (d->lowerZoneMasterPitchBendRange != lowerMasterPitchBendRange) {
        d->lowerZoneMasterPitchBendRange = std::clamp(lowerMasterPitchBendRange, 1, 96);
        Q_EMIT lowerMasterPitchBendRangeChanged();
    }
}

int MidiRouterDevice::lowerMemberPitchBendRange() const
{
    return d->lowerZoneMemberPitchBendRange;
}

void MidiRouterDevice::setLowerMemberPitchBendRange(const int& lowerMemberPitchBendRange)
{
    if (d->lowerZoneMemberPitchBendRange != lowerMemberPitchBendRange) {
        d->lowerZoneMemberPitchBendRange = std::clamp(lowerMemberPitchBendRange, 1, 96);
        Q_EMIT lowerMemberPitchBendRangeChanged();
    }
}

int MidiRouterDevice::upperMemberPitchBendRange() const
{
    return d->upperZoneMemberPitchBendRange;
}

void MidiRouterDevice::setUpperMemberPitchBendRange(const int& upperMemberPitchBendRange)
{
    if (d->upperZoneMemberPitchBendRange != upperMemberPitchBendRange) {
        d->upperZoneMemberPitchBendRange = std::clamp(upperMemberPitchBendRange, 1, 96);
        Q_EMIT upperMemberPitchBendRangeChanged();
    }
}

int MidiRouterDevice::upperMasterPitchBendRange() const
{
    return d->upperZoneMasterPitchBendRange;
}

void MidiRouterDevice::setUpperMasterPitchBendRange(const int& upperMasterPitchBendRange)
{
    if (d->upperZoneMasterPitchBendRange != upperMasterPitchBendRange) {
        d->upperZoneMasterPitchBendRange = std::clamp(upperMasterPitchBendRange, 1, 96);
        Q_EMIT upperMasterPitchBendRangeChanged();
    }
}

void MidiRouterDevice::sendMPESettingsToDevice()
{
    juce::MidiBuffer midiBuffer;
    // Configure the lower zone (RPN message on channel 1 with the number of member channels for the lower zone)
    midiBuffer.addEvents(juce::MidiRPNGenerator::generate(1, 6, d->lastLowerZoneMemberChannel), 0, 0, 0);
    // Configure the upper zone (RPN message on channel 16 with the number of member channels for the upper zone)
    midiBuffer.addEvents(juce::MidiRPNGenerator::generate(16, 6, 15 - d->lastLowerZoneMemberChannel), 0, 0, 0);
    // Configure the pitch bend for each channel
    for (int midiChannel = 0; midiChannel < 16; ++midiChannel) {
        int pitchBendRange{0};
        if (midiChannel == 0 && d->lastLowerZoneMemberChannel > 0) {
            // The lower zone is defined, and this is the master channel for that zone: Send out the global pitch bend for the lower zone
            pitchBendRange = d->lowerZoneMasterPitchBendRange;
        } else if (midiChannel == 16 && d->lastLowerZoneMemberChannel < 14) {
            // The upper zone is defined, and this is the master channel for that zone: Send out the global pitch bend for the upper zone
            pitchBendRange = d->upperZoneMasterPitchBendRange;
        } else {
            if (midiChannel <= d->lastLowerZoneMemberChannel) {
                // We are in the lower zone, send out pitch bend range for that zone
                pitchBendRange = d->lowerZoneMemberPitchBendRange;
            } else {
                // We are in the upper zone, send out pitch bend range for that zone
                pitchBendRange = d->upperZoneMemberPitchBendRange;
            }
        }
        midiBuffer.addEvents(juce::MidiRPNGenerator::generate(midiChannel + 1, 0, pitchBendRange), 0, 0, 0);
    }
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
                    // qDebug() << Q_FUNC_INFO << q << "Moving event down from" << eventChannel << "to" << eventChannel - 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel < globalMaster) {
                    // Then it's between global and device master, so we move it up one channel
                    event->buffer[0] = byte0 + 1;
                    // qDebug() << Q_FUNC_INFO << q << "Moving event up from" << eventChannel << "to" << eventChannel + 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel == globalMaster) {
                    // Then it's on the global master, and should be on the device master channel
                    event->buffer[0] = byte0 - globalMaster + masterChannel;
                    // qDebug() << Q_FUNC_INFO << q << "Moving event from global master" << globalMaster << "to device master" << masterChannel << "for byte0" << event->buffer[0];
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
                    // qDebug() << Q_FUNC_INFO << q << "Moving event down from" << eventChannel << "to" << eventChannel - 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel < masterChannel) {
                    // Then it's between global and device master, so we move it up one channel
                    event->buffer[0] = byte0 + 1;
                    // qDebug() << Q_FUNC_INFO << q << "Moving event up from" << eventChannel << "to" << eventChannel + 1 << "for byte0" << event->buffer[0];
                } else if (eventChannel == masterChannel) {
                    // Then it's on the device master, and should be on the global master channel
                    event->buffer[0] = byte0 - masterChannel + globalMaster;
                    // qDebug() << Q_FUNC_INFO << q << "Moving event from device master" << eventChannel << "to global master" << globalMaster << "for byte0" << event->buffer[0];
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
    Q_EMIT midiChannelTargetTracksChanged();
}

int MidiRouterDevice::targetTrackForMidiChannel(const int& midiChannel) const
{
    return d->midiChannelTargetTrack[std::clamp(midiChannel, 0, 15)];
}

QVariantList MidiRouterDevice::midiChannelTargetTracks() const
{
    QVariantList list;
    for (int channel = 0; channel < 16; ++channel) {
        list << d->midiChannelTargetTrack[channel];
    }
    return list;
}

bool MidiRouterDevice::saveDeviceSettings(const QString& filePath)
{
    bool result{false};
    QFile file{filePath};
    if (filePath > 10) {
        QJsonDocument document;
        QJsonObject settingsObject;
        QJsonArray receiveFromChannelArray, sendToChannelArray, midiChannelTargetTrackArray;
        for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
            receiveFromChannelArray.append(d->receiveFromChannel[channelIndex]);
            sendToChannelArray.append(d->sendToChannel[channelIndex]);
            midiChannelTargetTrackArray.append(d->midiChannelTargetTrack[channelIndex]);
        }
        settingsObject.insert("receiveFromChannel", receiveFromChannelArray);
        settingsObject.insert("sendToChannel", sendToChannelArray);
        settingsObject.insert("midiChannelTargetTrack", midiChannelTargetTrackArray);
        settingsObject.insert("sendTimecode", d->sendTimecode);
        settingsObject.insert("sendBeatClock", d->sendBeatClock);
        QJsonObject mpeSettingsObject;
        mpeSettingsObject.insert("lowerMasterChannel", d->lowerMasterChannel);
        mpeSettingsObject.insert("upperMasterChannel", d->upperMasterChannel);
        mpeSettingsObject.insert("noteSplitPoint", d->noteSplitPoint);
        mpeSettingsObject.insert("lastLowerZoneMemberChannel", d->lastLowerZoneMemberChannel);
        settingsObject.insert("MPEsettings", mpeSettingsObject);
        settingsObject.insert("inputEventFilter", d->inputEventFilter->serialize());
        settingsObject.insert("outputEventFilter", d->outputEventFilter->serialize());
        document.setObject(settingsObject);
        if (file.exists() == false || file.remove()) {
            if (file.open(QIODevice::WriteOnly)) {
                file.write(document.toJson(QJsonDocument::Indented));
                file.close();
                result = true;
            } else {
                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Could not open the file for writing" << filePath;
            }
        } else {
            if (file.exists()) {
                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "The file already exists, and we could not delete it:" << filePath;
            }
        }
    } else {
        qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "The filename passed to the function failed to pass basic sanity checks (don't save in the root, and please don't try and pass in the root directory - it won't delete, as it's not just a file, and also just, why?)" << filePath;
    }
    return result;
}

bool MidiRouterDevice::loadDeviceSettings(const QString& filePath)
{
    bool result{false};
    QFile file{filePath};
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            const QString fileContents{file.readAll()};
            file.close();
            if (fileContents.length() > 0) {
                QJsonParseError error;
                QJsonDocument document{QJsonDocument::fromJson(fileContents.toUtf8(), &error)};
                if (error.error == QJsonParseError::NoError) {
                    if (document.isObject()) {
                        QJsonObject settingsObject = document.object();
                        QJsonValue value = settingsObject.value("receiveFromChannel");
                        if (value.isArray()) {
                            QJsonArray receiveFromChannelArray = value.toArray();
                            if (receiveFromChannelArray.count() == 16) {
                                for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                                    d->receiveFromChannel[channelIndex] = std::clamp(receiveFromChannelArray[channelIndex].toInt(), 0, 16);
                                }
                                Q_EMIT midiChannelTargetTracksChanged();
                            } else if (receiveFromChannelArray.count() != 0) {
                                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Fetched the receiveFromChannel values - we've ended up with an unacceptable number of entries, and the retrieved value was" << receiveFromChannelArray;
                            }
                        }
                        value = settingsObject.value("sendToChannel");
                        if (value.isArray()) {
                            QJsonArray sendToChannelArray = value.toArray();
                            if (sendToChannelArray.count() == 16) {
                                for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                                    d->sendToChannel[channelIndex] = std::clamp(sendToChannelArray[channelIndex].toInt(), 0, 16);
                                }
                                Q_EMIT channelsToSendToChanged();
                            } else if (sendToChannelArray.count() != 0) {
                                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Fetched the sendToChannel values - we've ended up with an unacceptable number of entries, and the retrieved value was" << sendToChannelArray;
                            }
                        }
                        value = settingsObject.value("midiChannelTargetTrack");
                        if (value.isArray()) {
                            QJsonArray midiChannelTargetTrackArray = value.toArray();
                            if (midiChannelTargetTrackArray.count() == 16) {
                                for (int channelIndex = 0; channelIndex < 16; ++channelIndex) {
                                    d->midiChannelTargetTrack[channelIndex] = std::clamp(midiChannelTargetTrackArray[channelIndex].toInt(), -3, ZynthboxTrackCount);
                                }
                                Q_EMIT channelsToSendToChanged();
                            } else if (midiChannelTargetTrackArray.count() != 0) {
                                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Fetched the midiChannelTargetTrack values - we've ended up with an unacceptable number of entries, and the retrieved value was" << midiChannelTargetTrackArray;
                            }
                        }
                        value = settingsObject.value("sendTimecode");
                        if (value.isBool()) {
                            setSendTimecode(value.toBool());
                        } else {
                            setSendTimecode(true);
                        }
                        value = settingsObject.value("sendBeatClock");
                        if (value.isBool()) {
                            setSendBeatClock(value.toBool());
                        } else {
                            setSendBeatClock(true);
                        }
                        // Fetch the MPE settings
                        value = settingsObject.value("MPEsettings");
                        if (value.isObject()) {
                            QJsonObject mpeSettingsObject{value.toObject()};
                            value = mpeSettingsObject.value("lowerMasterChannel");
                            if (value.isDouble()) {
                                setLowerMasterChannel(value.toDouble(0));
                            } else {
                                setLowerMasterChannel(0);
                            }
                            value = mpeSettingsObject.value("upperMasterChannel");
                            if (value.isDouble()) {
                                setUpperMasterChannel(value.toDouble());
                            } else {
                                setUpperMasterChannel(15);
                            }
                            value = mpeSettingsObject.value("noteSplitPoint");
                            if (value.isDouble()) {
                                setNoteSplitPoint(value.toDouble());
                            } else {
                                setNoteSplitPoint(127);
                            }
                            value = mpeSettingsObject.value("lastLowerZoneMemberChannel");
                            if (value.isDouble()) {
                                setLastLowerZoneMemberChannel(value.toDouble());
                            } else {
                                setLastLowerZoneMemberChannel(7);
                            }
                        }
                        // Fetch the two event filters
                        value = settingsObject.value("inputEventFilter");
                        if (value.isString()) {
                            const QString storedInputEventFilter{value.toString()};
                            if (d->inputEventFilter->deserialize(storedInputEventFilter) == false) {
                                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Failed to deserialize the input event filter settings from the stored value" << storedInputEventFilter;
                            }
                        } else {
                            d->inputEventFilter->deserialize("");
                        }
                        value = settingsObject.value("outputEventFilter");
                        if (value.isString()) {
                            const QString storedOutputEventFilter{value.toString()};
                            if (d->outputEventFilter->deserialize(storedOutputEventFilter) == false) {
                                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Failed to deserialise the output event filter settings from the stored value" << storedOutputEventFilter;
                            }
                        } else {
                            d->outputEventFilter->deserialize("");
                        }
                        result = true;
                    } else {
                        qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "The contents of the file were not a json document as expected. The data was:\n" << fileContents;
                    }
                } else {
                    qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "There was an error while attempting to parse the saved json. The error description was" << error.errorString() << "and the data we attempted to parse was:\n" << fileContents;
                }
            } else {
                qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "The saved settings file contained no data";
            }
        } else {
            qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "Could not open file for reading. Error description given as:" << file.errorString();
        }
    } else {
        qWarning() << Q_FUNC_INFO << d->humanReadableName << objectName() << "No such file:" << filePath;
    }
    return result;
}

QObject * MidiRouterDevice::sysexHelper() const
{
    return d->sysexHelper;
}

void MidiRouterDevice::cuiaEventFeedback(const CUIAHelper::Event &cuiaEvent, const int& /*originId*/, const ZynthboxBasics::Track& track, const ZynthboxBasics::Slot& slot, const int& value)
{
    const MidiRouterFilterEntry* matchedEntry = d->outputEventFilter->matchCommand(cuiaEvent, track, slot, value);
    if (matchedEntry) {
        int trackIndex = track;
        if (track == ZynthboxBasics::AnyTrack || track == ZynthboxBasics::CurrentTrack) {
            trackIndex = MidiRouter::instance()->currentSketchpadTrack();
        }
        int slotIndex = slot;
        if (slot == ZynthboxBasics::AnySlot || slot == ZynthboxBasics::CurrentSlot) {
            slotIndex = 0;
            // TODO We need to be able to fetch the "current" slot of any track - for now we'll reset this to 0, but that'll need sorting out
        }
        juce::MidiBuffer midiBuffer;
        int bytes[3];
        for (const MidiRouterFilterEntryRewriter *rule : matchedEntry->rewriteRules()) {
            for (int byteIndex = 0; byteIndex < int(rule->byteSize()); ++byteIndex) {
                if (rule->m_bytes[byteIndex] == MidiRouterFilterEntryRewriter::OriginalByte1) {
                    bytes[byteIndex] = trackIndex;
                } else if (rule->m_bytes[byteIndex] == MidiRouterFilterEntryRewriter::OriginalByte2) {
                    bytes[byteIndex] = slotIndex;
                } else if (rule->m_bytes[byteIndex] == MidiRouterFilterEntryRewriter::OriginalByte3) {
                    bytes[byteIndex] = value;
                } else {
                    bytes[byteIndex] = rule->m_bytes[byteIndex];
                }
                if (rule->m_bytesAddChannel[0]) {
                    bytes[byteIndex] += trackIndex;
                }
            }
            if (rule->byteSize() == MidiRouterFilterEntryRewriter::EventSize1) {
                midiBuffer.addEvent(juce::MidiMessage(bytes[0], bytes[1], bytes[2]), 0);
            } else if (rule->byteSize() == MidiRouterFilterEntryRewriter::EventSize2) {
                midiBuffer.addEvent(juce::MidiMessage(bytes[0], bytes[1]), 0);
            } else if (rule->byteSize() == MidiRouterFilterEntryRewriter::EventSize3 || rule->byteSize() == MidiRouterFilterEntryRewriter::EventSizeSame) {
                midiBuffer.addEvent(juce::MidiMessage(bytes[0]), 0);
            }
        }
        midiOutputRing.write(midiBuffer);
    }
}

MidiRouterFilter * MidiRouterDevice::inputEventFilter() const
{
    return d->inputEventFilter;
}

MidiRouterFilter * MidiRouterDevice::outputEventFilter() const
{
    return d->outputEventFilter;
}
