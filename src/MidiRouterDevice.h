#pragma once

#include "MidiRouter.h"

#include <jack/midiport.h>
#include <QFlags>

class QString;
class MidiRouterDevicePrivate;
/**
 * \brief A representation of a device as they are known by MidiRouter (hardware or software device)
 *
 * Examples of devices are SyncTimer, TransportManager, the output channels for the Zynthian-defined synths, and hardware midi devices
 */
class MidiRouterDevice : public QObject {
    Q_OBJECT
public:
    enum DeviceDirection {
        InDevice = 0x1,
        OutDevice = 0x2,
    };
    Q_DECLARE_FLAGS(DeviceDirections, DeviceDirection)
    enum DeviceType {
        ControllerType = 0x1,
        SynthType = 0x2,
        TimeCodeGeneratorType = 0x4,
        HardwareDeviceType = 0x8,
        MasterTrackType = 0x16,
    };
    Q_DECLARE_FLAGS(DeviceTypes, DeviceType)
    explicit MidiRouterDevice(jack_client_t *jackClient, MidiRouter *parent = nullptr);
    ~MidiRouterDevice() override;

    /**
     * \brief Call this function at the start of MidiRouter's process function, to ensure the device's internal state is ready
     * @param nframes The number of frames for this process call
     */
    void processBegin(const jack_nframes_t &nframes);
    /**
     * \brief Write a midi event to the device's output buffer
     * The event will be readjusted to fit snugly into the event list (by pushing it ahead in time if needed)
     * @param event The event to be written
     * @param overrideChannel If greater than -1, the event will be written to the output using that channel (the event will be returned intact)
     */
    void writeEventToOutput(jack_midi_event_t &event, int overrideChannel = -1);
    /**
     * \brief The current input event - size will be 0 if the end was reached
     * Once processed, call nextInputEvent() to progress to the next one
     */
    jack_midi_event_t currentInputEvent;
    /**
     * \brief When you are done with the current input event, call this to fetch the next one
     */
    void nextInputEvent();
    /**
     * \brief Call this function at the end of MidiRouter's process function, to perform any cleanup required by the device
     */
    void processEnd();

    /**
     * \brief Resets the note activation state for the device
     */
    void resetNoteActivation();
    /**
     * \brief Mark a note on a channel as active or inactive
     * @param channel The midi channel the note has arrived on
     * @param note The note to mark as active or inactive
     * @param active True if it should be marked active, false if it should be marked inactive
     * @note We are internally tracking activations fully (that is, we know how many have happened - used for example to ensure we end up with zero if a device is unplugged)
     */
    void setNoteActive(const int &sketchpadTrack, const int &channel, const int &note, const bool &active);
    /**
     * \brief Get the current known activation level of the given note on the given channel
     * @param channel The channel the note exists on
     * @param note The note in question
     * @return A number of activations: Above 1 means the note has been activated on that channel so many times. 0 means the note is not known to be active. A negative number means note-off messages were lost.
     */
    const int &noteActivationState(const int &channel, const int &note) const;
    /**
     * \brief Get the track used for a given activated note
     * @param channel The midi channel the note arrived on
     * @param note The note in question
     * @return The sketchpad track the initial note activation was sent to (or -1 if there was no activation)
     */
    const int &noteActivationTrack(const int &channel, const int &note) const;

    /**
     * \brief Set the hardware ID of the device (essentially our internal ID used to distinguish the device)
     * @param hardwareId The hardware ID for this device
     */
    void setHardwareId(const QString &hardwareId);
    Q_SIGNAL void hardwareIdChanged();
    /**
     * \brief The hardware ID for this device (essentially our internal ID used to distinguish the device)
     * This is useful when a hardware device shows up as multiple devices with otherwise identical names. This
     * affects e.g. Keystation 61 MK3, which shows up as two inputs and two outputs, with the same name: One for
     * the keybed, and one for the DAW control buttons on the device. We need to distinguish between the two,
     * and so we want to have both as separate devices.
     * @return The hardware ID for this device
     */
    const QString &hardwareId() const;
    /**
     * \brief Set the name we expect zynthian to know this device by
     * This is used to translate between the settings from webconf (enabled/disabled, that sort of thing)
     * @param zynthianId The string ID for this device
     */
    void setZynthianId(const QString &zynthianId);
    Q_SIGNAL void zynthianIdChanged();
    /**
     * \brief The name we expect zynthian to know this device by
     * @return The zynthian ID of this device
     */
    const QString &zynthianId() const;
    /**
     * \brief Set the human-readable name for this device
     * @param humanReadableName A new human-readable name for this device
     */
    void setHumanReadableName(const QString &humanReadableName);
    Q_SIGNAL void humanReadableNameChanged();
    /**
     * \brief The human-readable name for this device
     * @return The human-readable name for this device
     */
    const QString &humanReadableName() const;

