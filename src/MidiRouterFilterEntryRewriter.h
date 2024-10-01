#pragma once

#include "MidiRouterDevice.h"

#include "CUIAHelper.h"

class MidiRouterFilterEntry;
/**
 * \brief A rule which defines how to interpret an incoming midi event for writing to an output buffer on an input filter, or an incoming cuia event in case of output filters
 * For input filters:
 * The default rule is to perform no true rewrite, and simply pass the event through unchanged
 * In other words: A rule of type Track, where all bytes are set to be their original values in an event of the same size as the input event
 *
 * For output filters:
 * Only size and byte values are relevant, as output rules define which messages are sent based on a cuia event (so rule type is irrelevant here)
 * The AddChannel toggles will add the incoming cuia event's track index to that byte (converting the CurrentTrack value to the actual track index)
 * To use the value on an event byte, use OriginalByte3 (track and slot are bytes 1 and 2 respectively, but those aren't likely to be the most useful)
 * EventSizeSame and EventSize3 are considered synonymous for output filter rules
 */
class MidiRouterFilterEntryRewriter : public QObject
{
    Q_OBJECT
    /**
     * \brief Whether this rule should result in a midi event (a Track rule) or a callable ui action event (a UI rule)
     */
    Q_PROPERTY(MidiRouterFilterEntryRewriter::RuleType type READ type WRITE setType NOTIFY typeChanged)
    /**
     * \brief For Track rules, how many bytes should be in the output event
     */
    Q_PROPERTY(MidiRouterFilterEntryRewriter::EventSize byteSize READ byteSize WRITE setByteSize NOTIFY byteSizeChanged)
    /**
     * \brief For Track rules, what byte 1 should be in the output event
     */
    Q_PROPERTY(MidiRouterFilterEntryRewriter::EventByte byte1 READ byte1 WRITE setByte1 NOTIFY byte1Changed)
    /**
     * \brief If set to true, this will add the midi channel value (if any) to byte 1
     */
    Q_PROPERTY(bool byte1AddChannel READ byte1AddChannel WRITE setByte1AddChannel NOTIFY byte1AddChannelChanged)
    /**
     * \brief For Track rules, what byte 2 should be in the output event
     */
    Q_PROPERTY(MidiRouterFilterEntryRewriter::EventByte byte2 READ byte2 WRITE setByte2 NOTIFY byte2Changed)
    /**
     * \brief If set to true, this will add the midi channel value (if any) to byte 2
     */
    Q_PROPERTY(bool byte2AddChannel READ byte2AddChannel WRITE setByte2AddChannel NOTIFY byte2AddChannelChanged)
    /**
     * \brief For Track rules, what byte 3 should be in the output event
     */
    Q_PROPERTY(MidiRouterFilterEntryRewriter::EventByte byte3 READ byte3 WRITE setByte3 NOTIFY byte3Changed)
    /**
     * \brief If set to true, this will add the midi channel value (if any) to byte 2
     */
    Q_PROPERTY(bool byte3AddChannel READ byte3AddChannel WRITE setByte3AddChannel NOTIFY byte3AddChannelChanged)
    /**
     * \brief For UI rules, which callable ui action should be performed when this event is encountered
     */
    Q_PROPERTY(CUIAHelper::Event cuiaEvent READ cuiaEvent WRITE setCuiaEvent NOTIFY cuiaEventChanged)
    Q_PROPERTY(ZynthboxBasics::Track cuiaTrack READ cuiaTrack WRITE setCuiaTrack NOTIFY cuiaTrackChanged)
    Q_PROPERTY(ZynthboxBasics::Slot cuiaSlot READ cuiaSlot WRITE setCuiaSlot NOTIFY cuiaSlotChanged)
    Q_PROPERTY(MidiRouterFilterEntryRewriter::ValueSpecifier cuiaValue READ cuiaValue WRITE setCuiaValue NOTIFY cuiaValueChanged)

    /**
     * \brief A human-readable description of the filter entry
     */
    Q_PROPERTY(QString description READ description NOTIFY descripionChanged)
public:
    explicit MidiRouterFilterEntryRewriter(MidiRouterFilterEntry *parent = nullptr);
    ~MidiRouterFilterEntryRewriter() override;

    enum RuleType {
        TrackRule = 0,
        UIRule = 1,
    };
    Q_ENUM(RuleType)

    enum EventSize {
        EventSizeSame = -1,
        EventSize1 = 1,
        EventSize2 = 2,
        EventSize3 = 3,
    };
    Q_ENUM(EventSize)

