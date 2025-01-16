#pragma once

#include "SyncTimer.h"
#include <QDebug>

class ClipAudioSource;
/**
 * \brief Used to schedule clips into the timer's playback queue
 *
 * Roughly equivalent to a midi message, but for clips
 */
struct ClipCommand {
    ClipCommand() {};
    ClipCommand(ClipAudioSource *clip, int midiNote) : clip(clip), midiNote(midiNote) {};
    ClipAudioSource* clip{nullptr}; ///@< The audio clip the command relates to
    int midiNote{-1}; ///@< The midi note to play the clip at
    int subvoice{-1}; ///@< -1 is the base voice, 0 through 15 is a specific subvoice
    int slice{-1}; /// @< -1 is the root slice, 0 and above is a specific slice. Invalid slices will be counted as the root slice
    int midiChannel{-1}; ///@< The midi channel the note message came from
    bool startPlayback{false}; ///@< Whether the command asks for the clip to be started (if an equivalent active clip exists, playback will be restarted)
    bool stopPlayback{false}; ///@< Whether to stop playback of the equivalent active clip (any playing clip which has the same midi note, subvoice, slice, and midi channel)
    bool changeLooping{false}; ///@< Whether to change the looping state of an equivalent active clip
    bool looping{false};
    bool changePitch{false}; ///@< Whether to change the pitch adjustment of an equivalent active clip
    float pitchChange{0.0f};
    bool changeSpeed{false}; ///@< Whether to change the speed ratio of an equivalent active clip
    float speedRatio{0.0f};
    bool changeGainDb{false}; ///@< Whether to change the gain (in dB) of an equivalent active clip
    float gainDb{0.0f};
    bool changeVolume{false}; ///@< Whether to change the volume (in absolute terms) of an equivalent active clip
    float volume{1.0f};
    bool changePan{false}; ///@< Whether to change the panning of an equivalent active clip
    float pan{0.0f}; ///@< The pan adjustment (-1 being panned fully left, 1 being panned fully right, 0 being center pan)
    bool setStartPosition{false}; ///@< Whether to change the playback start position of an equivalent active clip
    float startPosition{0}; ///@< The absolute start position in source samples
    bool setStopPosition{false}; ///@< Whether to change the playback stop position of an equivalent active clip
    float stopPosition{0}; ///@< The absolute stop position in source samples

    bool equivalentTo(ClipCommand *other) const {
        return clip == other->clip && midiNote == other->midiNote && subvoice == other->subvoice && slice == other->slice && midiChannel == other->midiChannel;
    }

    /**
     * \brief Create a command on the global channel, defaulted to midi note 60
     * @note To decide whether the clip should be played through effects or not, set it's lane affinity (0 for no effects, 1 for effects)
     * @see ClipAudioSource::laneAffinity
     */
    static ClipCommand* globalCommand(ClipAudioSource *clip)
    {
        ClipCommand *command = SyncTimer::instance()->getClipCommand();
        command->clip = clip;
        command->midiChannel = -1;
        command->midiNote = 60;
        command->subvoice = -1;
        command->slice = -1;
        return command;
    }
    /**
     * \brief Create a command for a specific channel
     */
    static ClipCommand* channelCommand(ClipAudioSource *clip, int channelID)
    {
        ClipCommand *command = SyncTimer::instance()->getClipCommand();
        command->clip = clip;
        command->midiNote = 60;
        command->subvoice = -1;
        command->slice = -1;
        command->midiChannel = channelID;
        return command;
    }

    static void clear(ClipCommand *command) {
        command->clip = nullptr;
        command->midiNote = -1;
        command->subvoice = -1;
        command->slice = -1;
        command->midiChannel = -1;
        command->startPlayback = false;
        command->stopPlayback = false;
        command->changeLooping = false;
        command->looping = false;
        command->changePitch = false;
        command->pitchChange = 0.0f;
        command->changeSpeed = false;
        command->speedRatio = 0.0f;
        command->changeGainDb = false;
        command->gainDb = 0.0f;
        command->changeVolume = false;
        command->volume = 1.0f;
        command->changePan = false;
        command->pan = 0.0f;
        command->setStartPosition = false;
        command->startPosition = 0;
        command->setStopPosition = false;
        command->stopPosition = 0;
    }
};

#define ClipCommandRingSize 4096
class ClipCommandRing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        ClipCommand *clipCommand{nullptr};
        quint64 timestamp;
        bool processed{true};
    };
    explicit ClipCommandRing() {
        Entry* entryPrevious{&ringData[ClipCommandRingSize - 1]};
        for (quint64 i = 0; i < ClipCommandRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~ClipCommandRing() {
    }

    void write(ClipCommand *command, quint64 timestamp) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location:" << entry->clipCommand << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->clipCommand = command;
        entry->timestamp = timestamp;
        entry->processed = false;
    }
    ClipCommand *read(quint64 *timestamp = nullptr) {
        Entry *entry = readHead;
        readHead = readHead->next;
        if (timestamp) {
            *timestamp = entry->timestamp;
        }
        ClipCommand *command = entry->clipCommand;
        entry->clipCommand = nullptr;
        entry->processed = true;
        return command;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[ClipCommandRingSize];
};
