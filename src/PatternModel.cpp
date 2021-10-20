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

#include "PatternModel.h"
#include "Note.h"

#include <QDebug>

class PatternModel::Private {
public:
    Private() {}
    int width{16};
    int midiChannel{0};
    int noteLength{3};
    int availableBars{1};
    int activeBar{0};
    int bankOffset{0};
    int bankLength{8};
    bool enabled{true};
    int playingRow{0};
    int playingColumn{0};
};

PatternModel::PatternModel(SequenceModel* parent)
    : NotesModel(parent->playGridManager())
    , d(new Private)
{
    // This will force the creation of a whole bunch of rows with the desired width and whatnot...
    setHeight(16);
}

PatternModel::~PatternModel()
{
    delete d;
}

int PatternModel::subnoteIndex(int row, int column, int midiNote) const
{
    int result{-1};
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const Note* note = qobject_cast<Note*>(getNote(row, column));
        if (note) {
            for (int i = 0; i < note->subnotes().count(); ++i) {
                const Note* subnote = qobject_cast<Note*>(note->subnotes().at(i).value<QObject*>());
                if (subnote && subnote->midiNote() == midiNote) {
                    result = i;
                    break;
                }
            }
        }
    }
    return result;
}

int PatternModel::addSubnote(int row, int column, QObject* note)
{
    int newPosition{-1};
    if (row > -1 && row < height() && column > -1 && column < width() && note) {
        Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
        QVariantList subnotes;
        QVariantList metadata;
        if (oldCompound) {
            subnotes = oldCompound->subnotes();
            metadata = getMetadata(row, column).toList();
        }
        newPosition = subnotes.count();

        // Ensure the note is correct according to our midi channel settings
        Note *newNote = qobject_cast<Note*>(note);
        if (newNote->midiChannel() != d->midiChannel) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->midiChannel));
        }
        subnotes.append(QVariant::fromValue<QObject*>(newNote));

        metadata.append(QVariantHash());
        setNote(row, column, playGridManager()->getCompoundNote(subnotes));
        setMetadata(row, column, metadata);
    }
    return newPosition;
}

void PatternModel::removeSubnote(int row, int column, int subnote)
{
    if (row > -1 && row < height() && column > -1 && column < width()) {
        Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
        QVariantList subnotes;
        QVariantList metadata;
        if (oldCompound) {
            subnotes = oldCompound->subnotes();
            metadata = getMetadata(row, column).toList();
        }
        if (subnote > -1 && subnote < subnotes.count()) {
            subnotes.removeAt(subnote);
            metadata.removeAt(subnote);
        }
        setNote(row, column, playGridManager()->getCompoundNote(subnotes));
        setMetadata(row, column, metadata);
    }
}

void PatternModel::setSubnoteMetadata(int row, int column, int subnote, const QString& key, const QVariant& value)
{
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const QVariant rawMeta(getMetadata(row, column).toList());
        QVariantList metadata;
        if (rawMeta.isValid() && rawMeta.canConvert<QVariantList>()) {
            metadata = rawMeta.toList();
        } else {
            Note *note = qobject_cast<Note*>(getNote(row, column));
            if (note) {
                for (int i = 0; i < note->subnotes().count(); ++i) {
                    metadata << QVariantHash();
                }
            }
        }
        if (subnote > -1 && subnote < metadata.count()) {
            QVariantHash noteMetadata = metadata.at(subnote).toHash();
            if (value.isValid()) {
                noteMetadata[key] = value;
            } else {
                noteMetadata.remove(key);
            }
            metadata[subnote] = noteMetadata;
        }
        setMetadata(row, column, metadata);
    }
}

QVariant PatternModel::subnoteMetadata(int row, int column, int subnote, const QString& key)
{
    QVariant result;
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const QVariantList metadata = getMetadata(row, column).toList();
        if (subnote > -1 && subnote < metadata.count()) {
            result.setValue(metadata.at(subnote).toHash().value(key));
        }
    }
    return result;
}

void PatternModel::clear()
{
    beginResetModel();
    for (int row = 0; row < rowCount(); ++row) {
        clearRow(row);
    }
    endResetModel();
}

void PatternModel::clearRow(int row)
{
    for (int column = 0; column < d->width; ++column) {
        setNote(row, column, nullptr);
        setMetadata(row, column, QVariantList());
    }
}

void PatternModel::setWidth(int width)
{
    if (this->width() < width) {
        // Force these to exist if wider than current
        for (int row = 0; row < height(); ++row) {
            setNote(row, width - 1, nullptr);
        }
    } else {
        // Remove any that are superfluous if narrower
        for (int row = 0; row < height(); ++row) {
            QVariantList rowNotes(getRow(row));
            QVariantList rowMetadata(getRowMetadata(row));
            while (rowNotes.count() > width) {
                rowNotes.removeAt(rowNotes.count() - 1);
                rowMetadata.removeAt(rowNotes.count() - 1);
            }
            setRowData(row, rowNotes, rowMetadata);
        }
    }
}

