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
#include "SegmentHandler.h"
#include "PlayfieldManager.h"
#include "ZynthboxBasics.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>

// Hackety hack - we don't need all the thing, just need some storage things (MidiBuffer and MidiNote specifically)
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_audio_formats/juce_audio_formats.h>

#include "ClipCommand.h"
#include "ClipAudioSource.h"
#include "MidiRouter.h"
#include "SyncTimer.h"
#include "TimerCommand.h"
#include "Plugin.h"

static const QString midiNoteNames[128]{
    "C-1", "C#-1", "D-1", "D#-1", "E-1", "F-1", "F#-1", "G-1", "G#-1", "A-1", "A#-1", "B-1",
    "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",
    "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",
    "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
    "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",
    "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
    "C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",
    "C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9"
};

struct NewNoteData {
    quint64 timestamp{0}; // Position in timer ticks
    quint64 timestampOffset{0}; // Offset in jack frames
    quint64 endTimestamp{0}; // Position in timer ticks
    quint64 endTimestampOffset{0}; // Offset in jack frames
    int step{0};
    int midiNote{0};
    int velocity{0};
    int duration{0};
    int delay{0};
    int row{0};
    int column{0};
};

class ZLPatternSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLPatternSynchronisationManager(PatternModel *parent = nullptr)
        : QObject(parent)
        , q(parent)
    {
        layerDataPuller = new QTimer(this);
        layerDataPuller->setInterval(100);
        layerDataPuller->setSingleShot(true);
        connect(layerDataPuller, &QTimer::timeout, this, &ZLPatternSynchronisationManager::retrieveLayerData, Qt::QueuedConnection);
        syncTimer = SyncTimer::instance();
    };
    PatternModel *q{nullptr};
    SyncTimer *syncTimer{nullptr};
    QObject *zlChannel{nullptr};
    QObject *zlPart{nullptr};
    QObject *zlScene{nullptr};
    QObject *zlDashboard{nullptr};
    QTimer *layerDataPuller{nullptr};

    bool channelMuted{false};
    bool channelOneToOnePlayback{false}; // Whether sample picking should be done equal to the clip position of the pattern (so pattern for clip a only plays samples in slot 1, and patterns for clip c only plays samples in slot 3)
    void setZlChannel(QObject *newZlChannel)
    {
        if (zlChannel != newZlChannel) {
            if (zlChannel) {
                zlChannel->disconnect(this);
            }
            zlChannel = newZlChannel;
            if (zlChannel) {
                connect(zlChannel, SIGNAL(channel_audio_type_changed()), this, SLOT(channelAudioTypeChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(channel_audio_type_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(externalMidiChannelChanged()), this, SLOT(externalMidiChannelChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(samples_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(selectedPartChanged()), this, SLOT(selectedPartChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(chained_sounds_changed()), this, SLOT(chainedSoundsChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(chainedSoundsAcceptedChannelsChanged()), this, SLOT(chainedSoundsChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(chained_sounds_changed()), layerDataPuller, SLOT(start()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(recordingPopupActiveChanged()), this, SIGNAL(recordingPopupActiveChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(mutedChanged()), this, SLOT(mutedChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(channel_routing_style_changed()), this, SLOT(routingStyleChanged()), Qt::QueuedConnection);
                q->setMidiChannel(zlChannel->property("id").toInt());
                channelAudioTypeChanged();
                externalMidiChannelChanged();
                updateSamples();
                selectedPartChanged();
                layerDataPuller->start();
                chainedSoundsChanged();
                routingStyleChanged();
            }
            mutedChanged();
            Q_EMIT q->zlChannelChanged();
        }
    }

    void setZlPart(QObject *newZlPart)
    {
        if (zlPart != newZlPart) {
            if (zlPart) {
                zlPart->disconnect(this);
            }
            zlPart = newZlPart;
            Q_EMIT q->zlPartChanged();
            if (zlPart) {
                connect(zlPart, SIGNAL(samples_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                updateSamples();
            }
        }
    }

    void setZlScene(QObject *newZlScene)
    {
        if (zlScene != newZlScene) {
            if (zlScene) {
                zlScene->disconnect(this);
            }
            zlScene = newZlScene;
            if (zlScene) {
                connect(zlScene, SIGNAL(enabled_changed(int, int)), this, SLOT(sceneEnabledChanged()), Qt::QueuedConnection);
                // This seems superfluous...
//                 connect(zlChannel, SIGNAL(enabled_changed()), this, SLOT(selectedPartChanged()), Qt::QueuedConnection);
                sceneEnabledChanged();
            }
            Q_EMIT q->zlSceneChanged();
        }
    }

    void setZlDashboard(QObject *newZlDashboard) {
        if (zlDashboard != newZlDashboard) {
            if (zlDashboard) {
                zlDashboard->disconnect(this);
            }
            zlDashboard = newZlDashboard;
            if (zlDashboard) {
                connect(zlDashboard, SIGNAL(selected_channel_changed()), this, SLOT(selectedPartChanged()), Qt::QueuedConnection);
                selectedPartChanged();
            }
        }
    }

    Q_SIGNAL void recordingPopupActiveChanged();

public Q_SLOTS:
    void sceneEnabledChanged() {
        q->setEnabled(zlScene->property("enabled").toBool());
    }
    void channelAudioTypeChanged() {
        static const QLatin1String sampleTrig{"sample-trig"};
        static const QLatin1String sampleSlice{"sample-slice"};
        static const QLatin1String sampleLoop{"sample-loop"};
        static const QLatin1String external{"external"};
//         static const QLatin1String synth{"synth"}; // the default
        const QString channelAudioType = zlChannel->property("channelAudioType").toString();
        TimerCommand* timerCommand = syncTimer->getTimerCommand();
        timerCommand->operation = TimerCommand::SamplerChannelEnabledStateOperation;
        timerCommand->parameter = q->channelIndex();
        if (channelAudioType == sampleTrig) {
            q->setNoteDestination(PatternModel::SampleTriggerDestination);
            timerCommand->parameter2 = true;
        } else if (channelAudioType == sampleSlice) {
            q->setNoteDestination(PatternModel::SampleSlicedDestination);
            timerCommand->parameter2 = true;
        } else if (channelAudioType == sampleLoop) {
            q->setNoteDestination(PatternModel::SampleLoopedDestination);
            timerCommand->parameter2 = true;
        } else if (channelAudioType == external) {
            q->setNoteDestination(PatternModel::ExternalDestination);
            timerCommand->parameter2 = false;
        } else { // or in other words "if (channelAudioType == synth)"
            q->setNoteDestination(PatternModel::SynthDestination);
            timerCommand->parameter2 = false;
        }
        syncTimer->scheduleTimerCommand(0, timerCommand);
    }
    void externalMidiChannelChanged() {
        q->setExternalMidiChannel(zlChannel->property("externalMidiChannel").toInt());
    }
    void selectedPartChanged() {
        SequenceModel *sequence = qobject_cast<SequenceModel*>(q->sequence());
        if (sequence && zlChannel && zlDashboard) {
            const int channelId{zlDashboard->property("selectedChannel").toInt()};
            const int selectedPart{zlChannel->property("selectedPart").toInt()};
            sequence->setActiveChannel(channelId, selectedPart);
        }
    }
    void updateSamples() {
        QVariantList clipIds;
        if (zlChannel && zlPart) {
            const QVariantList channelSamples = zlChannel->property("samples").toList();
            const QVariantList partSamples = zlPart->property("samples").toList();
            int sampleIndex{0};
            for (const QVariant& partSample : partSamples) {
                int sampleCppId{-1};
                // If we are in sample-trig mode, we want all five samples, otherwise we only want the equivalent sample to our associated part
                if (q->noteDestination() == PatternModel::SampleTriggerDestination || sampleIndex == q->partIndex()) {
                    const QObject *sample = channelSamples[partSample.toInt()].value<QObject*>();
                    if (sample) {
                        sampleCppId = sample->property("cppObjId").toInt();
                    }
                }
                clipIds << sampleCppId;
                ++sampleIndex;
            }
        }
        q->setClipIds(clipIds);
    }
    void chainedSoundsChanged() {
        if (zlChannel) {
            QList<int> chainedSounds;
            const QVariantList channelChainedSounds = zlChannel->property("chainedSounds").toList();
            const QVariantList channelChainedSoundsAcceptedChannels = zlChannel->property("chainedSoundsAcceptedChannels").toList();
            int index{0};
            for (const QVariant &channelChainedSound : channelChainedSounds) {
                const int chainedSound = channelChainedSound.toInt();
                if (chainedSound > -1) {
                    chainedSounds << chainedSound;
                    QList<int> acceptedChannelsActual;
                    const QVariantList &acceptedChannels = channelChainedSoundsAcceptedChannels[index].toList();
                    for (const QVariant &acceptedChannel : acceptedChannels) {
                        acceptedChannelsActual << acceptedChannel.toInt();
                    }
                    MidiRouter::instance()->setZynthianSynthAcceptedChannels(chainedSound, acceptedChannelsActual);
                }
                ++index;
            }
            MidiRouter::instance()->setZynthianChannels(q->channelIndex(), chainedSounds);
        }
    }
    void routingStyleChanged() {
        if (zlChannel) {
            channelOneToOnePlayback = (zlChannel->property("channelRoutingStyle").toString() == "one-to-one");
        } else {
            channelOneToOnePlayback = false;
        }
    }
    void mutedChanged() {
        if (zlChannel) {
            channelMuted = zlChannel->property("muted").toBool();
        } else {
            channelMuted = false;
        }
    }
    void retrieveLayerData() {
        if (zlChannel) {
            QString jsonSnapshot;
            QMetaObject::invokeMethod(zlChannel, "getChannelSoundSnapshotJson", Qt::DirectConnection, Q_RETURN_ARG(QString, jsonSnapshot));
            q->setLayerData(jsonSnapshot);
        }
    }

    void addRecordedNote(void* recordedNote);
};

#define NoteDataPoolSize 128
struct alignas(32) NoteDataPoolEntry {
    NewNoteData *object{nullptr};
    NoteDataPoolEntry *previous{nullptr};
    NoteDataPoolEntry *next{nullptr};
};

class PatternModel::Private {
public:
    Private(PatternModel *q) : q(q) {
        playGridManager = PlayGridManager::instance();
        playfieldManager = PlayfieldManager::instance();
        syncTimer = qobject_cast<SyncTimer*>(playGridManager->syncTimer());

        NoteDataPoolEntry* noteDataPrevious{&noteDataPool[NoteDataPoolSize - 1]};
        for (quint64 i = 0; i < NoteDataPoolSize; ++i) {
            noteDataPrevious->next = &noteDataPool[i];
            noteDataPool[i].previous = noteDataPrevious;
            noteDataPrevious = &noteDataPool[i];
        }
        noteDataPoolReadHead = noteDataPoolWriteHead = noteDataPool;

        beatSubdivision = syncTimer->getMultiplier();
        beatSubdivision2 = beatSubdivision / 2;
        beatSubdivision3 = beatSubdivision2 / 2;
        beatSubdivision4 = beatSubdivision3 / 2;
        beatSubdivision5 = beatSubdivision4 / 2;
        beatSubdivision6 = beatSubdivision5 / 2;
        patternTickToSyncTimerTick = beatSubdivision6;
    }
    ~Private() {
        for (int i = 0; i < NoteDataPoolSize; ++i) {
            delete noteDataPool[i].object;
        }
    }
    PatternModel *q{nullptr};
    ZLPatternSynchronisationManager *zlSyncManager{nullptr};
    SegmentHandler *segmentHandler{nullptr};
    PlayfieldManager *playfieldManager{nullptr};
    QHash<QString, qint64> lastSavedTimes;
    int width{16};
    PatternModel::NoteDestination noteDestination{PatternModel::SynthDestination};
    int midiChannel{15};
    int externalMidiChannel{-1};
    QString layerData;
    int defaultNoteDuration{0};
    int noteLength{3};
    int swing{0};
    int availableBars{1};
    int activeBar{0};
    int bankOffset{0};
    int bankLength{8};
    bool enabled{true};
    bool isPlaying{false};
    int playingRow{0};
    int playingColumn{0};
    int previouslyUpdatedMidiChannel{-1};
    bool updateMostRecentStartTimestamp{true};
    qint64 mostRecentStartTimestamp{0};

    juce::MidiBuffer &getOrCreateBuffer(QHash<int, juce::MidiBuffer> &collection, int position);
    void noteLengthDetails(int noteLength, qint64 &nextPosition, bool &relevantToUs, qint64 &noteDuration);
    int beatSubdivision{0};
    int beatSubdivision2{0};
    int beatSubdivision3{0};
    int beatSubdivision4{0};
    int beatSubdivision5{0};
    int beatSubdivision6{0};
    int patternTickToSyncTimerTick{0};

    bool recordingLive{false};
    QString liveRecordingSource;
    // First look at the external device index - if we're listening only to that, make sure we're doing that first
    int liveRecordingSourceExternalDeviceIndex{-1};
    // Then check the sketchpad track setting, and if that is set explicitly, handle that, otherwise just go with the pattern's own track
    int liveRecordingSourceSketchpadTrack{-1};
    QList<NewNoteData*> recordingLiveNotes;
    NoteDataPoolEntry noteDataPool[128];
    NoteDataPoolEntry *noteDataPoolReadHead{nullptr};
    NoteDataPoolEntry *noteDataPoolWriteHead{nullptr};

    // This bunch of lists is equivalent to the data found in each note, and is
    // stored per-position (index in the outer is row * width + column). The
    // must be cleared on any change of the notes (which should always be done
    // through setNote and setMetadata to ensure this). If they are not cleared
    // on changes, what ends up sent to SyncTimer during playback will not match
    // what the model contains. So, remember your pattern hygiene and clean your
    // buffers!
    // The inner hash contains commands for the given position, with the key being
    // the on-position delay (so that iterating over the hash gives the scheduling
    // delay for that buffer, and the buffer).
    QHash<int, QHash<int, juce::MidiBuffer> > positionBuffers;
    // Handy variable in case we want to adjust how far ahead we're looking sometime
    // in the future (right now it's one step ahead, but we could look further if we
    // wanted to)
    static const int lookaheadAmount{2};
    /**
     * \brief Invalidates the position buffers relevant to the given position
     * If you give -1 for the two position indicators, the entire list of buffers
     * will be invalidated.
     * This function is required to ensure that all buffers the position could
     * have an impact on (including those which are before it) are invalidated.
     * @param row The row of the position to invalidate
     * @param column The column of the position to invalidate
     */
   void invalidatePosition(int row = -1, int column = -1) {
        if (row == -1 || column == -1) {
            positionBuffers.clear();
        } else {
            const int basePosition = (row * width) + column;
            for (int subsequentNoteIndex = 0; subsequentNoteIndex < lookaheadAmount; ++subsequentNoteIndex) {
                // We clear backwards, just because might as well (by subtracting the subsequentNoteIndex from our base position)
                int ourPosition = (basePosition - subsequentNoteIndex) % (availableBars * width);
                positionBuffers.remove(ourPosition);
            }
        }
    }

    SyncTimer* syncTimer{nullptr};
    SequenceModel *sequence;
    int song{0}; // This is just... always zero at the moment, but maybe this would be the global sequence id or something like that?
    int channelIndex{-1};
    int partIndex{-1};

    PlayGridManager *playGridManager{nullptr};

    int gridModelStartNote{48};
    int gridModelEndNote{64};
    NotesModel *gridModel{nullptr};
    NotesModel *clipSliceNotes{nullptr};
    QList<ClipAudioSource*> clips;
    ClipCommandRing commandRing;
    /**
     * This function will return all clip sin the list which has a
     * keyZoneStart higher or equal to the given midi note and a keyZoneEnd
     * lower or equal to the given midi note (that is, all clips for
     * which the midi note is inside the keyzone).
     * @param midiNote The midi note to find a clip for
     * @return The list of clip audio source instances that matches the given midi note (list can be empty)
     */
    QList<ClipAudioSource*> clipsForMidiNote(int midiNote) const {
        QList<ClipAudioSource*> found;
        for (ClipAudioSource *needle : qAsConst(clips)) {
            if (needle && needle->keyZoneStart() <= midiNote && midiNote <= needle->keyZoneEnd()) {
                found << needle;
            }
        }
        return found;
    }
    /**
     * Writes any ClipCommands which match the midi message passed to the function to the list also passed in
     * @param listToPopulate The command ring that should have commands written to it
     * @param byte1 The first byte of a midi message (this is expected to be a channel message)
     * @param byte2 The seconds byte of a midi message
     * @param byte3 The third byte of a midi message
     */
    void midiMessageToClipCommands(ClipCommandRing *listToPopulate, const int &byte1, const int &byte2, const int &byte3) const {
        int clipIndex{0};
        for (ClipAudioSource *clip : qAsConst(clips)) {
            // There must be a clip or it just doesn't matter, and then the note must fit inside the clip's keyzone
            if (clip && clip->keyZoneStart() <= byte2 && byte2 <= clip->keyZoneEnd() && (zlSyncManager->channelOneToOnePlayback == false || clipIndex == partIndex)) {
                ClipCommand *command = ClipCommand::channelCommand(clip, (byte1 & 0xf));
                command->startPlayback = byte1 > 0x8F;
                command->stopPlayback = byte1 < 0x90;
                if (command->startPlayback) {
                    command->changeVolume = true;
                    command->volume = float(byte3) / float(127);
                }
                if (command->stopPlayback) {
                    // Don't actually set volume, just store the volume for velocity purposes... yes this is kind of a hack
                    command->volume = float(byte3) / float(127);
                }
                if (noteDestination == SampleSlicedDestination) {
                    command->midiNote = 60;
                    command->changeSlice = true;
                    command->slice = clip->sliceForMidiNote(byte2);
                } else {
                    command->midiNote = byte2;
                    command->changeLooping = true;
                    command->looping = clip->looping();
                }
                listToPopulate->write(command, 0);
            }
        }
        ++clipIndex;
    }
};

PatternModel::PatternModel(SequenceModel* parent)
    : NotesModel(parent ? parent->playGridManager() : nullptr)
    , d(new Private(this))
{
    d->zlSyncManager = new ZLPatternSynchronisationManager(this);
    d->segmentHandler = SegmentHandler::instance();
    connect(d->syncTimer, &SyncTimer::timerRunningChanged, this, [this](){
        if (d->syncTimer->timerRunning() == false) {
            setRecordLive(false);
        }
    });

    auto updateIsPlaying = [this](){
        bool isPlaying{false};
        if (d->segmentHandler->songMode()) {
            isPlaying = d->playfieldManager->clipPlaystate(d->song, d->channelIndex, d->partIndex) == PlayfieldManager::PlayingState;
        } else if (d->sequence && d->sequence->isPlaying()) {
            if (d->sequence->soloPattern() > -1) {
                isPlaying = (d->sequence->soloPatternObject() == this);
            } else {
                isPlaying = d->playfieldManager->clipPlaystate(d->song, d->channelIndex, d->partIndex) == PlayfieldManager::PlayingState;
            }
        } else {
            isPlaying = false;
        }
        if (d->isPlaying != isPlaying) {
            d->isPlaying = isPlaying;
            if (isPlaying) {
                d->updateMostRecentStartTimestamp = true;
            }
            QMetaObject::invokeMethod(this, "isPlayingChanged", Qt::QueuedConnection);
        }
    };
    connect(d->playfieldManager, &PlayfieldManager::directPlayfieldStateChanged, this, [this,updateIsPlaying](int song, int track, int part){
        if (d->sequence && song == d->song && track == d->channelIndex && part == d->partIndex) {
            updateIsPlaying();
        }
    }, Qt::DirectConnection);
    connect(d->segmentHandler, &SegmentHandler::songModeChanged, this, updateIsPlaying, Qt::DirectConnection);

    // We need to make sure that we support orphaned patterns (that is, a pattern that is not contained within a sequence)
    d->sequence = parent;
    if (parent) {
        connect(d->sequence, &SequenceModel::isPlayingChanged, this, updateIsPlaying, Qt::DirectConnection);
        connect(d->sequence, &SequenceModel::soloPatternChanged, this, updateIsPlaying, Qt::DirectConnection);
        // This is to ensure that when the current sound changes and we have no midi channel, we will schedule
        // the notes that are expected of us
        connect(d->sequence->playGridManager(), &PlayGridManager::currentMidiChannelChanged, this, [this](){
            if (d->midiChannel == 15 && d->sequence->playGridManager()->currentMidiChannel() > -1) {
                d->invalidatePosition();
            }
        });
        connect(d->sequence, &SequenceModel::isLoadingChanged, this, [=](){
            if (!d->sequence->isLoading()) {
                beginResetModel();
                endResetModel();
                gridModel();
                clipSliceNotes();
            }
        });
        // If we are currently recording live into this pattern, and the user switches away from it, turn off the live
        // recording, so we avoid doing changes to things the user's not looking at.
        connect(d->sequence, &SequenceModel::activePatternChanged, this, [this](){
            if (d->recordingLive && d->sequence->activePatternObject() != this) {
                setRecordLive(false);
            }
        });
    }
    // This will force the creation of a whole bunch of rows with the desired width and whatnot...
    setHeight(16);

    connect(this, &PatternModel::noteDestinationChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::midiChannelChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::layerDataChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::noteLengthChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::swingChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::availableBarsChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::activeBarChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::bankOffsetChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::bankLengthChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::enabledChanged, this, &NotesModel::registerChange);

    connect(this, &QObject::objectNameChanged, this, &PatternModel::nameChanged);
    connect(this, &QObject::objectNameChanged, this, &PatternModel::thumbnailUrlChanged);
    connect(this, &NotesModel::lastModifiedChanged, this, &PatternModel::hasNotesChanged);
    connect(this, &NotesModel::lastModifiedChanged, this, &PatternModel::thumbnailUrlChanged);
    connect(this, &PatternModel::bankOffsetChanged, this, &PatternModel::thumbnailUrlChanged);
    connect(this, &PatternModel::bankLengthChanged, this, &PatternModel::thumbnailUrlChanged);
    static const int noteDestinationTypeId = qRegisterMetaType<NoteDestination>();
    Q_UNUSED(noteDestinationTypeId)

    // Called whenever the effective midi channel changes (so both the midi channel and the external midi channel)
    QTimer* midiChannelUpdater = new QTimer(this);
    midiChannelUpdater->setInterval(100);
    midiChannelUpdater->setSingleShot(true);
    connect(midiChannelUpdater, &QTimer::timeout, this, [this](){
        int actualChannel = d->noteDestination == PatternModel::ExternalDestination && d->externalMidiChannel > -1 ? d->externalMidiChannel : d->midiChannel;
        MidiRouter::RoutingDestination routerDestination{MidiRouter::ZynthianDestination};
        switch(d->noteDestination) {
            case PatternModel::SampleSlicedDestination:
            case PatternModel::SampleTriggerDestination:
                routerDestination = MidiRouter::SamplerDestination;
                break;
            case PatternModel::ExternalDestination:
                routerDestination = MidiRouter::ExternalDestination;
                break;
            case PatternModel::SampleLoopedDestination:
            case PatternModel::SynthDestination:
            default:
                // Default destination
                break;
        }
        if (zlChannel() && zlChannel()->property("recordingPopupActive").toBool()) {
            // Recording Popup is active. Do connect midi channel to allow recording even if channel mode is trig/slice
            MidiRouter::instance()->setSkechpadTrackDestination(d->midiChannel, MidiRouter::ZynthianDestination, actualChannel == d->midiChannel ? -1 : actualChannel);
        } else {
            MidiRouter::instance()->setSkechpadTrackDestination(d->midiChannel, routerDestination, actualChannel == d->midiChannel ? -1 : actualChannel);
        }
        if (d->previouslyUpdatedMidiChannel != d->midiChannel) {
            startLongOperation();
            for (int row = 0; row < rowCount(); ++row) {
                for (int column = 0; column < columnCount(createIndex(row, 0)); ++column) {
                    Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
                    QVariantList newSubnotes;
                    if (oldCompound) {
                        const QVariantList &oldSubnotes = oldCompound->subnotes();
                        if (oldSubnotes.count() > 0) {
                            for (const QVariant &subnote :oldCompound->subnotes()) {
                                Note *oldNote = subnote.value<Note*>();
                                if (oldNote) {
                                    newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(oldNote->midiNote(), d->midiChannel));
                                } else {
                                    // This really shouldn't happen - spit out a warning and slap in something unknown so we keep the order intact
                                    newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(0, d->midiChannel));
                                    qWarning() << "Failed to convert a subnote value which must be a Note object to a Note object - something clearly isn't right.";
                                }
                            }
                            setNote(row, column, playGridManager()->getCompoundNote(newSubnotes));
                        }
                    }
                }
            }
            endLongOperation();
            d->invalidatePosition();
            d->previouslyUpdatedMidiChannel = d->midiChannel;
        }
    });
    connect(this, &PatternModel::midiChannelChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));
    connect(this, &PatternModel::externalMidiChannelChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));
    connect(this, &PatternModel::noteDestinationChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));
    connect(d->zlSyncManager, &ZLPatternSynchronisationManager::recordingPopupActiveChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));

    connect(d->playGridManager, &PlayGridManager::midiMessage, this, &PatternModel::handleMidiMessage, Qt::DirectConnection);
    connect(SyncTimer::instance(), &SyncTimer::clipCommandSent, this, [this](ClipCommand *clipCommand){
        for (ClipAudioSource *needle : qAsConst(d->clips)) {
            if (needle && needle == clipCommand->clip) {
                Note *note = qobject_cast<Note*>(PlayGridManager::instance()->getNote(clipCommand->midiNote, d->midiChannel));
                if (note) {
                    if (clipCommand->stopPlayback) {
                        note->registerOff(d->midiChannel);
                    }
                    if (clipCommand->startPlayback) {
                        note->registerOn(d->midiChannel);
                    }
                }
                break;
            }
        }
    }, Qt::QueuedConnection);
}

PatternModel::~PatternModel()
{
    delete d;
}

void PatternModel::cloneOther(PatternModel *otherPattern)
{
    if (otherPattern) {
        clear();
        setWidth(otherPattern->width());
        setHeight(otherPattern->height());
        setLayerData(otherPattern->layerData());
        setNoteLength(otherPattern->noteLength());
        setAvailableBars(otherPattern->availableBars());
        setActiveBar(otherPattern->activeBar());
        setBankOffset(otherPattern->bankOffset());
        setBankLength(otherPattern->bankLength());
        setEnabled(otherPattern->enabled());

        // Now clone all the notes
        for (int i = 0; i < rowCount(); ++i) {
            setRowData(i, otherPattern->getRow(i), otherPattern->getRowMetadata(i));
        }
    }
}

int PatternModel::subnoteIndex(int row, int column, int midiNote) const
{
    int result{-1};
    if (row > -1 && row < height() && column > -1 && column < width()) {
        const Note* note = qobject_cast<Note*>(getNote(row, column));
        if (note) {
            for (int i = 0; i < note->subnotes().count(); ++i) {
                const Note* subnote = note->subnotes().at(i).value<Note*>();
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
        if (newNote->sketchpadTrack() != d->midiChannel) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->midiChannel));
        }

        subnotes.append(QVariant::fromValue<QObject*>(newNote));
        metadata.append(QVariantHash());
        setNote(row, column, playGridManager()->getCompoundNote(subnotes));
        setMetadata(row, column, metadata);
    }
    return newPosition;
}

void PatternModel::insertSubnote(int row, int column, int subnoteIndex, QObject *note)
{
    if (row > -1 && row < height() && column > -1 && column < width() && note) {
        Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
        QVariantList subnotes;
        QVariantList metadata;
        int actualPosition{0};
        if (oldCompound) {
            subnotes = oldCompound->subnotes();
            metadata = getMetadata(row, column).toList();
            actualPosition = qMin(subnoteIndex, subnotes.count());
        }

        // Ensure the note is correct according to our midi channel settings
        Note *newNote = qobject_cast<Note*>(note);
        if (newNote->sketchpadTrack() != d->midiChannel) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->midiChannel));
        }

        subnotes.insert(actualPosition, QVariant::fromValue<QObject*>(newNote));
        metadata.insert(actualPosition, QVariantHash());
        setNote(row, column, playGridManager()->getCompoundNote(subnotes));
        setMetadata(row, column, metadata);
    }
}

int PatternModel::insertSubnoteSorted(int row, int column, QObject* note)
{
    int newPosition{0};
    if (row > -1 && row < height() && column > -1 && column < width() && note) {
        Note *newNote = qobject_cast<Note*>(note);
        Note* oldCompound = qobject_cast<Note*>(getNote(row, column));
        QVariantList subnotes;
        QVariantList metadata;
        if (oldCompound) {
            subnotes = oldCompound->subnotes();
            metadata = getMetadata(row, column).toList();
            for (int i = 0; i < subnotes.count(); ++i) {
                const Note* subnote = subnotes[i].value<Note*>();
                if (subnote->midiNote() <= newNote->midiNote()) {
                    newPosition = i + 1;
                } else {
                    break;
                }
            }
        }

        // Ensure the note is correct according to our midi channel settings
        if (newNote->sketchpadTrack() != d->midiChannel) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->midiChannel));
        }

        subnotes.insert(newPosition, QVariant::fromValue<QObject*>(newNote));
        metadata.insert(newPosition, QVariantHash());
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
            if (key.isEmpty()) {
                const QVariantHash rawMeta = metadata.at(subnote).toHash();
                QVariantMap qmlFriendlyMeta;
                for (const QString &key : rawMeta.keys()) {
                    qmlFriendlyMeta[key] = rawMeta[key];
                }
                result.setValue(qmlFriendlyMeta);
            } else {
                result.setValue(metadata.at(subnote).toHash().value(key));
            }
        }
    }
    return result;
}

