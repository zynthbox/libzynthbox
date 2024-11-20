/*
 * Copyright (C) 20214 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "PlayfieldManager.h"

#include "ClipAudioSource.h"
#include "ClipCommand.h"
#include "PatternModel.h"
#include "Plugin.h"
#include "SyncTimer.h"
#include "SegmentHandler.h"
#include "ZynthboxBasics.h"

#include <QDebug>
#include <QTimer>

struct ClipState {
    ClipState() {}
    void reset(qint64 resetOffset = 0) {
        state = PlayfieldManager::StoppedState;
        offset = resetOffset;
    }
    PlayfieldManager::PlaybackState state{PlayfieldManager::StoppedState};
    qint64 offset{0};
};

struct TrackState {
    TrackState() {}
    void reset(qint64 resetOffset = 0) {
        for (int clipIndex = 0; clipIndex < ZynthboxSlotCount; ++clipIndex) {
            clips[clipIndex].reset(resetOffset);
        }
    }
    ClipState clips[ZynthboxSlotCount];
};

struct SongState {
    SongState() {}
    void reset(qint64 resetOffset = 0) {
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            tracks[track].reset(resetOffset);
        }
    }
    TrackState tracks[ZynthboxTrackCount];
};

struct SketchpadState {
    SketchpadState() {}
    void reset(qint64 resetOffset = 0) {
        for (int song = 0; song < ZynthboxSongCount; ++song) {
            songs[song].reset(resetOffset);
        }
    }
    SongState songs[ZynthboxSongCount];
};

class ZLPlayfieldManagerSynchronisationManager;
class PlayfieldManagerPrivate {
public:
    PlayfieldManagerPrivate(PlayfieldManager *q)
        : q(q)
    {
        barLength = syncTimer->getMultiplier() * 4;
        nextBarState.reset(-1);
    }
    ~PlayfieldManagerPrivate() {}
    PlayfieldManager *q{nullptr};
    ZLPlayfieldManagerSynchronisationManager *zlSyncManager{nullptr};
    SketchpadState currentState;
    SketchpadState nextBarState;
    SyncTimer *syncTimer{SyncTimer::instance()};
    SegmentHandler *segmentHandler{SegmentHandler::instance()};
    qint64 barLength{0};
    qint64 playhead{-1};

    inline void handlePlaybackProgress();
    inline void handlePlayfieldStateChange(const int& songIndex, const int& trackIndex, const int& clipIndex);
};

class ZLPlayfieldManagerSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLPlayfieldManagerSynchronisationManager(PlayfieldManagerPrivate *d, PlayfieldManager *parent = nullptr)
        : QObject(parent)
        , q(parent)
        , d(d)
    {
        updateClips();
        clipUpdateThrottle.setInterval(0);
        clipUpdateThrottle.setSingleShot(true);
        connect(&clipUpdateThrottle, &QTimer::timeout, this, &ZLPlayfieldManagerSynchronisationManager::updateClips);
    };
    PlayfieldManager *q{nullptr};
    PlayfieldManagerPrivate* d{nullptr};
    QTimer clipUpdateThrottle;
    QObject *zlSketchpad{nullptr};
    PatternModel::NoteDestination destinations[ZynthboxSongCount][ZynthboxTrackCount];
    QObject *clips[ZynthboxSongCount][ZynthboxTrackCount][ZynthboxSlotCount];
    ClipAudioSource *sketches[ZynthboxSongCount][ZynthboxTrackCount][ZynthboxSlotCount];

    void setZlSketchpad(QObject *newZlSketchpad) {
//         qDebug() << "Setting new sketchpad" << newZlSketchpad;
        if (zlSketchpad != newZlSketchpad) {
            if (zlSketchpad) {
                zlSketchpad->disconnect(&clipUpdateThrottle);
            }
            zlSketchpad = newZlSketchpad;
            if (zlSketchpad) {
                connect(zlSketchpad, SIGNAL(isLoadingChanged()), &clipUpdateThrottle, SLOT(start()));
                updateClips();
            }
        }
    }
public Q_SLOTS:
    void updateClips() {
        static const QLatin1String sampleTrig{"sample-trig"};
        static const QLatin1String sampleLoop{"sample-loop"};
        static const QLatin1String external{"external"};
        // static const QLatin1String synth{"synth"}; // the default
        if (zlSketchpad) {
            for (int songIndex = 0; songIndex < ZynthboxSongCount; ++songIndex) {
                QObject *channelsModel = zlSketchpad->property("channelsModel").value<QObject*>();
                for (int trackIndex = 0; trackIndex < ZynthboxTrackCount; ++trackIndex) {
                    QObject *track{nullptr};
                    QMetaObject::invokeMethod(channelsModel, "getChannel", Qt::DirectConnection, Q_RETURN_ARG(QObject*, track), Q_ARG(int, trackIndex));
                    track->disconnect(&clipUpdateThrottle);
                    connect(track, SIGNAL(track_type_changed()), &clipUpdateThrottle, SLOT(start()));
                    const QString trackType = track->property("trackType").toString();
                    if (trackType == sampleTrig) {
                        destinations[songIndex][trackIndex] = PatternModel::SampleTriggerDestination;
                    } else if (trackType == sampleLoop) {
                        destinations[songIndex][trackIndex] = PatternModel::SampleLoopedDestination;
                    } else if (trackType == external) {
                        destinations[songIndex][trackIndex] = PatternModel::ExternalDestination;
                    } else { // or in other words "if (trackType == synth)"
                        destinations[songIndex][trackIndex] = PatternModel::SynthDestination;
                    }
                    for (int clipIndex = 0; clipIndex < ZynthboxSlotCount; ++clipIndex) {
                        QObject *clip{nullptr};
                        QMetaObject::invokeMethod(zlSketchpad, "getClipById", Qt::DirectConnection, Q_RETURN_ARG(QObject*, clip), Q_ARG(int, trackIndex), Q_ARG(int, songIndex), Q_ARG(int, clipIndex));
                        if (clips[songIndex][trackIndex][clipIndex]) {
                            clips[songIndex][trackIndex][clipIndex]->disconnect(&clipUpdateThrottle);
                        }
                        ClipAudioSource *sketch{nullptr};
                        if (clip) {
                            connect(clip, SIGNAL(cppObjIdChanged()), &clipUpdateThrottle, SLOT(start()));
                            int sampleCppId = clip->property("cppObjId").toInt();
                            sketch = qobject_cast<ClipAudioSource*>(Plugin::instance()->getClipById(sampleCppId));
                        }
                        clips[songIndex][trackIndex][clipIndex] = clip;
                        sketches[songIndex][trackIndex][clipIndex] = sketch;
                    }
                }
            }
        } else {
            for (int songIndex = 0; songIndex < ZynthboxSongCount; ++songIndex) {
                for (int trackIndex = 0; trackIndex < ZynthboxTrackCount; ++trackIndex) {
                    destinations[songIndex][trackIndex] = PatternModel::SynthDestination;
                    for (int clipIndex = 0; clipIndex < ZynthboxSlotCount; ++clipIndex) {
                        clips[songIndex][trackIndex][clipIndex] = nullptr;
                        sketches[songIndex][trackIndex][clipIndex] = nullptr;
                    }
                }
            }
        }
    }
};

void PlayfieldManagerPrivate::handlePlaybackProgress()
{
    if (syncTimer->timerRunning() && segmentHandler->songMode() == false) {
        ++playhead;
        // If this is the a strict beat step, we should update the state to be what is expected
        if (playhead == 0 || (playhead % barLength) == 0) {
            for (int songIndex = 0; songIndex < ZynthboxSongCount; ++songIndex) {
                for (int trackIndex = 0; trackIndex < ZynthboxTrackCount; ++trackIndex) {
                    for (int clipIndex = 0; clipIndex < ZynthboxSlotCount; ++clipIndex) {
                        handlePlayfieldStateChange(songIndex, trackIndex, clipIndex);
                    }
                }
            }
        }
    }
}

void PlayfieldManagerPrivate::handlePlayfieldStateChange(const int& songIndex, const int& trackIndex, const int& clipIndex)
{
    ClipState &currentClip = currentState.songs[songIndex].tracks[trackIndex].clips[clipIndex];
    ClipState &nextBarClip = nextBarState.songs[songIndex].tracks[trackIndex].clips[clipIndex];
    const bool playbackStateDiffers{currentClip.state != nextBarClip.state};
    const bool offsetNeedsAdjusting{nextBarClip.offset > -1};
    if (playbackStateDiffers || offsetNeedsAdjusting) {
        currentClip.state = nextBarClip.state;
        if (offsetNeedsAdjusting) {
            currentClip.offset = playhead + nextBarClip.offset;
            nextBarClip.offset = -1;
        }
        QMetaObject::invokeMethod(q, "playfieldStateChanged", Qt::QueuedConnection, Q_ARG(int, songIndex), Q_ARG(int, trackIndex), Q_ARG(int, clipIndex), Q_ARG(int, PlayfieldManager::CurrentPosition), Q_ARG(int, currentClip.state));
        Q_EMIT q->directPlayfieldStateChanged(songIndex, trackIndex, clipIndex, PlayfieldManager::CurrentPosition);
        // Depending on the sketchpad track's type, we'll want to either outright start the
        // clip playing (if it's sample-looped), or just set the state (if it's midi, at which point
        // PatternModel handles the playback stuff)
        // Also, don't do this if we're in song mode (as that does its own clip scheduling)
        // qDebug() << Q_FUNC_INFO << "Updating" << songIndex << trackIndex << clipIndex << "to" << nextBarClip.state << "with destination" << zlSyncManager->destinations[songIndex][trackIndex];
        if (segmentHandler->songMode() == false && zlSyncManager->destinations[songIndex][trackIndex] == PatternModel::SampleLoopedDestination && zlSyncManager->sketches[songIndex][trackIndex][clipIndex] != nullptr) {
            if (playbackStateDiffers) {
                ClipCommand *clipCommand = syncTimer->getClipCommand();
                clipCommand->startPlayback = currentClip.state == PlayfieldManager::PlayingState; // otherwise, the inversion below ensures it's a stop clip loop operation, and this function requires either a start or stop operation
                clipCommand->stopPlayback = !clipCommand->startPlayback;
                clipCommand->midiChannel = trackIndex;
                clipCommand->clip = zlSyncManager->sketches[songIndex][trackIndex][clipIndex];
                clipCommand->midiNote = 60;
                clipCommand->changeVolume = true;
                clipCommand->volume = 1.0; // this matches how the ClipAudioSource::Play function works
                clipCommand->changeLooping = true;
                clipCommand->looping = true;
                syncTimer->scheduleClipCommand(clipCommand, 0);
            } else if (offsetNeedsAdjusting) {
                // TODO We need a way for clip-command to reposition playback, so samples can do offset playback as well...
                // That'll need doing above as well, if we're doing both at the same time, but offset adjusting can also happen at runtime, so...
            }
        }
    }
}

PlayfieldManager::PlayfieldManager(QObject* parent)
    : QObject(parent)
    , d(new PlayfieldManagerPrivate(this))
{
    d->zlSyncManager = new ZLPlayfieldManagerSynchronisationManager(d, this);
    connect(this, &PlayfieldManager::playfieldStateChanged, this, [](const int &/*sketchpadSong*/, const int &sketchpadTrack, const int &clip, const int &position, const int &state){
        static const QLatin1String setClipActiveState{"SET_CLIP_ACTIVE_STATE"};
        if (position == CurrentPosition) {
            if (state == StoppedState) {
                MidiRouter::instance()->cuiaEventFeedback(setClipActiveState, -1, ZynthboxBasics::Track(sketchpadTrack), ZynthboxBasics::Slot(clip), 0);
            } else {
                MidiRouter::instance()->cuiaEventFeedback(setClipActiveState, -1, ZynthboxBasics::Track(sketchpadTrack), ZynthboxBasics::Slot(clip), 1);
            }
        } else {
            if (state == StoppedState) {
                MidiRouter::instance()->cuiaEventFeedback(setClipActiveState, -1, ZynthboxBasics::Track(sketchpadTrack), ZynthboxBasics::Slot(clip), 2);
            } else {
                MidiRouter::instance()->cuiaEventFeedback(setClipActiveState, -1, ZynthboxBasics::Track(sketchpadTrack), ZynthboxBasics::Slot(clip), 3);
            }
        }
    });
}

