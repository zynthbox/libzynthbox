/*
 * Copyright (C) 2021 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "Note.h"

// Hackety hack - we don't need all the thing, just need some storage things (MidiBuffer and MidiNote specifically)
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_audio_formats/juce_audio_formats.h>
#include <libzl.h>
#include <SyncTimer.h>

class Note::Private {
public:
    Private() { }
    PlayGridManager* playGridManager{nullptr};
    QString name;
    int midiNote{0};
    int midiChannel{0};
    bool isPlaying{false};
    QVariantList subnotes;
    int scaleIndex{0};

    SyncTimer *syncTimer{nullptr};
};

Note::Note(PlayGridManager* parent)
    : QObject(parent)
    , d(new Private)
{
    d->playGridManager = parent;
    d->syncTimer = qobject_cast<SyncTimer*>(SyncTimer_instance());
}

Note::~Note()
{
    delete d;
}

void Note::setName(const QString& name)
{
    if (name != d->name) {
        d->name = name;
        Q_EMIT nameChanged();
    }
}

QString Note::name() const
{
    return d->name;
}

void Note::setMidiNote(int midiNote)
{
    if (midiNote != d->midiNote) {
        d->midiNote = midiNote;
        Q_EMIT midiNoteChanged();
    }
}

int Note::midiNote() const
{
    return d->midiNote;
}

int Note::octave() const
{
    return d->midiNote / 12;
}

void Note::setMidiChannel(int midiChannel)
{
    if (midiChannel != d->midiChannel) {
        d->midiChannel = midiChannel;
        Q_EMIT midiChannelChanged();
    }
}

int Note::midiChannel() const
{
    return d->midiChannel;
}

void Note::setIsPlaying(bool isPlaying)
{
    if (isPlaying != d->isPlaying) {
        d->isPlaying = isPlaying;
        // This will tend to cause the UI to update while things are trying to happen that
        // are timing-critical, so let's postpone it for a quick tick
        // Also, timers don't work cross-threads, and this gets called from a thread,
        // so... queued metaobject invocation it is
       QMetaObject::invokeMethod(this, "isPlayingChanged", Qt::QueuedConnection);
    }
}

bool Note::isPlaying() const
{
    return d->isPlaying;
}

void Note::setSubnotes(const QVariantList& subnotes)
{
    bool different = false;
    if (subnotes.count() == d->subnotes.count()) {
        for (int i = 0; i < subnotes.count(); ++i) {
            if (subnotes[i] != d->subnotes[i]) {
                different = true;
                break;
            }
        }
    } else {
        different = true;
    }
    if (different) {
        d->subnotes = subnotes;
        Q_EMIT subnotesChanged();
    }
}

QVariantList Note::subnotes() const
{
    return d->subnotes;
}

void Note::setScaleIndex(int scaleIndex)
{
    if (d->scaleIndex != scaleIndex) {
        d->scaleIndex = scaleIndex;
        Q_EMIT scaleIndexChanged();
    }
}

int Note::scaleIndex() const
{
    return d->scaleIndex;
}

void Note::setSubnotesOn(const QVariantList &velocities) const
{
    int i = -1;
    juce::MidiBuffer onBuffer;
    for (const QVariant &note : d->subnotes) {
        if (++i >= d->subnotes.count()) {
            break;
        }
        const Note* subnote = note.value<Note*>();
        if (subnote) {
            onBuffer.addEvent(juce::MidiMessage::noteOn(subnote->midiChannel() + 1, subnote->midiNote(), juce::uint8(velocities[i].toUInt())), 0);
        }
    }
    if (d->syncTimer && onBuffer.getNumEvents() > 0) {
        d->syncTimer->sendMidiBufferImmediately(onBuffer);
    }
}

void Note::setOn(int velocity) const
{
    if (d->midiNote < 128) {
        if (d->syncTimer) {
            d->syncTimer->sendNoteImmediately(d->midiNote, d->midiChannel, true, velocity);
        }
    }
    juce::MidiBuffer onBuffer;
    for (const QVariant &note : d->subnotes) {
        const Note* subnote = note.value<Note*>();
        if (subnote) {
            onBuffer.addEvent(juce::MidiMessage::noteOn(subnote->midiChannel() + 1, subnote->midiNote(), juce::uint8(velocity)), 0);
        }
    }
    if (d->syncTimer && onBuffer.getNumEvents() > 0) {
        d->syncTimer->sendMidiBufferImmediately(onBuffer);
    }
}

void Note::setOff() const
{
    if (d->midiNote < 128) {
        if (d->syncTimer) {
            d->syncTimer->sendNoteImmediately(d->midiNote, d->midiChannel, false, 0);
        }
    }
    juce::MidiBuffer offBuffer;
    for (const QVariant &note : d->subnotes) {
        const Note* subnote = note.value<Note*>();
        if (subnote) {
            offBuffer.addEvent(juce::MidiMessage::noteOff(subnote->midiChannel() + 1, subnote->midiNote()), 0);
        }
    }
    if (d->syncTimer && offBuffer.getNumEvents() > 0) {
        d->syncTimer->sendMidiBufferImmediately(offBuffer);
    }
}