void PatternModel::setNote(int row, int column, QObject* note)
{
    d->invalidatePosition(row, column);
    NotesModel::setNote(row, column, note);
}

void PatternModel::setMetadata(int row, int column, QVariant metadata)
{
    d->invalidatePosition(row, column);
    NotesModel::setMetadata(row, column, metadata);
}

void PatternModel::resetPattern(bool clearNotes)
{
    startLongOperation();
    setNoteDestination(PatternModel::SynthDestination);
    setExternalMidiChannel(-1);
    setDefaultNoteDuration(0);
    setNoteLength(3);
    setSwing(0);
    setAvailableBars(1);
    setBankOffset(0);
    setBankLength(8);
    setGridModelStartNote(48);
    setGridModelEndNote(64);
    setWidth(16);
    if (clearNotes && hasNotes()) {
        setHeight(0);
    }
    setHeight(16);
    endLongOperation();
}

void PatternModel::clear()
{
    startLongOperation();
    const int oldHeight = height();
    setHeight(0);
    setHeight(oldHeight);
    endLongOperation();
}

void PatternModel::clearRow(int row)
{
    startLongOperation();
    for (int column = 0; column < d->width; ++column) {
        setNote(row, column, nullptr);
        setMetadata(row, column, QVariantList());
    }
    endLongOperation();
}