PlayfieldManager::~PlayfieldManager()
{
    delete d;
}

void PlayfieldManager::setSketchpad(QObject* sketchpad)
{
    if (d->zlSyncManager->zlSketchpad != sketchpad) {
        d->zlSyncManager->setZlSketchpad(sketchpad);
        Q_EMIT sketchpadChanged();
    }
}

QObject * PlayfieldManager::sketchpad() const
{
    return d->zlSyncManager->zlSketchpad;
}

void PlayfieldManager::setClipPlaystate(const int& sketchpadSong, const int& sketchpadTrack, const int& clip, const PlaybackState& newState, const PlayfieldStatePosition& position, const qint64 &offset)
{
    // qDebug() << Q_FUNC_INFO << sketchpadSong << sketchpadTrack << clip << newState << position;
    if (-1 < sketchpadSong && sketchpadSong < ZynthboxSongCount && -1 < sketchpadTrack && sketchpadTrack < ZynthboxTrackCount && -1 < clip && clip < ZynthboxSlotCount) {
        ClipState &nextBarClip = d->nextBarState.songs[sketchpadSong].tracks[sketchpadTrack].clips[clip];
        const bool playbackStateDiffers{nextBarClip.state != newState};
        const bool offsetNeedsAdjusting{offset > -1};
        if (playbackStateDiffers) {
            nextBarClip.state = newState;
        }
        if (offsetNeedsAdjusting) {
            nextBarClip.offset = offset;
        }
        // If the position we want to change is the current one, then... we should handle the change immediately rather than wait for playback to catch up
        if (position == CurrentPosition) {
            d->handlePlayfieldStateChange(sketchpadSong, sketchpadTrack, clip);
        } else if (playbackStateDiffers || offsetNeedsAdjusting) {
            QMetaObject::invokeMethod(this, "playfieldStateChanged", Qt::QueuedConnection, Q_ARG(int, sketchpadSong), Q_ARG(int, sketchpadTrack), Q_ARG(int, clip), Q_ARG(int, position), Q_ARG(int, newState));
            Q_EMIT directPlayfieldStateChanged(sketchpadSong, sketchpadTrack, clip, position);
        }
    }
}

