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
#include "KeyScales.h"
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
#include "ClipAudioSourceSliceSettings.h"
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
    int sketchpadTrack{0};
    QString hardwareDeviceId;
    MidiRouter::ListenerPort port{MidiRouter::UnknownPort};
};

class ZLPatternSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLPatternSynchronisationManager(PatternModel *parent = nullptr)
        : QObject(parent)
        , q(parent)
    {
        syncTimer = SyncTimer::instance();
    };
    PatternModel *q{nullptr};
    SyncTimer *syncTimer{nullptr};
    QObject *zlChannel{nullptr};
    QObject *zlClip{nullptr};
    QObject *zlScene{nullptr};

    bool channelMuted{false};
    ClipAudioSource::SamplePickingStyle samplePickingStyle{ClipAudioSource::SameOrFirstPickingStyle};
    void setZlChannel(QObject *newZlChannel)
    {
        if (zlChannel != newZlChannel) {
            if (zlChannel) {
                zlChannel->disconnect(this);
            }
            zlChannel = newZlChannel;
            if (zlChannel) {
                connect(zlChannel, SIGNAL(track_type_changed()), this, SLOT(trackTypeChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(track_type_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(externalMidiChannelChanged()), this, SLOT(externalMidiChannelChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(samples_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(selectedClipChanged()), this, SLOT(selectedClipChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(chained_sounds_changed()), this, SLOT(chainedSoundsChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(chainedSoundsAcceptedChannelsChanged()), this, SLOT(chainedSoundsChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(recordingPopupActiveChanged()), this, SIGNAL(recordingPopupActiveChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(mutedChanged()), this, SLOT(mutedChanged()), Qt::QueuedConnection);
                connect(zlChannel, SIGNAL(samplePickingStyleChanged()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                trackTypeChanged();
                externalMidiChannelChanged();
                updateSamples();
                selectedClipChanged();
                chainedSoundsChanged();
            }
            mutedChanged();
            Q_EMIT q->zlChannelChanged();
        }
    }

    void setZlClip(QObject *newZlClip)
    {
        if (zlClip != newZlClip) {
            if (zlClip) {
                zlClip->disconnect(this);
            }
            zlClip = newZlClip;
            Q_EMIT q->zlClipChanged();
            if (zlClip) {
                // connect(zlClip, SIGNAL(samples_changed()), this, SLOT(updateSamples()), Qt::QueuedConnection);
                // updateSamples();
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
//                 connect(zlChannel, SIGNAL(enabled_changed()), this, SLOT(selectedClipChanged()), Qt::QueuedConnection);
                sceneEnabledChanged();
            }
            Q_EMIT q->zlSceneChanged();
        }
    }

    Q_SIGNAL void recordingPopupActiveChanged();

public Q_SLOTS:
    void sceneEnabledChanged() {
        q->setEnabled(zlScene->property("enabled").toBool());
    }
    void trackTypeChanged() {
        static const QLatin1String sampleTrig{"sample-trig"};
        static const QLatin1String sampleLoop{"sample-loop"};
        static const QLatin1String external{"external"};
//         static const QLatin1String synth{"synth"}; // the default
        const QString trackType = zlChannel->property("trackType").toString();
        TimerCommand* timerCommand = syncTimer->getTimerCommand();
        timerCommand->operation = TimerCommand::SamplerChannelEnabledStateOperation;
        timerCommand->parameter = q->sketchpadTrack();
        if (trackType == sampleTrig) {
            q->setNoteDestination(PatternModel::SampleTriggerDestination);
            timerCommand->parameter2 = true;
        } else if (trackType == sampleLoop) {
            q->setNoteDestination(PatternModel::SampleLoopedDestination);
            timerCommand->parameter2 = true;
        } else if (trackType == external) {
            q->setNoteDestination(PatternModel::ExternalDestination);
            timerCommand->parameter2 = false;
        } else { // or in other words "if (trackType == synth)"
            q->setNoteDestination(PatternModel::SynthDestination);
            timerCommand->parameter2 = true;
        }
        syncTimer->scheduleTimerCommand(0, timerCommand);
    }
    void externalMidiChannelChanged() {
        q->setExternalMidiChannel(zlChannel->property("externalMidiChannel").toInt());
    }
    void selectedClipChanged() {
        SequenceModel *sequence = qobject_cast<SequenceModel*>(q->sequence());
        if (sequence && zlChannel) {
            const int selectedClip{zlChannel->property("selectedClip").toInt()};
            sequence->setActiveChannel(PlayGridManager::instance()->currentSketchpadTrack(), selectedClip);
        }
    }
    void updateSamples() {
        QVariantList clipIds;
        // qDebug() << Q_FUNC_INFO << q->sketchpadTrack() << q->clipName();
        if (zlChannel) {
            const QString zlSamplePickingStyle = zlChannel->property("samplePickingStyle").toString();
            // static const QLatin1String sameOrFirstStyle{"same-or-first"};
            static const QLatin1String sameStyle{"same"};
            static const QLatin1String firstStyle{"first"};
            static const QLatin1String allStyle{"all"};
            if (zlSamplePickingStyle == allStyle) {
                samplePickingStyle = ClipAudioSource::AllPickingStyle;
            } else if (zlSamplePickingStyle == firstStyle) {
                samplePickingStyle = ClipAudioSource::FirstPickingStyle;
            } else if (zlSamplePickingStyle == sameStyle) {
                samplePickingStyle = ClipAudioSource::SamePickingStyle;
            } else {
                // Default is same-or-first, so on real need to check here, and it's our delegated fallback option
                samplePickingStyle = ClipAudioSource::SameOrFirstPickingStyle;
            }
            const QVariantList channelSamples = zlChannel->property("samples").toList();
            QList<int> slotIndices{0, 1, 2, 3, 4};
            switch(samplePickingStyle) {
                case ClipAudioSource::AllPickingStyle:
                case ClipAudioSource::FirstPickingStyle:
                    // All is well, just use them all, in order
                    break;
                case ClipAudioSource::SamePickingStyle:
                    // Only use the equivalent slot to our own position
                    slotIndices = {q->clipIndex()};
                    break;
                case ClipAudioSource::SameOrFirstPickingStyle:
                default:
                    // Try our own slot first, and then try the others in order
                    slotIndices.removeAll(q->clipIndex());
                    slotIndices.insert(0, q->clipIndex());
                    break;
            }

            for (const int &slotIndex : qAsConst(slotIndices)) {
                const QObject *sample = channelSamples[slotIndex].value<QObject*>();
                if (sample) {
                    const int cppObjId{sample->property("cppObjId").toInt()};
                    clipIds << cppObjId;
                    // qDebug() << Q_FUNC_INFO << "Sample in slot" << slotIndex << "has cppObjId" << cppObjId;
                    if (samplePickingStyle == ClipAudioSource::SameOrFirstPickingStyle && cppObjId > -1 && slotIndex == q->clipIndex()) {
                        // In SameOrFirst, if there is a sample in the matches-me slot, ignore any sample that isn't that one
                        // If there is no sample in that slot, we want to try all the others in order
                        break;
                    }
                }
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
            MidiRouter::instance()->setZynthianChannels(q->sketchpadTrack(), chainedSounds);
        }
    }
    void mutedChanged() {
        if (zlChannel) {
            channelMuted = zlChannel->property("muted").toBool();
        } else {
            channelMuted = false;
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

#define ProbabilitySequenceMax 8
// The options available for probability based playback
static const QList<QList<double>> probabilitySequenceData{
    {1}, // 100% (the default, really just here to take up space and avoid having to off-by-one some stuff
    {0.9}, // 90%
    {0.8}, // 80%
    {0.7}, // 70%
    {0.6}, // 60%
    {0.5}, // 50%
    {0.4}, // 40%
    {0.3}, // 30%
    {0.2}, // 20%
    {0.1}, // 10%
    {1}, // Same As Previous - Will use the most recently evaluated probability result for the same pattern (that is, not the most recently scheduled note)
    {1, 0}, // Play 1, Skip 1
    {1, 0.5}, // Play 1, 50% Next
    {1, 0, 0}, // Play 1, Skip 2
    {1, 0, 0, 0}, // Play 1, Skip 3
    {1, 0, 0, 0, 0}, // Play 1, Skip 4
    {1, 0, 0, 0, 0, 0}, // Play 1, Skip 5
    {1, 0, 0, 0, 0, 0, 0}, // Play 1, Skip 6
    {1, 0, 0, 0, 0, 0, 0, 0}, // Play 1, Skip 7
    {0, 1}, // Skip 1, Play 1
    {0.5, 1}, // 50% One, 100% Next
    {0, 0, 1}, // Skip 2, Play 1
    {0, 0, 0, 1}, // Skip 3, Play 1
    {0, 0, 0, 0, 1}, // Skip 4, Play 1
    {0, 0, 0, 0, 0, 1}, // Skip 5, Play 1
    {0, 0, 0, 0, 0, 0, 1}, // Skip 6, Play 1
    {0, 0, 0, 0, 0, 0, 0, 1}, // Skip 7, Play 1
    {1, 1, 0}, // Play 2, Skip 1
    {1, 1, 0, 0}, // Play 2, Skip 2
    {1, 1, 0, 0, 0}, // Play 2, Skip 3
    {0, 1, 1}, // Skip 1, Play 2
    {0, 0, 1, 1}, // Skip 2, Play 2
    {0, 0, 0, 1, 1}, // Skip 3, Play 2
    {1, 1, 1, 0}, // Play 3, Skip 1
    {1, 1, 1, 0, 0}, // Play 3, Skip 2
    {1, 1, 1, 0, 0, 0}, // Play 3, Skip 3
    {0, 1, 1, 1}, // Skip 1, Play 3
    {0, 0, 1, 1, 1}, // Skip 2, Play 3
    {0, 0, 0, 1, 1, 1}, // Skip 3, Play 3
    {1, 1, 1, 1, 0}, // Play 4, Skip 1
    {1, 1, 1, 1, 0, 0}, // Play 4, Skip 2
    {1, 1, 1, 1, 0, 0, 0}, // Play 4, Skip 3
    {1, 1, 1, 1, 0, 0, 0, 0}, // Play 4, Skip 4
    {1, 1, 1, 1, 1, 0}, // Play 5, Skip 1
    {1, 1, 1, 1, 1, 1, 0}, // Play 6, Skip 1
    {1, 1, 1, 1, 1, 1, 1, 0}, // Play 7, Skip 1
};
/**
 * \brief Tiny class for handling progressing through the steps of a "probability" sequence
 * This could eventually serve as the basis for an arpegiator implementation, but that's later
 */
class ProbabilitySequence {
public:
    ProbabilitySequence() {
        steps[0] = 1;
        length = 1;
    }
    ~ProbabilitySequence() {}

    /**
     * \brief Get the probability result of the next step and progress playback
     * This will increase the current step by one (or wrap), and calculate the
     * probability for that step, returning whether the step should play or not.
     * @return Whether the next step in the sequence should play
     */
    bool nextStep() {
        ++current;
        if (current == length) {
            current = 0;
        }
        // qDebug() << Q_FUNC_INFO << steps << "New current is" << current << "with value" << steps[current];
        if (steps[current] == 0) {
            return false;
        } else if (steps[current] == 1) {
            return true;
        }
        return QRandomGenerator::global()->generateDouble() < steps[current];
    }
    void reset() {
        current = length - 1;
    }
    void setSequence(const QList<double> sequence) {
        length = qMin(ProbabilitySequenceMax, sequence.length());
        current = length - 1;
        for (int position = 0; position < length; ++position) {
            steps[position] = sequence[position];
        }
    }
private:
    double steps[ProbabilitySequenceMax];
    int length{0};
    int current{0};
};

struct StepData {
    // positionBuffers contains commands for the given position, with the key being
    // the on-position delay (so that iterating over the hash gives the scheduling
    // delay for that buffer, and the buffer).
    QHash<int, juce::MidiBuffer> positionBuffers;
    // This contains a hash of probability sequences for each entry on the step.
    // It is cleared when stopping playback, and will be filled during playback
    // per-step-entry (where appropriate, none will exist for entries without
    // probability)
    QHash<int, ProbabilitySequence> probabilitySequences;
    // The amount of offset this step would use for swing purposes (that is, the
    // calculated offset value, rather than the setting, which is a percentage)
    int swingOffset{0};
    // Whether or not this step's data has been constructed by the playback routine
    bool isValid{false};
    void clear() {
        positionBuffers.clear();
        probabilitySequences.clear();
        swingOffset = 0;
        isValid = false;
    }
    void updateSwing(double noteDuration, double swingValue) {
        swingOffset = (2 * noteDuration * swingValue / 100.0) - noteDuration;
        // qDebug() << Q_FUNC_INFO << noteDuration << swingValue << swingOffset;
    }
    juce::MidiBuffer &getOrCreateBuffer(int position) {
        if (!positionBuffers.contains(position)) {
            positionBuffers[position] = juce::MidiBuffer();
        }
        return positionBuffers[position];
    }
    ProbabilitySequence &getOrCreateProbabilitySequence(int stepEntry, int probabilityValue) {
        if (!probabilitySequences.contains(stepEntry)) {
            probabilitySequences.insert(stepEntry, ProbabilitySequence());
            probabilitySequences[stepEntry].setSequence(probabilitySequenceData[probabilityValue]);
        }
        return probabilitySequences[stepEntry];
    }
    void invalidateProbabilityPosition(int stepEntry) {
        if (stepEntry > -1) {
            probabilitySequences.clear();
        } else if (probabilitySequences.contains(stepEntry)) {
            probabilitySequences.remove(stepEntry);
        }
    }
};

struct PatternModelDefaults {
public:
    constexpr static const int externalMidiChannel{-1};
    constexpr static const int defaultNoteDuration{0};
    constexpr static const float stepLength{24.0f};
    constexpr static const int swing{50};
    constexpr static const int patternLength{16};
    constexpr static const KeyScales::Scale scale{KeyScales::ScaleChromatic};
    constexpr static const KeyScales::Pitch pitch{KeyScales::PitchC};
    constexpr static const KeyScales::Octave octave{KeyScales::Octave4};
    constexpr static const int gridModelStartNote{48};
    constexpr static const int gridModelEndNote{64};
};

class PatternModel::Private {
public:
    Private(PatternModel *q) : q(q) {
        playGridManager = PlayGridManager::instance();
        playfieldManager = PlayfieldManager::instance();
        syncTimer = SyncTimer::instance();

        NoteDataPoolEntry* noteDataPrevious{&noteDataPool[NoteDataPoolSize - 1]};
        for (quint64 i = 0; i < NoteDataPoolSize; ++i) {
            noteDataPrevious->next = &noteDataPool[i];
            noteDataPool[i].previous = noteDataPrevious;
            noteDataPrevious = &noteDataPool[i];
        }
        noteDataPoolReadHead = noteDataPoolWriteHead = noteDataPool;
        patternTickToSyncTimerTick = syncTimer->getMultiplier() / 32;
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
    int externalMidiChannel{PatternModelDefaults::externalMidiChannel};
    int defaultNoteDuration{PatternModelDefaults::defaultNoteDuration};
    float stepLength{PatternModelDefaults::stepLength};
    int swing{PatternModelDefaults::swing};
    int availableBars{1};
    int patternLength{PatternModelDefaults::patternLength};
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

    PatternModel *performanceClone{nullptr};
    bool performanceActive{false};

    void noteLengthDetails(int noteLength, qint64 &nextPosition, bool &relevantToUs, qint64 &noteDuration);
    int patternTickToSyncTimerTick{0};

    bool recordingLive{false};
    int liveRecordingQuantizingAmount{0};
    QString liveRecordingSource;
    // First look at the external device id - if we're listening only to that, make sure we're doing that first
    QString liveRecordingSourceExternalDeviceId;
    // Then check the sketchpad track setting, and if that is set explicitly, handle that, otherwise just go with the pattern's own track
    int liveRecordingSourceSketchpadTrack{-1};
    QList<NewNoteData*> recordingLiveNotes;
    NoteDataPoolEntry noteDataPool[128];
    NoteDataPoolEntry *noteDataPoolReadHead{nullptr};
    NoteDataPoolEntry *noteDataPoolWriteHead{nullptr};

    // If true, the most recent result was to play the step entry, otherwise it will be false
    // It is cleared when stopping playback, and will be true until the first probability
    // calculation returns false.
    // nb: this documents intent, and is used by the Same As Previous probability option
    bool mostRecentProbabilityResult{true};

    // These contain the generated information per step, stored per-pisition (that
    // is, the key of each entry is rot * width + column). The position must be cleared
    // on any change made to the step (which should always be done through stetNote and
    // setMetadata which both ensure that this happens). If they are not cleared on all
    // changes, what ends up sent to SyncTimer during playback will not match what the
    // model contains. So, remember your pattern hygiene and clean your buffers!
    QHash<int, StepData> stepData;
    StepData &getOrCreateStepData(int position) {
        if (performanceActive && performanceClone) {
            return performanceClone->d->getOrCreateStepData(position);
        } else {
            if (!stepData.contains(position)) {
                stepData[position] = StepData();
            }
            return stepData[position];
        }
    }

    // Handy variable in case we want to adjust how far ahead we're looking sometime
    // in the future - we look two steps ahead (3 because it's a < comparison), as
    // we need to consider both swing and delay being at their minimum amounts, which
    // puts the thing being considered at the position of the previous previous step.
    static const int lookaheadAmount{3};

    /**
     * \brief Invalidates the position buffers relevant to the given position
     * If you give -1 for the two position indicators, the entire list of steps
     * will be invalidated.
     * This function is required to ensure that all buffers the position could
     * have an impact on (including those which are before it) are invalidated.
     * @param row The row of the position to invalidate
     * @param column The column of the position to invalidate
     */
    void invalidatePosition(int row = -1, int column = -1) {
        if (performanceActive && performanceClone) {
            performanceClone->d->invalidatePosition(row, column);
        } else {
            if (row == -1 || column == -1) {
                stepData.clear();
            } else {
                const int basePosition = (row * width) + column;
                for (int subsequentNoteIndex = 0; subsequentNoteIndex < lookaheadAmount; ++subsequentNoteIndex) {
                    // We clear backwards, just because might as well (by subtracting the subsequentNoteIndex from our base position)
                    int ourPosition = (basePosition - subsequentNoteIndex) % patternLength;
                    stepData.remove(ourPosition);
                }
            }
        }
    }
    /**
     * \brief Invalidates only the note buffers on the position buffers relevant to the given position
     * If you give -1 for the two position indicators, the entire list of buffers
     * will be invalidated
     * @param row The row of the position to invalidate
     * @param column The column of the position to invalidate
     */
    void invalidateNotes(int row = -1, int column = -1) {
        if (performanceActive && performanceClone) {
            performanceClone->d->invalidateNotes(row, column);
        } else {
            if (row == -1 || column == -1) {
                QHash<int, StepData>::iterator stepDataIterator;
                for (stepDataIterator = stepData.begin(); stepDataIterator != stepData.end(); ++stepDataIterator) {
                    stepDataIterator.value().positionBuffers.clear();
                    stepDataIterator.value().isValid = false;
                }
            } else {
                const int basePosition = (row * width) + column;
                for (int subsequentNoteIndex = 0; subsequentNoteIndex < lookaheadAmount; ++subsequentNoteIndex) {
                    // We clear backwards, just because might as well (by subtracting the subsequentNoteIndex from our base position)
                    int ourPosition = (basePosition - subsequentNoteIndex) % patternLength;
                    stepData[ourPosition].positionBuffers.clear();
                    stepData[ourPosition].isValid = false;
                }
            }
        }
    }
    /**
     * \brief Invalidates only the probability sequencer on the position buffers relevant to the given position
     * If you give -1 for the two position indicators, the entire list of sequences
     * will be invalidated.
     * @param row The row of the position to invalidate
     * @param column The column of the position to invalidate
     */
    void invalidateProbabilities(int row = -1, int column = -1) {
        if (performanceActive && performanceClone) {
            performanceClone->d->invalidateProbabilities(row, column);
        } else {
            if (row == -1 || column == -1) {
                QHash<int, StepData>::iterator stepDataIterator;
                for (stepDataIterator = stepData.begin(); stepDataIterator != stepData.end(); ++stepDataIterator) {
                    stepDataIterator.value().probabilitySequences.clear();
                }
            } else {
                const int basePosition = (row * width) + column;
                for (int subsequentNoteIndex = 0; subsequentNoteIndex < lookaheadAmount; ++subsequentNoteIndex) {
                    // We clear backwards, just because might as well (by subtracting the subsequentNoteIndex from our base position)
                    int ourPosition = (basePosition - subsequentNoteIndex) % patternLength;
                    stepData[ourPosition].probabilitySequences.clear();
                }
            }
            }
    }

    SyncTimer* syncTimer{nullptr};
    SequenceModel *sequence;
    int song{0}; // This is just... always zero at the moment, but maybe this would be the global sequence id or something like that?
    int sketchpadTrack{-1};
    int clipIndex{-1};

    PlayGridManager *playGridManager{nullptr};

    KeyScales::Scale scale{PatternModelDefaults::scale};
    KeyScales::Pitch pitch{PatternModelDefaults::pitch};
    KeyScales::Octave octave{PatternModelDefaults::octave};
    PatternModel::KeyScaleLockStyle lockToKeyAndScale{PatternModel::KeyScaleLockOff};

    int gridModelStartNote{PatternModelDefaults::gridModelStartNote};
    int gridModelEndNote{PatternModelDefaults::gridModelEndNote};
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
            if (needle && needle->rootSliceActual()->keyZoneStart() <= midiNote && midiNote <= needle->rootSliceActual()->keyZoneEnd()) {
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
        bool matchedClip{false};
        const bool stopPlayback{byte1 < 0x90 || byte3 == 0};
        const float velocity{float(byte3) / float(127)};
        const int midiChannel{(byte1 & 0xf)};
        for (ClipAudioSource *clip : qAsConst(clips)) {
            // There must be a clip or it just doesn't matter, and then the note must fit inside the clip's keyzone
            if (clip) {
                const QList<ClipAudioSourceSliceSettings*> slices{clip->sliceSettingsActual()};
                const int &sliceCount{clip->sliceCount()};
                const int extraSliceCount{sliceCount + 1};
                bool matchedSlice{false};
                // This little trick (going to slice count + 1) ensures that we run through the slices in defined order, and also process the root slice last
                for (int sliceIndex = 0; sliceIndex < extraSliceCount; ++sliceIndex) {
                    const ClipAudioSourceSliceSettings *slice{sliceIndex == sliceCount ? clip->rootSliceActual() : slices.at(sliceIndex)};
                    if (slice->keyZoneStart() <= byte2 && byte2 <= slice->keyZoneEnd()) {
                        // Since the stop velocity is actually "lift", we can't count on it to match whatever the start velocity was, so... let's stop all notes that match
                        if (stopPlayback || (slice->velocityMinimum() <= byte3 && byte3 <= slice->velocityMaximum())) {
                            if (slice->effectivePlaybackStyle() == ClipAudioSource::OneshotPlaybackStyle && stopPlayback) {
                                // if stop command and clip playback style is Oneshot, don't submit the stop command - just let it run out
                                // to force one-shots to stop, all-notes-off is handled by SamplerSynth directly
                            } else {
                                // subvoice -1 is conceptually the prime voice, anything from 0 inclusive to the amount non-inclusive are the subvoices
                                for (int subvoice = -1; subvoice < slice->subvoiceCountPlayback(); ++subvoice) {
                                    ClipCommand *command = ClipCommand::channelCommand(clip, midiChannel);
                                    command->startPlayback = !stopPlayback;
                                    command->stopPlayback = stopPlayback;
                                    command->subvoice = subvoice;
                                    command->slice = slice->index();
                                    command->exclusivityGroup = slice->exclusivityGroup();
                                    if (command->startPlayback) {
                                        command->changeVolume = true;
                                        command->volume = velocity;
                                    }
                                    if (command->stopPlayback) {
                                        // Don't actually set volume, just store the volume for velocity purposes... yes this is kind of a hack
                                        command->volume = velocity;
                                    }
                                        command->midiNote = byte2;
                                        command->changeLooping = true;
                                        command->looping = slice->looping();
                                    // }
                                    matchedClip = matchedSlice = true;
                                    listToPopulate->write(command, 0);
                                    // qDebug() << Q_FUNC_INFO << "Wrote command to list for" << clip << "slice" << slice << "subvoice" << subvoice;
                                }
                            }
                            // If our selection mode is a one-sample-only mode, bail now (that is,
                            // as with samples, only AllPickingStyle wants us to pick more than one slice)
                            if (matchedSlice && clip->slicePickingStyle() != ClipAudioSource::AllPickingStyle) {
                                break;
                            }
                        }
                    }
                }
                // If our selection mode is a one-sample-only mode, bail now (that is,
                // only AllPickingStyle wants us to pick more than one sample)
                if (matchedClip && zlSyncManager->samplePickingStyle != ClipAudioSource::AllPickingStyle) {
                    break;
                }
            }
        }
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
            isPlaying = d->playfieldManager->clipPlaystate(d->song, d->sketchpadTrack, d->clipIndex) == PlayfieldManager::PlayingState;
        } else if (d->sequence && d->sequence->isPlaying()) {
            if (d->sequence->soloPattern() > -1) {
                isPlaying = (d->sequence->soloPatternObject() == this);
            } else {
                isPlaying = d->playfieldManager->clipPlaystate(d->song, d->sketchpadTrack, d->clipIndex) == PlayfieldManager::PlayingState;
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
    connect(d->playfieldManager, &PlayfieldManager::directPlayfieldStateChanged, this, [this,updateIsPlaying](int song, int track, int clip){
        if (d->sequence && song == d->song && track == d->sketchpadTrack && clip == d->clipIndex) {
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
        connect(d->sequence->playGridManager(), &PlayGridManager::currentSketchpadTrackChanged, this, [this](){
            if (d->sketchpadTrack == -1 && d->sequence->playGridManager()->currentSketchpadTrack() > -1) {
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
    connect(this, &PatternModel::stepLengthChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::swingChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::patternLengthChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::activeBarChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::bankOffsetChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::bankLengthChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::enabledChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::pitchChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::octaveChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::scaleChanged, this, &NotesModel::registerChange);
    connect(this, &PatternModel::lockToKeyAndScaleChanged, this, &NotesModel::registerChange);

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
        int actualChannel = d->noteDestination == PatternModel::ExternalDestination && d->externalMidiChannel > -1 ? d->externalMidiChannel : d->sketchpadTrack;
        MidiRouter::RoutingDestination routerDestination{MidiRouter::ZynthianDestination};
        switch(d->noteDestination) {
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
            MidiRouter::instance()->setSkechpadTrackDestination(d->sketchpadTrack, MidiRouter::ZynthianDestination, actualChannel == d->sketchpadTrack ? -1 : actualChannel);
        } else {
            MidiRouter::instance()->setSkechpadTrackDestination(d->sketchpadTrack, routerDestination, actualChannel == d->sketchpadTrack ? -1 : actualChannel);
        }
        if (d->previouslyUpdatedMidiChannel != d->sketchpadTrack) {
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
                                    newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(oldNote->midiNote(), d->sketchpadTrack));
                                } else {
                                    // This really shouldn't happen - spit out a warning and slap in something unknown so we keep the order intact
                                    newSubnotes << QVariant::fromValue<QObject*>(playGridManager()->getNote(0, d->sketchpadTrack));
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
            d->previouslyUpdatedMidiChannel = d->sketchpadTrack;
        }
    });
    connect(this, &PatternModel::externalMidiChannelChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));
    connect(this, &PatternModel::noteDestinationChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));
    connect(d->zlSyncManager, &ZLPatternSynchronisationManager::recordingPopupActiveChanged, midiChannelUpdater, QOverload<>::of(&QTimer::start));

    connect(d->playGridManager, &PlayGridManager::midiMessage, this, &PatternModel::handleMidiMessage, Qt::DirectConnection);
    connect(SyncTimer::instance(), &SyncTimer::clipCommandSent, this, [this](ClipCommand *clipCommand){
        for (ClipAudioSource *needle : qAsConst(d->clips)) {
            if (needle && needle == clipCommand->clip) {
                Note *note = qobject_cast<Note*>(PlayGridManager::instance()->getNote(clipCommand->midiNote, d->sketchpadTrack));
                if (note) {
                    if (clipCommand->stopPlayback) {
                        note->registerOff(d->sketchpadTrack);
                    }
                    if (clipCommand->startPlayback) {
                        note->registerOn(d->sketchpadTrack);
                    }
                }
                break;
            }
        }
    }, Qt::QueuedConnection);
    // Finally, create our performance clone (last, because it uses some things we've constructed)
    d->performanceClone = new PatternModel(this);
}

PatternModel::PatternModel(PatternModel* parent)
    : NotesModel(parent->playGridManager())
    , d(new Private(this))
{
    // Register the performance model changes in the parent (basically "just" for thumbnail purposes and ui updates
    connect(this, &PatternModel::noteDestinationChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::stepLengthChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::swingChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::patternLengthChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::activeBarChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::bankOffsetChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::bankLengthChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::enabledChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::pitchChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::octaveChanged, parent, &NotesModel::registerChange);
    connect(this, &PatternModel::scaleChanged, parent, &NotesModel::registerChange);
    connect(this, &NotesModel::lastModifiedChanged, parent, &NotesModel::registerChange);
    d->sequence = parent->d->sequence;
}

PatternModel::~PatternModel()
{
    delete d;
}

void PatternModel::cloneOther(PatternModel *otherPattern)
{
    if (otherPattern) {
        startLongOperation();
        clear();
        setWidth(otherPattern->width());
        setHeight(otherPattern->height());
        setStepLength(otherPattern->stepLength());
        setPatternLength(otherPattern->patternLength());
        setActiveBar(otherPattern->activeBar());
        setBankOffset(otherPattern->bankOffset());
        setBankLength(otherPattern->bankLength());
        setEnabled(otherPattern->enabled()); // FIXME This is... not proper, is it?
        setScale(otherPattern->scale());
        setOctave(otherPattern->octave());
        setPitch(otherPattern->pitch());
        setDefaultNoteDuration(otherPattern->defaultNoteDuration());
        setGridModelStartNote(otherPattern->gridModelStartNote());
        setGridModelEndNote(otherPattern->gridModelEndNote());

        // Now clone all the notes
        for (int i = 0; i < rowCount(); ++i) {
            setRowData(i, otherPattern->getRow(i), otherPattern->getRowMetadata(i));
        }
        endLongOperation();
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
        if (newNote->sketchpadTrack() != d->sketchpadTrack) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->sketchpadTrack));
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
        if (newNote->sketchpadTrack() != d->sketchpadTrack) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->sketchpadTrack));
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
        if (newNote->sketchpadTrack() != d->sketchpadTrack) {
            newNote = qobject_cast<Note*>(playGridManager()->getNote(newNote->midiNote(), d->sketchpadTrack));
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
        if (key == "probability") {
            const int stepPosition{row * width() + column};
            if (d->stepData.contains(stepPosition)) {
                d->stepData[stepPosition].invalidateProbabilityPosition(subnote);
            }
        } else if (key == "delay") {
            d->invalidatePosition();
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

void PatternModel::nudge(int firstStep, int lastStep, int amount, const QVariantList &noteFilter)
{
    // qDebug() << Q_FUNC_INFO << firstStep << lastStep << amount << noteFilter;
    if (abs(amount) > 0 && firstStep > -1 && lastStep > -1 && firstStep < d->patternLength && lastStep < d->patternLength) {
        startLongOperation();
        // In case there's no entries in the filter, just add all the notes (which allows us to just always apply the filter)
        QList<int> noteFilterActual;
        if (noteFilter.isEmpty()) {
            for (int midiNote = 0; midiNote < 128; ++midiNote) {
                noteFilterActual << midiNote;
            }
        } else {
            for (const QVariant &variantNote : noteFilter) {
                Note *note = variantNote.value<Note*>();
                if (note) {
                    noteFilterActual << note->midiNote();
                } else if (variantNote.type() == QVariant::Int) {
                    noteFilterActual << variantNote.value<int>();
                }
            }
        }
        // These could kind of be in any order, but let's just make sure that for our own algorithmic sanity, they're linguistically sound
        if (firstStep > lastStep) {
            const int tempLast{lastStep};
            lastStep = firstStep;
            firstStep = tempLast;
        }
        // Find the offset amount by fitting it inside the range (that is, normalise the amount)
        const int range{lastStep - firstStep};
        while (abs(amount) > range) {
            amount += (amount > 0) ? -range : range;
        }
        // Remove all the notes in noteFilter from all the entries in the step range, and store them in lists
        QList<QList<Note*>> originalNotes;
        QList<QList<QVariantHash>> originalMetadata;
        for (int rangeStep = firstStep; rangeStep < lastStep + 1; ++rangeStep) {
            const int row{rangeStep / d->width};
            const int column{rangeStep - (row * d->width)};
            Note* stepNote = qobject_cast<Note*>(getNote(row, column));
            QVariantList stepMetadata = getMetadata(row, column).toList();
            QList<Note*> filteredStepNotes;
            QList<QVariantHash> filteredStepMetadata;
            // Run through all the existing subnotes, and pull out the ones that match a note value we've been asked to handle
            if (stepNote) {
                for (int subNoteIndex = stepNote->subnotes().count() - 1; subNoteIndex > -1 ; --subNoteIndex) {
                    Note *subNote = stepNote->subnotes().at(subNoteIndex).value<Note*>();
                    if (noteFilterActual.contains(subNote->midiNote())) {
                        filteredStepNotes << subNote;
                        QVariantHash subnoteMetadata;
                        filteredStepMetadata << stepMetadata[subNoteIndex].toHash();
                        removeSubnote(row, column, subNoteIndex);
                    }
                }
            }
            originalNotes << filteredStepNotes;
            originalMetadata << filteredStepMetadata;
        }
        // qDebug() << Q_FUNC_INFO << "Original notes:" << originalNotes;
        // Depending on the direction of movement, move an amount of step data from the front to the end, or vice versa
        if (amount > 0) {
            for (int remainingMoves = amount; remainingMoves > 0; --remainingMoves) {
                // qDebug() << "Swapping last step to the front, remaining moves:" << remainingMoves - 1;
                originalNotes.prepend(originalNotes.takeLast());
                originalMetadata.prepend(originalMetadata.takeLast());
            }
        } else {
            for (int remainingMoves = abs(amount); remainingMoves > 0; --remainingMoves) {
                // qDebug() << "Swapping first step to the rear, remaining moves:" << remainingMoves - 1;
                originalNotes.append(originalNotes.takeFirst());
                originalMetadata.append(originalMetadata.takeFirst());
            }
        }
        // qDebug() << Q_FUNC_INFO << "Shifted  notes:" << originalNotes;
        // Re-add the now rotated notes and metadata into their new homes
        for (int rangeStep = firstStep; rangeStep < lastStep + 1; ++rangeStep) {
            const int row{rangeStep / d->width};
            const int column{rangeStep - (row * d->width)};
            const QList<Note*> &stepNotes = originalNotes[rangeStep - firstStep];
            const QList<QVariantHash> &stepMetadata = originalMetadata[rangeStep - firstStep];
            for (int stepNoteIndex = 0; stepNoteIndex < stepNotes.count(); ++stepNoteIndex) {
                const int subNoteIndex = insertSubnoteSorted(row, column, stepNotes[stepNoteIndex]);
                const QVariantHash &temporarySubnoteMetadata = stepMetadata[stepNoteIndex];
                for (QVariantHash::const_iterator tempMetadataIterator = temporarySubnoteMetadata.constBegin(); tempMetadataIterator != temporarySubnoteMetadata.constEnd(); ++tempMetadataIterator) {
                    setSubnoteMetadata(row, column, subNoteIndex, tempMetadataIterator.key(), tempMetadataIterator.value());
                }
            }
        }
        endLongOperation();
        registerChange();
    }
}

void PatternModel::resetPattern(bool clearNotes)
{
    startLongOperation();
    setNoteDestination(PatternModel::SynthDestination);
    setExternalMidiChannel(PatternModelDefaults::externalMidiChannel);
    setDefaultNoteDuration(PatternModelDefaults::defaultNoteDuration);
    setStepLength(PatternModelDefaults::stepLength);
    setSwing(PatternModelDefaults::swing);
    setPatternLength(PatternModelDefaults::patternLength);
    setBankOffset(0);
    setBankLength(8);
    setGridModelStartNote(PatternModelDefaults::gridModelStartNote);
    setGridModelEndNote(PatternModelDefaults::gridModelEndNote);
    setWidth(16);
    setPitch(PatternModelDefaults::pitch);
    setOctave(PatternModelDefaults::octave);
    setScale(PatternModelDefaults::scale);
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

int PatternModel::sketchpadTrack() const
{
    return d->sketchpadTrack;
}

void PatternModel::setSketchpadTrack(int sketchpadTrack)
{
    if (d->sketchpadTrack != sketchpadTrack) {
        d->sketchpadTrack = sketchpadTrack;
        Q_EMIT sketchpadTrackChanged();
    }
}

int PatternModel::clipIndex() const
{
    return d->clipIndex;
}

QString PatternModel::clipName() const
{
    static const QString clipNames[5]{"a", "b", "c", "d", "e"};
    return (d->clipIndex > -1 && d->clipIndex < 5) ? clipNames[d->clipIndex] : "";
}

void PatternModel::setClipIndex(int clipIndex)
{
    if (d->clipIndex != clipIndex) {
        d->clipIndex = clipIndex;
        Q_EMIT clipIndexChanged();
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
        SyncTimer::instance()->sendMidiBufferImmediately(buffer, d->sketchpadTrack);
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

void PatternModel::setStepLength(const double& stepLength)
{
    // 384 * 16 == 6144
    double adjusted = std::clamp(stepLength, 1.0, 6144.0);
    if (d->stepLength != adjusted) {
        d->stepLength = adjusted;
        d->invalidatePosition();
        Q_EMIT stepLengthChanged();
    }
}

double PatternModel::stepLength() const
{
    return d->stepLength;
}

QString PatternModel::stepLengthName(const double& stepLength) const
{
    static const QMap<double, QString> lengthNames{{384, "4"}, {288, "3"}, {192, "2"}, {168, "7/4"}, {160, "5/3"}, {144, "3/2"}, {128, "4/3"}, {120, "5/4"}, {96, "1"}, {64, "2/3"}, {48, "1/2"}, {32, "1/3"}, {24, "1/4"}, {16, "1/6"}, {12, "1/8"}, {8, "1/12"}, {6, "1/16"}, {4, "1/24"}, {3, "1/32"}, {2, "1/48"}, {1, "1/96"}};
    if (lengthNames.contains(stepLength)) {
        return lengthNames[stepLength];
    } else if (stepLength > 96) {
        const int beatCount{int(stepLength) / 96};
        return QString::fromUtf8("%1♩%2/96").arg(beatCount).arg(int(stepLength - (beatCount * 96)) % 96);
    }
    return QString::fromUtf8("%1/96").arg(stepLength);
}

double PatternModel::nextStepLengthStep(const double &startingPoint, const int& direction) const
{
    static const QList<double> stepLengthSteps{1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 288, 384};
    double returnValue = startingPoint;
    if (direction > 0) {
        // Increasing the step position to the next nearest upward position (or self if there isn't one)
        int index{0};
        while (index < stepLengthSteps.count() - 1 && stepLengthSteps[index] <= startingPoint) {
            ++index;
        }
        if (returnValue < stepLengthSteps[index]) {
            returnValue = stepLengthSteps[index];
        }
    } else {
        // Decreasing the step position to the next nearest downward position (or self if there isn't one)
        int index{stepLengthSteps.count() - 1};
        while (index > 0 && stepLengthSteps[index] >= startingPoint) {
            --index;
        }
        if (returnValue > stepLengthSteps[index]) {
            returnValue = stepLengthSteps[index];
        }
    }
    return returnValue;
}

void PatternModel::setSwing(int swing)
{
    if (d->swing != swing) {
        if (swing == 0) {
            d->swing = 50;
        } else {
            d->swing = std::clamp(swing, 1, 99);
        }
        // Invalidate all positions (as swing might be scheduled in a previous step due to microtiming settings for the individual step/note)
        d->invalidatePosition();
        Q_EMIT swingChanged();
    }
}

int PatternModel::swing() const
{
    return d->swing;
}

int PatternModel::availableBars() const
{
    return d->availableBars;
}

void PatternModel::setPatternLength(const int& patternLength)
{
    int adjusted = qMin(qMax(1, patternLength), bankLength() * d->width);
    if (d->patternLength != adjusted) {
        d->patternLength = adjusted;
        d->availableBars = ((d->patternLength - 1) / d->width) + 1;
        Q_EMIT patternLengthChanged();
        // Ensure that we don't have an active bar that's outside our available range
        setActiveBar(qMin(d->activeBar, d->availableBars - 1));
    }
}

int PatternModel::patternLength() const
{
    return d->patternLength;
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
        setPatternLength(d->patternLength);
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

bool PatternModel::hasContent() const
{
    bool hasContent{false};
    if (d->externalMidiChannel != PatternModelDefaults::externalMidiChannel) {
        hasContent = true;
    }
    if (!hasContent && d->defaultNoteDuration != PatternModelDefaults::defaultNoteDuration) {
        hasContent = true;
    }
    if (!hasContent && d->stepLength != PatternModelDefaults::stepLength) {
        hasContent = true;
    }
    if (!hasContent && d->swing != PatternModelDefaults::swing) {
        hasContent = true;
    }
    if (!hasContent && d->patternLength != PatternModelDefaults::patternLength) {
        hasContent = true;
    }
    if (!hasContent && d->scale != PatternModelDefaults::scale) {
        hasContent = true;
    }
    if (!hasContent && d->pitch != PatternModelDefaults::pitch) {
        hasContent = true;
    }
    if (!hasContent && d->octave != PatternModelDefaults::octave) {
        hasContent = true;
    }
    if (!hasContent && d->gridModelStartNote != PatternModelDefaults::gridModelStartNote) {
        hasContent = true;
    }
    if (!hasContent && d->gridModelEndNote != PatternModelDefaults::gridModelEndNote) {
        hasContent = true;
    }
    if (!hasContent && hasNotes()) {
        hasContent = true;
    }
    return hasContent;
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
            // ClipAudioSource *previousClip{nullptr};
            // for (int i = 0; i < d->clips.count(); ++i) {
            //     ClipAudioSource *clip = d->clips.at(i);
            //     if (clip) {
            //         int sliceStart{clip->sliceBaseMidiNote()};
            //         int nextClipStart{129};
            //         for (int j = i + 1; j < d->clips.count(); ++j) {
            //             ClipAudioSource *nextClip = d->clips.at(j);
            //             if (nextClip) {
            //                 nextClipStart = nextClip->sliceBaseMidiNote();
            //                 break;
            //             }
            //         }
            //         // Let's see if we can push it /back/ a bit, and still get a full thing... less lovely, but it gives a full spread
            //         if (nextClipStart - clip->slices() < sliceStart) {
            //             sliceStart = qMax(previousClip ? previousClip->sliceBaseMidiNote() + previousClip->slices(): 0, nextClipStart - clip->slices());
            //         }
            //         // Now let's add as many notes as we need, or have space for, whichever is smaller
            //         int addedNotes{0};
            //         for (int note = sliceStart; note < nextClipStart && addedNotes < clip->slices(); ++note) {
            //             notesToFit << note;
            //             noteTitles << QString("Sample %1\nSlice %2").arg(QString::number(i + 1)).arg(QString::number(clip->sliceForMidiNote(note) + 1));
            //             ++addedNotes;
            //         }
            //         previousClip = clip;
            //     }
            // }
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
                    notes << QVariant::fromValue<QObject*>(PlayGridManager::instance()->getNote(notesToFit[i], d->sketchpadTrack));
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
        connect(this, &PatternModel::sketchpadTrackChanged, refilTimer, QOverload<>::of(&QTimer::start));
        refilTimer->start();
    }
    return d->clipSliceNotes;
}

int PatternModel::scale() const
{
    return KeyScales::instance()->scaleEnumKeyToIndex(d->scale);
}

KeyScales::Scale PatternModel::scaleKey() const
{
    return d->scale;
}

void PatternModel::setScale(int scale)
{
    if (-1 < scale && scale < KeyScales::instance()->scaleNames().count()) {
        KeyScales::Scale newScale = KeyScales::instance()->scaleIndexToEnumKey(scale);
        if (d->scale != newScale) {
            d->scale = newScale;
            Q_EMIT scaleChanged();
        }
    }
}

void PatternModel::setScaleKey(const KeyScales::Scale& scale)
{
    if (d->scale != scale) {
        d->scale = scale;
        Q_EMIT scaleChanged();
    }
}

int PatternModel::pitch() const
{
    return KeyScales::instance()->pitchEnumKeyToIndex(d->pitch);
}

KeyScales::Pitch PatternModel::pitchKey() const
{
    return d->pitch;
}

void PatternModel::setPitch(int pitch)
{
    if (-1 < pitch && pitch < KeyScales::instance()->pitchNames().count()) {
        KeyScales::Pitch newPitch = KeyScales::instance()->pitchIndexToEnumKey(pitch);
        if (d->pitch != newPitch) {
            d->pitch = newPitch;
            Q_EMIT pitchChanged();
        }
    }
}

void PatternModel::setPitchKey(const KeyScales::Pitch& pitch)
{
    if (d->pitch != pitch) {
        d->pitch = pitch;
        Q_EMIT pitchChanged();
    }
}

int PatternModel::octave() const
{
    return KeyScales::instance()->octaveEnumKeyToIndex(d->octave);
}

KeyScales::Octave PatternModel::octaveKey() const
{
    return d->octave;
}

void PatternModel::setOctave(int octave)
{
    if (-1 < octave && octave < KeyScales::instance()->octaveNames().count()) {
        KeyScales::Octave newOctave = KeyScales::instance()->octaveIndexToEnumKey(octave);
        if (d->octave != newOctave) {
            d->octave = newOctave;
            Q_EMIT octaveChanged();
        }
    }
}

void PatternModel::setOctaveKey(const KeyScales::Octave& octave)
{
    if (d->octave != octave) {
        d->octave = octave;
        Q_EMIT octaveChanged();
    }
}

PatternModel::KeyScaleLockStyle PatternModel::lockToKeyAndScale() const
{
    return d->lockToKeyAndScale;
}

void PatternModel::setLockToKeyAndScale(const PatternModel::KeyScaleLockStyle& lockToKeyAndScale)
{
    if (d->lockToKeyAndScale != lockToKeyAndScale) {
        d->lockToKeyAndScale = lockToKeyAndScale;
        Q_EMIT lockToKeyAndScaleChanged();
    }
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
            // qDebug() << "Rebuilding" << d->gridModel << "for destination" << d->noteDestination << "for track" << d->sketchpadTrack;
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
                    Note* note = qobject_cast<Note*>(PlayGridManager::instance()->getNote(notesToFit[i], d->sketchpadTrack));
                    notes << QVariant::fromValue<QObject*>(note);
                    QList<ClipAudioSource*> clips = d->clipsForMidiNote(note->midiNote());
                    if (noteDestination() == SampleTriggerDestination) {
                        QString noteTitle{midiNoteNames[note->midiNote()]};
                        if (clips.length() > 0) {
                            for (ClipAudioSource* clip : clips) {
                                int clipIndex = d->clips.indexOf(clip);
                                QString actualNote{};
                                if (clip->rootSliceActual()->rootNote() != 60) {
                                    int actualNoteValue = note->midiNote() + (60 - clip->rootSliceActual()->rootNote());
                                    if (actualNoteValue > -1 && actualNoteValue < 128) {
                                        actualNote = QString(" (%1)").arg(midiNoteNames[actualNoteValue]);
                                    }
                                }
                                noteTitle += QString("\nSample %1%2").arg(QString::number(clipIndex + 1)).arg(actualNote);
                            }
                        // } else {
                            // No need to write this out explicitly...
                            // noteTitle += QString{"\n(no sample)"};
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
        connect(this, &PatternModel::sketchpadTrackChanged, refilTimer, QOverload<>::of(&QTimer::start));
        connect(this, &PatternModel::gridModelStartNoteChanged, refilTimer, QOverload<>::of(&QTimer::start));
        connect(this, &PatternModel::gridModelEndNoteChanged, refilTimer, QOverload<>::of(&QTimer::start));
        // To ensure we also update when the clips for each position change
        connect(this, &PatternModel::noteDestinationChanged, refilTimer, QOverload<>::of(&QTimer::start));
        auto updateClips = [this,refilTimer](){
            for (ClipAudioSource *clip : d->clips) {
                if (clip) {
                    connect(clip->rootSliceActual(), &ClipAudioSourceSliceSettings::keyZoneStartChanged, refilTimer, QOverload<>::of(&QTimer::start));
                    connect(clip->rootSliceActual(), &ClipAudioSourceSliceSettings::keyZoneEndChanged, refilTimer, QOverload<>::of(&QTimer::start));
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

void PatternModel::setLiveRecordingQuantizingAmount(const int& liveRecordingQuantizingAmount)
{
    if (d->liveRecordingQuantizingAmount != liveRecordingQuantizingAmount) {
        d->liveRecordingQuantizingAmount = liveRecordingQuantizingAmount;
        Q_EMIT liveRecordingQuantizingAmountChanged();
    }
}

int PatternModel::liveRecordingQuantizingAmount() const
{
    return d->liveRecordingQuantizingAmount;
}

void PatternModel::setLiveRecordingSource(const QString& newLiveRecordingSource)
{
    static const QLatin1String sketchpadTrackSource{"sketchpadTrack:"};
    static const QLatin1String externalDeviceSource{"external:"};
    if (d->liveRecordingSource != newLiveRecordingSource) {
        d->liveRecordingSource = newLiveRecordingSource;
        if (d->liveRecordingSource.startsWith(sketchpadTrackSource)) {
            d->liveRecordingSourceExternalDeviceId.clear();
            d->liveRecordingSourceSketchpadTrack = d->liveRecordingSource.midRef(15).toInt();
            if (d->liveRecordingSourceSketchpadTrack < -2 || ZynthboxTrackCount > d->liveRecordingSourceSketchpadTrack) {
                d->liveRecordingSourceSketchpadTrack = -1;
            }
        } else if (d->liveRecordingSource.startsWith(externalDeviceSource)) {
            d->liveRecordingSourceSketchpadTrack = -1;
            d->liveRecordingSourceExternalDeviceId = d->liveRecordingSource.mid(9);
        } else {
            d->liveRecordingSourceExternalDeviceId.clear();
            d->liveRecordingSourceSketchpadTrack = -1;
        }
        Q_EMIT liveRecordingSourceChanged();
    }
}

QString PatternModel::liveRecordingSource() const
{
    return d->liveRecordingSource;
}

void PatternModel::startPerformance()
{
    qDebug() << Q_FUNC_INFO;
    if (d->performanceClone) {
        d->performanceClone->cloneOther(this);
        d->performanceActive = true;
        Q_EMIT performanceActiveChanged();
    }
}

void PatternModel::applyPerformance()
{
    qDebug() << Q_FUNC_INFO;
    if (d->performanceClone) {
        cloneOther(d->performanceClone);
    }
}

void PatternModel::stopPerformance()
{
    qDebug() << Q_FUNC_INFO;
    if (d->performanceClone && d->performanceActive) {
        d->performanceActive = false;
        Q_EMIT performanceActiveChanged();
    }
}

QObject * PatternModel::workingModel()
{
    return d->performanceActive && d->performanceClone ? d->performanceClone : this;
}

QObject * PatternModel::performanceClone() const
{
    return d->performanceClone;
}

bool PatternModel::performanceActive() const
{
    return d->performanceActive;
}

QObject *PatternModel::zlChannel() const
{
    return d->zlSyncManager->zlChannel;
}

void PatternModel::setZlChannel(QObject *zlChannel)
{
    d->zlSyncManager->setZlChannel(zlChannel);
}

QObject *PatternModel::zlClip() const
{
    return d->zlSyncManager->zlClip;
}

void PatternModel::setZlClip(QObject *zlClip)
{
    d->zlSyncManager->setZlClip(zlClip);
}

QObject *PatternModel::zlScene() const
{
    return d->zlSyncManager->zlScene;
}

void PatternModel::setZlScene(QObject *zlScene)
{
    d->zlSyncManager->setZlScene(zlScene);
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

static inline void addNoteToBuffer(juce::MidiBuffer &buffer, const Note *theNote, const unsigned char &velocity, const bool &setOn, const int &availableChannel) {
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

inline void PatternModel::Private::noteLengthDetails(int stepLength, qint64 &nextPosition, bool &relevantToUs, qint64 &noteDuration)
{
    if (nextPosition % stepLength == 0) {
        noteDuration = stepLength;
        nextPosition = nextPosition / stepLength;
        relevantToUs = true;
    } else {
        relevantToUs = false;
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
    static const QLatin1String nextStepString{"next-step"};
    if (!d->zlSyncManager->channelMuted && isPlaying()) {
        if (d->updateMostRecentStartTimestamp) {
            d->updateMostRecentStartTimestamp = false;
            d->mostRecentStartTimestamp = sequencePosition;
        }
        qint64 noteDuration{0};
        bool relevantToUs{false};
        for (int progressionIncrement = 0; progressionIncrement <= progressionLength; ++progressionIncrement) {
            // As we might change the offset on some step, we'll need that in here
            const qint64 playbackOffset{d->playfieldManager->clipOffset(d->song, d->sketchpadTrack, d->clipIndex) - (d->segmentHandler->songMode() ? d->segmentHandler->startOffset() : 0)};
            // check whether the sequencePosition + progressionIncrement matches our note length
            qint64 nextPosition = sequencePosition - playbackOffset + progressionIncrement;
            d->noteLengthDetails(d->stepLength, nextPosition, relevantToUs, noteDuration);
            const int schedulingIncrement{progressionIncrement * d->patternTickToSyncTimerTick};

            if (relevantToUs) {
                // Get the next row/column combination, and schedule the previous one off, and the next one on
                // squish nextPosition down to fit inside our available range d->patternLength
                // start + (numberToBeWrapped - start) % (limit - start)
                nextPosition = nextPosition % d->patternLength;
                // If we have any kind of probability involved in this step (including the look-ahead), we'll
                // need to clear it immediately, so that probability is also taken into account for the next
                // time it's due for scheduling
                bool invalidateNoteBuffersImmediately{false};
                // qDebug() << "Swing offset for" << nextPosition << "is" << swingOffset << "with swing" << d->swing << "and note duration" << noteDuration;

                StepData &stepData = d->getOrCreateStepData(nextPosition + (d->bankOffset * d->width));
                if (stepData.isValid == false) {
                    auto subnoteSender = [this, nextPosition, &invalidateNoteBuffersImmediately, noteDuration, schedulingIncrement, &stepData](const Note* subnote, const QVariantHash &metaHash, const qint64 &delay, StepData &noteStepData, int subnoteIndex) {
                        bool sendNotes{true};
                        // qDebug() << Q_FUNC_INFO << "Preparing note" << subnote << "at index" << subnoteIndex << "with meta hash" << metaHash;
                        const int probability{metaHash.value(probabilityString, 0).toInt()};
                        if (probability > 0) {
                            invalidateNoteBuffersImmediately = true;
                            if (probability != 10) { // 10 is the Same As Previous option (meaning simply use whatever the most recent probability result was for this pattern)
                                d->mostRecentProbabilityResult = noteStepData.getOrCreateProbabilitySequence(subnoteIndex, probability).nextStep();
                            }
                            sendNotes = d->mostRecentProbabilityResult;
                        }
                        if (sendNotes) {
                            int nextStep{metaHash.value(nextStepString, 0).toInt()};
                            if (nextStep > 0) {
                                // Technically the steps are 0-indexed, but this makes displaying it a little easier, and it's inexpensive here anyway
                                --nextStep;
                                // Reset this clip's playfield offset by the distance from this clip to the clip we are asking to play
                                // next (or, rather, move it forward to the end of the pattern, and then set it to the next step)
                                nextStep = (d->patternLength - nextPosition + nextStep) * noteDuration;
                                d->playfieldManager->setClipPlaystate(d->song, d->sketchpadTrack, d->clipIndex, PlayfieldManager::PlayingState, PlayfieldManager::CurrentPosition, d->playfieldManager->clipOffset(d->song, d->sketchpadTrack, d->clipIndex) + nextStep);
                            }
                            int velocity{metaHash.value(velocityString, 64).toInt()};
                            if (velocity == 0) {
                                velocity = 64;
                            } else if (velocity == -1) {
                                sendNotes = false;
                            }
                            if (sendNotes) {
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
                                        invalidateNoteBuffersImmediately = true;
                                    }
                                    int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack, quint64(schedulingIncrement));
                                    for (int ratchetIndex = 0; ratchetIndex < ratchetCount; ++ratchetIndex) {
                                        sendNotes = true;
                                        if (ratchetProbability < 100) {
                                            if (QRandomGenerator::global()->generateDouble() * 100 < ratchetProbability) {
                                                sendNotes = false;
                                            }
                                        }
                                        if (sendNotes) {
                                            if (ratchetIndex + 1 == ratchetCount) {
                                                ratchetDuration = ratchetLastDuration;
                                            }
                                            addNoteToBuffer(stepData.getOrCreateBuffer(delay + (ratchetDelay * ratchetIndex)), subnote, velocity, true, avaialbleChannel);
                                            addNoteToBuffer(stepData.getOrCreateBuffer(delay + (ratchetDelay * ratchetIndex) + ratchetDuration), subnote, velocity, false, avaialbleChannel);
                                            if (reuseChannel == false && ratchetIndex + 1 < ratchetCount) {
                                                avaialbleChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack, quint64(schedulingIncrement));
                                            }
                                        }
                                    }
                                } else {
                                    const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack, quint64(schedulingIncrement));
                                    addNoteToBuffer(stepData.getOrCreateBuffer(delay), subnote, velocity, true, avaialbleChannel);
                                    addNoteToBuffer(stepData.getOrCreateBuffer(delay + duration), subnote, velocity, false, avaialbleChannel);
                                }
                            }
                        }
                    };
                    // Do a lookup for any notes after this position that want playing before their step (currently
                    // just looking ahead two steps, accounting for delay and swing both adjusting one step backwards)
                    for (int subsequentStepIndex = 0; subsequentStepIndex < d->lookaheadAmount; ++subsequentStepIndex) {
                        const int ourPosition = (nextPosition + subsequentStepIndex) % d->patternLength;
                        StepData &subsequentStep = d->getOrCreateStepData(ourPosition);
                        // Swing is applied to every even step as counted by humans (so every uneven step as counted by our indices)
                        subsequentStep.updateSwing(noteDuration, ourPosition % 2 == 0 ? 50 : (d->performanceActive && d->performanceClone ? d->performanceClone->d->swing : d->swing));
                        const int row = (ourPosition / d->width) % d->availableBars;
                        const int column = ourPosition - (row * d->width);
                        const Note *note = (d->performanceActive && d->performanceClone) ? qobject_cast<const Note*>(d->performanceClone->getNote(row + d->bankOffset, column)) : qobject_cast<const Note*>(getNote(row + d->bankOffset, column));
                        if (note) {
                            const QVariantList &subnotes = note->subnotes();
                            const QVariantList &meta = (d->performanceActive && d->performanceClone) ? d->performanceClone->getMetadata(row + d->bankOffset, column).toList() : getMetadata(row + d->bankOffset, column).toList();
                            // The first step (that is, the "current" step) we want to treat to all the things
                            if (subsequentStepIndex == 0) {
                                // qDebug() << Q_FUNC_INFO << "Step: Position" << ourPosition;
                                if (meta.count() == subnotes.count()) {
                                    for (int subnoteIndex = 0; subnoteIndex < subnotes.count(); ++subnoteIndex) {
                                        const Note *subnote = subnotes[subnoteIndex].value<Note*>();
                                        const QVariantHash &metaHash = meta[subnoteIndex].toHash();
                                        if (subnote) {
                                            const qint64 delay{(metaHash.value(delayString, 0).toInt() * d->patternTickToSyncTimerTick) + subsequentStep.swingOffset};
                                            // Only handle if the delay is zero or in the future (since if it's in
                                            // the past, we'd be handling it twice, and at the wrong time)
                                            // qDebug() << Q_FUNC_INFO << "Delay is" << delay;
                                            if (delay >= 0) {
                                                subnoteSender(subnote, metaHash, delay, subsequentStep, subnoteIndex);
                                            }
                                        }
                                    }
                                } else if (subnotes.count() > 0) {
                                    for (const QVariant &subnoteVar : subnotes) {
                                        const Note *subnote = subnoteVar.value<Note*>();
                                        if (subnote && subsequentStep.swingOffset >= 0) {
                                            const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack, quint64(schedulingIncrement));
                                            addNoteToBuffer(stepData.getOrCreateBuffer(subsequentStep.swingOffset), subnote, 64, true, avaialbleChannel);
                                            addNoteToBuffer(stepData.getOrCreateBuffer(subsequentStep.swingOffset + noteDuration), subnote, 64, false, avaialbleChannel);
                                        }
                                    }
                                } else if (subsequentStep.swingOffset >= 0) {
                                    const int avaialbleChannel = d->syncTimer->nextAvailableChannel(d->sketchpadTrack, quint64(schedulingIncrement));
                                    addNoteToBuffer(stepData.getOrCreateBuffer(subsequentStep.swingOffset), note, 64, true, avaialbleChannel);
                                    addNoteToBuffer(stepData.getOrCreateBuffer(subsequentStep.swingOffset + noteDuration), note, 64, false, avaialbleChannel);
                                }
                            // The lookahead notes only need handling if, and only if, there is matching meta (or negative swing), and the delay+swing is negative (that meaning, the position of the entry is before that step)
                            } else {
                                // qDebug() << Q_FUNC_INFO << "Step: Subsequent" << subsequentStepIndex;
                                if (meta.count() == subnotes.count() || subsequentStep.swingOffset < 0) {
                                    const qint64 positionAdjustment{subsequentStepIndex * noteDuration};
                                    for (int subnoteIndex = 0; subnoteIndex < subnotes.count(); ++subnoteIndex) {
                                        const Note *subnote = subnotes[subnoteIndex].value<Note*>();
                                        const QVariantHash &metaHash = meta[subnoteIndex].toHash();
                                        if (subnote) {
                                            const qint64 delay{(metaHash.value(delayString, 0).toInt() * d->patternTickToSyncTimerTick) + subsequentStep.swingOffset};
                                            // qDebug() << Q_FUNC_INFO << "Delay is" << delay;
                                            if (delay < 0) {
                                                subnoteSender(subnote, metaHash, positionAdjustment +  delay, subsequentStep, subnoteIndex);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    stepData.isValid = true;
                }
                switch (d->noteDestination) {
                    case PatternModel::SampleLoopedDestination:
                        // If this channel is supposed to loop its sample, we are not supposed to be making patterny sounds
                        break;
                    case PatternModel::SampleTriggerDestination:
                    {
                        const StepData &stepData = d->getOrCreateStepData(nextPosition + (d->bankOffset * d->width));
                        QHash<int, juce::MidiBuffer>::const_iterator position;
                        for (position = stepData.positionBuffers.constBegin(); position != stepData.positionBuffers.constEnd(); ++position) {
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
                        const StepData &stepData = d->getOrCreateStepData(nextPosition + (d->bankOffset * d->width));
                        QHash<int, juce::MidiBuffer>::const_iterator position;
                        for (position = stepData.positionBuffers.constBegin(); position != stepData.positionBuffers.constEnd(); ++position) {
                            d->syncTimer->scheduleMidiBuffer(position.value(), quint64(qMax(0, schedulingIncrement + position.key())), d->sketchpadTrack);
                        }
                        break;
                    }
                }
                if (invalidateNoteBuffersImmediately) {
                    for (int subsequentNoteIndex = 0; subsequentNoteIndex < d->lookaheadAmount; ++subsequentNoteIndex) {
                        const int ourPosition = (nextPosition + subsequentNoteIndex) % d->patternLength;
                        const int row = (ourPosition / d->width) % d->availableBars;
                        const int column = ourPosition - (row * d->width);
                        d->invalidateNotes(row, column);
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
    if (isPlaying() || sequencePosition == 0) {
        const qint64 playbackOffset{d->playfieldManager->clipOffset(d->song, d->sketchpadTrack, d->clipIndex) - (d->segmentHandler->songMode() ? d->segmentHandler->startOffset() : 0)};
        bool relevantToUs{false};
        qint64 nextPosition{sequencePosition - playbackOffset};
        qint64 noteDuration{0};
        d->noteLengthDetails(d->stepLength, nextPosition, relevantToUs, noteDuration);

        if (relevantToUs) {
            nextPosition = nextPosition % d->patternLength;
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
    d->invalidateProbabilities();
    d->mostRecentProbabilityResult = true;
    setRecordLive(false);
}

void PatternModel::handleMidiMessage(const MidiRouter::ListenerPort &port, const quint64 &timestamp, const unsigned char &byte1, const unsigned char &byte2, const unsigned char &byte3, const int& sketchpadTrack, const QString& hardwareDeviceId)
{
    if (d->liveRecordingSourceExternalDeviceId.isEmpty()
            ? d->liveRecordingSourceSketchpadTrack == -1
                ? (port == MidiRouter::ListenerPort::HardwareInPassthroughPort || port == MidiRouter::ListenerPort::InternalControllerPassthroughPort) // Ignoring events that are from the sequencer, only controller things interest us here
                    ? sketchpadTrack == d->sketchpadTrack
                    : sketchpadTrack == d->liveRecordingSourceSketchpadTrack
                : false
            : port == MidiRouter::HardwareInPassthroughPort
                ? d->liveRecordingSourceExternalDeviceId == hardwareDeviceId
                : false
        ) {
        // if we're recording live, and it's a note-on message, create a newnotedata and add to list of notes being recorded (byte3 > 0 because velocity 0 is how some gear sends a note off message)
        if (d->recordingLive && 0x8F < byte1 && byte1 < 0xA0 && byte3 > 0) {
            // Belts and braces here - it shouldn't really happen (a hundred notes is kind of a lot to add in a single shot), but just in case...
            if (d->noteDataPoolReadHead->object) {
                NewNoteData *newNote = d->noteDataPoolReadHead->object;
                d->noteDataPoolReadHead = d->noteDataPoolReadHead->next;
                newNote->timestamp = d->syncTimer->timerTickForJackPlayhead(timestamp, &newNote->timestampOffset);
                newNote->midiNote = byte2;
                newNote->velocity = byte3;
                newNote->sketchpadTrack = sketchpadTrack;
                newNote->hardwareDeviceId = hardwareDeviceId;
                newNote->port = port;
                d->recordingLiveNotes << newNote;
            }
        }
        // if note-off, check whether there's a matching on note, and if there is, add that note with velocity, delay, and duration as appropriate for current time and step
        // either any note off message, or a note on message with velocity 0 should be considered a note off by convention
        if (d->recordingLiveNotes.count() > 0 && ((0x7F < byte1 && byte1 < 0x90) || (0x8F < byte1 && byte1 < 0xA0 && byte3 == 0))) {
            QMutableListIterator<NewNoteData*> iterator(d->recordingLiveNotes);
            NewNoteData *newNote{nullptr};
            while (iterator.hasNext()) {
                iterator.next();
                newNote = iterator.value();
                if (newNote->midiNote == byte2 && newNote->port == port && newNote->sketchpadTrack == sketchpadTrack && newNote->hardwareDeviceId == hardwareDeviceId) {
                    iterator.remove();
                    newNote->endTimestamp = d->syncTimer->timerTickForJackPlayhead(timestamp, &newNote->endTimestampOffset);
                    QMetaObject::invokeMethod(d->zlSyncManager, "addRecordedNote", Qt::QueuedConnection, Q_ARG(void*, newNote));
                    break;
                }
            }
        }
    }
}

void PatternModel::midiMessageToClipCommands(ClipCommandRing *listToPopulate, const int &samplerIndex, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3) const
{
    if (samplerIndex == d->sketchpadTrack && (!d->sequence || (d->sequence->shouldMakeSounds() && (d->sequence->soloPatternObject() == this || d->enabled)))
        // But also, only send notes there if we're in one of the internal-midi-triggered-sounds modes (essentially meaning "not external" but also let's honour no destination, so just be explicit about which the accepted ones are)
        && (d->noteDestination == SampleTriggerDestination || d->noteDestination == SynthDestination || d->noteDestination == SampleLoopedDestination)) {
            d->midiMessageToClipCommands(listToPopulate, byte1, byte2, byte3);
    }
}

void ZLPatternSynchronisationManager::addRecordedNote(void *recordedNote)
{
    NewNoteData *newNote = static_cast<NewNoteData*>(recordedNote);

    // Note duration in the majority of this is in pattern ticks (that is, 1/128th of a bar), so let's trim things down a bit
    qint64 noteDuration = q->stepLength() / q->d->patternTickToSyncTimerTick;

    // Quantize the two timestamps to the grid we've been asked to use
    const double quantizingAmount{q->d->liveRecordingQuantizingAmount == 0 ? q->stepLength() : double(q->d->liveRecordingQuantizingAmount)};
    newNote->timestamp = quantizingAmount * qRound(double(newNote->timestamp) / quantizingAmount);
    newNote->endTimestamp = quantizingAmount * qRound(double(newNote->endTimestamp) / quantizingAmount);

    // convert the timer ticks to pattern ticks, and adjust for whatever was the most recent restart of the pattern's playback
    newNote->timestamp = (newNote->timestamp - quint64(q->d->mostRecentStartTimestamp)) / quint64(q->d->patternTickToSyncTimerTick);
    newNote->endTimestamp = (newNote->endTimestamp - quint64(q->d->mostRecentStartTimestamp)) / quint64(q->d->patternTickToSyncTimerTick);

    const double normalisedTimestamp{double(qint64(newNote->timestamp) % (q->patternLength() * noteDuration))};
    newNote->step = normalisedTimestamp / noteDuration;
    newNote->delay = normalisedTimestamp - (newNote->step * noteDuration);
    newNote->duration = newNote->endTimestamp - newNote->timestamp;

    int row = (newNote->step / q->width()) % q->availableBars();
    int column = newNote->step - (row * q->width());

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
        subnoteIndex = q->addSubnote(newNote->row, newNote->column, q->playGridManager()->getNote(newNote->midiNote, q->sketchpadTrack()));
        // qDebug() << Q_FUNC_INFO << "Didn't find a subnote with this midi note to change values on, created a new subnote at subnote index" << subnoteIndex;
    } else {
        // Check whether this is what we already know about, and if it is, abort the changes
        const int oldVelocity = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "velocity").toInt();
        const int oldDuration = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "duration").toInt();
        const int oldDelay = q->subnoteMetadata(newNote->row, newNote->column, subnoteIndex, "delay").toInt();
        if (oldVelocity == newNote->velocity && oldDuration == newNote->duration && oldDelay == newNote->delay) {
            qDebug() <<  Q_FUNC_INFO << "This is a note we already have in the pattern, with the same data set on it, so no need to do anything with that" << newNote << newNote->timestamp << newNote->endTimestamp << newNote->step << newNote->row << newNote->column << newNote->midiNote << newNote->velocity << newNote->delay << newNote->duration;
            subnoteIndex = -1;
        }
    }
    if (subnoteIndex > -1) {
        // And then, finally, set the three values (always set them, because we might be changing an existing entry
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "velocity", newNote->velocity);
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "duration", newNote->duration);
        q->setSubnoteMetadata(newNote->row, newNote->column, subnoteIndex, "delay", newNote->delay);
        qDebug() << Q_FUNC_INFO << "Handled a recorded new note:" << newNote << newNote->timestamp << newNote->endTimestamp << newNote->step << newNote->row << newNote->column << newNote->midiNote << newNote->velocity << newNote->delay << newNote->duration;
    }

    // And at the end, get rid of the thing
    delete newNote;
}

// Since we got us a qobject up there a bit that we need to mocify
#include "PatternModel.moc"
