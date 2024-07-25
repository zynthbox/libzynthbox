#include "ClipAudioSourcePositionsModelEntry.h"

ClipAudioSourcePositionsModelEntry::ClipAudioSourcePositionsModelEntry(QObject* parent)
    : QObject(parent)
{
}

void ClipAudioSourcePositionsModelEntry::clear()
{
    updateData(-1, 0, 0, 0, 0);
    m_keepUntil = -1;
}

void ClipAudioSourcePositionsModelEntry::updateData(const int &id, const float& progress, const float& gainLeft, const float& gainRight, const float& pan)
{
    m_id = id;
    m_progress = progress;
    m_gain = qMax(gainLeft, gainRight);
    m_gainLeft = gainLeft;
    m_gainRight = gainRight;
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