    /**
     * \brief If the device has an input port registered, set it using this function
     * This function will also automatically mark the device as being an input device,
     * and create the input port with the given name on the MidiRouter jack client
     * @param portName The jack port's name (fully qualified)
     */
    void setInputPortName(const QString &portName);
    Q_SIGNAL void inputPortNameChanged();
    /**
     * \brief The fully qualified jack name of the input port associated with this device
     */
    const QString &inputPortName() const;
    /**
     * \brief Set whether or not this device should be considered for input
     * @param enabled True if the device should be used for input, false if not
     */
    void setInputEnabled(const bool &enabled);
    /**
     * \brief Whether or not this device should be considered for input
     * @return True if the device should be considered for input
     */
    const bool &inputEnabled() const;

    /**
     * \brief If the device has an output port registered, set it using this function
     * This function will also automatically mark the device as being an output device,
     * and create the output port with the given name on the MidiRouter jack client
     * @param portName The jack port's name (fully qualified)
     */
    void setOutputPortName(const QString &portName);
    /**
     * \brief The fully qualified jack name of the output port associated with this device
     */
    const QString &outputPortName() const;
    /**
     * \brief Set whether or not this device should be considered for output
     * @param enabled True if the device should be used for output, false if not
     */
    void setOutputEnabled(const bool &enabled);
    /**
     * \brief Whether or not this device should be considered for output
     * @return True if the device should be considered for output
     */
    const bool &outputEnabled() const;

    /**
     * \brief Manually mark the device as supporting (or not) the given direction
     * @param direction The direction to mark as supported or otherwise
     * @param supportsDirection Whether the direction is supported or not
     */
    void setDeviceDirection(const DeviceDirection &direction, const bool &supportsDirection = true);
    /**
     * \brief Whether or not the device supports the given direction
     * @return True if the device supports the direction, false if not
     */
    bool supportsDirection(const DeviceDirection &direction) const;

    /**
     * \brief Set the acceptability state of the given list of notes to the state described by the given acceptance value
     * @param notes The list of notes whose state to set (values can be from 0 through 127, and will be clamped to those values if outside)
     * @param accepted If true, the notes will be marked as accepted by this device. If false, they will be marked as not accepted
     * @param setOthersOpposite If true, any note not in the list will be marked as the opposite value to what is given by accepted
     * @see setAcceptsNote(int, bool)
     */
    void setAcceptedNotes(const QList<int> notes, bool accepted = true, bool setOthersOpposite = false);
    /**
     * \brief Sets the acceptability state of the given note to the state described by the given acceptance value
     * Setting a note to be not accepted means that any event passed to be
     * written to the device's output will be checked against that list, and
     * if it is not accepted, it will not be written to the output.
     * @param note The note to set the acceptability state of
     * @param accepted If true, set the note as accepted. If false, they will be marked as not accepted
     */
    void setAcceptsNote(const int &note, bool accepted = true);

    /**
     * \brief Sets the amount by which notes sent to the device will be transposed
     * @note This happens during event writing, and values outside the range will be clamped rather than sent
     * @param transposeAmount The amount by which notes get transposed (default is 0, value will be clamped to +/- 127)
     */
    void setTransposeAmount(int transposeAmount = 0);

