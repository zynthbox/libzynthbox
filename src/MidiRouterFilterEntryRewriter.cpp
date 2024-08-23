#include "MidiRouterFilterEntryRewriter.h"
#include "MidiRouterFilterEntry.h"

MidiRouterFilterEntryRewriter::MidiRouterFilterEntryRewriter(MidiRouterFilterEntry* parent)
    : QObject(parent)
    , m_bufferEvent{new jack_midi_event_t}
{
    m_bufferEvent->buffer = new jack_midi_data_t[3];
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

ZynthboxBasics::Part MidiRouterFilterEntryRewriter::cuiaPart() const
{
    return m_cuiaPart;
}

void MidiRouterFilterEntryRewriter::setCuiaPart(const ZynthboxBasics::Part& cuiaPart)
{
    if (m_cuiaPart != cuiaPart) {
        m_cuiaPart = cuiaPart;
        Q_EMIT cuiaPartChanged();
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
