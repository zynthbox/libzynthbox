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
#include "SyncTimer.h"

class Note::Private {
public:
    Private() { }
    QString name;
    int midiNote{0};
    int sketchpadTrack{-1};
    int isPlaying{0};
    QVariantList subnotes;
    int scaleIndex{0};
    int activeChannel{-1};
    int internalOnChannel{-1};
    int pitch{0};

    SyncTimer *syncTimer{nullptr};
};

Note::Note(PlayGridManager* parent)
    : QObject(parent)
    , d(new Private)
{
    d->syncTimer = SyncTimer::instance();
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

void Note::setSketchpadTrack(const int& sketchpadTrack)
{
    if (d->sketchpadTrack != sketchpadTrack) {
        d->sketchpadTrack = sketchpadTrack;
        Q_EMIT sketchpadTrackChanged();
    }
}

int Note::sketchpadTrack() const
{
    return d->sketchpadTrack;
}

int Note::activeChannel() const
{
    return d->activeChannel;
}

void Note::resetRegistrations()
{
    d->isPlaying = 0;
    QMetaObject::invokeMethod(this, "isPlayingChanged", Qt::QueuedConnection);
    d->activeChannel = -1;
    QMetaObject::invokeMethod(this, "activeChannelChanged", Qt::QueuedConnection);
    d->internalOnChannel = -1;
}

void Note::registerOn(const int& midiChannel)
{
    d->activeChannel = midiChannel;
    QMetaObject::invokeMethod(this, "activeChannelChanged", Qt::QueuedConnection);
    d->isPlaying = d->isPlaying + 1;
    QMetaObject::invokeMethod(this, "isPlayingChanged", Qt::QueuedConnection);
}

void Note::registerOff(const int& midiChannel)
{
    d->isPlaying = qMax(0, d->isPlaying - 1);
    QMetaObject::invokeMethod(this, "isPlayingChanged", Qt::QueuedConnection);
    if (d->isPlaying == 0) {
        if (d->activeChannel > -1 && d->activeChannel != midiChannel) {
            qWarning() << Q_FUNC_INFO << "Received an off registration on a midi channel we're supposedly not active, this is a bit weird, but ok. Active channel is" << d->activeChannel << "and we received the event on" << midiChannel;
        }
        d->activeChannel = -1;
        QMetaObject::invokeMethod(this, "activeChannelChanged", Qt::QueuedConnection);
    }
}

bool Note::isPlaying() const
{
    return d->isPlaying > 0;
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

void Note::setSubnotesOn(const QVariantList &velocities)
{
    int i = -1;
    for (const QVariant &note : qAsConst(d->subnotes)) {
        if (++i >= velocities.count()) {
            break;
        }
        Note* subnote = note.value<Note*>();
        if (subnote) {
            subnote->setOn(velocities[i].toInt());
        }
    }
}

void Note::setOn(int velocity)
{
    d->internalOnChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack);
    registerOn(d->internalOnChannel);
    if (d->midiNote < 128) {
        d->syncTimer->sendNoteImmediately(d->midiNote, d->internalOnChannel, true, velocity, d->sketchpadTrack);
    }
    for (const QVariant &note : qAsConst(d->subnotes)) {
        const Note* subnote = note.value<Note*>();
        if (subnote) {
            d->syncTimer->sendNoteImmediately(subnote->midiNote(), d->internalOnChannel, true, velocity, d->sketchpadTrack);
        }
    }
}

void Note::setOff()
{
    // Don't attempt to set a note to off if we don't have a channel to send the message to
    // qDebug() << Q_FUNC_INFO << "is playing:" << d->isPlaying << "- internal on channel:" << d->internalOnChannel << "- note:" << d->midiNote << " - track:" << d->sketchpadTrack;
    if (d->internalOnChannel == -1) {
        d->internalOnChannel = d->activeChannel;
    }
    if (d->internalOnChannel > -1) {
        registerOff(d->internalOnChannel);
        if (d->midiNote < 128) {
            d->syncTimer->sendNoteImmediately(d->midiNote, d->internalOnChannel, false, 0, d->sketchpadTrack);
        }
        for (const QVariant &note : qAsConst(d->subnotes)) {
            const Note* subnote = note.value<Note*>();
            if (subnote) {
                d->syncTimer->sendNoteImmediately(subnote->midiNote(), d->internalOnChannel, false, 0, d->sketchpadTrack);
            }
        }
        d->internalOnChannel = -1;
    } else {
        registerOff(d->activeChannel);
    }
}

void Note::registerPitchChange(const int& pitch)
{
    if (d->pitch != pitch - 8192) {
        d->pitch = pitch - 8192;
        Q_EMIT pitchChanged();
    }
}

int Note::pitch() const
{
    return d->pitch;
}

void Note::sendPitchChange(const int& pitch)
{
    const int adjusted = qBound(0, pitch + 8192, 16383);
    d->syncTimer->sendMidiMessageImmediately(3, 0xE0 + d->activeChannel,  adjusted & 127, (adjusted >> 7) & 127, d->sketchpadTrack);
}