    /**
     * \brief Set the channels which this device will accept
     * Any note which arrives on a different channel will be moved to one of the given channels
     * There is no guarantee of an even spread here (in essence, an unaccepted channel will be
     * moved to the last channel in the accepted list, which might potentially result in clashes,
     * but is inexpensive to calculate, as well as consistent)
     * @param acceptedMidiChannels The midi channels this device will accept notes on
     */
    void setAcceptedMidiChannels(const QList<int> &acceptedMidiChannels);

    /**
     * \brief Set to true if this device should filter output sent to zynthian destinations by the event's channel
     * @param filterZynthianByChannel True if the device should filter before sending, false if not
     */
    void setFilterZynthianOutputByChannel(const bool &filterZynthianByChannel);
    /**
     * \brief Whether this device should filter output sent to zynthian destinations by the event's channel
     * @note This will force all events with a channel coming from this device to be routed specifically and explicitly to the related zynthian output, whatever the current track is
     * @return True if the device should filter events by matching the events channel to zynthian destinations' indices
     * @default false
     */
    bool filterZynthianOutputByChannel() const;

    /**
     * \brief Mark the device as being (or not being) some type or another
     * @param type The type of midi device (controller, synth, ...)
     * @param isType Whether or not the device is the given type
     */
    void setDeviceType(const DeviceType &type, const bool &isType = true);
    /**
     * \brief Whether or not the device is the given type of device
     * @return True if the device is the given type, false if not
     */
    bool deviceType(const DeviceType &type) const;

    /**
     * \brief Inform the device about the global channel set in webconf
     * This is used for translating messages from the external device's master channel
     * @param globalMaster The master channel set in webconf
     */
    void setZynthianMasterChannel(int globalMaster);

    /**
     * \brief Set the target track for the given midi channel (instructs MidiRouter to always deliver messages received on that channel)
     * Use this to lock the device's messages to always be sent to a given track, instead of the current one
     * @param midiChannel -1 will set the track target for all channel
     * @param sketchpadTrack -1 will make MidiRouter deliver the messages to the current track, any other value will be clamped to the sketchpad track range
     */
    void setMidiChannelTargetTrack(const int &midiChannel, const int &sketchpadTrack);
    /**
     * \brief The target track for a given midi channel
     * @param midiChannel The midi channel to get the target track for
     * @return The sketchpad track set for the given midi channel
     */
    int targetTrackForMidiChannel(const int &midiChannel) const;

    /**
     * \brief Mark whether or not we should retrieve events from a given list of channels
     * @param channels A list of midi channel indices (0 through 16, all other numbers will be ignored)
     * @param receive If true, events will be collected from this channel
     */
    void setReceiveChannels(const QList<int> &channels, const bool &receive);
    /**
     * \brief Whether or not MidiRouter should accept events from this channel
     */
    const bool &receiveChannel(const int &channel) const;
    /**
     * \brief Mark whether or not we should send events to a given list of channels
     * @param channels A list of midi channel indices (0 through 16, all other numbers will be ignored)
     * @param sendTo If true, events will be sent to this channel
     */
    void setSendToChannels(const QList<int> &channels, const bool &sendTo);
    /**
     * \brief Whether or not MidiRouter should send events to this channel
     */
    const bool &sendToChannel(const int &channel) const;

    /**
     * \brief Mark whether this device wants to have midi timecode events sent to it
     * @param sendTimecode True if midi timecode events should be sent, false if not
     */
    void setSendTimecode(const bool &sendTimecode);
    /**
     * \brief Whether or not MidiRouter should send midi timecode events to this device
     * @return True if midi timecode events should be sent, false if not
     */
    const bool &sendTimecode() const;
    /**
     * \brief Mark whether this device wants to have midi beat clock events sent to it
     * @param sendBeatClock True if midi beat clock events should be sent, false if not
     */
    void setSendBeatClock(const bool &sendBeatClock);
    /**
     * \brief Whether or not MidiRouter should send Midi Beat clock events to this device
     * @return True if midi beat clock events should be sent, false if not
     */
    const bool &sendBeatClock() const;
private:
    MidiRouterDevicePrivate *d{nullptr};
};
Q_DECLARE_OPERATORS_FOR_FLAGS(MidiRouterDevice::DeviceDirections)
Q_DECLARE_OPERATORS_FOR_FLAGS(MidiRouterDevice::DeviceTypes)
