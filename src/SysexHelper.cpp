#include "SysexHelper.h"
#include "MidiRing.h"

#include <QBitArray>

#include <jack/jack.h>
#include <jack/midiport.h>

#define SysexHelperMessageRingSize 512
class SysexHelperMessageRing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        bool processed{true};
        SysexMessage *message{nullptr};
    };
    explicit SysexHelperMessageRing() {
        Entry* entryPrevious{&ringData[SysexHelperMessageRingSize - 1]};
        for (quint64 i = 0; i < SysexHelperMessageRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~SysexHelperMessageRing() {
    }

    void write(SysexMessage *message) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location: SysEx Message object" << entry->message << ". This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->message = message;
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
    Entry ringData[SysexHelperMessageRingSize];
};

class SysexHelperPrivate {
public:
    SysexHelperPrivate(SysexHelper *q, MidiRouterDevice *device)
        : q(q)
        , device(device)
    {}
    ~SysexHelperPrivate() {
        qDeleteAll(createdMessages);
    }
    SysexHelper *q{nullptr};
    MidiRouterDevice *device{nullptr};
    int sysexChannel{0x7F};
    SysexIdentity *identity{nullptr};
    QList<SysexMessage*> createdMessages;
    SysexHelperMessageRing outputRing;
    MidiRing incomingEvents;
};

SysexHelper::SysexHelper(MidiRouterDevice* parent)
    : QObject(parent)
    , d(new SysexHelperPrivate(this, parent))
{
}

SysexHelper::~SysexHelper()
{
    delete d;
}

SysexMessage * SysexHelper::createMessage(QVariantList bytes, SysexMessage::MessageSettings messageSettings)
{
    SysexMessage *message = new SysexMessage(messageSettings, this);
    if(message->setBytes(bytes)) {
        connect(message, &QObject::destroyed, this, [this](QObject *obj){ 
            SysexMessage *message{qobject_cast<SysexMessage*>(obj)};
            if (message) {
                d->createdMessages.removeOne(message);
            }
        });
        d->createdMessages << message;
    } else {
        qDebug() << Q_FUNC_INFO << "Error setting bytes, error was:" << message->errorDescription();
        message->deleteLater();
        message = nullptr;
    }
    return message;
}

SysexMessage * SysexHelper::createKnownMessage(KnownMessage messageType, const QVariantList& extraFields)
{
    QVariantList bytes;
    SysexMessage::MessageSettings messageSettings;
    switch(messageType) {
        case GMEnableMessage:
            messageSettings.setFlag(SysexMessage::UniversaleNonRealtimeSetting, true);
            bytes = {d->sysexChannel, 0x09}; // The Universal SysEx message identifier for GM System Enable/Disable
            if (extraFields.length() > 0) {
                bytes << std::clamp(extraFields[0].toInt(), 0, 1);
            } else {
                bytes << 0x00;
            }
            break;
        case SetMasterVolumeMessage:
            messageSettings.setFlag(SysexMessage::UniversalRealtimeSetting, true);
            bytes = {d->sysexChannel, 0x04, 0x01}; // The Universal SysEx message identifier for the Master Volume
            if (extraFields.length() == 1) {
                bytes << numberToBytes(extraFields[0].toInt(), 2);
            } else if (extraFields.length() == 2) {
                bytes << std::clamp(extraFields[0].toInt(), 0, 127);
                bytes << std::clamp(extraFields[1].toInt(), 0, 127);
            } else {
                bytes << 0x00; bytes << 0x00;
            }
            break;
        case IdentityRequestMessage:
            messageSettings.setFlag(SysexMessage::UniversaleNonRealtimeSetting, true);
            bytes = {d->sysexChannel, 0x06, 0x01}; // The Universal SysEx message identifier for identity request
            break;
        case SampleDumpRequestMessage:
            messageSettings.setFlag(SysexMessage::UniversaleNonRealtimeSetting, true);
            bytes = {d->sysexChannel, 0x03}; // The Universal SysEx message identifier for sample dump request
            if (extraFields.length() == 1) {
                bytes << numberToBytes(extraFields[0].toInt(), 2);
            } else if (extraFields.length() == 2) {
                bytes << std::clamp(extraFields[0].toInt(), 0, 127);
                bytes << std::clamp(extraFields[1].toInt(), 0, 127);
            } else {
                bytes << 0x00; bytes << 0x00;
            }
            break;
    }
    return createMessage(bytes, messageSettings);
}

inline bool bitAtIndex(int number, int bitIndex) {
    return (number >> bitIndex) & 1;
}
inline int bitSetTo(int number, int bitIndex, bool setTo) {
    return (number & ~((int)1 << bitIndex)) | ((int)setTo << bitIndex);
}
QVariantList SysexHelper::numberToBytes(const int& value, const int& byteCount, const int &bitSize, const DataAlignment& alignment) const
{
    // Alright, so, it is entirely reasonable for you to drop in and say
    // Hey, this looks inefficient, i can do better.
    // If that is how you feel, i would very much like for you to do so!
    // Please submit a patch in which you change this to something less
    // made of spaghetti and hopes, and more of proper maths-knowing
    // engineer type code ;)
    // This is of course true of all of the codebase, but these in particular
    // heebies my personal jeebies, and i would welcome more eyes on them.
    const int destinationSize{byteCount * 7};
    QBitArray bitArray(destinationSize, false);
    const int valueSize{sizeof(value)};
    for (int originIndex = 0, destinationIndex = (alignment == RightJustified ? valueSize - bitSize : 0); destinationIndex < valueSize; ++originIndex, ++destinationIndex) {
        if (destinationIndex < destinationSize) {
            bitArray.setBit(destinationIndex, bitAtIndex(value, originIndex));
        }
    }
    QVariantList bytes;
    for (int byteIndex = 0; byteIndex < byteCount; ++byteIndex) {
        int byte{0};
        const int byteBitStart{byteIndex * 7};
        for (int bitIndex = 0; bitIndex < 7; ++bitIndex) {
            bitSetTo(byte, bitIndex, bitArray.testBit(byteBitStart + bitIndex));
        }
        bytes << QVariant::fromValue<int>(byte);
    }
    return bytes;
}

QVariantList SysexHelper::positionToBytes(const float& position, const int& minimumValue, const int maximumValue, const int& byteCount, const DataAlignment& alignment) const
{
    return numberToBytes(minimumValue + (position * (maximumValue - minimumValue)), byteCount, alignment);
}

int SysexHelper::bytesToNumber(const QVariantList& bytes, const int &bitSize, const DataAlignment& alignment) const
{
    const int originSize{bytes.count() * 7};
    int result{0};
    int byteBitStart{alignment == RightJustified ? originSize - bitSize : 0};
    for (const QVariant &byte : bytes) {
        const int actualByte{byte.toInt()};
        for (int bitIndex = 0; bitIndex < 7; ++bitIndex) {
            bitSetTo(result, byteBitStart + bitIndex, bitAtIndex(actualByte, bitIndex));
        }
        byteBitStart += 7;
    }
    return result;
}

void SysexHelper::send(SysexMessage* message)
{
    d->outputRing.write(message);
}

int SysexHelper::channel() const
{
    return d->sysexChannel;
}

void SysexHelper::setChannel(const int& channel)
{
    if (d->sysexChannel != channel) {
        d->sysexChannel = channel;
        Q_EMIT channelChanged();
    }
}

QObject * SysexHelper::identity() const
{
    return d->identity;
}

SysexIdentity * SysexHelper::identityActual() const
{
    return d->identity;
}

void SysexHelper::process(void* outputBuffer) const
{
    // Write all the messages written to the output ring by calling send() to the given output buffer
    while (d->outputRing.readHead->processed == false) {
        // qDebug() << Q_FUNC_INFO << "There is an unposted outgoing sysex message, let's post that" << d->outputRing.readHead->message << "into" << outputBuffer;
        if (d->outputRing.readHead->message) {
            const juce::MidiMessageMetadata &juceMessage{d->outputRing.readHead->message->juceMessage()};
            // qDebug() << Q_FUNC_INFO << "The juce message has" << juceMessage.numBytes << "bytes and describes itself as" << juceMessage.getMessage().getDescription().toRawUTF8();
            int errorCode = jack_midi_event_write(outputBuffer, 0,
                const_cast<jack_midi_data_t*>(juceMessage.data), // this might seems odd, but it's really only because juce's internal store is const here, and the data types are otherwise the same
                size_t(juceMessage.numBytes) // this changes signedness, but from a lesser space (int) to a larger one (unsigned long)
            );
            if (errorCode == -ENOBUFS) {
                // Then we have run out of space, and need to try again later. Assume sysex must be in order, and wait until the next round
                // qDebug() << Q_FUNC_INFO << "We have apparently run out of buffer space, and will need to wait for the next round...";
                break; // We explicitly do not mark the read head as having been read, which means the above is true
            } else if (errorCode == -EINVAL) {
                // This happens when there is either an invalid buffer that we're being asked to write to, or we are asked to write past the end of the buffer's frame size, or we are asked to write before the most recent event's time
                if (outputBuffer == nullptr) {
                    qDebug() << Q_FUNC_INFO << "Attempted to write to an null buffer, which will fail badly. We will drop this message.";
                } else {
                    qDebug() << Q_FUNC_INFO << "We have apparently been asked to write past the end of the buffer's length (but we are writing to time 0), or there are events in there already that have a later time (but how)?";
                }
            } else if (errorCode != 0) {
                qDebug() << Q_FUNC_INFO << "Some other error, what in the world is it, when we're only supposed (according to the docs) to get -ENOBUFFS, but also get -EINVAL sometimes?" << errorCode;
            }
            if (d->outputRing.readHead->message->deleteOnSend()) {
                d->outputRing.readHead->message->deleteLater();
                d->outputRing.readHead->message = nullptr;
            }
        }
        d->outputRing.markAsRead();
    }
}

void SysexHelper::handleInputEvent(const jack_midi_event_t& currentInputEvent) const
{
    // qDebug() << Q_FUNC_INFO << "Received input event, writing to ring";
    juce::MidiBuffer midiBuffer;
    midiBuffer.addEvent(currentInputEvent.buffer, currentInputEvent.size, 0);
    d->incomingEvents.write(midiBuffer);
}

void SysexHelper::handlePostponedEvents()
{
    // FIXME Handle chunked inputs (basically, we will need to have instructions from SysexMessage whether it is complete, or we need to keep reading into the same message... and then also have a way to abort the ongoing read... and a way to inform MidiRouterDevice that we are reading sysex... so, ongoingSysexRead field in the protected area for that?)
    // Convert the various incoming events into SysexMessage objects, and announce their existence to anybody who cares
    while (d->incomingEvents.readHead->processed == false) {
        // qDebug() << Q_FUNC_INFO << "Unprocessed input event found, handling...";
        const juce::MidiBuffer &midiBuffer{d->incomingEvents.readHead->buffer};
        for (const juce::MidiMessageMetadata &message : midiBuffer) {
            if (message.numBytes > 3 && message.data[0] == 0xF0 && message.data[message.numBytes - 1] == 0xF7) {
                // Super-double-checkery to ensure this is, in fact, a SysEx message
                bool isIdentityResponse{false};
                SysexMessage::MessageSettings messageSettings;
                if (message.numBytes > 5) {
                    // Then this might very well be a Universal SysEx message, so let's interpret that...
                    bool identifiedAsSysex{false};
                    const int subID{message.data[2]};
                    const int subID2{message.data[3]};
                    if (message.numBytes == 5 && subID == 0x09 && (subID2 == 0x00 || subID2 == 0x01)) {
                        // Universal SysEx: GM Enable
                        identifiedAsSysex = true;
                    } else if (message.numBytes == 7 && subID == 0x04 && subID2 == 0x01) {
                        // Universal SysEx: Master Volume
                        identifiedAsSysex = true;
                    } else if (message.numBytes == 5 && subID == 0x06 && subID2 == 0x01) {
                        // Universal SysEx: Identity Request
                        identifiedAsSysex = true;
                    } else if (message.numBytes == 16 && subID == 0x06 && subID2 == 0x02) {
                        // Universal SysEx: Identity Response
                        identifiedAsSysex = true;
                        isIdentityResponse = true;
                    }
                    if (identifiedAsSysex) {
                        // NOTE: Checking the channel must happen *after* we have identified this as a Universal SyxEx message, otherwise things are going to go weirdly for things that are not one such
                        const int sysexChannel{message.data[2]};
                        if (sysexChannel == 0x7F || sysexChannel == d->sysexChannel) {
                            // We are either supposed to disregard channel, or this is on our channel
                            const bool isRealTime{message.data[1] == 0x7F};
                            if (isRealTime) {
                                messageSettings.setFlag(SysexMessage::UniversalRealtimeSetting, true);
                            }
                            const bool isNonRealTime{message.data[1] == 0x7E};
                            if (isNonRealTime) {
                                messageSettings.setFlag(SysexMessage::UniversaleNonRealtimeSetting, true);
                            }
                        }
                    }
                }
                SysexMessage *sysexMessage = new SysexMessage(messageSettings, this);
                sysexMessage->beginOperation();
                const int bytesLength{message.numBytes - 2};
                sysexMessage->setBytesLength(bytesLength);
                for (int i = 0; i < bytesLength; ++i) {
                    sysexMessage->setByte(i, message.data[i - 1]);
                }
                sysexMessage->endOperation();
                Q_EMIT messageReceived(sysexMessage);
                if (isIdentityResponse) {
                    if (d->identity) {
                        d->identity->deleteLater();
                    }
                    d->identity = new SysexIdentity(sysexMessage, this);
                    Q_EMIT identityChanged();
                }
            }
        }
        d->incomingEvents.markAsRead();
    }
}
