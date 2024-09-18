#pragma once

#include "MidiRouterDevice.h"
#include "MidiRouterFilterEntryRewriter.h"

#include "ZynthboxBasics.h"

class MidiRouterFilter;
/**
 * \brief A single entry in a MidiRouterFilter
 *
 * The entry has a set of requirements that an event will have to match to, well, match.
 * Once matched to an entry, an event can be mangled by the filter entry on request, according
 * to a number of requirements set on the entry (for example, a note on event can turn into
 * no midi event, and an event sent into the UI, or a cc event can turn into a set of other
 * events).
 */
class MidiRouterFilterEntry : public QObject
{
    Q_OBJECT
    /**
     * \brief The output track for the given event (valid on input filters)
     * @default ZynthboxBasics::CurrentTrack
     */
    Q_PROPERTY(ZynthboxBasics::Track targetTrack READ targetTrack WRITE setTargetTrack NOTIFY targetTrackChanged)
    /**
     * \brief The number of bytes that the event must contain for this entry to match
     * @minimum 1
     * @maximum 3
     * @default 3
     */
    Q_PROPERTY(int requiredBytes READ requiredBytes WRITE setRequiredBytes NOTIFY requiredBytesChanged)
    /**
     * \brief Whether this filter requires a range of bytes or not (if not, only byte minimums will matter) (valid on input filters)
     * @default false
     */
    Q_PROPERTY(bool requireRange READ requireRange WRITE setRequireRange NOTIFY requireRangeChanged)
    /**
     * \brief The minimum value of byte1 for a match to occur (valid on input filters)
     * Setting this to a value higher than the maximum will set the maximum to the same value
     * @minimum 128
     * @maximum 255
     * @default 128 (Note-off for the first midi channel)
     */
    Q_PROPERTY(int byte1Minimum READ byte1Minimum WRITE setByte1Minimum NOTIFY byte1MinimumChanged)
    /**
     * \brief The maximum value of byte1 for a match to occur (valid on input filters)
     * Setting this value to a lower value than the minimum will set the minimum to the same value
     * @minimum 128
     * @maximum 255
     * @default 128 (Note-off for the first midi channel)
     */
    Q_PROPERTY(int byte1Maximum READ byte1Maximum WRITE setByte1Maximum NOTIFY byte1MaximumChanged)
    /**
     * \brief The minimum value of byte2 for a match to occur (valid on input filters)
     * Setting this to a value higher than the maximum will set the maximum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int byte2Minimum READ byte2Minimum WRITE setByte2Minimum NOTIFY byte2MinimumChanged)
    /**
     * \brief The maximum value of byte2 for a match to occur (valid on input filters)
     * Setting this value to a lower value than the minimum will set the minimum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int byte2Maximum READ byte2Maximum WRITE setByte2Maximum NOTIFY byte2MaximumChanged)
    /**
     * \brief The minimum value of byte3 for a match to occur (valid on input filters)
     * Setting this to a value higher than the maximum will set the maximum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int byte3Minimum READ byte3Minimum WRITE setByte3Minimum NOTIFY byte3MinimumChanged)
    /**
     * \brief The maximum value of byte3 for a match to occur (valid on input filters)
     * Setting this value to a lower value than the minimum will set the minimum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int byte3Maximum READ byte3Maximum WRITE setByte3Maximum NOTIFY byte3MaximumChanged)
    /**
     * \brief The cuia event this filter should react to (valid on output filters)
     */
    Q_PROPERTY(CUIAHelper::Event cuiaEvent READ cuiaEvent WRITE setCuiaEvent NOTIFY cuiaEventChanged)
    /**
     * \brief The origin track (valid on output filters)
     * @default ZynthboxBasics::AnyTrack
     */
    Q_PROPERTY(ZynthboxBasics::Track originTrack READ originTrack WRITE setOriginTrack NOTIFY originTrackChanged)
    /**
     * \brief The origin part (valid on output filters)
     * @default ZynthboxBasics::AnyPart
     */
    Q_PROPERTY(ZynthboxBasics::Part originPart READ originPart WRITE setOriginPart NOTIFY originPartChanged)
    /**
     * \brief The minimum value of the event value (valid on output filters)
     * Setting this to a value higher than the maximum will set the maximum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int valueMinimum READ valueMinimum WRITE setValueMinimum NOTIFY valueMinimumChanged)
    /**
     * \brief The maximum value of the event value (valid on output filters)
     * Setting this value to a lower value than the minimum will set the minimum to the same value
     * @minimum 0
     * @maximum 127
     * @default 0
     */
    Q_PROPERTY(int valueMaximum READ valueMaximum WRITE setValueMaximum NOTIFY valueMaximumChanged)
    /**
     * \brief A list of the rules used to perform rewriting operations for this filter entry
     * To modify this list, use the functions provided
     * @see addRewriteRule(int)
     * @see deleteRewriteRule(int)
     * @see indexOf(MidiRouterFilterEntryRewriter*)
     * @see swapRewriteRules(MidiRouterFilterEntryRewriter*, MidiRouterFilterEntryRewriter*)
     */
    Q_PROPERTY(QList<MidiRouterFilterEntryRewriter*> rewriteRules READ rewriteRules NOTIFY rewriteRulesChanged)

    /**
     * \brief A human-readable description of the filter entry
     */
    Q_PROPERTY(QString description READ description NOTIFY descripionChanged)
public:
    explicit MidiRouterFilterEntry(MidiRouterDevice* routerDevice, MidiRouterFilter *parent = nullptr);
    ~MidiRouterFilterEntry() override;