void PatternModel::clearBank(int bank)
{
    startLongOperation();
    for (int i = 0; i < bankLength(); ++i) {
        clearRow((bankLength() * bank) + i);
    }
    endLongOperation();
}

void PatternModel::setWidth(int width)
{
    startLongOperation();
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
    endLongOperation();
}

bool PatternModel::exportToFile(const QString &fileName) const
{
    bool success{false};
    QFile patternFile(fileName);
    if (!d->lastSavedTimes.contains(fileName) || d->lastSavedTimes[fileName] < lastModified()) {
        if (patternFile.open(QIODevice::WriteOnly)) {
            patternFile.write(playGridManager()->modelToJson(this).toUtf8());
            patternFile.close();
            success = true;
            d->lastSavedTimes[fileName] = QDateTime::currentMSecsSinceEpoch();
        }
    }
    return success;
}

QObject* PatternModel::sequence() const
{
    return d->sequence;
}

int PatternModel::channelIndex() const
{
    return d->channelIndex;
}

void PatternModel::setChannelIndex(int channelIndex)
{
    if (d->channelIndex != channelIndex) {
        d->channelIndex = channelIndex;
        Q_EMIT channelIndexChanged();
    }
}

int PatternModel::partIndex() const
{
    return d->partIndex;
}

QString PatternModel::partName() const
{
    static const QString partNames[5]{"a", "b", "c", "d", "e"};
    return (d->partIndex > -1 && d->partIndex < 5) ? partNames[d->partIndex] : "";
}

