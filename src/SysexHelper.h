#pragma once

#include "MidiRouterDevice.h"
#include "SysexIdentity.h"
#include "SysexMessage.h"

class SysexHelperPrivate;
class SysexHelper : public QObject {
    Q_OBJECT
    /**
     * \brief The SysEx channel this device is supposed to use
     * This channel is used by Universal SysEx messages to target a specific device in a chain
     * @minimum 0x00
     * @maximum 0x7F
     * @default 0x7F (instruction for devices to disregard the channel byte)
     */
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)
    /**
     * \brief An instance of the SysexIdentity class, or null if none has been successfully retrieved
     */
    Q_PROPERTY(QObject* identity READ identity NOTIFY identityChanged)
public:
    explicit SysexHelper(MidiRouterDevice * parent = nullptr);
    ~SysexHelper() override;

    /**
     * \brief Create a SysexMessage based on the given bytes
     * @note To clear the message from memory, you can call deleteLater() on it to schedule its removal
     * The list of bytes can be either integers, or hex-like strings. The integers must be
     * between 0 and 127, and the hex-like values similarly must be between 00 (or 0x00)
     * and 7F (or 0x7F, or 0x7f).
     * @param bytes The bytes to construct a message from (not including the start and end bytes 0xF0 and 0xF7, and what the settings describe)
     * @param messageSettings Which extras to automatically include (e.g. the manufacturer ID, device ID, a checksum...)
     * @return If the format of bytes was valid, a SysexMessage instance, otherwise null
     */
    Q_INVOKABLE SysexMessage *createMessage(QVariantList bytes, SysexMessage::MessageSettings messageSettings = SysexMessage::NoSetting);

    enum KnownMessage {
        ///@< The Universal SysEx message for setting the enabled state of the General MIDI mode of a sound module.
        ///   Requires a single extra field:
        ///   - Pass a list with a 0 in it to disable General MIDI (this will be the assumed value if nothing is passed)
        ///   - Pass a list with a 1 in it to enable General MIDI
        GMEnableMessage,
        ///@< The Universal Sysex message for setting the device's Master Volume
        ///   Requires either one or two extra fields:
        ///   - Pass a list with a single value to give the master volume, as a number from 0 through 16383 (which will be clamped to this range)
        ///   - Pass a list with two values to manually pass in the two segments (index 0 being bits 0 through 6, index 1 being bits 7 through 14, both will be clamped to the range 0 through 127)
        ///   - Any other size of extra fields will be interpreted as setting the volume to 0
        SetMasterVolumeMessage,
        ///@< The Universal SysEx message for requesting the device's identity
        ///   Sending this message will, if the request was successful, result in the identity property changing
        ///   If the request fails, nothing will happen (we have no way to detect such a failure)
        IdentityRequestMessage,
        ///@< The Universal SysEx message for requesting the dump of a given sample
        ///   Requires either one or two extra fields:
        ///   - Pass a list with a single value to give the sample position, as a number from 0 through 16383 (which will be clamped to this range)
        ///   - Pass a list with two values to manually pass in the two segments (index 0 being bits 0 through 6, index 1 being bits 7 through 14, both will be clamped to the range 0 through 127)
        ///   - Any other size of extra fields will be interpreted as requesting the sample at index 0
        SampleDumpRequestMessage,
        // Once we get to implementing the MIDI Sample Dump Standard, this gets a fair bit bigger
        // See this for details on MIDI SDS: http://midi.teragonaudio.com/tech/sds.htm
    };
    Q_ENUM(KnownMessage)
    /**
     * \brief Get a specific type of message
     * @param messageType The message you would like to get one of
     * @param extraFields The extra fields (dependent on the type, if the required fields are not present, we will assume zero bytes for those positions if required, or empty where not)
     * @return 
     */
    Q_INVOKABLE SysexMessage *createKnownMessage(KnownMessage messageType, const QVariantList &extraFields = {});

    enum DataAlignment {
        LeftJustified,
        RightJustified
    };
    Q_ENUM(DataAlignment)
    /**
     * \brief Convert an integer value to a list of MIDI 7-bit bytes for use in a message
     * @param value The value you wish to convert to midi bytes
     * @param byteCount The number of 7-bit MIDI bytes the number should be turned into (most commonly it seems to be two bytes)
     * @param bitSize How many bits the destination is supposed to use (for a two midi-byte value, this would usually be 2*7=14 bits, but for e.g. samples you might be fitting a 16 bit value into three bytes, meaning technically 21 bits of space)
     * @param alignment The alignment of the byte data (commonly right-aligned, though for example sample dump data has the data packet bytes left justified - this is most relevant when bytSize and byteCount don't match)
     * @return A list of 7-bit integer values, suitable for adding to a MIDI message
     */
    Q_INVOKABLE QVariantList numberToBytes(const int &value, const int &byteCount = 2, const int &bitSize = 14, const DataAlignment &alignment = RightJustified) const;
    /**
     * \brief Convert a position within a range of values directly to a number of bytes
     * @param position The position inside the range of values to convert (this will be clamped to between 0.0 through 1.0)
     * @param minimumValue The lowest value of the range (the value for position 0.0)
     * @param maximumValue The highest value of the range (the value for position 1.0)
     * @param byteCount The number of 7-bit MIDI bytes the number should be turned into (most commonly it seems to be two bytes)
     * @param alignment The alignment of the byte data (commonly right-aligned, though for example sample dump data has the data packet bytes left justified)
     * @return A list of 7-bit integer values, suitable for adding to a MIDI message
     */
    Q_INVOKABLE QVariantList positionToBytes(const float &position, const int &minimumValue, const int maximumValue, const int &byteCount = 2, const DataAlignment &alignment = RightJustified) const;
    /**
     * \brief Convert a number of MIDI 7-bit bytes from a message into an integer value
     * @param bytes The bytes you wish to convert into an integer
     * @param bitSize How many bits the value occupies within the bytes (for a two midi-byte value this would usually be 14, but for samples, you might find there is more space than the value actually takes up - this is most relevant when bytSize and byteCount don't match)
     * @param alignment The alignment of the bits within the byte data (commonly right-aligned, though for example sample dump data has the data packet bytes left justified)
     * @return The value represented by the bytes, or if any of the bytes are not valid 7 bit values (0x00 through 0x7F), the function returns 0
     */
    Q_INVOKABLE int bytesToNumber(const QVariantList &bytes, const int &bitSize = 14, const DataAlignment &alignment = RightJustified) const;

    /**
     * \brief Queues up the given message to be sent out as soon as possible
     * @note Once passed to this function, you should endeavour to not perform any further changes to the message (as this would result potentially in undefined behaviour and potentially even crashes)
     * @param message The message you wish to have sent out
     */
    Q_INVOKABLE void send(SysexMessage *message);

    /**
     * \brief Emitted after a message has been received by this device
     * @param message The received message
     */
    Q_SIGNAL void messageReceived(SysexMessage *message);

    int channel() const;
    void setChannel(const int &channel);
    Q_SIGNAL void channelChanged();

    QObject *identity() const;
    SysexIdentity *identityActual() const;
    Q_SIGNAL void identityChanged();
protected:
    friend class MidiRouterDevice;
    // Called by MidiRouterDevice when processing is begun, to write any scheduled messages and write them to the output buffer
    void process(void *outputBuffer) const;
    // Called by MidiRouterDevice during its input event processing, when a sysex message is encountered
    // Prerequisite: currentInputEvent must be a sysex message
    void handleInputEvent(const jack_midi_event_t &currentInputEvent) const;
    // Called by MidiRouterDevice in its similarly named function, ensuring we don't clog up the dsp process with ui related things
    void handlePostponedEvents();
private:
    SysexHelperPrivate *d{nullptr};
};
