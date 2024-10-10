#include "ClipAudioSourcePositionsModelEntry.h"

ClipAudioSourcePositionsModelEntry::ClipAudioSourcePositionsModelEntry(QObject* parent)
    : QObject(parent)
{
}

void ClipAudioSourcePositionsModelEntry::clear()
{
    updateData(-1, -1, 0, 0, 0, 0);
    m_gain = m_gainRight = m_gainLeft = 0;
    m_keepUntil = -1;
}

void ClipAudioSourcePositionsModelEntry::updateData(const int &id, const int& playheadId, const float& progress, const float& gainLeft, const float& gainRight, const float& pan)
{
    m_id = id;
    m_playheadId = playheadId;
    m_progress = progress;
    // This is an imperfect thing (the fade doesn't take time into account, just number of updates), so fades will not be amazing, but they'll happen
    m_gainLeft = (gainLeft >= m_gainLeft) ? gainLeft : qMin(m_gainLeft * 0.9f, m_gainLeft - 0.01f);
    m_gainRight = (gainRight >= m_gainRight) ? gainRight : qMin(m_gainRight * 0.9f, m_gainRight - 0.01f);
    m_gain = qMax(m_gainLeft, m_gainRight);
    m_pan = pan;
    Q_EMIT dataChanged();
}

int ClipAudioSourcePositionsModelEntry::id() const
{
    return m_id;
}

float ClipAudioSourcePositionsModelEntry::progress() const
{
    return m_progress;
}

float ClipAudioSourcePositionsModelEntry::gain() const
{
    return m_gain;
}

float ClipAudioSourcePositionsModelEntry::gainLeft() const
{
    return m_gainLeft;
}

float ClipAudioSourcePositionsModelEntry::gainRight() const
{
    return m_gainRight;
}

float ClipAudioSourcePositionsModelEntry::pan() const
{
    return m_pan;
}