    enum EventByte {
        OriginalByte1 = -1,
        OriginalByte2 = -2,
        OriginalByte3 = -3,
        ExplicitByte0 = 0,
        ExplicitByte1 = 1,
        ExplicitByte2 = 2,
        ExplicitByte3 = 3,
        ExplicitByte4 = 4,
        ExplicitByte5 = 5,
        ExplicitByte6 = 6,
        ExplicitByte7 = 7,
        ExplicitByte8 = 8,
        ExplicitByte9 = 9,
        ExplicitByte10 = 10,
        ExplicitByte11 = 11,
        ExplicitByte12 = 12,
        ExplicitByte13 = 13,
        ExplicitByte14 = 14,
        ExplicitByte15 = 15,
        ExplicitByte16 = 16,
        ExplicitByte17 = 17,
        ExplicitByte18 = 18,
        ExplicitByte19 = 19,
        ExplicitByte20 = 20,
        ExplicitByte21 = 21,
        ExplicitByte22 = 22,
        ExplicitByte23 = 23,
        ExplicitByte24 = 24,
        ExplicitByte25 = 25,
        ExplicitByte26 = 26,
        ExplicitByte27 = 27,
        ExplicitByte28 = 28,
        ExplicitByte29 = 29,
        ExplicitByte30 = 30,
        ExplicitByte31 = 31,
        ExplicitByte32 = 32,
        ExplicitByte33 = 33,
        ExplicitByte34 = 34,
        ExplicitByte35 = 35,
        ExplicitByte36 = 36,
        ExplicitByte37 = 37,
        ExplicitByte38 = 38,
        ExplicitByte39 = 39,
        ExplicitByte40 = 40,
        ExplicitByte41 = 41,
        ExplicitByte42 = 42,
        ExplicitByte43 = 43,
        ExplicitByte44 = 44,
        ExplicitByte45 = 45,
        ExplicitByte46 = 46,
        ExplicitByte47 = 47,
        ExplicitByte48 = 48,
        ExplicitByte49 = 49,
        ExplicitByte50 = 50,
        ExplicitByte51 = 51,
        ExplicitByte52 = 52,
        ExplicitByte53 = 53,
        ExplicitByte54 = 54,
        ExplicitByte55 = 55,
        ExplicitByte56 = 56,
        ExplicitByte57 = 57,
        ExplicitByte58 = 58,
        ExplicitByte59 = 59,
        ExplicitByte60 = 60,
        ExplicitByte61 = 61,
        ExplicitByte62 = 62,
        ExplicitByte63 = 63,
        ExplicitByte64 = 64,
        ExplicitByte65 = 65,
        ExplicitByte66 = 66,
        ExplicitByte67 = 67,
        ExplicitByte68 = 68,
        ExplicitByte69 = 69,
        ExplicitByte70 = 70,
        ExplicitByte71 = 71,
        ExplicitByte72 = 72,
        ExplicitByte73 = 73,
        ExplicitByte74 = 74,
        ExplicitByte75 = 75,
        ExplicitByte76 = 76,
        ExplicitByte77 = 77,
        ExplicitByte78 = 78,
        ExplicitByte79 = 79,
        ExplicitByte80 = 80,
        ExplicitByte81 = 81,
        ExplicitByte82 = 82,
        ExplicitByte83 = 83,
        ExplicitByte84 = 84,
        ExplicitByte85 = 85,
        ExplicitByte86 = 86,
        ExplicitByte87 = 87,
        ExplicitByte88 = 88,
        ExplicitByte89 = 89,
        ExplicitByte90 = 90,
        ExplicitByte91 = 91,
        ExplicitByte92 = 92,
        ExplicitByte93 = 93,
        ExplicitByte94 = 94,
        ExplicitByte95 = 95,
        ExplicitByte96 = 96,
        ExplicitByte97 = 97,
        ExplicitByte98 = 98,
        ExplicitByte99 = 99,
        ExplicitByte100 = 100,
        ExplicitByte101 = 101,
        ExplicitByte102 = 102,
        ExplicitByte103 = 103,
        ExplicitByte104 = 104,
        ExplicitByte105 = 105,
        ExplicitByte106 = 106,
        ExplicitByte107 = 107,
        ExplicitByte108 = 108,
        ExplicitByte109 = 109,
        ExplicitByte110 = 110,
        ExplicitByte111 = 111,
        ExplicitByte112 = 112,
        ExplicitByte113 = 113,
        ExplicitByte114 = 114,
        ExplicitByte115 = 115,
        ExplicitByte116 = 116,
        ExplicitByte117 = 117,
        ExplicitByte118 = 118,
        ExplicitByte119 = 119,
        ExplicitByte120 = 120,
        ExplicitByte121 = 121,
        ExplicitByte122 = 122,
        ExplicitByte123 = 123,
        ExplicitByte124 = 124,
        ExplicitByte125 = 125,
        ExplicitByte126 = 126,
        ExplicitByte127 = 127,
    };
    Q_ENUM(EventByte)