    /**
     * \brief Test whether the given midi event matches this filter's requirements
     * @param event The midi event to test against this filter entry
     * @return True if the event matches, false otherwise
     */
    bool match(const jack_midi_event_t &event) const;

    /**
     * \brief Writes the most recently matched event to the given buffer
     * @note It is vital that you match prior to calling this function, as mangling is done there, to avoid doing it more than once
     * @see MidiRouterFilter::match(jack_midi_event_t)
     */
    void writeEventToDevice(MidiRouterDevice* device) const;

    /**
     * \brief Test whether the given values match this filter entry's settings
     * @param cuiaEvent The cuia event which was fired
     * @param track The sketchpad track this applies to (where relevant)
     * @param part The sketchpad part this applies to (where relevant)
     * @param value The value associated with this command (where relevant) - this will be an integer between 0 and 127 inclusive (a midi byte value)
     */
    bool matchCommand(const CUIAHelper::Event &cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Part& part, const int& value) const;

    ZynthboxBasics::Track targetTrack() const;
    void setTargetTrack(const ZynthboxBasics::Track &targetTrack);
    Q_SIGNAL void targetTrackChanged();

    int requiredBytes() const;
    void setRequiredBytes(const int &requiredBytes);
    Q_SIGNAL void requiredBytesChanged();
    bool requireRange() const;
    void setRequireRange(const bool &requireRange);
    Q_SIGNAL void requireRangeChanged();
    int byte1Minimum() const;
    void setByte1Minimum(const int &byte1Minimum);
    Q_SIGNAL void byte1MinimumChanged();
    int byte1Maximum() const;
    void setByte1Maximum(const int &byte1Maximum);
    Q_SIGNAL void byte1MaximumChanged();
    int byte2Minimum() const;
    void setByte2Minimum(const int &byte2Minimum);
    Q_SIGNAL void byte2MinimumChanged();
    int byte2Maximum() const;
    void setByte2Maximum(const int &byte2Maximum);
    Q_SIGNAL void byte2MaximumChanged();
    int byte3Minimum() const;
    void setByte3Minimum(const int &byte3Minimum);
    Q_SIGNAL void byte3MinimumChanged();
    int byte3Maximum() const;
    void setByte3Maximum(const int &byte3Maximum);
    Q_SIGNAL void byte3MaximumChanged();

    CUIAHelper::Event cuiaEvent() const;
    void setCuiaEvent(const CUIAHelper::Event &cuiaEvent);
    Q_SIGNAL void cuiaEventChanged();
    ZynthboxBasics::Track originTrack() const;
    void setOriginTrack(const ZynthboxBasics::Track &originTrack);
    Q_SIGNAL void originTrackChanged();
    ZynthboxBasics::Part originPart() const;
    void setOriginPart(const ZynthboxBasics::Part &originPart);
    Q_SIGNAL void originPartChanged();
    int valueMinimum() const;
    void setValueMinimum(const int &valueMinimum);
    Q_SIGNAL void valueMinimumChanged();
    int valueMaximum() const;
    void setValueMaximum(const int &valueMaximum);
    Q_SIGNAL void valueMaximumChanged();

    QList<MidiRouterFilterEntryRewriter*> rewriteRules() const;
    Q_SIGNAL void rewriteRulesChanged();
    /**
     * \brief Add a new rewrite rule at the given position and return the object instance
     * @param index The position at which to insert the new rule (if index is out of bounds it will be appended)
     * @return The newly created rule
     */
    Q_INVOKABLE MidiRouterFilterEntryRewriter * addRewriteRule(const int &index = -1);
    /**
     * \brief Remove the given rule from this list (if the index is not valid, the function will simply return)
     * @param index The position of rule you wish to remove from the list (
     */
    Q_INVOKABLE void deleteRewriteRule(const int &index);
    /**
     * \brief Returns the index of the given rule
     * @param rule The rule that you want to get the index of
     * @return The index of the rule in the list, or -1 if the rule is not in the list
     */
    Q_INVOKABLE int indexOf(MidiRouterFilterEntryRewriter *rule) const;
    /**
     * \brief Swap the location of the two given rules in the list
     * If either of the two rules is not found in the list, the function will do nothing
     * @param swapThis The rule you want to swap locations with the second
     * @param withThis The rule you want to swap locations with the first
     */
    Q_INVOKABLE void swapRewriteRules(MidiRouterFilterEntryRewriter *swapThis, MidiRouterFilterEntryRewriter *withThis);

    QString description() const;
    Q_SIGNAL void descripionChanged();
private:
    ZynthboxBasics::Track m_targetTrack{ZynthboxBasics::CurrentTrack};
    ZynthboxBasics::Part m_targetPart{ZynthboxBasics::CurrentPart};
    ZynthboxBasics::Track m_originTrack{ZynthboxBasics::AnyTrack};
    ZynthboxBasics::Part m_originPart{ZynthboxBasics::AnyPart};
    int m_requiredBytes{3};
    bool m_requireRange{false};
    int m_byte1Minimum{128};
    int m_byte1Maximum{128};
    int m_byte2Minimum{0};
    int m_byte2Maximum{0};
    int m_byte3Minimum{0};
    int m_byte3Maximum{0};
    CUIAHelper::Event m_cuiaEvent{CUIAHelper::NoCuiaEvent};
    int m_valueMinimum{0};
    int m_valueMaximum{0};
    QList<MidiRouterFilterEntryRewriter*> m_rewriteRules;
    MidiRouterDevice *m_routerDevice{nullptr};
    void mangleEvent(const jack_midi_event_t &event) const;
};
