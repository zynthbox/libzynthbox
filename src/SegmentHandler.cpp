/*
 * Copyright (C) 2022 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "SegmentHandler.h"
#include "PlayfieldManager.h"
#include "PlayGridManager.h"
#include "SequenceModel.h"

// Hackety hack - we don't need all the thing, just need to convince CAS it exists
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include "ClipAudioSource.h"
#include "ClipCommand.h"
#include "SyncTimer.h"
#include "TimerCommand.h"
#include "Plugin.h"

#include <QDebug>
#include <QTimer>
#include <QVariant>

class ZLSegmentHandlerSynchronisationManager;
class SegmentHandlerPrivate {
public:
    SegmentHandlerPrivate(SegmentHandler *q)
        : q(q)
    {
        syncTimer = SyncTimer::instance();
        playGridManager = PlayGridManager::instance();
    }
    SegmentHandler* q{nullptr};
    SyncTimer* syncTimer{nullptr};
    PlayGridManager* playGridManager{nullptr};
    ZLSegmentHandlerSynchronisationManager *zlSyncManager{nullptr};
    QList<SequenceModel*> sequenceModels;
    bool songMode{false};
    qint64 startOffset{0};
    qint64 duration{0};

    PlayfieldManager *playfieldManager{nullptr};
    qint64 playhead{0};
    int playheadSegment{0};
    QHash<qint64, QList<TimerCommand*> > playlist;
    QList<ClipAudioSource*> runningLoops;

    inline void ensureTimerClipCommand(TimerCommand* command) {
        if (command->dataParameter == nullptr) {
            // Since the clip command is swallowed each time, we'll need to reset it
            ClipCommand* clipCommand = syncTimer->getClipCommand();
            clipCommand->startPlayback = (command->operation == TimerCommand::StartClipLoopOperation); // otherwise, the inversion below ensures it's a stop clip loop operation, and this function requires either a start or stop operation
            clipCommand->stopPlayback = !clipCommand->startPlayback;
            clipCommand->midiChannel = command->parameter;
            clipCommand->clip = Plugin::instance()->getClipById(command->parameter2);
            clipCommand->midiNote = command->parameter3;
            clipCommand->changeVolume = true;
            clipCommand->volume = 1.0; // this matches how the ClipAudioSource::Play function works
            clipCommand->changeLooping = true;
            clipCommand->looping = true;
            command->operation = TimerCommand::ClipCommandOperation;
            command->dataParameter = clipCommand;
            // qDebug() << Q_FUNC_INFO << "Added clip command to timer command:" << command->dataParameter << clipCommand << "Start playback?" << clipCommand->startPlayback << "Stop playback?" << clipCommand->stopPlayback << clipCommand->midiChannel << clipCommand->midiNote << clipCommand->clip;
        }
    }

    void progressPlayback() {
        if (syncTimer->timerRunning() && songMode) {
            ++playhead;
            // Instead of using cumulative beat, we keep this one in hand so we don't have to juggle offsets of we start somewhere uneven
            if (playlist.contains(playhead)) {
                // qDebug() << Q_FUNC_INFO << "Playhead is now at" << playhead << "and we have things to do";
                const QList<TimerCommand*> commands = playlist[playhead];
                for (TimerCommand* command : commands) {
                    if (command->operation == TimerCommand::StartClipLoopOperation || command->operation == TimerCommand::StopClipLoopOperation) {
                        if (command->parameter2 < 1) {
                            // If there's no clip to start or stop looping, we should really just ignore the command
                            continue;
                        }
                        ensureTimerClipCommand(command);
                    }
                    if (command->operation == TimerCommand::StartPartOperation || command->operation == TimerCommand::StopPartOperation) {
                        // qDebug() << Q_FUNC_INFO << "Handling part start/stop operation immediately" << command;
                        handleTimerCommand(command);
                    } else if (command->operation == TimerCommand::StopPlaybackOperation) {
                        // Disconnect the global sequences, as we want them to stop making noises immediately
                        for (SequenceModel* sequence : qAsConst(sequenceModels)) {
                            sequence->disconnectSequencePlayback();
                            sequence->resetSequence();
                        }
                        // qDebug() << Q_FUNC_INFO << "Scheduled stop command" << command;
                        syncTimer->scheduleTimerCommand(0, TimerCommand::cloneTimerCommand(command));
                    } else {
                        // qDebug() << Q_FUNC_INFO << "Scheduled" << command << command->operation;
                        syncTimer->scheduleTimerCommand(0, TimerCommand::cloneTimerCommand(command));
                    }
                }
                ++playheadSegment;
                Q_EMIT q->playheadSegmentChanged();
            }
            Q_EMIT q->playheadChanged();
        }
    }

    inline void handleTimerCommand(TimerCommand* command) {
        // Yes, these are dangerous, but also we really, really want this to be fast
        if (command->operation == TimerCommand::StartPartOperation) {
            if (playfieldManager == nullptr) { playfieldManager = PlayfieldManager::instance(); }
//             qDebug() << Q_FUNC_INFO << "Timer command says to start part" << command->parameter << command->parameter2 << command->parameter3;
            playfieldManager->setClipPlaystate(0, command->parameter, command->parameter3, PlayfieldManager::PlayingState, PlayfieldManager::CurrentPosition, qint64(command->bigParameter));
        } else if(command->operation == TimerCommand::StopPartOperation) {
            if (playfieldManager == nullptr) { playfieldManager = PlayfieldManager::instance(); }
//             qDebug() << Q_FUNC_INFO << "Timer command says to stop part" << command->parameter << command->parameter2 << command->parameter3;
            playfieldManager->setClipPlaystate(0, command->parameter, command->parameter3, PlayfieldManager::StoppedState, PlayfieldManager::CurrentPosition);
        } else if (command->operation == TimerCommand::StopPlaybackOperation) {
            q->stopPlayback();
        }
    }

    void movePlayhead(qint64 newPosition, bool ignoreStop = false) {
        // Cycle through all positions from the current playhead
        // position to the new one and handle them all - but only
        // if the new position's actually different to the old one
        if (newPosition != playhead) {
            // qDebug() << Q_FUNC_INFO << "Moving playhead from" << playhead << "to" << newPosition;
            int direction = (playhead > newPosition) ? -1 : 1;
            while (playhead != newPosition) {
                playhead = playhead + direction;
                // qDebug() << Q_FUNC_INFO << "Moved playhead to" << playhead;
                if (playlist.contains(playhead)) {
                    const QList<TimerCommand*> commands = playlist[playhead];
                    if (commands.count() > 0) {
                        // When moving backward, we need to handle the stop and start commands in opposite direction
                        // Forward playback: Stop things first, then start things
                        // Backward playback: Start things first, then stop things
                        const int startCommand = (direction == -1) ? 0 : commands.count() - 1;
                        const int endCommand = (direction == -1) ? commands.count() : -1;
                        for (int commandIndex = startCommand; commandIndex != endCommand; commandIndex -= direction) {
                            TimerCommand *command = commands[commandIndex];
                            if (ignoreStop && command->operation == TimerCommand::StopPlaybackOperation) {
                                continue;
                            } else if (command->operation == TimerCommand::StartClipLoopOperation || command->operation == TimerCommand::StopClipLoopOperation) {
                                // If there's no clip to start or stop looping, we should really just ignore the command
                                if (command->parameter2 > 0) {
                                    TimerCommand *clonedCommand = TimerCommand::cloneTimerCommand(command);
                                    if (direction == -1) {
                                        clonedCommand->operation = (clonedCommand->operation == TimerCommand::StartClipLoopOperation) ? TimerCommand::StopClipLoopOperation : TimerCommand::StartClipLoopOperation;
                                    }
                                    ensureTimerClipCommand(clonedCommand);
                                    syncTimer->scheduleTimerCommand(0, clonedCommand);
                                }
                            } else {
                                if (direction == -1) {
                                    TimerCommand *clonedCommand = TimerCommand::cloneTimerCommand(command);
                                    clonedCommand->operation = (clonedCommand->operation == TimerCommand::StartPartOperation) ? TimerCommand::StopPartOperation : TimerCommand::StartPartOperation;
                                    handleTimerCommand(clonedCommand);
                                } else {
                                    handleTimerCommand(command);
                                }
                            }
                        }
                    }
                    playheadSegment = playheadSegment + direction;
                    Q_EMIT q->playheadSegmentChanged();
                }
            }
        }
        Q_EMIT q->playheadChanged();
    }
};

class ZLSegmentHandlerSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLSegmentHandlerSynchronisationManager(SegmentHandlerPrivate *d, SegmentHandler *parent = nullptr)
        : QObject(parent)
        , q(parent)
        , d(d)
    { };
    SegmentHandler *q{nullptr};
    SegmentHandlerPrivate* d{nullptr};
    QObject *zlSong{nullptr};
    QObject *zlSketchesModel{nullptr};
    QObject *zLSelectedSketch{nullptr};
    QObject *zLSegmentsModel{nullptr};
    QList<QObject*> zlChannels;

    void setZlSong(QObject *newZlSong) {
        // qDebug() << "Setting new song" << newZlSong;
        if (zlSong != newZlSong) {
            if (zlSong) {
                zlSong->disconnect(this);
                d->sequenceModels.clear();
            }
            zlSong = newZlSong;
            if (zlSong) {
                setZLSketchesModel(zlSong->property("sketchesModel").value<QObject*>());
                fetchSequenceModels();
            }
            updateChannels();
        }
    }

    void setZLSketchesModel(QObject *newZLSketchesModel) {
        // qDebug() << Q_FUNC_INFO << "Setting new sketches model:" << newZLSketchesModel;
        if (zlSketchesModel != newZLSketchesModel) {
            if (zlSketchesModel) {
                zlSketchesModel->disconnect(this);
            }
            zlSketchesModel = newZLSketchesModel;
            if (zlSketchesModel) {
                connect(zlSketchesModel, SIGNAL(selectedSketchIndexChanged()), this, SLOT(selectedSketchIndexChanged()), Qt::QueuedConnection);
                selectedSketchIndexChanged();
            }
        }
    }

    void setZLSelectedSketch(QObject *newSelectedSketch) {
        if (zLSelectedSketch != newSelectedSketch) {
            if (zLSelectedSketch) {
                zLSelectedSketch->disconnect(this);
                setZLSegmentsModel(nullptr);
            }
            zLSelectedSketch = newSelectedSketch;
            if (zLSelectedSketch) {
                connect(zLSelectedSketch, SIGNAL(segmentsModelChanged()), this, SLOT(selectedSegmentModelChanged()));
                setZLSegmentsModel(zLSelectedSketch->property("segmentsModel").value<QObject*>());
            }
        }
    }
    void setZLSegmentsModel(QObject *newSegmentsModel) {
        if (zLSegmentsModel != newSegmentsModel) {
            if (zLSegmentsModel) {
                zLSegmentsModel->disconnect(this);
            }
            zLSegmentsModel = newSegmentsModel;
            if (zLSegmentsModel) {
            }
        }
    }
    void updateChannels() {
        if (zlChannels.count() > 0) {
            zlChannels.clear();
        }
        if (zlSong) {
            QObject *channelsModel = zlSong->property("channelsModel").value<QObject*>();
            for (int channelIndex = 0; channelIndex < 10; ++channelIndex) {
                QObject *channel{nullptr};
                QMetaObject::invokeMethod(channelsModel, "getChannel", Q_RETURN_ARG(QObject*, channel), Q_ARG(int, channelIndex));
                if (channel) {
                    zlChannels << channel;
                }
            }
            // qDebug() << Q_FUNC_INFO << "Updated channels, we now keep a hold of" << zlChannels.count();
        }
    }
public Q_SLOTS:
    void selectedSketchIndexChanged() {
        int sketchIndex = zlSketchesModel->property("selectedSketchIndex").toInt();
        QObject *sketch{nullptr};
        QMetaObject::invokeMethod(zlSketchesModel, "getSketch", Qt::DirectConnection, Q_RETURN_ARG(QObject*, sketch), Q_ARG(int, sketchIndex));
        setZLSelectedSketch(sketch);
    }
    void fetchSequenceModels() {
        const QObjectList sequenceModels{d->playGridManager->getSequenceModels()};
        for (QObject *object : qAsConst(sequenceModels)) {
            SequenceModel *sequence{qobject_cast<SequenceModel*>(object)};
            if (sequence) {
                d->sequenceModels << sequence;
            } else {
                qWarning() << Q_FUNC_INFO << "Sequence in object" << object << "was apparently not a SequenceModel, and will be unavailable for playback management";
            }
        }
    }
    void selectedSegmentModelChanged() {
        setZLSegmentsModel(zLSelectedSketch->property("segmentsModel").value<QObject*>());
    }
    void updateSegments(qint64 stopAfter) {
        static const QLatin1String sampleLoopedType{"sample-loop"};
        QHash<qint64, QList<TimerCommand*> > playlist;
        if (d->songMode && zLSegmentsModel && zlChannels.count() > 0) {
            // The position of the next set of commands to be added to the hash
            qint64 segmentPosition{0};
            QList<QObject*> clipsInPrevious;
            int segmentCount = zLSegmentsModel->property("count").toInt();
            // qDebug() << Q_FUNC_INFO << "Working with" << segmentCount << "segments...";
            for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
                QObject *segment{nullptr};
                QMetaObject::invokeMethod(zLSegmentsModel, "get_segment", Qt::DirectConnection, Q_RETURN_ARG(QObject*, segment), Q_ARG(int, segmentIndex));
                if (segment) {
                    // qDebug() << Q_FUNC_INFO <<  "Working on segment at index" << segmentIndex;
                    QList<TimerCommand*> commands;
                    QVariantList clips = segment->property("clips").toList();
                    const QVariantList restartClipsData = segment->property("restartClips").toList();
                    QList<QObject*> restartClips;
                    for (const QVariant &clip : qAsConst(restartClipsData)) {
                        restartClips << clip.value<QObject*>();
                    }
                    QList<QObject*> includedClips;
                    for (const QVariant &variantClip : clips) {
                        QObject *clip = variantClip.value<QObject*>();
                        includedClips << clip;
                        // Set the playback offset if: Either we explicitly get asked to restart the clip, Or the clip wasn't in the previous segment
                        const bool shouldResetPlaybackposition{restartClips.contains(clip) || !clipsInPrevious.contains(clip)};
                        if (shouldResetPlaybackposition || !clipsInPrevious.contains(clip)) {
                            // qDebug() << Q_FUNC_INFO << "The clip" << clip << "was not in the previous segment, so we should start playing it";
                            // If the clip was not there in the previous step, that means we should turn it on
                            TimerCommand* command = new TimerCommand; // This does not need to use the pool, as we might make a LOT of these, and also don't do so during playback time.
                            command->parameter = clip->property("row").toInt();
                            const QObject *channelObject = zlChannels.at(command->parameter);
                            const QString trackType = channelObject->property("trackType").toString();
                            if (trackType == sampleLoopedType) {
                                command->operation = TimerCommand::StartClipLoopOperation;
                                command->parameter2 = clip->property("cppObjId").toInt();
                                command->parameter3 = 60;
                            } else {
                                command->operation = TimerCommand::StartPartOperation;
                                command->parameter2 = clip->property("column").toInt();
                                command->parameter3 = clip->property("part").toInt();
                                command->bigParameter = quint64(shouldResetPlaybackposition ? segmentPosition : 0);
                            }
                            commands << command;
                        } else {
                            // qDebug() << Q_FUNC_INFO << "Clip was already in the previous segment, leaving in";
                        }
                    }
                    for (QObject *clip : clipsInPrevious) {
                        if (!includedClips.contains(clip) || restartClips.contains(clip)) {
                            // qDebug() << Q_FUNC_INFO << "The clip" << clip << "was in the previous segment but not in this one, so we should stop playing that clip";
                            // If the clip was in the previous step, but not in this step, that means it
                            // should be turned off when reaching this position
                            TimerCommand* command = new TimerCommand; // This does not need to use the pool, as we might make a LOT of these, and also don't do so during playback time.
                            command->parameter = clip->property("row").toInt();
                            const QObject *channelObject = zlChannels.at(command->parameter);
                            const QString trackType = channelObject->property("trackType").toString();
                            if (trackType == sampleLoopedType) {
                                command->operation = TimerCommand::StopClipLoopOperation;
                                command->parameter2 = clip->property("cppObjId").toInt();
                                command->parameter3 = 60;
                            } else {
                                command->operation = TimerCommand::StopPartOperation;
                                command->parameter2 = clip->property("column").toInt();
                                command->parameter3 = clip->property("part").toInt();
                            }
                            commands << command;
                        }
                    }
                    clipsInPrevious = includedClips;
                    // TODO Sort commands before adding - we really kind of want stop things before the start things, for when we have restarting added
                    playlist[segmentPosition] = commands;
                    // Finally, make sure the next step is covered
                    qint64 segmentDuration = ((segment->property("barLength").toInt() * 4) + segment->property("beatLength").toInt()) * d->syncTimer->getMultiplier();
                    segmentPosition += segmentDuration;
                } else {
                    qWarning() << Q_FUNC_INFO << "Failed to get segment" << segmentIndex;
                }
                if (stopAfter > 0 && segmentPosition >= stopAfter) {
                    // qDebug() << Q_FUNC_INFO <<  "Stopping after the segment at index" << segmentIndex << "as we'll be stopping playback after" << stopAfter;
                    break;
                }
            }
            // qDebug() << Q_FUNC_INFO << "Done processing segments, adding the final stops for any ongoing clips, and the timer stop command";
            // Run through the clipsInPrevious segment and add commands to stop them all
            QList<TimerCommand*> commands;
            for (QObject *clip : clipsInPrevious) {
                // qDebug() << Q_FUNC_INFO << "The clip" << clip << "was in the final segment, so we should stop playing that clip at the end of playback";
                TimerCommand* command = new TimerCommand; // This does not need to use the pool, as we might make a LOT of these, and also don't do so during playback time.
                command->parameter = clip->property("row").toInt();
                const QObject *channelObject = zlChannels.at(command->parameter);
                const QString trackType = channelObject->property("trackType").toString();
                if (trackType == sampleLoopedType) {
                    command->operation = TimerCommand::StopClipLoopOperation;
                    command->parameter2 = clip->property("cppObjId").toInt();
                    command->parameter3 = 60;
                } else {
                    command->operation = TimerCommand::StopPartOperation;
                    command->parameter2 = clip->property("column").toInt();
                    command->parameter3 = clip->property("part").toInt();
                }
                commands << command;
            }
            // And finally, add one stop command right at the end, so playback will stop itself when we get to the end of the song
            TimerCommand *stopCommand = d->syncTimer->getTimerCommand();
            stopCommand->operation = TimerCommand::StopPlaybackOperation;
            commands << stopCommand;
            playlist[segmentPosition] = commands;
            d->duration = segmentPosition;
        } else {
            d->duration = 0;
        }
        Q_EMIT q->durationChanged();
        d->playlist = playlist;
    }
};

SegmentHandler::SegmentHandler(QObject *parent)
    : QObject(parent)
    , d(new SegmentHandlerPrivate(this))
{
    d->zlSyncManager = new ZLSegmentHandlerSynchronisationManager(d, this);
    connect(d->syncTimer, &SyncTimer::timerCommand, this, [this](TimerCommand* command){ d->handleTimerCommand(command); }, Qt::DirectConnection);
    connect(d->syncTimer, &SyncTimer::clipCommandSent, this, [this](ClipCommand* command) {
        // We don't bother clearing stuff that's been stopped, stopping a non-running clip is essentially an nop anyway
        if (command->startPlayback && !d->runningLoops.contains(command->clip)) {
            d->runningLoops << command->clip;
        }
    }, Qt::DirectConnection);
    connect(d->syncTimer, &SyncTimer::timerRunningChanged, this, [this](){
        if (!d->syncTimer->timerRunning()) {
            // First, stop any sounds currently running
            for (ClipAudioSource *clip : d->runningLoops) {
                // Less than the best thing - having to do this to ensure we stop the ones looper
                // queued for starting as well, otherwise they'll get missed for stopping... We'll
                // want to handle this more precisely later, but for now this should do the trick.
                ClipCommand *command = ClipCommand::globalCommand(clip);
                command->stopPlayback = true;
                d->syncTimer->scheduleClipCommand(command, 0);
                for (int i = 0; i < 10; ++i) {
                    command = ClipCommand::channelCommand(clip, i);
                    command->midiNote = 60;
                    command->stopPlayback = true;
                    d->syncTimer->scheduleClipCommand(command, 0);
                }
            }
        }
    }, Qt::QueuedConnection);
}

SegmentHandler::~SegmentHandler()
{
    delete d;
}

void SegmentHandler::setSong(QObject *song)
{
    if (d->zlSyncManager->zlSong != song) {
        d->zlSyncManager->setZlSong(song);
        Q_EMIT songChanged();
    }
}

QObject *SegmentHandler::song() const
{
    return d->zlSyncManager->zlSong;
}

bool SegmentHandler::songMode() const
{
    return d->songMode;
}

int SegmentHandler::playhead() const
{
    return d->playhead;
}

qint64 SegmentHandler::duration() const
{
    return d->duration;
}

int SegmentHandler::playheadSegment() const
{
    return d->playheadSegment;
}

void SegmentHandler::startPlayback(qint64 startOffset, quint64 duration)
{
    d->songMode = true;
    Q_EMIT songModeChanged();
    d->startOffset = startOffset;
    if (duration == 0) {
        d->zlSyncManager->updateSegments(0);
    } else {
        d->zlSyncManager->updateSegments(startOffset + qint64(duration));
    }
    // If we're starting with a new playfield anyway, we want to ensure the first movement also catches that first position, so start counting for the playhead at a logical -1 position with nothing on it
    d->playhead = -1;
    d->playheadSegment = -1;
    // Since SegmentHandler works directly on the current data, ensure PlayfieldManager is in a correctly stopped state before operating on it
    PlayfieldManager::instance()->stopPlayback();
    d->movePlayhead(startOffset, true);
    if (d->duration > 0) {
        if (duration > 0) {
            TimerCommand *stopCommand = d->syncTimer->getTimerCommand();
            stopCommand->operation = TimerCommand::StopPlaybackOperation;
            d->syncTimer->scheduleTimerCommand(duration, stopCommand);
        }
        // Hook up the global sequences to playback
        const QObjectList sequenceModels{d->playGridManager->getSequenceModels()};
        for (QObject *object : qAsConst(sequenceModels)) {
            SequenceModel *sequence{qobject_cast<SequenceModel*>(object)};
            if (sequence) {
                sequence->prepareSequencePlayback();
            } else {
                qWarning() << Q_FUNC_INFO << "Sequence in object" << object << "was apparently not a SequenceModel, and playback could not be prepared";
            }
        }
        d->playGridManager->hookUpTimer();
        d->syncTimer->start();
    }
}

qint64 SegmentHandler::startOffset() const
{
    return d->startOffset;
}

void SegmentHandler::stopPlayback()
{
    // Disconnect the global sequences
    for (SequenceModel* sequence : qAsConst(d->sequenceModels)) {
        sequence->disconnectSequencePlayback();
    }
    d->playGridManager->stopMetronome();
    d->movePlayhead(-1, true);
    PlayfieldManager::instance()->stopPlayback();
    d->songMode = false;
    Q_EMIT songModeChanged();
}

void SegmentHandler::progressPlayback() const
{
    d->progressPlayback();
}

// Since we've got a QObject up at the top that wants mocing
#include "SegmentHandler.moc"