const PlayfieldManager::PlaybackState PlayfieldManager::clipPlaystate(const int& sketchpadSong, const int& sketchpadTrack, const int& clip, const PlayfieldStatePosition& position) const
{
    if (-1 < sketchpadSong && sketchpadSong < ZynthboxSongCount && -1 < sketchpadTrack && sketchpadTrack < ZynthboxTrackCount && -1 < clip && clip < ZynthboxSlotCount) {
        switch (position) {
            case NextBarPosition:
                return d->nextBarState.songs[sketchpadSong].tracks[sketchpadTrack].clips[clip].state;
                break;
            case CurrentPosition:
            default:
                return d->currentState.songs[sketchpadSong].tracks[sketchpadTrack].clips[clip].state;
                break;
        };
    }
    return PlayfieldManager::StoppedState;
}

const qint64 PlayfieldManager::clipOffset(const int& sketchpadSong, const int& sketchpadTrack, const int& clip) const
{
    if (-1 < sketchpadSong && sketchpadSong < ZynthboxSongCount && -1 < sketchpadTrack && sketchpadTrack < ZynthboxTrackCount && -1 < clip && clip < ZynthboxSlotCount) {
        return d->currentState.songs[sketchpadSong].tracks[sketchpadTrack].clips[clip].offset;
    }
    return 0;
}

void PlayfieldManager::startPlayback()
{
    d->playhead = -1;
    d->currentState.reset();
}

void PlayfieldManager::progressPlayback()
{
    d->handlePlaybackProgress();
}

void PlayfieldManager::stopPlayback()
{
    d->playhead = 0;
    d->nextBarState.reset(-1);
    d->currentState.reset();
}

// Since we've got a QObject up at the top that wants mocing
#include "PlayfieldManager.moc"