int PatternModel::width() const
{
    return d->width;
}

void PatternModel::setHeight(int height)
{
    if (this->height() < height) {
        // Force these to exist if taller than current
        for (int i = this->height(); i < height; ++i) {
            setNote(i, width() - 1, nullptr);
        }
    } else {
        // Remove any that are superfluous if shorter
        while (this->height() > height) {
            removeRow(this->height() - 1);
        }
    }
}

int PatternModel::height() const
{
    return rowCount();
}

void PatternModel::setMidiChannel(int midiChannel)
{
    int actualChannel = qMin(qMax(0, midiChannel), 15);
    if (d->midiChannel != actualChannel) {
        d->midiChannel = actualChannel;
        for (int row = 0; row < rowCount(); ++row) {
            for (int column = 0; column < columnCount(createIndex(row, 0)); ++column) {
                Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
                QVariantList newSubnotes;
                if (oldCompound) {
                    for (const QVariant &subnote :oldCompound->subnotes()) {
                        Note *oldNote = qobject_cast<Note*>(subnote.value<QObject*>());
                        if (oldNote) {
                            newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(oldNote->midiNote(), actualChannel));
                        } else {
                            // This really shouldn't happen - spit out a warning and slap in something unknown so we keep the order intact
                            newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(0, actualChannel));
                            qWarning() << "Failed to convert a subnote value which must be a Note object to a Note object - something clearly isn't right.";
                        }
                    }
                }
                setNote(row, column, playGridManager()->getCompoundNote(newSubnotes));
            }
        }
        Q_EMIT midiChannelChanged();
    }
}

int PatternModel::midiChannel() const
{
    return d->midiChannel;
}

void PatternModel::setNoteLength(int noteLength)
{
    if (d->noteLength != noteLength) {
        d->noteLength = noteLength;
        Q_EMIT noteLengthChanged();
    }
}

int PatternModel::noteLength() const
{
    return d->noteLength;
}

void PatternModel::setAvailableBars(int availableBars)
{
    int adjusted = qMin(qMax(0, availableBars), height());
    if (d->availableBars != adjusted) {
        d->availableBars = adjusted;
        Q_EMIT availableBarsChanged();
    }
}

int PatternModel::availableBars() const
{
    return d->availableBars;
}

void PatternModel::setActiveBar(int activeBar)
{
    if (d->activeBar != activeBar) {
        d->activeBar = activeBar;
        Q_EMIT activeBarChanged();
    }
}

int PatternModel::activeBar() const
{
    return d->activeBar;
}

void PatternModel::setBankOffset(int bankOffset)
{
    if (d->bankOffset != bankOffset) {
        d->bankOffset = bankOffset;
        Q_EMIT bankOffsetChanged();
    }
}

int PatternModel::bankOffset() const
{
    return d->bankOffset;
}

void PatternModel::setBankLength(int bankLength)
{
    if (d->bankLength != bankLength) {
        d->bankLength = bankLength;
        Q_EMIT bankLengthChanged();
    }
}

int PatternModel::bankLength() const
{
    return d->bankLength;
}

void PatternModel::setEnabled(bool enabled)
{
    if (d->enabled != enabled) {
        d->enabled = enabled;
        Q_EMIT enabledChanged();
    }
}

bool PatternModel::enabled() const
{
    return d->enabled;
}

int PatternModel::playingRow() const
{
    return d->playingRow;
}

int PatternModel::playingColumn() const
{
    return d->playingColumn;
}

void PatternModel::setPositionOff(int row, int column) const
{
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const Note *note = qobject_cast<Note*>(getNote(row, column));
        if (note) {
            for (const QVariant &subnoteVar : note->subnotes()) {
                Note *subnote = qobject_cast<Note*>(subnoteVar.value<QObject*>());
                if (subnote) {
                    subnote->setOff();
                }
            }
        }
    }
}