    enum ValueSpecifier {
        ValueByte1 = -1,
        ValueByte2 = -2,
        ValueByte3 = -3,
        ValueEventChannel = -4,
        ExplicitValue0 = 0,
        ExplicitValue1 = 1,
        ExplicitValue2 = 2,
        ExplicitValue3 = 3,
        ExplicitValue4 = 4,
        ExplicitValue5 = 5,
        ExplicitValue6 = 6,
        ExplicitValue7 = 7,
        ExplicitValue8 = 8,
        ExplicitValue9 = 9,
        ExplicitValue10 = 10,
        ExplicitValue11 = 11,
        ExplicitValue12 = 12,
        ExplicitValue13 = 13,
        ExplicitValue14 = 14,
        ExplicitValue15 = 15,
        ExplicitValue16 = 16,
        ExplicitValue17 = 17,
        ExplicitValue18 = 18,
        ExplicitValue19 = 19,
        ExplicitValue20 = 20,
        ExplicitValue21 = 21,
        ExplicitValue22 = 22,
        ExplicitValue23 = 23,
        ExplicitValue24 = 24,
        ExplicitValue25 = 25,
        ExplicitValue26 = 26,
        ExplicitValue27 = 27,
        ExplicitValue28 = 28,
        ExplicitValue29 = 29,
        ExplicitValue30 = 30,
        ExplicitValue31 = 31,
        ExplicitValue32 = 32,
        ExplicitValue33 = 33,
        ExplicitValue34 = 34,
        ExplicitValue35 = 35,
        ExplicitValue36 = 36,
        ExplicitValue37 = 37,
        ExplicitValue38 = 38,
        ExplicitValue39 = 39,
        ExplicitValue40 = 40,
        ExplicitValue41 = 41,
        ExplicitValue42 = 42,
        ExplicitValue43 = 43,
        ExplicitValue44 = 44,
        ExplicitValue45 = 45,
        ExplicitValue46 = 46,
        ExplicitValue47 = 47,
        ExplicitValue48 = 48,
        ExplicitValue49 = 49,
        ExplicitValue50 = 50,
        ExplicitValue51 = 51,
        ExplicitValue52 = 52,
        ExplicitValue53 = 53,
        ExplicitValue54 = 54,
        ExplicitValue55 = 55,
        ExplicitValue56 = 56,
        ExplicitValue57 = 57,
        ExplicitValue58 = 58,
        ExplicitValue59 = 59,
        ExplicitValue60 = 60,
        ExplicitValue61 = 61,
        ExplicitValue62 = 62,
        ExplicitValue63 = 63,
        ExplicitValue64 = 64,
        ExplicitValue65 = 65,
        ExplicitValue66 = 66,
        ExplicitValue67 = 67,
        ExplicitValue68 = 68,
        ExplicitValue69 = 69,
        ExplicitValue70 = 70,
        ExplicitValue71 = 71,
        ExplicitValue72 = 72,
        ExplicitValue73 = 73,
        ExplicitValue74 = 74,
        ExplicitValue75 = 75,
        ExplicitValue76 = 76,
        ExplicitValue77 = 77,
        ExplicitValue78 = 78,
        ExplicitValue79 = 79,
        ExplicitValue80 = 80,
        ExplicitValue81 = 81,
        ExplicitValue82 = 82,
        ExplicitValue83 = 83,
        ExplicitValue84 = 84,
        ExplicitValue85 = 85,
        ExplicitValue86 = 86,
        ExplicitValue87 = 87,
        ExplicitValue88 = 88,
        ExplicitValue89 = 89,
        ExplicitValue90 = 90,
        ExplicitValue91 = 91,
        ExplicitValue92 = 92,
        ExplicitValue93 = 93,
        ExplicitValue94 = 94,
        ExplicitValue95 = 95,
        ExplicitValue96 = 96,
        ExplicitValue97 = 97,
        ExplicitValue98 = 98,
        ExplicitValue99 = 99,
        ExplicitValue100 = 100,
        ExplicitValue101 = 101,
        ExplicitValue102 = 102,
        ExplicitValue103 = 103,
        ExplicitValue104 = 104,
        ExplicitValue105 = 105,
        ExplicitValue106 = 106,
        ExplicitValue107 = 107,
        ExplicitValue108 = 108,
        ExplicitValue109 = 109,
        ExplicitValue110 = 110,
        ExplicitValue111 = 111,
        ExplicitValue112 = 112,
        ExplicitValue113 = 113,
        ExplicitValue114 = 114,
        ExplicitValue115 = 115,
        ExplicitValue116 = 116,
        ExplicitValue117 = 117,
        ExplicitValue118 = 118,
        ExplicitValue119 = 119,
        ExplicitValue120 = 120,
        ExplicitValue121 = 121,
        ExplicitValue122 = 122,
        ExplicitValue123 = 123,
        ExplicitValue124 = 124,
        ExplicitValue125 = 125,
        ExplicitValue126 = 126,
        ExplicitValue127 = 127,
    };
    Q_ENUM(ValueSpecifier)

