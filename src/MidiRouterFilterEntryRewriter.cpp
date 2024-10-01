#include "MidiRouterFilterEntryRewriter.h"
#include "MidiRouterFilterEntry.h"

#include <QTimer>

MidiRouterFilterEntryRewriter::MidiRouterFilterEntryRewriter(MidiRouterFilterEntry* parent)
    : QObject(parent)
    , m_bufferEvent{new jack_midi_event_t}
{
    m_bufferEvent->buffer = new jack_midi_data_t[3];
    // During loading, this is likely to get hit quite a lot, so let's ensure we throttle it, make it a bit lighter
    QTimer *descriptionThrottle = new QTimer(this);
    descriptionThrottle->setInterval(0);
    descriptionThrottle->setSingleShot(true);
    descriptionThrottle->callOnTimeout(this, &MidiRouterFilterEntryRewriter::descripionChanged);
    connect(this, &MidiRouterFilterEntryRewriter::typeChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byteSizeChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte1Changed, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte1AddChannelChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte2Changed, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte2AddChannelChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte3Changed, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::byte3AddChannelChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::cuiaEventChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::cuiaTrackChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::cuiaSlotChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntryRewriter::cuiaValueChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
}

MidiRouterFilterEntryRewriter::~MidiRouterFilterEntryRewriter()
{
    delete m_bufferEvent->buffer;
    delete m_bufferEvent;
}

MidiRouterFilterEntryRewriter::RuleType MidiRouterFilterEntryRewriter::type() const
{
    return m_type;
}

void MidiRouterFilterEntryRewriter::setType(const RuleType& type)
{
    if (m_type != type) {
        m_type = type;
        Q_EMIT typeChanged();
    }
}

MidiRouterFilterEntryRewriter::EventSize MidiRouterFilterEntryRewriter::byteSize() const
{
    return m_byteSize;
}

void MidiRouterFilterEntryRewriter::setByteSize(const EventSize& byteSize)
{
    if (m_byteSize != byteSize) {
        m_byteSize = byteSize;
        Q_EMIT byteSizeChanged();
    }
}

MidiRouterFilterEntryRewriter::EventByte MidiRouterFilterEntryRewriter::byte1() const
{
    return m_bytes[0];
}

void MidiRouterFilterEntryRewriter::setByte1(const EventByte& byte1)
{
    if (m_bytes[0] != byte1) {
        m_bytes[0] = byte1;
        Q_EMIT byte1Changed();
    }
}

bool MidiRouterFilterEntryRewriter::byte1AddChannel() const
{
    return m_bytesAddChannel[0];
}

void MidiRouterFilterEntryRewriter::setByte1AddChannel(const bool& byte1AddChannel)
{
    if (m_bytesAddChannel[0] != byte1AddChannel) {
        m_bytesAddChannel[0] = byte1AddChannel;
        Q_EMIT byte1AddChannelChanged();
    }
}

MidiRouterFilterEntryRewriter::EventByte MidiRouterFilterEntryRewriter::byte2() const
{
    return m_bytes[1];
}

void MidiRouterFilterEntryRewriter::setByte2(const EventByte& byte2)
{
    if (m_bytes[1] != byte2) {
        m_bytes[1] = byte2;
        Q_EMIT byte2Changed();
    }
}

bool MidiRouterFilterEntryRewriter::byte2AddChannel() const
{
    return m_bytesAddChannel[1];
}

void MidiRouterFilterEntryRewriter::setByte2AddChannel(const bool& byte2AddChannel)
{
    if (m_bytesAddChannel[1] != byte2AddChannel) {
        m_bytesAddChannel[1] = byte2AddChannel;
        Q_EMIT byte2AddChannelChanged();
    }
}

MidiRouterFilterEntryRewriter::EventByte MidiRouterFilterEntryRewriter::byte3() const
{
    return m_bytes[2];
}

void MidiRouterFilterEntryRewriter::setByte3(const EventByte& byte3)
{
    if (m_bytes[2] != byte3) {
        m_bytes[2] = byte3;
        Q_EMIT byte3Changed();
    }
}

bool MidiRouterFilterEntryRewriter::byte3AddChannel() const
{
    return m_bytesAddChannel[2];
}

void MidiRouterFilterEntryRewriter::setByte3AddChannel(const bool& byte3AddChannel)
{
    if (m_bytesAddChannel[2] != byte3AddChannel) {
        m_bytesAddChannel[2] = byte3AddChannel;
        Q_EMIT byte3AddChannelChanged();
    }
}

CUIAHelper::Event MidiRouterFilterEntryRewriter::cuiaEvent() const
{
    return m_cuiaEvent;
}

void MidiRouterFilterEntryRewriter::setCuiaEvent(const CUIAHelper::Event& cuiaEvent)
{
    if (m_cuiaEvent != cuiaEvent) {
        m_cuiaEvent = cuiaEvent;
        Q_EMIT cuiaEventChanged();
    }
}

ZynthboxBasics::Track MidiRouterFilterEntryRewriter::cuiaTrack() const
{
    return m_cuiaTrack;
}

void MidiRouterFilterEntryRewriter::setCuiaTrack(const ZynthboxBasics::Track& cuiaTrack)
{
    if (m_cuiaTrack != cuiaTrack) {
        m_cuiaTrack = cuiaTrack;
        Q_EMIT cuiaTrackChanged();
    }
}

ZynthboxBasics::Slot MidiRouterFilterEntryRewriter::cuiaSlot() const
{
    return m_cuiaSlot;
}

void MidiRouterFilterEntryRewriter::setCuiaSlot(const ZynthboxBasics::Slot& cuiaSlot)
{
    if (m_cuiaSlot != cuiaSlot) {
        m_cuiaSlot = cuiaSlot;
        Q_EMIT cuiaSlotChanged();
    }
}

MidiRouterFilterEntryRewriter::ValueSpecifier MidiRouterFilterEntryRewriter::cuiaValue() const
{
    return m_cuiaValue;
}

void MidiRouterFilterEntryRewriter::setCuiaValue(const ValueSpecifier& cuiaValue)
{
    if (m_cuiaValue != cuiaValue) {
        m_cuiaValue = cuiaValue;
        Q_EMIT cuiaValueChanged();
    }
}

QString MidiRouterFilterEntryRewriter::description() const
{
    QString description{"Invalid Rule Type"};
    if (m_type == MidiRouterFilterEntryRewriter::TrackRule) {
        const MidiRouterFilterEntry* parentEntry{qobject_cast<const MidiRouterFilterEntry*>(parent())};
        int byteSizeActual{m_byteSize};
        if (m_byteSize == EventSizeSame) {
            byteSizeActual = parentEntry->requiredBytes();
        }
        switch (byteSizeActual) {
            case EventSize1:
                if (m_bytes[0] == OriginalByte1) {
                    description = QString{"Send the matched message"};
                } else {
                    description = QString{"Send %1"}.arg(juce::MidiMessage(m_bytes[0]).getDescription().toRawUTF8());
                }
                if (m_bytesAddChannel[0]) {
                    description = QString{"%1, add matched message channel"}.arg(description);
                }
                break;
            case EventSize2:
                if (m_bytes[0] == OriginalByte1 && m_bytes[1] == OriginalByte2) {
                    description = QString{"Send the matched message"};
                } else if (m_bytes[0] == OriginalByte1) {
                    description = QString{"Send the matched message, setting byte 2 to %2"}.arg(int(m_bytes[1]));
                } else if (m_bytes[0] == OriginalByte1) {
                    description = QString{"Send %1, with byte 2 from the matched message"}.arg(juce::MidiMessage(m_bytes[0], 0).getDescription().toRawUTF8());
                } else {
                    description = QString{"Send %1"}.arg(juce::MidiMessage(m_bytes[0], m_bytes[1]).getDescription().toRawUTF8());
                }
                if (m_bytesAddChannel[0] && m_bytesAddChannel[1]) {
                    description = QString{"%1, add matched message channel to bytes 1 and 2"}.arg(description);
                } else if (m_bytesAddChannel[0]) {
                    description = QString{"%1, add matched message channel to byte 1"}.arg(description);
                } else if (m_bytesAddChannel[1]) {
                    description = QString{"%1, add matched message channel to byte 2"}.arg(description);
                }
                break;
            case EventSize3:
                if (m_bytes[0] == OriginalByte1 && m_bytes[1] == OriginalByte2 && m_bytes[2] == OriginalByte3) {
                    description = QString{"Send the matched message"};
                } else if (m_bytes[0] == OriginalByte1 && m_bytes[1] == OriginalByte2) {
                    description = QString{"Send the matched message, setting byte 3 to %2"}.arg(juce::MidiMessage(m_bytes[0]).getDescription().toRawUTF8()).arg(int(m_bytes[2]));
                } else if (m_bytes[0] == OriginalByte1 && m_bytes[2] == OriginalByte3) {
                    description = QString{"Send the matched message, setting byte 2 to %2"}.arg(juce::MidiMessage(m_bytes[0]).getDescription().toRawUTF8()).arg(int(m_bytes[1]));
                } else if (m_bytes[1] == OriginalByte1 && m_bytes[2] == OriginalByte3) {
                    description = QString{"Send %1, with bytes 2 and 3 from the matched message"}.arg(juce::MidiMessage(m_bytes[0], 0, 0).getDescription().toRawUTF8());
                } else if (m_bytes[1] == OriginalByte1) {
                    description = QString{"Send %1, with byte 2 from the matched message"}.arg(juce::MidiMessage(m_bytes[0], 0, m_bytes[2]).getDescription().toRawUTF8());
                } else if (m_bytes[2] == OriginalByte1) {
                    description = QString{"Send %1, with byte 3 from the matched message"}.arg(juce::MidiMessage(m_bytes[0], m_bytes[1], 0).getDescription().toRawUTF8()).arg(int(m_bytes[2]));
                } else {
                    description = QString{"Send %1"}.arg(juce::MidiMessage(m_bytes[0], m_bytes[1], m_bytes[2]).getDescription().toRawUTF8());
                }
                if (m_bytesAddChannel[0] && m_bytesAddChannel[1] && m_bytesAddChannel[2]) {
                    description = QString{"%1, add matched message channel to all three bytes"}.arg(description);
                } else if (m_bytesAddChannel[0] && m_bytesAddChannel[1]) {
                    description = QString{"%1, add matched message channel to bytes 1 and 2"}.arg(description);
                } else if (m_bytesAddChannel[0] && m_bytesAddChannel[2]) {
                    description = QString{"%1, add matched message channel to bytes 1 and 3"}.arg(description);
                } else if (m_bytesAddChannel[0] && m_bytesAddChannel[2]) {
                    description = QString{"%1, add matched message channel to bytes 1 and 3"}.arg(description);
                } else if (m_bytesAddChannel[0]) {
                    description = QString{"%1, add matched message channel to byte 1"}.arg(description);
                } else if (m_bytesAddChannel[1]) {
                    description = QString{"%1, add matched message channel to byte 2"}.arg(description);
                } else if (m_bytesAddChannel[2]) {
                    description = QString{"%1, add matched message channel to byte 3"}.arg(description);
                }
                break;
        }
    } else if (m_type == MidiRouterFilterEntryRewriter::UIRule) {
        description = CUIAHelper::instance()->describe(m_cuiaEvent, m_cuiaTrack, m_cuiaSlot, m_cuiaValue);
    }
    return description;
}
