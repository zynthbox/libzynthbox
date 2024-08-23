#include "MidiRouterFilter.h"

#include "MidiRouterFilterEntry.h"

MidiRouterFilter::MidiRouterFilter(MidiRouterDevice* parent)
    : QObject(parent)
{
}

MidiRouterFilter::~MidiRouterFilter()
{
}

const MidiRouterFilterEntry * MidiRouterFilter::match(const jack_midi_event_t& event) const
{
    for (const MidiRouterFilterEntry *entry : qAsConst(m_entries)) {
        if (entry->match(event)) {
            // ##################################
            // EARLY RETURN - to retain constness
            // ##################################
            return entry;
            break;
        }
    }
    return nullptr;
}

const MidiRouterFilterEntry * MidiRouterFilter::matchCommand(const CUIAHelper::Event& cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Part& part, const int& value)
{
    for (const MidiRouterFilterEntry *entry : qAsConst(m_entries)) {
        if (entry->matchCommand(cuiaEvent, track, part, value)) {
            // ##################################
            // EARLY RETURN - to retain constness
            // ##################################
            return entry;
            break;
        }
    }
    return nullptr;
}

QList<MidiRouterFilterEntry *> MidiRouterFilter::entries() const
{
    return m_entries;
}

MidiRouterFilterEntry * MidiRouterFilter::createEntry(const int& index)
{
    MidiRouterFilterEntry *entry = new MidiRouterFilterEntry(qobject_cast<MidiRouterDevice*>(parent()), this);
    // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
    auto tempList = m_entries;
    if (-1 < index && index < tempList.count()) {
        tempList.insert(index, entry);
    } else {
        tempList.append(entry);
    }
    m_entries = tempList;
    Q_EMIT entriesChanged();
    return entry;
}

void MidiRouterFilter::deleteEntry(const int& index)
{
    if (-1 < index && index < m_entries.count()) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_entries;
        tempList.removeAt(index);
        m_entries = tempList;
        Q_EMIT entriesChanged();
    }
}

int MidiRouterFilter::indexOf(MidiRouterFilterEntry* entry) const
{
    return m_entries.indexOf(entry);
}

void MidiRouterFilter::swap(MidiRouterFilterEntry* swapThis, MidiRouterFilterEntry* withThis)
{
    const int firstPosition{m_entries.indexOf(swapThis)};
    const int secondPosition{m_entries.indexOf(withThis)};
    if (firstPosition > -1 && secondPosition > -1) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_entries;
        tempList.swapItemsAt(firstPosition, secondPosition);
        m_entries = tempList;
        Q_EMIT entriesChanged();
    }
}
