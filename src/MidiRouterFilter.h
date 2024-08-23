#pragma once

#include "MidiRouterDevice.h"
#include <jack/midiport.h>

class MidiRouterFilterEntry;
/**
 * \brief A stack of filters which take a midi event and either accept or reject them
 */
class MidiRouterFilter : public QObject
{
    Q_OBJECT
    /**
     * \brief The list of entries in the filter stack
     * To modify the list of entries, use the functions provided
     * @see createEntry(int)
     * @see removeEntry(int)
     * @see indexOf(MidiRouterFilterEntry*)
     */
    Q_PROPERTY(QList<MidiRouterFilterEntry*> entries READ entries NOTIFY entriesChanged)
public:
    explicit MidiRouterFilter(MidiRouterDevice *parent = nullptr);
    ~MidiRouterFilter() override;

    /**
     * \brief Test whether any filter matches the filter, and return the one that does (if any)
     * The matching is done in the order of the entries list, and the first match is returned
     * @param event The midi event to attempt to find a match for
     * @return The filter entry which matches the event (if null, there were no matches)
     */
    const MidiRouterFilterEntry * match(const jack_midi_event_t &event) const;

    /**
     * \brief Test whether the given values match this filter entry's settings
     * @param cuiaEvent The cuia event which was fired
     * @param track The sketchpad track this applies to (where relevant)
     * @param part The sketchpad part this applies to (where relevant)
     * @param value The value associated with this command (where relevant) - this will be an integer between 0 and 127 inclusive (a midi byte value)
     * @return The filter entry which matches the event (if null, there were no matches)
     */
    const MidiRouterFilterEntry *  matchCommand(const CUIAHelper::Event &cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Part& part, const int& value);

    QList<MidiRouterFilterEntry*> entries() const;
    Q_SIGNAL void entriesChanged();
    /**
     * \brief Creates a new entry and returns it
     * @param index The index at which to insert the new entry (any out of bounds index will append it)
     * @return The newly created entry
     */
    MidiRouterFilterEntry * createEntry(const int &index = -1);
    /**
     * \brief Removes the entry at the given index
     * @param index The index of the entry that you want removed
     */
    void deleteEntry(const int &index);
    /**
     * \brief Returns the index of the given entry
     * @param entry The entry you wish to get the index of
     * @return The index of the given entry in this filter (or -1 if not found)
     */
    int indexOf(MidiRouterFilterEntry *entry) const;
    /**
     * \brief Swaps the position of the two given entries
     * @note If either of the two entries is not found, the function will do nothing
     * @param swapThis The first entry that you want to swap with the other
     * @param withThis The second entry that you want to swap with the first
     */
    void swap(MidiRouterFilterEntry *swapThis, MidiRouterFilterEntry *withThis);
private:
    QList<MidiRouterFilterEntry*> m_entries;
};
Q_DECLARE_METATYPE(QList<MidiRouterFilterEntry*>)