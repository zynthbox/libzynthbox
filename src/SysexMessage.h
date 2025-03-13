#pragma once

#include "JUCEHeaders.h"
#include <QObject>
#include <QFlags>

class SysexMessagePrivate;
class SysexMessage : public QObject {
    Q_OBJECT
    /**
     * \brief The bytes which make up the message (not including the wrapper bytes as defined by messageSettings)
     * @note Setting this to a list of bytes where at least one byte is invalid will result in the change being rejected (and the old value retained)
     */
    Q_PROPERTY(QVariantList bytes READ bytes WRITE setBytes NOTIFY bytesChanged)
    /**
     * \brief Defines what will automatically be added to the message when sending
     * @note To set various settings here, 
     */
    Q_PROPERTY(MessageSettings settings READ settings NOTIFY settingsChanged)
    /**
     * \brief If this is set to true, the message will be automatically deleted once it has been sent
     * @default false
     */
    Q_PROPERTY(bool deleteOnSend READ deleteOnSend WRITE setDeleteOnSend NOTIFY deleteOnSendChanged)
public:
    enum MessageSetting {
        ///@< Adds only the start and end bytes (0xF0 and 0xF7) to the message
        NoSetting = 0,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and adds the device's manufacturer ID
        IncludeManufacturerIDSetting = 1,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and adds the device's family ID
        IncludeFamilyIDSetting = 2,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and adds the device's device ID
        IncludeDeviceIDSetting = 4,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and adds the data checksum at the end
        IncludeChecksumSetting = 8,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and marks the message as a Realtime Universal SysEx message (setting both UniversalRealtimeSetting and UniversaleNonRealtimeSetting will cause UniversalRealtimeSetting to take precedence)
        UniversalRealtimeSetting = 16,
        ///@< Adds the start and end bytes (0xF0 and 0xF7) to the message, and marks the message as a Non-Realtime Universal SysEx message (setting both UniversalRealtimeSetting and UniversaleNonRealtimeSetting will cause UniversalRealtimeSetting to take precedence)
        UniversaleNonRealtimeSetting = 32,
    };
    Q_ENUM(MessageSetting)
    Q_DECLARE_FLAGS(MessageSettings, MessageSetting)
    Q_FLAG(MessageSettings)

    explicit SysexMessage(const MessageSettings &settings, QObject *parent = nullptr);
    ~SysexMessage() override;

    QVariantList bytes() const;
    Q_SIGNAL void bytesChanged();
    /**
     * \brief Set the list of bytes to the given values
     * @note If the given list of new bytes contains any invalid bytes, the entire list will be rejected
     * @param bytes The list of byte-like values (must be either integers, or strings in a hex-like format, with or without the leading 0x, and strictly between 0 (0x00) through 127 (0x7F))
     * @return True if successful, otherwise false (the errorNumber and errorDescription functions will return informative data in this case)
     */
    Q_INVOKABLE bool setBytes(const QVariantList &bytes);
    /**
     * \brief Append the given list of values to the existing list
     * @note If the given list of new bytes contains any invalid bytes, the entire list will be rejected
     * @param bytes The list of byte-like values (must be either integers, or strings in a hex-like format, with or without the leading 0x, and strictly between 0 (0x00) through 127 (0x7F))
     * @return True if successful, otherwise false (the errorNumber and errorDescription functions will return informative data in this case)
     */
    Q_INVOKABLE bool appendBytes(const QVariantList &bytes);
    /**
     * \brief Set the byte at the given position to the given byte value
     * @param position The index of the byte in the bytes array to change to the new value (for negative values, it will count back from the last position (-1 being the last position, -2 being second to last, and so on); if the position is invalid, the list will be padded with 0x00 bytes to get to that position)
     * @param byte The new value to set on the given position (must be either integers, or strings in a hex-like format, with or without the leading 0x, and strictly between 0 (0x00) through 127 (0x7F))
     * @return True if successful, otherwise false (the errorNumber and errorDescription functions will return informative data in this case)
     */
    Q_INVOKABLE bool setByte(const int &position, const QVariant &byte);
    /**
     * \brief Set the length of the bytes list to the given amount, setting the new bytes to the given padding if required
     * @param length Set the bytes list to the given amount of entries
     * @param padding If the new length is longer than the previous length, the newly added bytes will be set to this value (clamped between to between 0 (0x00) and 127 (0x7F))
     */
    Q_INVOKABLE void setBytesLength(const int &length, const int &padding = 0);
    /**
     * \brief Convenience function which returns a list of the raw integer byte values
     * @return A list containing the raw integers for the bytes in the bytes list (which also contains integers, but wrapped in QVariants)
     */
    QList<int> bytesRaw() const;

    /**
     * \brief The error number relevant to the most recently performed function
     * @return The error number relevant to the most recently performed function (or 0 if there was no error)
     */
    Q_INVOKABLE const int &errorNumber() const;
    /**
     * \brief The description of the current error state
     * @return A human-readable description of the current error state
     * @see errorNumber()
     */
    Q_INVOKABLE const QString &errorDescription() const;

    MessageSettings settings() const;
    void setSettings(const MessageSettings &settings);
    Q_SIGNAL void settingsChanged();
    /**
     * \brief Set the state of a specific message setting to the given value
     * @param setting The setting that you wish to change the value of
     * @param enabled The new state of the setting
     */
    Q_INVOKABLE void setMessageSetting(const MessageSetting &setting, const bool &enabled = true);
    /**
     * \brief Retrieve the current value of a given setting
     * @param setting The setting that you wish to retrieve the value of
     * @return The value of the setting
     */
    Q_INVOKABLE bool checkMessageSetting(const MessageSetting &setting) const;

    bool deleteOnSend() const;
    void setDeleteOnSend(const bool &deleteOnSend);
    Q_SIGNAL void deleteOnSendChanged();

    /**
     * \brief Call this before starting any operations that will cause many data changes
     * Once this function has been called, the juce message updates will stop happening until endOperation() is called
     */
    Q_INVOKABLE void beginOperation();
    /**
     * \brief Call this once the operations which would cause many data changes have concluded
     * The result of this message is to cause the juce message to be rebuild
     */
    Q_INVOKABLE void endOperation();
    juce::MidiMessageMetadata &juceMessage() const;
private:
    SysexMessagePrivate *d{nullptr};
};
Q_DECLARE_OPERATORS_FOR_FLAGS(SysexMessage::MessageSettings)