void PatternModel::setPartIndex(int partIndex)
{
    if (d->partIndex != partIndex) {
        d->partIndex = partIndex;
        Q_EMIT partIndexChanged();
    }
}

QString PatternModel::thumbnailUrl() const
{
    return QString("image://pattern/%1/%2?%3").arg(objectName()).arg(QString::number(int(d->bankOffset / d->bankLength))).arg(lastModified());
}

QString PatternModel::name() const
{
    // To ensure we can have orphaned models, we can't assume an associated sequence
    int parentNameLength{0};
    if (d->sequence) {
        parentNameLength = d->sequence->objectName().length();
    }
    return objectName().left(objectName().length() - (parentNameLength + 3));
}

PatternModel::NoteDestination PatternModel::noteDestination() const
{
    return d->noteDestination;
}

void PatternModel::setNoteDestination(const PatternModel::NoteDestination &noteDestination)
{
    if (d->noteDestination != noteDestination) {
        // Before switching the destination, first let's quickly send a little note off for aaaaall notes on this track
        juce::MidiBuffer buffer;
        for (int midiChannel = 1; midiChannel < 17; ++midiChannel) {
            buffer.addEvent(juce::MidiMessage::allNotesOff(midiChannel), 0);
        }
        SyncTimer::instance()->sendMidiBufferImmediately(buffer, d->midiChannel);
        d->noteDestination = noteDestination;
        Q_EMIT noteDestinationChanged();
    }
}

int PatternModel::width() const
{
    return d->width;
}

void PatternModel::setHeight(int height)
{
    startLongOperation();
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
    d->invalidatePosition();
    endLongOperation();
}

int PatternModel::height() const
{
    return rowCount();
}

void PatternModel::setMidiChannel(int midiChannel)
{
    int actualChannel = qMin(qMax(-1, midiChannel), 15);
    if (d->midiChannel != actualChannel) {
        d->midiChannel = actualChannel;
        Q_EMIT midiChannelChanged();
    }
}

int PatternModel::midiChannel() const
{
    return d->midiChannel;
}

void PatternModel::setExternalMidiChannel(int externalMidiChannel)
{
    if (d->externalMidiChannel != externalMidiChannel) {
        d->externalMidiChannel = externalMidiChannel;
        Q_EMIT externalMidiChannelChanged();
    }
}

int PatternModel::externalMidiChannel() const
{
    return d->externalMidiChannel;
}

void PatternModel::setLayerData(const QString &layerData)
{
    if (d->layerData != layerData) {
        d->layerData = layerData;
        Q_EMIT layerDataChanged();
    }
}

QString PatternModel::layerData() const
{
    return d->layerData;
}

void PatternModel::setDefaultNoteDuration(int defaultNoteDuration)
{
    if (d->defaultNoteDuration != defaultNoteDuration) {
        d->defaultNoteDuration = defaultNoteDuration;
        Q_EMIT defaultNoteDurationChanged();
    }
}

int PatternModel::defaultNoteDuration() const
{
    return d->defaultNoteDuration;
}

void PatternModel::setNoteLength(int noteLength)
{
    if (d->noteLength != noteLength) {
        d->noteLength = noteLength;
        d->invalidatePosition();
        Q_EMIT noteLengthChanged();
    }
}

int PatternModel::noteLength() const
{
    return d->noteLength;
}

void PatternModel::setSwing(int swing)
{
    if (d->swing != swing) {
        d->swing = swing;
        // Invalidate all positions (as swing might be scheduled in a previous step due to microtiming settings for the individual step/note)
        d->invalidatePosition();
        Q_EMIT swingChanged();
    }
}

int PatternModel::swing() const
{
    return d->swing;
}

