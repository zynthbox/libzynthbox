#pragma once

#include <QVariant>
#include <QDebug>

#include "SyncTimer.h"

/**
 * \brief Used to schedule various operations into the timer's playback queue
 */
struct alignas(64) TimerCommand {
    TimerCommand() {};
    // TODO Before shipping this, make sure these are sequential...
    enum Operation {
        InvalidOperation = 0, ///@< An invalid operation, ignored
        StartPlaybackOperation = 1, ///@< Start global playback. If parameter is 1, playback will be started in song mode. For song mode, parameter is startOffset, and bigParameter is the duration. See also SegmentHandler::startPlayback(qint64, quint64)
        StopPlaybackOperation = 2, ///@< Stop all playback
        StartClipOperation = 3, ///@< Start playing the given clip. Pass channel index as parameter 1, track index as parameter2 and clip index as parameter3
        StopClipOperation = 4, ///@< Stop playing the given clip. Pass channel index as parameter 1, track index as parameter2 and clip index as parameter3
        StartClipLoopOperation = 6, ///@< DEPRECATED Use ClipCommandOperation (now handled by segmenthandler, was originally: Start playing a clip looped, parameter being the midi channel, parameter2 being the clip ID, and parameter3 being the note, and bigParameter can be used to define a timer offset value for adjusting the clip's playback position relative to the timer's cumulative beat)
        StopClipLoopOperation = 7, ///@< DEPRECATED Use ClipCommandOperation (now handled by segmenthandler, was originally: Stop playing a clip looping style, parameter being the midi channel to stop it on, parameter2 being the clip ID, and parameter3 being the note)
        SamplerChannelEnabledStateOperation = 8, ///@< Sets the state of a SamplerSynth channel to enabled or not enabled. parameter is the sampler channel (-2 through 9, -2 being uneffected global, -1 being effected global, and 0 through 9 being zl channels), and parameter2 is 0 for disabled, any other number for enabled
        ClipCommandOperation = 9, ///@< Handle a clip command at the given timer point (this could also be done by scheduling the clip command directly)
        SetBpmOperation = 10, ///@< Set the BPM of the timer to the value in stored in parameter (this will be clamped to fit between SyncTimer's allowed values)
        AutomationOperation = 11, ///@< Set the value of a given parameter on a given engine on a given channel to a given value. parameter contains the channel (-1 is global fx engines, 0 through 9 being zl channels), parameter2 contains the engine index, parameter3 is the parameter's index, parameter4 is the value
        PassthroughClientOperation = 12, ///@< Set the volume of the given volume channel to the given value. parameter is the channel (-1 is global playback, 0 through 9 being zl channels), parameter2 is the setting index in the list (dry, wetfx1, wetfx2, pan, muted), parameter3 being the left value, parameter4 being right value. If parameter2 is pan or muted, parameter4 is ignored. For volumes, parameter3 and parameter4 can be 0 through 100. For pan, -100 for all left through 100 for all right, with 0 being no pan. For muted, 0 is not muted, any other value is muted.
        GuiMessageOperation = 13, ///@< Emits a signal on SyncTimer (timerMessage) which must be consumed by the UI in a queued manner. Set variantParameter to the message you wish to pass to the UI. You can also pass parameter, parameter2 and so on, but there is no guarantees made how these are interpreted by the UI (so you'll have to do your own filtering)
        ChannelRecorderStartOperation = 20, ///@< Start recording a channel. Make sure you have set up the channel recorder before scheduling this command (see AudioLevels::setChannelToRecord and AudioLevels::setChannelFilenamePrefix). Alternatively, set parameter to 1, parameter2 to the sketchpadTrack to begin recording, and variantParameter to the full recording filename)
        ChannelRecorderStopOperation = 21, ///@< Stop recording a channel (optionally set parameter to 1, and parameter 2 to the sketchpadTrack to stop recording)
        MidiRecorderStartOperation = 30, ///@< Start recording a midi channel. parameter is the sketchpad track to record (-1 for global channel, 0 through 9 for sketchpad tracks)
        MidiRecorderStopOperation = 31, ///@< Stop any ongoing midi recordings
        SendMidiMessageOperation = 100, ///@< Send a midi message (will be inserted at the list of the current frame's other messages). Parameter is the sketchpadTrack to send the message out on, and the three further int parameters can be either a number from 0 through 255 (for midi) or any other value for to say the handling should stop there. E.g. you might send parameter=1, parameter2=176, parameter3=120, parameter4=-1 for length 2 message which sends all sounds off on channel 0 on sketchpad track 2
        RegisterCASOperation = 10001, ///@< INTERNAL - Register a ClipAudioSource with SamplerSynth, so it can be used for playback - dataParameter should contain a ClipAudioSource* object instance
        UnregisterCASOperation = 10002, ///@< INTERNAL - Unregister a ClipAudioSource with SamplerSynth, so it can be used for playback - dataParameter should contain a ClipAudioSource* object instance
    };
    Operation operation{InvalidOperation};
    int parameter{0};
    int parameter2{0};
    int parameter3{0};
    int parameter4{0};
    quint64 bigParameter{0};
    void *dataParameter{nullptr};
    // NOTE: To implementers: Use this sparingly, as QVariants can be expensive to use and this gets handled from a jack call
    QVariant variantParameter;

    static TimerCommand *cloneTimerCommand(const TimerCommand *other) {
        TimerCommand *clonedCommand = SyncTimer::instance()->getTimerCommand();
        clonedCommand->operation = other->operation;
        clonedCommand->parameter = other->parameter;
        clonedCommand->parameter2 = other->parameter2;
        clonedCommand->parameter3 = other->parameter3;
        clonedCommand->parameter4 = other->parameter4;
        clonedCommand->bigParameter = other->bigParameter;
        clonedCommand->dataParameter = other->dataParameter;
        if (other->variantParameter.isValid()) {
            clonedCommand->variantParameter = other->variantParameter;
        }
        return clonedCommand;
    }

    static void clear(TimerCommand *command) {
        command->operation = InvalidOperation;
        command->parameter = command->parameter2 = command->parameter3 = command->parameter4 = 0;
        command->bigParameter = 0;
        command->dataParameter = nullptr;
        if (command->variantParameter.isValid()) {
            command->variantParameter.clear();
        }
    }
};

#define TimerCommandRingSize 4096
class TimerCommandRing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        TimerCommand *timerCommand{nullptr};
        quint64 timestamp;
        bool processed{true};
    };
    explicit TimerCommandRing() {
        Entry* entryPrevious{&ringData[TimerCommandRingSize - 1]};
        for (quint64 i = 0; i < TimerCommandRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~TimerCommandRing() {
    }

    void write(TimerCommand *command, quint64 timestamp) {
        if (writeHead->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location:" << writeHead->timerCommand << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        writeHead->timerCommand = command;
        writeHead->timestamp = timestamp;
        writeHead->processed = false;
        writeHead = writeHead->next;
    }
    TimerCommand *read(quint64 *timestamp = nullptr) {
        if (timestamp) {
            *timestamp = readHead->timestamp;
        }
        TimerCommand *command = readHead->timerCommand;
        readHead->processed = true;
        readHead->timerCommand = nullptr;
        readHead = readHead->next;
        return command;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[TimerCommandRingSize];
};
