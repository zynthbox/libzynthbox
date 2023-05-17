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
    ClipAudioSource* clip{nullptr};
    int midiNote{-1};
    int midiChannel{-1};
    bool startPlayback{false};
    bool stopPlayback{false};
    // Which slice to use (-1 means no slice, play normal)
    bool changeSlice{false};
    int slice{-1};
    bool changeLooping{false};
    bool looping{false};
    bool changePitch{false};
    float pitchChange{0.0f};
    bool changeSpeed{false};
    float speedRatio{0.0f};
    bool changeGainDb{false};
    float gainDb{0.0f};
    bool changeVolume{false};
    float volume{0.0f};
    bool changePan{false};
    float pan{0.0f};
    bool setStartPosition{false};
    float startPosition{0};
    bool setStopPosition{false};
    float stopPosition{0};

    bool equivalentTo(ClipCommand *other) const {
        return clip == other->clip
            && (
                (changeSlice == true && other->changeSlice == true && slice == other->slice)
                || (changeSlice == false && other->changeSlice == false && midiNote == other->midiNote && midiChannel == other->midiChannel)
            );
    }

    /**
     * \brief Create a command on the no-effects global channel, defaulted to midi note 60
     */
    static ClipCommand* noEffectCommand(ClipAudioSource *clip)
    {
        ClipCommand *command = SyncTimer::instance()->getClipCommand();
        command->clip = clip;
        command->midiChannel = -2;
        command->midiNote = 60;
        return command;
    }
    /**
     * \brief Create a command on the effects-enabled global channel, defaulted to midi note 60
     */
    static ClipCommand* effectedCommand(ClipAudioSource *clip)
    {
        ClipCommand *command = SyncTimer::instance()->getClipCommand();
        command->clip = clip;
        command->midiChannel = -1;
        command->midiNote = 60;
        return command;
    }
    /**
     * \brief Create a command for a specific channel
     */
    static ClipCommand* channelCommand(ClipAudioSource *clip, int channelID)
    {
        ClipCommand *command = SyncTimer::instance()->getClipCommand();
        command->clip = clip;
        command->midiChannel = channelID;
        return command;
    }

    static void clear(ClipCommand *command) {
        command->clip = nullptr;
        command->midiNote = -1;
        command->startPlayback = false;
        command->stopPlayback = false;
        command->changeSlice = false;
        command->slice = -1;
        command->changeLooping = false;
        command->looping = false;
        command->changePitch = false;
        command->pitchChange = 0.0f;
        command->changeSpeed = false;
        command->speedRatio = 0.0f;
        command->changeGainDb = false;
        command->gainDb = 0.0f;
        command->changeVolume = false;
        command->volume = 0.0f;
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
        if (writeHead->clipCommand) {
            qWarning() << Q_FUNC_INFO << "There is already a clip command stored at the write location:" << writeHead->clipCommand << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        writeHead->clipCommand = command;
        writeHead->timestamp = timestamp;
        writeHead = writeHead->next;
    }
    ClipCommand *read(quint64 *timestamp = nullptr) {
        if (timestamp) {
            *timestamp = readHead->timestamp;
        }
        ClipCommand *command = readHead->clipCommand;
        readHead = readHead->next;
        return command;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[ClipCommandRingSize];
};