void PatternModel::setAvailableBars(int availableBars)
{
    int adjusted = qMin(qMax(1, availableBars), bankLength());
    if (d->availableBars != adjusted) {
        d->availableBars = adjusted;
        Q_EMIT availableBarsChanged();
        // Ensure that we don't have an active bar that's outside our available range
        setActiveBar(qMin(d->activeBar, d->availableBars - 1));
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

void PatternModel::setBank(const QString& bank)
{
    // A, B, and C are some old fallback stuff...
    int newOffset{d->bankOffset};
    if (bank.toUpper() == "A" || bank.toUpper() == "I") {
        newOffset = 0;
    } else if (bank.toUpper() == "B" || bank.toUpper() == "II") {
        newOffset = d->bankLength;
    } else if (bank.toUpper() == "C" || bank.toUpper() == "III") {
        newOffset = d->bankLength * 2;
    }
    setBankOffset(newOffset);
}

QString PatternModel::bank() const
{
    static const QString names[3]{QLatin1String{"I"}, QLatin1String{"II"}, QLatin1String{"III"}};
    int bankNumber{d->bankOffset / d->bankLength};
    QString result{"(?)"};
    if (bankNumber < 3) {
        result = names[bankNumber];
    }
    return result;
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
        // Ensure that the available bars are not outside the number of bars available in a bank
        setAvailableBars(d->availableBars);
    }
}

int PatternModel::bankLength() const
{
    return d->bankLength;
}

bool PatternModel::bankHasNotes(int bankIndex) const
{
    bool hasNotes{false};
    for (int row = 0; row < d->bankLength; ++row) {
        for (int column = 0; column < d->width; ++column) {
            Note* note = qobject_cast<Note*>(getNote(row + (bankIndex * d->bankLength), column));
            if (note && note->subnotes().length() > 0) {
                hasNotes = true;
                break;
            }
        }
        if (hasNotes) {
            break;
        }
    }
    return hasNotes;
}

bool PatternModel::hasNotes() const
{
    bool hasNotes{false};
    for (int row = 0; row < rowCount(); ++row) {
        for (int column = 0; column < d->width; ++column) {
            Note* note = qobject_cast<Note*>(getNote(row , column));
            if (note && note->subnotes().length() > 0) {
                hasNotes = true;
                break;
            }
        }
        if (hasNotes) {
            break;
        }
    }
    return hasNotes;
}

bool PatternModel::currentBankHasNotes() const
{
    return bankHasNotes(floor(d->bankOffset / d->bankLength));
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

void PatternModel::setClipIds(const QVariantList &clipIds)
{
    bool changed{false};
    if (clipIds.length() == d->clips.length()) {
        int i{0};
        for (const QVariant &clipId : clipIds) {
            if (!d->clips[i] || d->clips[i]->id() != clipId) {
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
        for (const QVariant &clipId: clipIds) {
            ClipAudioSource *newClip = Plugin::instance()->getClipById(clipId.toInt());
            newClips << newClip;
            if (newClip) {
                connect(newClip, &QObject::destroyed, this, [this, newClip](){ d->clips.removeAll(newClip); });
            }
        }
        d->clips = newClips;
        Q_EMIT clipIdsChanged();
    }
}

QVariantList PatternModel::clipIds() const
{
    QVariantList ids;
    for (ClipAudioSource *clip : qAsConst(d->clips)) {
        if (clip) {
            ids << clip->id();
        } else {
            ids << -1;
        }
    }
    return ids;
}

QObject *PatternModel::clipSliceNotes() const
{
    if (!d->clipSliceNotes) {
        d->clipSliceNotes = qobject_cast<NotesModel*>(PlayGridManager::instance()->getNotesModel(objectName() + " - Clip Slice Notes Model"));
        auto fillClipSliceNotes = [this](){
            QList<int> notesToFit;
            QList<QString> noteTitles;
            ClipAudioSource *previousClip{nullptr};
            for (int i = 0; i < d->clips.count(); ++i) {
                ClipAudioSource *clip = d->clips.at(i);
                if (clip) {
                    int sliceStart{clip->sliceBaseMidiNote()};
                    int nextClipStart{129};
                    for (int j = i + 1; j < d->clips.count(); ++j) {
                        ClipAudioSource *nextClip = d->clips.at(j);
                        if (nextClip) {
                            nextClipStart = nextClip->sliceBaseMidiNote();
                            break;
                        }
                    }
                    // Let's see if we can push it /back/ a bit, and still get a full thing... less lovely, but it gives a full spread
                    if (nextClipStart - clip->slices() < sliceStart) {
                        sliceStart = qMax(previousClip ? previousClip->sliceBaseMidiNote() + previousClip->slices(): 0, nextClipStart - clip->slices());
                    }
                    // Now let's add as many notes as we need, or have space for, whichever is smaller
                    int addedNotes{0};
                    for (int note = sliceStart; note < nextClipStart && addedNotes < clip->slices(); ++note) {
                        notesToFit << note;
                        noteTitles << QString("Sample %1\nSlice %2").arg(QString::number(i + 1)).arg(QString::number(clip->sliceForMidiNote(note) + 1));
                        ++addedNotes;
                    }
                    previousClip = clip;
                }
            }
            int howManyRows{int(sqrt(notesToFit.length()))};
            int i{0};
            d->clipSliceNotes->startLongOperation();
            d->clipSliceNotes->clear();
            for (int row = 0; row < howManyRows; ++row) {
                QVariantList notes;
                QVariantList metadata;
                for (int column = 0; column < notesToFit.count() / howManyRows; ++column) {
                    if (i == notesToFit.count()) {
                        break;
                    }
                    notes << QVariant::fromValue<QObject*>(PlayGridManager::instance()->getNote(notesToFit[i], d->midiChannel));
                    metadata << QVariantMap{{"displayText", QVariant::fromValue<QString>(noteTitles[i])}};
                    ++i;
                }
                d->clipSliceNotes->appendRow(notes, metadata);
            }
            d->clipSliceNotes->endLongOperation();
        };
        QTimer *refilTimer = new QTimer(d->gridModel);
        refilTimer->setInterval(100);
        refilTimer->setSingleShot(true);
        connect(refilTimer, &QTimer::timeout, d->gridModel, fillClipSliceNotes);
        connect(this, &PatternModel::clipIdsChanged, refilTimer, QOverload<>::of(&QTimer::start));
        connect(this, &PatternModel::midiChannelChanged, refilTimer, QOverload<>::of(&QTimer::start));
        refilTimer->start();
    }
    return d->clipSliceNotes;
}

int PatternModel::gridModelStartNote() const
{
    return d->gridModelStartNote;
}

void PatternModel::setGridModelStartNote(int gridModelStartNote)
{
    if (d->gridModelStartNote != gridModelStartNote) {
        d->gridModelStartNote = gridModelStartNote;
        Q_EMIT gridModelStartNoteChanged();
    }
}

int PatternModel::gridModelEndNote() const
{
    return d->gridModelEndNote;
}

void PatternModel::setGridModelEndNote(int gridModelEndNote)
{
    if (d->gridModelEndNote != gridModelEndNote) {
        d->gridModelEndNote = gridModelEndNote;
        Q_EMIT gridModelEndNoteChanged();
    }
}

QObject *PatternModel::gridModel() const
{
    if (!d->gridModel) {
        d->gridModel = qobject_cast<NotesModel*>(PlayGridManager::instance()->getNotesModel(objectName() + " - Grid Model"));
        auto rebuildGridModel = [this](){
            // qDebug() << "Rebuilding" << d->gridModel << "for destination" << d->noteDestination << "for channel" << d->midiChannel;
            d->gridModel->startLongOperation();
            QList<int> notesToFit;
            for (int note = d->gridModelStartNote; note <= d->gridModelEndNote; ++note) {
                notesToFit << note;
            }
            int howManyRows{int(sqrt(notesToFit.length()))};
            int i{0};
            d->gridModel->clear();
            for (int row = 0; row < howManyRows; ++row) {
                QVariantList notes;
                QVariantList metadata;
                for (int column = 0; column < notesToFit.count() / howManyRows; ++column) {
                    if (i == notesToFit.count()) {
                        break;
                    }
                    Note* note = qobject_cast<Note*>(PlayGridManager::instance()->getNote(notesToFit[i], d->midiChannel));
                    notes << QVariant::fromValue<QObject*>(note);
                    QList<ClipAudioSource*> clips = d->clipsForMidiNote(note->midiNote());
                    if (noteDestination() == SampleTriggerDestination) {
                        QString noteTitle{midiNoteNames[note->midiNote()]};
                        if (clips.length() > 0) {
                            for (ClipAudioSource* clip : clips) {
                                int clipIndex = d->clips.indexOf(clip);
                                QString actualNote{};
                                if (clip->rootNote() != 60) {
                                    int actualNoteValue = note->midiNote() + (60 - clip->rootNote());
                                    if (actualNoteValue > -1 && actualNoteValue < 128) {
                                        actualNote = QString(" (%1)").arg(midiNoteNames[actualNoteValue]);
                                    }
                                }
                                noteTitle += QString("\nSample %1%2").arg(QString::number(clipIndex + 1)).arg(actualNote);
                            }
                        } else {
                            noteTitle += QString{"\n(no sample)"};
                        }
                        metadata << QVariantMap{{"displayText", QVariant::fromValue<QString>(noteTitle)}};
                    } else {
                        metadata << QVariantMap();
                    }
                    ++i;
                }
                d->gridModel->addRow(notes, metadata);
            }
            d->gridModel->endLongOperation();
        };
        QTimer *refilTimer = new QTimer(d->gridModel);
        refilTimer->setInterval(100);
        refilTimer->setSingleShot(true);
        connect(refilTimer, &QTimer::timeout, d->gridModel, rebuildGridModel);
        connect(this, &PatternModel::midiChannelChanged, refilTimer, QOverload<>::of(&QTimer::start));
        connect(this, &PatternModel::gridModelStartNoteChanged, refilTimer, QOverload<>::of(&QTimer::start));
        connect(this, &PatternModel::gridModelEndNoteChanged, refilTimer, QOverload<>::of(&QTimer::start));
        // To ensure we also update when the clips for each position change
        connect(this, &PatternModel::noteDestinationChanged, refilTimer, QOverload<>::of(&QTimer::start));
        auto updateClips = [this,refilTimer](){
            for (ClipAudioSource *clip : d->clips) {
                if (clip) {
                    connect(clip, &ClipAudioSource::keyZoneStartChanged, refilTimer, QOverload<>::of(&QTimer::start));
                    connect(clip, &ClipAudioSource::keyZoneEndChanged, refilTimer, QOverload<>::of(&QTimer::start));
                }
            }
        };
        connect(this, &PatternModel::clipIdsChanged, d->gridModel, updateClips);
        updateClips();
        refilTimer->start();
    }
    return d->gridModel;
}

void PatternModel::setRecordLive(bool recordLive)
{
    if (d->recordingLive != recordLive) {
        d->recordingLive = recordLive;
        Q_EMIT recordLiveChanged();
    }
}

bool PatternModel::recordLive() const
{
    return d->recordingLive;
}

void PatternModel::setLiveRecordingSource(const QString& newLiveRecordingSource)
{
    static const QLatin1String sketchpadTrackSource{"sketchpadTrack:"};
    static const QLatin1String externalDeviceSource{"external:"};
    if (d->liveRecordingSource != newLiveRecordingSource) {
        d->liveRecordingSource = newLiveRecordingSource;
        if (d->liveRecordingSource.startsWith(sketchpadTrackSource)) {
            d->liveRecordingSourceExternalDeviceIndex = -1;
            d->liveRecordingSourceSketchpadTrack = d->liveRecordingSource.midRef(15).toInt();
            if (d->liveRecordingSourceSketchpadTrack < -2 || ZynthboxTrackCount > d->liveRecordingSourceSketchpadTrack) {
                d->liveRecordingSourceSketchpadTrack = -1;
            }
        } else if (d->liveRecordingSource.startsWith(externalDeviceSource)) {
            d->liveRecordingSourceSketchpadTrack = -1;
            d->liveRecordingSourceSketchpadTrack = d->liveRecordingSource.midRef(9).toInt();
        } else {
            d->liveRecordingSourceExternalDeviceIndex = -1;
            d->liveRecordingSourceSketchpadTrack = -1;
        }
        Q_EMIT liveRecordingSourceChanged();
    }
}

QString PatternModel::liveRecordingSource() const
{
    return d->liveRecordingSource;
}

QObject *PatternModel::zlChannel() const
{
    return d->zlSyncManager->zlChannel;
}

void PatternModel::setZlChannel(QObject *zlChannel)
{
    d->zlSyncManager->setZlChannel(zlChannel);
}

QObject *PatternModel::zlPart() const
{
    return d->zlSyncManager->zlPart;
}

void PatternModel::setZlPart(QObject *zlPart)
{
    d->zlSyncManager->setZlPart(zlPart);
}

QObject *PatternModel::zlScene() const
{
    return d->zlSyncManager->zlScene;
}

void PatternModel::setZlScene(QObject *zlScene)
{
    d->zlSyncManager->setZlScene(zlScene);
}

QObject *PatternModel::zlDashboard() const
{
    return d->zlSyncManager->zlDashboard;
}

void PatternModel::setZlDashboard(QObject *zlDashboard)
{
    if (d->zlSyncManager->zlDashboard != zlDashboard) {
        d->zlSyncManager->setZlDashboard(zlDashboard);
        Q_EMIT zlDashboardChanged();
    }
}

int PatternModel::playingRow() const
{
    return d->playingRow;
}

int PatternModel::playingColumn() const
{
    return d->playingColumn;
}

int PatternModel::playbackPosition() const
{
    return isPlaying()
        ? (d->playingRow * d->width) + d->playingColumn
        : -1;
}

int PatternModel::bankPlaybackPosition() const
{
    return isPlaying()
        ? (d->playingRow * d->width) + d->playingColumn - (d->bankOffset * d->width)
        : -1;
}

bool PatternModel::isPlaying() const
{
    return d->isPlaying;
}

void addNoteToBuffer(juce::MidiBuffer &buffer, const Note *theNote, unsigned char velocity, bool setOn, int availableChannel) {
    unsigned char note[3];
    if (setOn) {
        note[0] = 0x90 + availableChannel;
    } else {
        note[0] = 0x80 + availableChannel;
    }
    note[1] = theNote->midiNote();
    note[2] = velocity;
    const int onOrOff = setOn ? 1 : 0;
    buffer.addEvent(note, 3, onOrOff);
}

inline juce::MidiBuffer &PatternModel::Private::getOrCreateBuffer(QHash<int, juce::MidiBuffer> &collection, int position)
{
    if (!collection.contains(position)) {
        collection[position] = juce::MidiBuffer();
    }
    return collection[position];
}

inline void PatternModel::Private::noteLengthDetails(int noteLength, qint64 &nextPosition, bool &relevantToUs, qint64 &noteDuration)
{
    // Potentially it'd be tempting to try and optimise this manually to use bitwise operators,
    // but GCC already does that for you at -O2, so don't bother :)
    switch (noteLength) {
    case 1:
        if (nextPosition % beatSubdivision == 0) {
            relevantToUs = true;
            nextPosition = nextPosition / beatSubdivision;
            noteDuration = 32;
        } else {
            relevantToUs = false;
        }
        break;
    case 2:
        if (nextPosition % beatSubdivision2 == 0) {
            relevantToUs = true;
            nextPosition = nextPosition / beatSubdivision2;
            noteDuration = 16;
        } else {
            relevantToUs = false;
        }
        break;
    case 3:
        if (nextPosition % beatSubdivision3 == 0) {
            relevantToUs = true;
            nextPosition = nextPosition / beatSubdivision3;
            noteDuration = 8;
        } else {
            relevantToUs = false;
        }
        break;
    case 4:
        if (nextPosition % beatSubdivision4 == 0) {
            relevantToUs = true;
            nextPosition = nextPosition / beatSubdivision4;
            noteDuration = 4;
        } else {
            relevantToUs = false;
        }
        break;
    case 5:
        if (nextPosition % beatSubdivision5 == 0) {
            relevantToUs = true;
            nextPosition = nextPosition / beatSubdivision5;
            noteDuration = 2;
        } else {
            relevantToUs = false;
        }
        break;
    case 6:
        relevantToUs = true;
        noteDuration = 1;
        break;
    default:
        qWarning() << "Incorrect note length in pattern, no notes will be played from this one, ever";
        break;
    }
}

void PatternModel::handleSequenceAdvancement(qint64 sequencePosition, int progressionLength) const
{
    static const QLatin1String velocityString{"velocity"};
    static const QLatin1String delayString{"delay"};
    static const QLatin1String durationString{"duration"};
    static const QLatin1String probabilityString{"probability"};
    static const QLatin1String ratchetStyleString{"ratchet-style"};
    static const QLatin1String ratchetCountString{"ratchet-count"};
    static const QLatin1String ratchetProbabilityString{"ratchet-probability"};
    if (!d->zlSyncManager->channelMuted
        && (isPlaying()
            // Play any note if the pattern is set to sliced or trigger destination, since then it's not sending things through the midi graph
            && (d->noteDestination == PatternModel::SampleSlicedDestination || d->noteDestination == PatternModel ::SampleTriggerDestination
                // Don't play notes on channel 15, because that's the control channel, and we don't want patterns to play to that
                || (d->midiChannel > -1 && d->midiChannel < 15)
                // And if we're playing midi, but don't have a good channel of our own, if the current channel is good, use that
                || d->playGridManager->currentMidiChannel() > -1
            )
        )
    ) {
        if (d->updateMostRecentStartTimestamp) {
            d->updateMostRecentStartTimestamp = false;
            d->mostRecentStartTimestamp = sequencePosition;
        }
        const qint64 playbackOffset{d->playfieldManager->clipOffset(d->song, d->channelIndex, d->partIndex) - (d->segmentHandler->songMode() ? d->segmentHandler->startOffset() : 0)};
        qint64 noteDuration{0};
        bool relevantToUs{false};
        for (int progressionIncrement = 0; progressionIncrement <= progressionLength; ++progressionIncrement) {
            // check whether the sequencePosition + progressionIncrement matches our note length
            qint64 nextPosition = sequencePosition - playbackOffset + progressionIncrement;
            d->noteLengthDetails(d->noteLength, nextPosition, relevantToUs, noteDuration);
            // Since the schedule operates at a higher subdivision than the pattern model, adjust to match
            noteDuration = noteDuration * d->patternTickToSyncTimerTick;
            const int schedulingIncrement{progressionIncrement * d->patternTickToSyncTimerTick};

            if (relevantToUs) {
                // Get the next row/column combination, and schedule the previous one off, and the next one on
                // squish nextPosition down to fit inside our available range (d->availableBars * d->width)
                // start + (numberToBeWrapped - start) % (limit - start)
                nextPosition = nextPosition % (d->availableBars * d->width);
                // If we have any kind of probability involved in this step (including the look-ahead), we'll
                // need to clear it immediately, so that probability is also taken into account for the next
                // time it's due for scheduling
                bool clearPositionBufferImmediately{false};
                // Swing is applied to every even step as counted by humans (so every uneven step as counted by our indices)
                const qint64 swingOffset{nextPosition % 2 == 0 ? 0 : noteDuration * d->swing / 100};
                // qDebug() << "Swing offset for" << nextPosition << "is" << swingOffset << "with swing" << d->swing << "and note duration" << noteDuration;

                if (d->positionBuffers.contains(nextPosition + (d->bankOffset * d->width)) == false) {
                    QHash<int, juce::MidiBuffer> positionBuffers;
                    auto subnoteSender = [this, &clearPositionBufferImmediately, swingOffset, noteDuration, schedulingIncrement, &positionBuffers](const Note* subnote, const QVariantHash &metaHash, int positionAdjustment = 0) {
                        bool sendNotes{true};
                        const int probability{metaHash.value(probabilityString, 100).toInt()};
                        if (probability < 100) {
                            clearPositionBufferImmediately = true;
                            if (QRandomGenerator::global()->generateDouble() * 100 > probability) {
                                sendNotes = false;
                            }
                        }
                        if (sendNotes) {
                            const int velocity{metaHash.value(velocityString, 64).toInt()};
                            const qint64 delay{(metaHash.value(delayString, 0).toInt() + swingOffset + positionAdjustment) * d->patternTickToSyncTimerTick};
                            int duration{metaHash.value(durationString, noteDuration / d->patternTickToSyncTimerTick).toInt() * d->patternTickToSyncTimerTick};
                            if (duration < 1) {
                                duration = noteDuration;
                            }
                            const int ratchetCount{metaHash.value(ratchetCountString, 0).toInt()};
                            if (ratchetCount > 0) {
                                const int ratchetStyle{metaHash.value(ratchetStyleString, 0).toInt()};
                                qint64 ratchetDelay{qMax(qint64(1), noteDuration / ratchetCount)};
                                qint64 ratchetDuration{duration};
                                qint64 ratchetLastDuration{duration};
                                bool reuseChannel{false}; // This only works in choke modes, and will fail with overlap modes
                                switch(ratchetStyle) {
                                    case 3: // Split Length, Choke
                                        ratchetDelay = qMax(1, duration / ratchetCount);
                                        ratchetDuration = ratchetDelay;
                                        reuseChannel = true;
                                        break;
                                    case 2: // Split Length, Overlap
                                        ratchetDelay = qMax(1, duration / ratchetCount);
                                        break;
                                    case 1: // Split Step, Choke
                                        ratchetDuration = ratchetDelay;
                                        reuseChannel = true;
                                        break;
                                    case 0: // Split Step, Overlap
                                    default:
                                        // These are the default values, so just pass this through
                                        break;
                                }
                                const int ratchetProbability{metaHash.value(ratchetProbabilityString, 100).toInt()};
                                if (ratchetProbability < 100) {
                                    clearPositionBufferImmediately = true;
                                }
                                int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                for (int ratchetIndex = 0; ratchetIndex < ratchetCount; ++ratchetIndex) {
                                    sendNotes = true;
                                    if (ratchetProbability < 100) {
                                        if (QRandomGenerator::global()->generateDouble() * 100 > ratchetProbability) {
                                            sendNotes = false;
                                        }
                                    }
                                    if (sendNotes) {
                                        if (ratchetIndex + 1 == ratchetCount) {
                                            ratchetDuration = ratchetLastDuration;
                                        }
                                        addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, delay + (ratchetDelay * ratchetIndex)), subnote, velocity, true, avaialbleChannel);
                                        addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, delay + (ratchetDelay * ratchetIndex) + ratchetDuration), subnote, velocity, false, avaialbleChannel);
                                        if (reuseChannel == false && ratchetIndex + 1 < ratchetCount) {
                                            avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                        }
                                    }
                                }
                            } else {
                                const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, delay), subnote, velocity, true, avaialbleChannel);
                                addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, delay + duration), subnote, velocity, false, avaialbleChannel);
                            }
                        }
                    };
                    // Do a lookup for any notes after this position that want playing before their step (currently
                    // just looking ahead one step, we could probably afford to do a bunch, but one for now)
                    for (int subsequentNoteIndex = 0; subsequentNoteIndex < d->lookaheadAmount; ++subsequentNoteIndex) {
                        const int ourPosition = (nextPosition + subsequentNoteIndex) % (d->availableBars * d->width);
                        const int row = (ourPosition / d->width) % d->availableBars;
                        const int column = ourPosition - (row * d->width);
                        const Note *note = qobject_cast<const Note*>(getNote(row + d->bankOffset, column));
                        if (note) {
                            const QVariantList &subnotes = note->subnotes();
                            const QVariantList &meta = getMetadata(row + d->bankOffset, column).toList();
                            // The first note we want to treat to all the things
                            if (subsequentNoteIndex == 0) {
                                if (meta.count() == subnotes.count()) {
                                    for (int subnoteIndex = 0; subnoteIndex < subnotes.count(); ++subnoteIndex) {
                                        const Note *subnote = subnotes[subnoteIndex].value<Note*>();
                                        const QVariantHash &metaHash = meta[subnoteIndex].toHash();
                                        if (subnote) {
                                            if (metaHash.isEmpty()) {
                                                const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                                addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset), subnote, 64, true, avaialbleChannel);
                                                addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset + noteDuration), subnote, 64, false, avaialbleChannel);
                                            } else {
                                                subnoteSender(subnote, metaHash);
                                            }
                                        }
                                    }
                                } else if (subnotes.count() > 0) {
                                    for (const QVariant &subnoteVar : subnotes) {
                                        const Note *subnote = subnoteVar.value<Note*>();
                                        if (subnote) {
                                            const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                            addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset), subnote, 64, true, avaialbleChannel);
                                            addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset + noteDuration), subnote, 64, false, avaialbleChannel);
                                        }
                                    }
                                } else {
                                    const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->midiChannel, quint64(schedulingIncrement));
                                    addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset), note, 64, true, avaialbleChannel);
                                    addNoteToBuffer(d->getOrCreateBuffer(positionBuffers, swingOffset + noteDuration), note, 64, false, avaialbleChannel);
                                }
                            // The lookahead notes only need handling if, and only if, there is matching meta, and the delay is negative (as in, position before that step)
                            } else {
                                if (meta.count() == subnotes.count()) {
                                    const int positionAdjustment = subsequentNoteIndex * noteDuration;
                                    for (int subnoteIndex = 0; subnoteIndex < subnotes.count(); ++subnoteIndex) {
                                        const Note *subnote = subnotes[subnoteIndex].value<Note*>();
                                        const QVariantHash &metaHash = meta[subnoteIndex].toHash();
                                        if (subnote) {
                                            if (!metaHash.isEmpty() && metaHash.contains(delayString)) {
                                                const qint64 delay{metaHash.value(delayString, 0).toInt() + swingOffset};
                                                if (delay < 0) {
                                                    subnoteSender(subnote, metaHash, positionAdjustment);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    d->positionBuffers[nextPosition + (d->bankOffset * d->width)] = positionBuffers;
                }
                switch (d->noteDestination) {
                    case PatternModel::SampleLoopedDestination:
                        // If this channel is supposed to loop its sample, we are not supposed to be making patterny sounds
                        break;
                    case PatternModel::SampleTriggerDestination:
                    case PatternModel::SampleSlicedDestination:
                    {
                        const QHash<int, juce::MidiBuffer> &positionBuffers = d->positionBuffers[nextPosition + (d->bankOffset * d->width)];
                        QHash<int, juce::MidiBuffer>::const_iterator position;
                        for (position = positionBuffers.constBegin(); position != positionBuffers.constEnd(); ++position) {
                            for (const juce::MidiMessageMetadata &juceMessage : qAsConst(position.value())) {
                                d->midiMessageToClipCommands(&d->commandRing, juceMessage.data[0], juceMessage.data[1], juceMessage.data[2]);
                                while (d->commandRing.readHead->processed == false) {
                                    d->syncTimer->scheduleClipCommand(d->commandRing.read(), quint64(qMax(0, schedulingIncrement + position.key())));
                                }
                            }
                        }
                        break;
                    }
                    case PatternModel::ExternalDestination:
                        // While external destination /is/ somewhere else, libzl's MidiRouter does the actual work of the somewhere-else-ness
                        // We set this up in the midiChannelUpdater timeout handler (see PatternModel's ctor)
                    case PatternModel::SynthDestination:
                    default:
                    {
                        const QHash<int, juce::MidiBuffer> &positionBuffers = d->positionBuffers[nextPosition + (d->bankOffset * d->width)];
                        QHash<int, juce::MidiBuffer>::const_iterator position;
                        for (position = positionBuffers.constBegin(); position != positionBuffers.constEnd(); ++position) {
                            d->syncTimer->scheduleMidiBuffer(position.value(), quint64(qMax(0, schedulingIncrement + position.key())), d->midiChannel);
                        }
                        break;
                    }
                }
                if (clearPositionBufferImmediately) {
                    for (int subsequentNoteIndex = 0; subsequentNoteIndex < d->lookaheadAmount; ++subsequentNoteIndex) {
                        const int ourPosition = (nextPosition + subsequentNoteIndex) % (d->availableBars * d->width);
                        const int row = (ourPosition / d->width) % d->availableBars;
                        const int column = ourPosition - (row * d->width);
                        d->invalidatePosition(row, column);
                    }
                }
            }
        }
    } else {
        d->updateMostRecentStartTimestamp = true;
    }
}

void PatternModel::updateSequencePosition(qint64 sequencePosition)
{
    // Don't play notes on channel 15, because that's the control channel, and we don't want patterns to play to that
    if ((isPlaying()
            && (d->noteDestination == PatternModel::SampleSlicedDestination || d->noteDestination == PatternModel ::SampleTriggerDestination
            || (d->midiChannel > -1 && d->midiChannel < 15)
            || d->playGridManager->currentMidiChannel() > -1))
        || sequencePosition == 0
    ) {
        const qint64 playbackOffset{d->playfieldManager->clipOffset(d->song, d->channelIndex, d->partIndex) - (d->segmentHandler->songMode() ? d->segmentHandler->startOffset() : 0)};
        bool relevantToUs{false};
        qint64 nextPosition{sequencePosition - playbackOffset};
        qint64 noteDuration{0};
        d->noteLengthDetails(d->noteLength, nextPosition, relevantToUs, noteDuration);

        if (relevantToUs) {
            nextPosition = nextPosition % (d->availableBars * d->width);
            int row = (nextPosition / d->width) % d->availableBars;
            int column = nextPosition - (row * d->width);
            d->playingRow = row + d->bankOffset;
            d->playingColumn = column;
            QMetaObject::invokeMethod(this, "playingRowChanged", Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, "playingColumnChanged", Qt::QueuedConnection);
        }
    }
    while (d->noteDataPoolWriteHead->object == nullptr) {
        d->noteDataPoolWriteHead->object = new NewNoteData;
        d->noteDataPoolWriteHead = d->noteDataPoolWriteHead->next;
    }
}

void PatternModel::handleSequenceStop()
{
    setRecordLive(false);
}

void PatternModel::handleMidiMessage(const unsigned char &byte1, const unsigned char &byte2, const unsigned char &byte3, const double& timeStamp, const int& sketchpadTrack)
{
    if (d->liveRecordingSourceExternalDeviceIndex == -1
            ? d->liveRecordingSourceSketchpadTrack == -1 ? sketchpadTrack == d->midiChannel : sketchpadTrack == d->liveRecordingSourceSketchpadTrack
            : true // TODO implement the live recording external device logic thing...
        ) {
        // if we're recording live, and it's a note-on message, create a newnotedata and add to list of notes being recorded
        if (d->recordingLive && 0x8F < byte1 && byte1 < 0xA0) {
            // Belts and braces here - it shouldn't really happen (a hundred notes is kind of a lot to add in a single shot), but just in case...
            if (d->noteDataPoolReadHead->object) {
                NewNoteData *newNote = d->noteDataPoolReadHead->object;
                d->noteDataPoolReadHead = d->noteDataPoolReadHead->next;
                newNote->timestamp = d->syncTimer->timerTickForJackPlayhead(timeStamp, &newNote->timestampOffset);
                newNote->midiNote = byte2;
                newNote->velocity = byte3;
                d->recordingLiveNotes << newNote;
            }
        }
        // if note-off, check whether there's a matching on note, and if there is, add that note with velocity, delay, and duration as appropriate for current time and step
        if (d->recordingLiveNotes.count() > 0 && 0x7F < byte1 && byte1 < 0x90) {
            QMutableListIterator<NewNoteData*> iterator(d->recordingLiveNotes);
            NewNoteData *newNote{nullptr};
            while (iterator.hasNext()) {
                iterator.next();
                newNote = iterator.value();
                if (newNote->midiNote == byte2) {
                    iterator.remove();
                    newNote->endTimestamp = d->syncTimer->timerTickForJackPlayhead(timeStamp, &newNote->endTimestampOffset);
                    QMetaObject::invokeMethod(d->zlSyncManager, "addRecordedNote", Qt::QueuedConnection, Q_ARG(void*, newNote));
                    break;
                }
            }
        }
    }
}

void PatternModel::midiMessageToClipCommands(ClipCommandRing *listToPopulate, const int &samplerIndex, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3) const
{
    if (samplerIndex == d->midiChannel && (!d->sequence || (d->sequence->shouldMakeSounds() && (d->sequence->soloPatternObject() == this || d->enabled)))
        // But also, don't make sounds unless we're sample-triggering or slicing (otherwise the synths will handle it)
        && (d->noteDestination == SampleTriggerDestination || d->noteDestination == SampleSlicedDestination)) {
            d->midiMessageToClipCommands(listToPopulate, byte1, byte2, byte3);
    }
}

void ZLPatternSynchronisationManager::addRecordedNote(void *recordedNote)
{
    NewNoteData *newNote = static_cast<NewNoteData*>(recordedNote);

    qint64 nextPosition{0}; // not relevant
    bool relevantToUs{false}; // not relevant
    qint64 noteDuration{0};
    q->d->noteLengthDetails(q->noteLength(), nextPosition, relevantToUs, noteDuration);

    // Unless we're in the "all the zoomies" mode where each step is one pattern tick, allow for a deviation of 2 before auto-quantizing
    int deviationAllowance = qMin(qint64(2), noteDuration);

    // convert the timer ticks to pattern ticks, and adjust for whatever was the most recent restart of the pattern's playback
    newNote->timestamp = (newNote->timestamp - quint64(q->d->mostRecentStartTimestamp)) / quint64(q->d->beatSubdivision6);
    newNote->endTimestamp = (newNote->endTimestamp - quint64(q->d->mostRecentStartTimestamp)) / quint64(q->d->beatSubdivision6);

    const int patternLength = q->width() * q->availableBars();
    const double normalisedTimestamp{double(qint64(newNote->timestamp) % (patternLength * noteDuration))};
    newNote->step = normalisedTimestamp / noteDuration;
    newNote->delay = normalisedTimestamp - (newNote->step * noteDuration);

    int row = (newNote->step / q->width()) % q->availableBars();
    int column = newNote->step - (row * q->width());

    // Sanity check the delay - if it's within a small amount of the start position of the current step, or very near
    // the next step, assume it wants to be quantized and make sure we're setting it on the appropriate step)
    if (newNote->delay < deviationAllowance) {
        newNote->delay = 0;
    } else if (noteDuration - newNote->delay < deviationAllowance) {
        newNote->step = (newNote->step + 1) % patternLength;
        row = (newNote->step / q->width()) % q->availableBars();
        column = newNote->step - (row * q->width());
        newNote->delay = 0;
    }

    newNote->duration = newNote->endTimestamp - newNote->timestamp;
    // Sanity check the duration - if it's within a small amount of the length of the pattern's note, reset it to 0 (for auto-quantizing)
    if (abs(newNote->duration - qint64(noteDuration)) < deviationAllowance) {
        newNote->duration = 0;
    }

    // Now let's make sure that if there's already a note with this note value on the given step, we change that instead of adding a new one
    newNote->row = q->bankOffset() + row; // reset row to the internal actual row (otherwise we'd end up with the wrong one)
    newNote->column = column;
    int subnoteIndex{-1};
    Note *note = qobject_cast<Note*>(q->getNote(newNote->row, newNote->column));
    if (note) {
        for (int i = 0; i < note->subnotes().count(); ++i) {
            Note* subnote = note->subnotes().at(i).value<Note*>();
            if (subnote && subnote->midiNote() == newNote->midiNote) {
                subnoteIndex = i;
                break;
            }
        }
    }
    // If we didn't find one there already, /then/ we can create one
    if (subnoteIndex == -1) {
        subnoteIndex = q->addSubnote(newNote->row, newNote->column, q->playGridManager()->getNote(newNote->midiNote, q->midiChannel()));
        // qDebug() << Q_FUNC_INFO << "Didn't find a subnote with this midi note to change values on, created a new subnote at subnote index" << subnoteIndex;
    } else {
        // Check whether this is what we already know about, and if it is, abort the changes
        const int oldVelocity = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "velocity").toInt();
        const int oldDuration = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "duration").toInt();
        const int oldDelay = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "delay").toInt();
        if (oldVelocity == newNote->velocity && oldDuration == newNote->duration && oldDelay == newNote->delay) {
            // qDebug() <<  Q_FUNC_INFO << "This is a note we already have in the pattern, with the same data set on it, so no need to do anything with that" << newNote << newNote->timestamp << newNote->endTimestamp << newNote->step << newNote->row << newNote->column << newNote->midiNote << newNote->velocity << newNote->delay << newNote->duration;
            subnoteIndex = -1;
        }
    }
    if (subnoteIndex > -1) {
        // And then, finally, set the three values (always set them, because we might be changing an existing entry
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "velocity", newNote->velocity);
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "duration", newNote->duration);
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "delay", newNote->delay);
        qDebug() << Q_FUNC_INFO << "Handled a recorded new note:" << newNote << newNote->timestamp << newNote->endTimestamp << newNote->step << newNote->row << newNote->column << newNote->midiNote << newNote->velocity << newNote->delay << newNote->duration << "with deviation allowance" << deviationAllowance;
    }

    // And at the end, get rid of the thing
    delete newNote;
}

// Since we got us a qobject up there a bit that we need to mocify
#include "PatternModel.moc"