    RuleType type() const;
    void setType(const RuleType &type);
    Q_SIGNAL void typeChanged();

    EventSize byteSize() const;
    void setByteSize(const EventSize &byteSize);
    Q_SIGNAL void byteSizeChanged();
    EventByte byte1() const;
    void setByte1(const EventByte &byte1);
    Q_SIGNAL void byte1Changed();
    bool byte1AddChannel() const;
    void setByte1AddChannel(const bool &byte1AddChannel);
    Q_SIGNAL void byte1AddChannelChanged();
    EventByte byte2() const;
    void setByte2(const EventByte &byte2);
    Q_SIGNAL void byte2Changed();
    bool byte2AddChannel() const;
    void setByte2AddChannel(const bool &byte2AddChannel);
    Q_SIGNAL void byte2AddChannelChanged();
    EventByte byte3() const;
    void setByte3(const EventByte &byte3);
    Q_SIGNAL void byte3Changed();
    bool byte3AddChannel() const;
    void setByte3AddChannel(const bool &byte3AddChannel);
    Q_SIGNAL void byte3AddChannelChanged();

    CUIAHelper::Event cuiaEvent() const;
    void setCuiaEvent(const CUIAHelper::Event &cuiaEvent);
    Q_SIGNAL void cuiaEventChanged();
    ZynthboxBasics::Track cuiaTrack() const;
    void setCuiaTrack(const ZynthboxBasics::Track &cuiaTrack);
    Q_SIGNAL void cuiaTrackChanged();
    ZynthboxBasics::Slot cuiaSlot() const;
    void setCuiaSlot(const ZynthboxBasics::Slot &cuiaSlot);
    Q_SIGNAL void cuiaSlotChanged();
    ValueSpecifier cuiaValue() const;
    void setCuiaValue(const ValueSpecifier &cuiaValue);
    Q_SIGNAL void cuiaValueChanged();

    QString description() const;
    Q_SIGNAL void descripionChanged();
private:
    friend class MidiRouterFilterEntry;
    friend class MidiRouterDevice;
    RuleType m_type{TrackRule};
    EventSize m_byteSize{EventSizeSame};
    EventByte m_bytes[3]{OriginalByte1, OriginalByte2, OriginalByte3};
    bool m_bytesAddChannel[3]{false, false, false};
    CUIAHelper::Event m_cuiaEvent{CUIAHelper::NoCuiaEvent};
    ZynthboxBasics::Track m_cuiaTrack{ZynthboxBasics::CurrentTrack};
    ZynthboxBasics::Slot m_cuiaSlot{ZynthboxBasics::CurrentSlot};
    ValueSpecifier m_cuiaValue{ValueByte3};
    jack_midi_event_t *m_bufferEvent{nullptr};
};
Q_DECLARE_METATYPE(QList<MidiRouterFilterEntryRewriter*>)
Q_DECLARE_METATYPE(MidiRouterFilterEntryRewriter::RuleType)
Q_DECLARE_METATYPE(MidiRouterFilterEntryRewriter::EventSize)
Q_DECLARE_METATYPE(MidiRouterFilterEntryRewriter::EventByte)
Q_DECLARE_METATYPE(MidiRouterFilterEntryRewriter::ValueSpecifier)