QObjectList PatternModel::setPositionOn(int row, int column) const
{
    static const QLatin1String velocityString{"velocity"};
    QObjectList onifiedNotes;
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const Note *note = qobject_cast<Note*>(getNote(row, column));
        if (note) {
            const QVariantList &subnotes = note->subnotes();
            const QVariantList &meta = getMetadata(row, column).toList();
            if (meta.count() == subnotes.count()) {
                for (int i = 0; i < subnotes.count(); ++i) {
                    Note *subnote = qobject_cast<Note*>(subnotes[i].value<QObject*>());
                    const QVariantHash &metaHash = meta[i].toHash();
                    if (metaHash.isEmpty() && subnote) {
                        playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true);
                        onifiedNotes << subnote;
                    } else if (subnote) {
                        int velocity{64};
                        if (metaHash.contains(velocityString)) {
                            velocity = metaHash.value(velocityString).toInt();
                        }
                        playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true, velocity);
                        onifiedNotes << subnote;
                    }
                }
            } else {
                for (const QVariant &subnoteVar : subnotes) {
                    Note *subnote = qobject_cast<Note*>(subnoteVar.value<QObject*>());
                    if (subnote) {
                        playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true);
                        onifiedNotes << subnote;
                    }
                }
            }
        }
    }
    return onifiedNotes;
}

void PatternModel::handleSequenceAdvancement(quint64 sequencePosition)
{
    static const QLatin1String velocityString{"velocity"};
    if (d->enabled) {
        // check whether the sequencePosition + 1 matches our note length
        quint64 nextPosition = sequencePosition + 1;
        bool relevantToUs{false};
        switch (d->noteLength) {
        case 1:
            if (nextPosition % 32 == 0) {
                relevantToUs = true;
                nextPosition = nextPosition / 32;
            }
            break;
        case 2:
            if (nextPosition % 16 == 0) {
                relevantToUs = true;
                nextPosition = nextPosition / 16;
            }
            break;
        case 3:
            if (nextPosition % 8 == 0) {
                relevantToUs = true;
                nextPosition = nextPosition / 8;
            }
            break;
        case 4:
            if (nextPosition % 4 == 0) {
                relevantToUs = true;
                nextPosition = nextPosition / 4;
            }
            break;
        case 5:
            if (nextPosition % 2 == 0) {
                relevantToUs = true;
                nextPosition = nextPosition / 2;
            }
            break;
        case 6:
            relevantToUs = true;
            break;
        default:
            qWarning() << "Incorrect note length in pattern, no notes will be played from this one, ever" << objectName();
            break;
        }

        if (relevantToUs) {
            // Get the next row/column combination, and schedule the previous one off, and the next one on
            // squish nextPosition down to fit inside our available range (d->availableBars * d->width)
            // start + (numberToBeWrapped - start) % (limit - start)
            nextPosition = nextPosition % (d->availableBars * d->width);
            int row = (nextPosition / d->width) % d->availableBars;
            int column = nextPosition - (row * d->availableBars);
            const Note *note = qobject_cast<Note*>(getNote(row + d->bankOffset, column));
            if (note) {
                const QVariantList &subnotes = note->subnotes();
                const QVariantList &meta = getMetadata(row + d->bankOffset, column).toList();
                if (meta.count() == subnotes.count()) {
                    for (int i = 0; i < subnotes.count(); ++i) {
                        Note *subnote = qobject_cast<Note*>(subnotes[i].value<QObject*>());
                        const QVariantHash &metaHash = meta[i].toHash();
                        if (metaHash.isEmpty() && subnote) {
                            playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true);
                        } else if (subnote) {
                            int velocity{64};
                            if (metaHash.contains(velocityString)) {
                                velocity = metaHash.value(velocityString).toInt();
                            }
                            playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true, velocity);
                        }
                    }
                } else if (subnotes.count() > 0) {
                    for (const QVariant &subnoteVar : subnotes) {
                        Note *subnote = qobject_cast<Note*>(subnoteVar.value<QObject*>());
                        if (subnote) {
                            playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), true);
                        }
                    }
                } else {
                    playGridManager()->scheduleNote(note->midiNote(), note->midiChannel(), true);
                }
            }
            int previousRow = row;
            int previousColumn = column - 1;
            if (previousColumn == -1) {
                previousColumn = d->width - 1;
                previousRow--;
            }
            if (previousRow == -1) {
                previousRow = d->availableBars - 1;
            }
            const Note *previousNote = qobject_cast<Note*>(getNote(previousRow + d->bankOffset, previousColumn));
            if (previousNote) {
                const QVariantList &subnotes = previousNote->subnotes();
                if (subnotes.count() > 0) {
                    for (int i = 0; i < subnotes.count(); ++i) {
                        const Note *subnote = qobject_cast<Note*>(subnotes[i].value<QObject*>());
                        playGridManager()->scheduleNote(subnote->midiNote(), subnote->midiChannel(), false);
                    }
                } else {
                    playGridManager()->scheduleNote(previousNote->midiNote(), previousNote->midiChannel(), false);
                }
            }
            d->playingRow = row;
            d->playingColumn = column;
            QMetaObject::invokeMethod(this, "playingRowChanged", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "playingColumnChanged", Qt::QueuedConnection);
        }
    }
}