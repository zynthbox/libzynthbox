/*
 * Copyright (C) 2025 Dan Leinir Turthra Jensen <admin@leinir.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "SysexMessage.h"
#include "SysexHelper.h"

class SysexMessagePrivate {
public:
    SysexMessagePrivate(SysexMessage *q, const SysexMessage::MessageSettings &settings)
        : q(q)
        , settings(settings)
    {}
    SysexMessage *q{nullptr};
    SysexMessage::MessageSettings settings;
    SysexHelper *parent{nullptr};

    QVariantList bytes;
    QList<int> bytesRaw;

    int errorNumber{0};
    QString errorDescription;

    bool deleteOnSend{false};
    juce::MidiMessageMetadata juceMessage;
    uint8* juceData{nullptr};
    size_t juceDataSize{0};

    bool operationOngoing{false};
    void updateJuceMessage() {
        if (operationOngoing == false) {
            SysexIdentity *identity{parent->identityActual()};
            // Our initial buffer size is the number of bytes, plus the first and last bytes
            size_t totalByteCount{size_t(bytes.length()) + 2};
            // Test all the various flags to make sure we end up with the correct number of bytes
            if (settings.testFlag(SysexMessage::UniversalRealtimeSetting) || settings.testFlag(SysexMessage::UniversaleNonRealtimeSetting)) {
                totalByteCount += 1;
            }
            if (settings.testFlag(SysexMessage::IncludeManufacturerIDSetting)) {
                totalByteCount += size_t(identity->manufacturerIdRaw().count());
            }
            if (settings.testFlag(SysexMessage::IncludeFamilyIDSetting)) {
                totalByteCount += size_t(identity->familyIdRaw().count());
            }
            if (settings.testFlag(SysexMessage::IncludeDeviceIDSetting)) {
                totalByteCount += size_t(identity->modelIdRaw().count());
            }
            if (settings.testFlag(SysexMessage::IncludeChecksumSetting)) {
                totalByteCount += 1;
            }
            uint8* newData = juceData;
            // If we have fundamentally different things going on, create a new buffer for our bytes
            if (totalByteCount != juceDataSize) {
                newData = new uint8[totalByteCount];
            }
            // Write the message data into the buffer given the various options we've got set
            int position{0};
            newData[position] = 0xF0;
            ++position;
            if (settings.testFlag(SysexMessage::UniversalRealtimeSetting)) {
                newData[position] = 0x7F;
                ++position;
            } else if (settings.testFlag(SysexMessage::UniversaleNonRealtimeSetting)) {
                newData[position] = 0x7E;
                ++position;
            }
            if (settings.testFlag(SysexMessage::IncludeManufacturerIDSetting)) {
                const QList<int> manufacturerIdBytes = identity->manufacturerIdRaw();
                for (int i = 0; manufacturerIdBytes.count(); ++i) {
                    newData[position] = manufacturerIdBytes[i];
                    ++position;
                }
            }
            if (settings.testFlag(SysexMessage::IncludeFamilyIDSetting)) {
                const QList<int> familyIdBytes = identity->familyIdRaw();
                for (int i = 0; familyIdBytes.count(); ++i) {
                    newData[position] = familyIdBytes[i];
                    ++position;
                }
            }
            if (settings.testFlag(SysexMessage::IncludeDeviceIDSetting)) {
                const QList<int> modelIdBytes = identity->modelIdRaw();
                for (int i = 0; modelIdBytes.count(); ++i) {
                    newData[position] = modelIdBytes[i];
                    ++position;
                }
            }
            for (const int &byte : bytesRaw) {
                newData[position] = byte;
                ++position;
            }
            // Checksum goes at the end, just before the sysex-end field
            if (settings.testFlag(SysexMessage::IncludeChecksumSetting)) {
                int calculatedChecksum{0};
                // For sample data, this would be an XOR on all bytes up to this point, not including the sysex start byte (so all bytes from index 1 through (position - 1))
                // TODO Implement checksum calculation (we will need this for MIDI SDS support)
                newData[position] = calculatedChecksum;
                ++position;
            }
            newData[position] = 0xF7;
            // If we have created a new buffer, replace that, and the message
            if (totalByteCount != juceDataSize) {
                if (juceData != nullptr) {
                    delete juceData;
                }
                // Assign the new juce buffer
                juceData = newData;
                juceDataSize = totalByteCount;
                juceMessage = juce::MidiMessageMetadata(juceData, totalByteCount, 0);
            }
        }
    }

    int validateByteValue(const QVariant &byte, bool *byteValid, int position = -1) {
        int byteValue{0};
        *byteValid = true;
        // TODO When updating to Qt 6, this all wants to turn into checking against QMetaType::Type instead
        const QVariant::Type &byteType{byte.type()};
        if (byteType == QVariant::Int || byteType == QVariant::Double || byteType == QVariant::UInt || byteType == QVariant::LongLong || byteType == QVariant::ULongLong) {
            byteValue = byte.toInt();
            if (byteValue < 0 || byteValue > 127) {
                errorNumber = -2;
                if (position == -1) {
                    errorDescription = QString("The value is not between 0 and 127");
                } else {
                    errorDescription = QString("The entry at position %1 is not between 0 and 127").arg(position);
                }
                *byteValid = false;
            }
        } else if (byteType == QVariant::String) {
            bool ok{false};
            byteValue = byte.toString().toInt(&ok, 16);
            if (ok == false || (byteValue < 0 || byteValue > 127)) {
                errorNumber = -3;
                if (position == -1) {
                    errorDescription = QString("The value is not a valid hexadecimal value (accepted formats are 0x## or ##): %1").arg(byte.toString());
                } else {
                    errorDescription = QString("The value is not a valid hexadecimal value (accepted formats are 0x## or ##): %2").arg(position).arg(byte.toString());
                }
                *byteValid = false;
            } else if (byteValue < 0 || byteValue > 127) {
                errorNumber = -4;
                if (position == -1) {
                    errorDescription = QString("The value is not a hexadecimal value between 0x00 and 0x7F: %1").arg(byte.toString());
                } else {
                    errorDescription = QString("The entry at position %1 is not a hexadecimal value between 0x00 and 0x7F: %2").arg(position).arg(byte.toString());
                }
                *byteValid = false;
            }
        } else {
            errorNumber = -1;
            if (position == -1) {
                errorDescription = QString("The value is not a valid integer or hexadecimal value (accepted formats are 0x## or ##): %1 of data type %2").arg(byte.toString()).arg(byte.typeName());
            } else {
                errorDescription = QString("The entry at position %1 is not a valid integer or hexadecimal value (accepted formats are 0x## or ##): %2 of data type %3").arg(position).arg(byte.toString()).arg(byte.typeName());
            }
            *byteValid = false;
        }
        return byteValue;
    }
};

SysexMessage::SysexMessage(const MessageSettings& settings, QObject* parent)
    : QObject(parent)
    , d(new SysexMessagePrivate(this, settings))
{
    d->parent = qobject_cast<SysexHelper*>(parent);
    connect(this, &SysexMessage::bytesChanged, this, [this](){ d->updateJuceMessage(); });
    connect(this, &SysexMessage::settingsChanged, this, [this](){ d->updateJuceMessage(); });
}

SysexMessage::~SysexMessage()
{
    delete d;
}

QVariantList SysexMessage::bytes() const
{
    return d->bytes;
}

QList<int> SysexMessage::bytesRaw() const
{
    return d->bytesRaw;
}

bool SysexMessage::setBytes(const QVariantList& bytes)
{
    bool allBytesValid{true};
    QList<int> newBytes;
    // Run through all the entries in the new bytes, and actually fetch out the data, ensuring that each byte is a valid integer-like, and within tolerances (0-127)
    int position{0};
    for (const QVariant &byte : bytes) {
        int byteValue{d->validateByteValue(byte, &allBytesValid, position)};
        if (allBytesValid == false) {
            break;
        }
        newBytes << byteValue;
        ++position;
    }
    if (allBytesValid) {
        d->errorNumber = 0;
        d->errorDescription = "";
        d->bytesRaw = newBytes;
        QVariantList publicNewBytes;
        for (const int &byte : newBytes) {
            publicNewBytes << QVariant::fromValue<int>(byte);
        }
        d->bytes = publicNewBytes;
        Q_EMIT bytesChanged();
    }
    return allBytesValid;
}

bool SysexMessage::appendBytes(const QVariantList& bytes)
{
    bool allBytesValid{true};
    QList <int> newBytes;
    // Run through all the entries in the new bytes, and actually fetch out the data, ensuring that each byte is a valid integer-like, and within tolerances (0-127)
    int position{0};
    for (const QVariant &byte : bytes) {
        int byteValue{d->validateByteValue(byte, &allBytesValid, position)};
        if (allBytesValid == false) {
            break;
        }
        newBytes << byteValue;
        ++position;
    }
    if (allBytesValid) {
        d->errorNumber = 0;
        d->errorDescription = "";
        for (const int &byte : newBytes) {
            d->bytesRaw = newBytes;
            d->bytes << QVariant::fromValue<int>(byte);
        }
        Q_EMIT bytesChanged();
    }
    return allBytesValid;
}

bool SysexMessage::setByte(const int& position, const QVariant& byte)
{
    bool byteValid{true};
    int actualPosition{position};
    if (position < 0) {
        actualPosition = d->bytesRaw.length() - std::clamp(position, -d->bytesRaw.length(), -1);
    } else if (position > d->bytesRaw.length() - 1) {
        setBytesLength(position + 1);
    }
    const int byteValue{d->validateByteValue(byte, &byteValid)};
    if (byteValid) {
        d->bytesRaw[actualPosition] = byteValue;
        d->bytes[actualPosition] = QVariant::fromValue<int>(byteValue);
    }
    return byteValid;
}

void SysexMessage::setBytesLength(const int& length, const int& padding)
{
    const int oldLength{d->bytesRaw.length()};
    const int actualPadding{std::clamp(padding, 0, 127)};
    if (oldLength != length) {
        if (oldLength < length) {
            d->bytes.reserve(length);
            d->bytesRaw.reserve(length);
            for (int i = oldLength - 1; i < length; ++i) {
                d->bytes << QVariant::fromValue<int>(actualPadding);
                d->bytesRaw << actualPadding;
            }
        } else if (length == 0) {
            d->bytes = {};
            d->bytesRaw = {};
        } else {
            for (int i = oldLength - 1; i < length; ++i) {
                d->bytes.removeLast();
                d->bytesRaw.removeLast();
            }
        }
        Q_EMIT bytesChanged();
    }
}

const int & SysexMessage::errorNumber() const
{
    return d->errorNumber;
}

const QString & SysexMessage::errorDescription() const
{
    return d->errorDescription;
}

SysexMessage::MessageSettings SysexMessage::settings() const
{
    return d->settings;
}

void SysexMessage::setSettings(const MessageSettings& settings)
{
    if (d->settings != settings) {
        d->settings = settings;
        Q_EMIT settingsChanged();
    }
}

void SysexMessage::setMessageSetting(const MessageSetting& setting, const bool& enabled)
{
    if (d->settings.testFlag(setting) != enabled) {
        d->settings.setFlag(setting, enabled);
        Q_EMIT settingsChanged();
    }
}

bool SysexMessage::checkMessageSetting(const MessageSetting& setting) const
{
    return d->settings.testFlag(setting);
}

bool SysexMessage::deleteOnSend() const
{
    return d->deleteOnSend;
}

void SysexMessage::setDeleteOnSend(const bool& deleteOnSend)
{
    if (d->deleteOnSend != deleteOnSend) {
        d->deleteOnSend = deleteOnSend;
        Q_EMIT deleteOnSendChanged();
    }
}

void SysexMessage::beginOperation()
{
    d->operationOngoing = true;
}

void SysexMessage::endOperation()
{
    d->operationOngoing = false;
    d->updateJuceMessage();
}

juce::MidiMessageMetadata & SysexMessage::juceMessage() const
{
    return d->juceMessage;
}
