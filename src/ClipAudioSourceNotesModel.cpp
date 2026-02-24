/*
 * Copyright (C) 2026 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "ClipAudioSourceNotesModel.h"
#include "ClipAudioSource.h"
#include "ClipAudioSourceSliceSettings.h"
#include "Plugin.h"

#include <QDateTime>
#include <QTimer>

struct ClipAudioSourceNotesModelEntry {
    int midiNote{0};
    QList<ClipAudioSource*> clips;
    QObjectList clipsVariant; // Convenience for sending through the model
    QList<ClipAudioSourceSliceSettings*> slices;
    QObjectList slicesVariant; // Convenience for sending through the model
};


class ClipAudioSourceNotesModelPrivate {
public:
    ClipAudioSourceNotesModelPrivate(ClipAudioSourceNotesModel *q)
        : q(q)
    {
        for (int midiNote = 0; midiNote < 128; ++midiNote) {
            entries[midiNote].midiNote = midiNote;
        }
        refreshTimer.setInterval(100);
        refreshTimer.setSingleShot(true);
        QObject::connect(&refreshTimer, &QTimer::timeout, &refreshTimer, [this](){ refreshData(); });
    }
    ClipAudioSourceNotesModel *q{nullptr};
    ClipAudioSourceNotesModelEntry entries[128];
    QVariantList clipIDs;
    QList<ClipAudioSource*> clips;
    int lastModified{0};
    QTimer refreshTimer;

    void refreshData() {
        for (int midiNote = 0; midiNote < 128; ++midiNote) {
            bool hasChanged{false};
            ClipAudioSourceNotesModelEntry &entry{entries[midiNote]};
            // First, get rid of any clips in the entry list that we no longer have in the main list
            for (ClipAudioSource *clip : qAsConst(entry.clips)) {
                if (clips.contains(clip) == false) {
                    entry.clips.removeAll(clip);
                    entry.clipsVariant.removeAll(clip);
                    hasChanged = true;
                }
            }
            // Now add any new clips into the entry where relevant
            for (ClipAudioSource *clip : qAsConst(clips)) {
                // TODO We need to handle all slices, not just the root...
                if (clip->rootSliceActual()->keyZoneStart() <= midiNote && midiNote <= clip->rootSliceActual()->keyZoneEnd()) {
                    // Add to the entry's list, if it isn't there already
                    if (entry.clips.contains(clip) == false) {
                        entry.clips.append(clip);
                        entry.clipsVariant.append(clip);
                        hasChanged = true;
                    }
                } else {
                    // Remove it if it's in the entry's list
                    if (entry.clips.contains(clip)) {
                        entry.clips.removeAll(clip);
                        entry.clipsVariant.removeAll(clip);
                        hasChanged = true;
                    }
                }
            }
            // And finally, if we've actually made any changes, report that
            if (hasChanged) {
                QModelIndex idx{q->index(midiNote)};
                q->dataChanged(idx, idx);
                lastModified = QDateTime::currentMSecsSinceEpoch();
                Q_EMIT q->lastModifiedChanged();
            }
        }
    }
};

ClipAudioSourceNotesModel::ClipAudioSourceNotesModel(QObject* parent)
    : QAbstractListModel(parent)
    , d(new ClipAudioSourceNotesModelPrivate(this))
{
}

ClipAudioSourceNotesModel::~ClipAudioSourceNotesModel()
{
    delete d;
}

QVariantMap ClipAudioSourceNotesModel::roles() const
{
    static const QVariantMap roles{
        {"note", NoteRole},
        {"hasClips", HasClipsRole},
        {"clips", ClipsRole},
        {"slices", SlicesRole},
    };
    return roles;
}

QHash<int, QByteArray> ClipAudioSourceNotesModel::roleNames() const
{
    static const QHash<int, QByteArray> roles{
        {NoteRole, "note"},
        {HasClipsRole, "hasClips"},
        {ClipsRole, "clips"},
        {SlicesRole, "slices"},
    };
    return roles;
}

int ClipAudioSourceNotesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 128;
}

QVariant ClipAudioSourceNotesModel::data(const QModelIndex& index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        const ClipAudioSourceNotesModelEntry &entry{d->entries[index.row()]};
        switch(role) {
            case ClipsRole:
                result.setValue(entry.clipsVariant);
                break;
            case SlicesRole:
                result.setValue(entry.slicesVariant);
                break;
            case HasClipsRole:
                result.setValue<bool>(entry.clips.isEmpty() == false);
                break;
            case NoteRole:
            default:
                result.setValue<int>(entry.midiNote);
                break;
        }
    }
    return result;
}

QModelIndex ClipAudioSourceNotesModel::index(int row, int column, const QModelIndex& parent) const
{
    QModelIndex idx;
    if (parent.isValid() == false && -1 < row && row < 128 && column == 0) {
        idx = createIndex(row, column);
    }
    return idx;
}

QVariantList ClipAudioSourceNotesModel::clipIds() const
{
    return d->clipIDs;
}

void ClipAudioSourceNotesModel::setClipIds(const QVariantList& newValue)
{
    bool changed{false};
    if (newValue.length() == d->clipIDs.length()) {
        int i{0};
        for (const QVariant &clipId : newValue) {
            if (d->clipIDs[i] != clipId) {
                changed = true;
                break;
            }
            ++i;
        }
    } else {
        changed = true;
    }
    if (changed) {
        QList<ClipAudioSource*> newClips;
        for (const QVariant &clipId : newValue) {
            const int actualId{clipId.toInt()};
            if (actualId > -1) {
                ClipAudioSource *newClip = Plugin::instance()->getClipById(actualId);
                newClips << newClip;
                if (newClip) {
                    connect(newClip, &QObject::destroyed, this, [this, newClip](){
                        d->clips.removeAll(newClip);
                        d->refreshTimer.start();
                    });
                    connect(newClip, &ClipAudioSource::sliceDataChanged, &d->refreshTimer, QOverload<>::of(&QTimer::start));
                }
            }
        }
        d->clips = newClips;
        d->clipIDs = newValue;
        d->refreshTimer.start();
        Q_EMIT clipIdsChanged();
    }
}

int ClipAudioSourceNotesModel::lastModified() const
{
    return d->lastModified;
}
