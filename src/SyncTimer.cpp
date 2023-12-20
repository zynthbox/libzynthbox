#include <sched.h>
#include <sys/mman.h>

#include "SyncTimer.h"
#include "ZynthboxBasics.h"
#include "ClipAudioSource.h"
#include "ClipCommand.h"
#include "Helper.h"
#include "MidiRouter.h"
#include "SamplerSynth.h"
#include "TimerCommand.h"
#include "TransportManager.h"
#include "JackThreadAffinitySetter.h"
#include "AudioLevels.h"
#include "MidiRecorder.h"
#include "SegmentHandler.h"
#include "PlayGridManager.h"
#include "SequenceModel.h"

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include <jack/jack.h>
#include <jack/statistics.h>
#include <jack/midiport.h>

#include "JUCEHeaders.h"

#define BPM_MINIMUM 50
#define BPM_MAXIMUM 200

// Defining this will cause the sync timer to collect the intervals of each beat, and output them when you call stop
// It will also make the timer thread output the discrepancies and internal counter states on a per-pseudo-minute basis
// #define DEBUG_SYNCTIMER_TIMING

// Defining this will make the jack process call output a great deal of information about each frame, and is likely to
// itself cause xruns (that is, it considerably increases the amount of processing for each step, including text output)
// Use this to find note oddity and timing issues where note delivery is concerned.
// #define DEBUG_SYNCTIMER_JACK

using namespace std;
using namespace juce;

struct alignas(64) StepData {
    StepData() { }
    ~StepData() {
        qDeleteAll(timerCommands);
        qDeleteAll(clipCommands);
    }
    // Call this before accessing the data to ensure that it is fresh
    void ensureFresh() {
        if (played) {
            played = false;
            // It's our job to delete the timer commands, so do that first
            for (TimerCommand* command : timerCommands) {
                delete command;
            }
            // The clip commands, once sent out, become owned by SampelerSynth, so leave them alone
            timerCommands.clear();
            clipCommands.clear();
            for (int track = 0; track < ZynthboxTrackCount; ++track) {
                trackBuffer[track].clear();
            }
            // midiBuffer.clear();
        }
    }
    void insertMidiBuffer(const juce::MidiBuffer &buffer, int sketchpadTrack);
    // juce::MidiBuffer midiBuffer;
    juce::MidiBuffer trackBuffer[ZynthboxTrackCount];
    QList<ClipCommand*> clipCommands;
    QList<TimerCommand*> timerCommands;

    StepData *previous{nullptr};
    StepData *next{nullptr};

    quint64 index{0};

    // SyncTimer sets this true to mark that it has played the step
    // Conceptually, a step starts out having been played (meaning it is not interesting to the process call),
    // and it is set to false by ensureFresh above, which is called any time just before adding anything to a step.
    bool played{true};

    SyncTimerPrivate *d{nullptr};
};

using frame_clock = std::conditional_t<
    std::chrono::high_resolution_clock::is_steady,
    std::chrono::high_resolution_clock,
    std::chrono::steady_clock>;

#define NanosecondsPerMinute 60000000000
#define NanosecondsPerSecond 1000000000
#define NanosecondsPerMillisecond 1000000
#define BeatSubdivisions 96
#define BeatsPerBar 4
// The midi beat clock signal should go out at a rate of 24ppqn - at the current beat subdivision of 96, that makes it every 3rd tick of our step ring
#define TicksPerMidiBeatClock 3
static const jack_midi_data_t jackMidiBeatMessage{0xF8};
// There's BeatsPerBar * BeatSubdivisions ticks per bar
#define TicksPerBar 384

template <typename T>
static typename std::enable_if<std::is_integral_v<T>, T>::type from_HANDLE(Qt::HANDLE id)
{
    return static_cast<T>(reinterpret_cast<intptr_t>(id));
}
template <typename T>
static typename std::enable_if<std::is_pointer_v<T>, T>::type from_HANDLE(Qt::HANDLE id)
{
    return static_cast<T>(id);
}

class SyncTimerThread : public QThread {
    Q_OBJECT
public:
    SyncTimerThread(SyncTimer *q)
        : QThread(q)
    {}

    void waitTill(frame_clock::time_point till) {
        //spinTimeMs is used to adjust for scheduler inaccuracies. default is 2.1 milliseconds. anything lower makes fps jump around
        auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(till - frame_clock::now() - spinTime);
        if (waitTime.count() > 0) { //only sleep if waitTime is positive
            usleep((long unsigned int)waitTime.count());
        } else {
            // overrun situation this is bad, we should tell someone!
            // qWarning() << "The playback synchronisation timer had a falling out with reality and ended up asked to wait for a time in the past. This is not awesome, so now we make it even slower by outputting this message complaining about it.";
        }
        while (till > frame_clock::now()) {
            //spin till actual timepoint
        }
    }

    void run() override {
        startTime = frame_clock::now();
        pthread_t threadId = from_HANDLE<pthread_t>(currentThreadId());
        zl_set_dsp_thread_affinity(threadId);
        std::chrono::time_point< std::chrono::_V2::steady_clock, std::chrono::duration< long long unsigned int, std::ratio< 1, NanosecondsPerSecond > > > nextMinute;
        while (true) {
            if (aborted) {
                break;
            }
            nextMinute = startTime + ((minuteCount + 1) * nanosecondsPerMinute);
            while (count < bpm * BeatSubdivisions) {
                mutex.lock();
                if (paused)
                {
                    qDebug() << "SyncTimer thread is paused, let's wait...";
                    waitCondition.wait(&mutex);
                    qDebug() << "Unpaused, let's goooo!";

                    // Set thread policy to SCHED_FIFO with maximum possible priority
                    struct sched_param param;
                    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
                    sched_setscheduler(0, SCHED_FIFO, &param);

                    nextExtraTickAt = 0;
                    adjustment = 0;
                    count = 0;
                    cumulativeCount = 0;
                    minuteCount = 0;
                    startTime = frame_clock::now();
                    nextMinute = startTime + nanosecondsPerMinute;
                }
                mutex.unlock();
                if (aborted) {
                    break;
                }
                Q_EMIT timeout(); // Do the thing!
                ++count;
                ++cumulativeCount;
                waitTill(frame_clock::now() + frame_clock::duration(subbeatCountToNanoseconds(bpm, 1)));
            }
#ifdef DEBUG_SYNCTIMER_TIMING
            qDebug() << "Sync timer reached minute:" << minuteCount << "with interval" << interval.count();
            qDebug() << "The most recent pseudo-minute took an extra" << (frame_clock::now() - nextMinute).count() << "nanoseconds";
#endif
            count = 0; // Reset the count each minute
            ++minuteCount;
        }
    }

    Q_SIGNAL void timeout();

    void setBPM(quint64 bpm) {
        this->bpm = bpm;
        interval = frame_clock::duration(subbeatCountToNanoseconds(bpm, 1));
    }
    inline const quint64 getBpm() const {
        return bpm;
    }

    static inline quint64 subbeatCountToNanoseconds(const quint64 &bpm, const quint64 &subBeatCount)
    {
        return (subBeatCount * NanosecondsPerMinute) / (bpm * BeatSubdivisions);
    };
    static inline float nanosecondsToSubbeatCount(const quint64 &bpm, const quint64 &nanoseconds)
    {
        return nanoseconds / (NanosecondsPerMinute / (bpm * BeatSubdivisions));
    };
    void requestAbort() {
        aborted = true;
    }

    Q_SLOT void pause() { setPaused(true); }
    Q_SLOT void resume() { setPaused(false); }
    inline const bool &isPaused() const {
        return paused;
    }
    void setPaused(bool shouldPause) {
        mutex.lock();
        paused=shouldPause;
        if (!paused)
            waitCondition.wakeAll();
        mutex.unlock();
        Q_EMIT pausedChanged();
    }
    Q_SIGNAL void pausedChanged();

    void addAdjustmentByMicroseconds(qint64 microSeconds) {
        mutex.lock();
        if (adjustment == 0) {
            currentExtraTick = 0;
        }
        adjustment += (1000 * microSeconds);
        // When we adjust past another "there should have been a beat here" amount for
        // the adjustment, schedule an extra run of the logic in the timer callback
        while (nextExtraTickAt < adjustment) {
            QMetaObject::invokeMethod(this, "timeout", Qt::QueuedConnection);
            ++currentExtraTick;
            nextExtraTickAt = qint64(subbeatCountToNanoseconds(bpm, currentExtraTick));
        }
        mutex.unlock();
    }
    const qint64 getAdjustment() const {
        return adjustment;
    }
    const quint64 getExtraTickCount() const {
        return currentExtraTick;
    }

    const frame_clock::time_point adjustedCumulativeRuntime() const {
        return frame_clock::duration(adjustment) + startTime + (nanosecondsPerMinute * minuteCount) + (interval * count);
    }
    const frame_clock::time_point adjustedRuntimeForTick(const quint64 tick) const {
        return frame_clock::duration(adjustment) + startTime + (interval * tick);
    }
    const frame_clock::time_point getStartTime() const {
        return startTime;
    }
    const std::chrono::nanoseconds getInterval() {
        return interval;
    }
    /**
     * \brief This is a workaround for firing a signal in a queued fashion (this could be anywhere, just as long as it's not public)
     */
    Q_SIGNAL void timerMessage(const QString& message, const int &parameter, const int &parameter2, const int &parameter3, const int &parameter4, const quint64 &bigParameter);
private:
    qint64 nextExtraTickAt{0};
    quint64 currentExtraTick{0};
    qint64 adjustment{0};
    quint64 count{0};
    quint64 cumulativeCount{0};
    quint64 minuteCount{0};
    frame_clock::time_point startTime;

    quint64 bpm{120};
    std::chrono::nanoseconds interval;

    QMutex mutex;
    QWaitCondition waitCondition;

    // This is equivalent to .1 ms
    const frame_clock::duration spinTime{frame_clock::duration(100000)};
    const std::chrono::nanoseconds nanosecondsPerMinute{NanosecondsPerMinute};

    bool aborted{false};
    bool paused{true};
};

struct SketchpadTrack {
public:
    SketchpadTrack() {
        clearActivations();
    }
    // This must be updated by anything that schedules events into the ring
    // - Set note activation to infinite when activating any note
    // - On note-off mark as that timestamp, or the current timestamp, whichever is later
    // - Set channelAvailableAfter to the highest timestamp of all notes on that channel
    void registerActivation(int channel, int note) {
        // qDebug() << Q_FUNC_INFO << "Registering activation for track" << index << ": note" << note << "on channel" << channel;
        noteActivations[channel][note] = UINT64_MAX;
        channelAvailableAfter[channel] = UINT64_MAX;
    }
    void registerDeactivation(int channel, int note, quint64 timestamp) {
        noteActivations[channel][note] = timestamp;
        quint64 highestActivationTimestamp{0};
        for (int testNote = 0; testNote < 128; ++testNote) {
            if (highestActivationTimestamp < noteActivations[channel][testNote]) {
                highestActivationTimestamp = noteActivations[channel][testNote];
            }
        }
        channelAvailableAfter[channel] = highestActivationTimestamp;
        // qDebug() << Q_FUNC_INFO << "Registering deactivation for track" << index << ": note" << note << "on channel" << channel << "for timestamp" << timestamp << "- highest activation timestamp is now" << highestActivationTimestamp;
    }
    void clearActivations() {
        for (int channel = 0; channel < 16; ++channel) {
            channelAvailableAfter[channel] = 0;
            for (int note = 0; note < 128; ++note) {
                noteActivations[channel][note] = 0;
            }
        }
    }
    quint64 noteActivations[16][128];
    quint64 channelAvailableAfter[16];
    int index{-1};
};

#define StepRingCount 32768
SyncTimerThread *timerThread{nullptr};
class SyncTimerPrivate {
public:
    SyncTimerPrivate(SyncTimer *q)
        : q(q)
    {
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            jackPort[track] = nullptr;
            tracks[track].index = track;
        }
        transportManager = TransportManager::instance(q);
        timerThread = new SyncTimerThread(q);
        int result = mlock(stepRing, sizeof(StepData) * StepRingCount);
        if (result != 0) {
            qDebug() << Q_FUNC_INFO << "Error locking step ring memory" << strerror(result);
        }
        StepData* previous{&stepRing[StepRingCount - 1]};
        for (quint64 i = 0; i < StepRingCount; ++i) {
            stepRing[i].index = i;
            stepRing[i].d = this;
            previous->next = &stepRing[i];
            stepRing[i].previous = previous;
            previous = &stepRing[i];
        }
        stepReadHead = stepRing;

        for (int i = 0; i < ClipCommandRingSize; ++i) {
            freshClipCommands.write(new ClipCommand, 0);
        }
        for (int i = 0; i < TimerCommandRingSize; ++i) {
            freshTimerCommands.write(new TimerCommand, 0);
        }

        samplerSynth = SamplerSynth::instance();
        // Dangerzone - direct connection from another thread. Yes, dangerous, but also we need the precision, so we need to dill whit it
        QObject::connect(timerThread, &SyncTimerThread::timeout, q, [this](){ hiResTimerCallback(); }, Qt::DirectConnection);
        QObject::connect(timerThread, &QThread::started, q, [q](){ Q_EMIT q->timerRunningChanged(); });
        QObject::connect(timerThread, &QThread::finished, q, [q](){ Q_EMIT q->timerRunningChanged(); });
        QObject::connect(timerThread, &SyncTimerThread::pausedChanged, q, [q](){ q->timerRunningChanged(); });
        QObject::connect(timerThread, &SyncTimerThread::timerMessage, q, &SyncTimer::timerMessage, Qt::QueuedConnection);
        timerThread->start();
    }
    ~SyncTimerPrivate() {
        timerThread->requestAbort();
        timerThread->wait();
        if (jackClient) {
            jack_client_close(jackClient);
        }
    }
    SyncTimer *q{nullptr};
    SamplerSynth *samplerSynth{nullptr};
    TransportManager *transportManager{nullptr};
    int currentTrack{0};
    int playingClipsCount = 0;
    int beat = 0;
    quint64 cumulativeBeat = 0;
    int callbackCount{0};

    int timerCommandBundleStarts{0};
    QHash<TimerCommand*, quint64> bundledTimerCommands;

    ClipCommandRing sentOutClipsRing;

    StepData stepRing[StepRingCount];
    // The next step to be read in the step ring
    StepData* stepReadHead{nullptr};
    quint64 stepNextPlaybackPosition{0};
    /**
     * \brief Get the ring buffer position based on the given delay from the current playback position (cumulativeBeat if playing, or stepReadHead if not playing)
     * @param delay The delay of the position to use
     * @param ensureFresh Set this to false to disable the freshness insurance
     * @param ignorePlaybackState Set this to true to ignore whether or not playback is ongoing (usually done for sending things with zero delay, and just very immediately)
     * @return The stepRing position to use for the given delay
     */
    inline StepData* delayedStep(quint64 delay, bool ensureFresh = true, bool ignorePlaybackState = false) {
        quint64 step{0};
        if (ignorePlaybackState || isPaused) {
            // If paused, base the delay on the current stepReadHead
            step = (stepReadHead->index + delay) % StepRingCount;
        } else {
            // If running, base the delay on the current cumulativeBeat (adjusted to at least stepReadHead, just in case)
            step = (stepReadHeadOnStart + qMax(cumulativeBeat + delay, jackPlayhead + 1)) % StepRingCount;
        }
        StepData *stepData = &stepRing[step];
        if (ensureFresh) {
            stepData->ensureFresh();
        }
        return stepData;
    }
    /**
     * \brief Convert the given sketchpadTrack to a reasonable number (clamp and adjust for defaults)
     * @param sketchpadTrack An integer number
     * @return If given a -1, value becomes the current track. Otherwise it the given value is clamped between 0 and ZynthboxTrackCount
     */
    inline const int sketchpadTrack(int sketchpadTrack) {
        return sketchpadTrack == -1 ? currentTrack : std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1);
    }

    TimerCommandRing timerCommandsToDelete;
    TimerCommandRing freshTimerCommands;
    ClipCommandRing clipCommandsToDelete;
    ClipCommandRing freshClipCommands;

    bool audibleMetronome{false};
    ClipAudioSource *metronomeTick{nullptr};
    ClipAudioSource *metronomeTock{nullptr};

    #ifdef DEBUG_SYNCTIMER_TIMING
    frame_clock::time_point lastRound;
    QList<long> intervals;
#endif
    int i{0};
    void hiResTimerCallback() {
#ifdef DEBUG_SYNCTIMER_TIMING
        frame_clock::time_point thisRound = frame_clock::now();
        intervals << (thisRound - lastRound).count();
        lastRound = thisRound;
#endif
        while (cumulativeBeat < (jackPlayhead + (scheduleAheadAmount * 2))) {
            Q_EMIT q->timerTick(beat);

            ClipCommand *command{nullptr};
            if (beat == 0) {
                // Spit out a touch of useful information on beat zero
                qDebug() << Q_FUNC_INFO << "Current jack process call saturation:" << MidiRouter::instance()->processingLoad();
                if (audibleMetronome) {
                    command = ClipCommand::globalCommand(metronomeTick);
                }
            } else if (audibleMetronome && (beat % BeatSubdivisions == 0)) {
                command = ClipCommand::globalCommand(metronomeTock);
            }
            if (command) {
                command->startPlayback = true;
                command->changeVolume = true;
                command->volume = 1.0;
                q->scheduleClipCommand(command, 0);
            }

            // Increase the current beat as we understand it
            beat = (beat + 1) % (BeatSubdivisions * 4);
            ++cumulativeBeat;
        }

        // Finally, notify any listeners that commands have been sent out
        // You must not delete the commands themselves here, as SamplerSynth takes ownership of them
        while (sentOutClipsRing.readHead->processed == false) {
            Q_EMIT q->clipCommandSent(sentOutClipsRing.read());
        }
    }

    void setBpm(quint64 bpm) {
        if (timerThread->getBpm() != bpm) {
            timerThread->setBPM(bpm);
            jackSubbeatLengthInMicroseconds = timerThread->subbeatCountToNanoseconds(timerThread->getBpm(), 1) / 1000;
            updateScheduleAheadAmount();
            QMetaObject::invokeMethod(q, "bpmChanged", Qt::QueuedConnection);
        }
    }
    quint64 recentlyRequestedBpm{120};

    // The time after which a midi channel is available on a given track
    SketchpadTrack tracks[ZynthboxTrackCount];

    jack_client_t* jackClient{nullptr};
    jack_port_t* jackPort[ZynthboxTrackCount];
    quint64 jackPlayhead{0};
    quint64 jackCumulativePlayhead{0};
    // Used to calculate the quantized block rate BPM for the jack transport position's beats_per_minute field (jackBeatsPerMinute)
    double jackPlayheadBpm{120};
    int32_t jackBar{0};
    int32_t jackBeat{0};
    int32_t jackBeatTick{0};
    int32_t jackTick{0};
    int32_t jackBarStartTick{0};
    int32_t jackMidiBeatTick{0};
    double jackBeatsPerMinute{0.0};
    quint64 stepReadHeadOnStart{0};
    jack_time_t jackMostRecentNextUsecs{0};
    jack_time_t jackStartTime{0};
    quint64 jackPlayheadAtStart{0};
    quint64 jackNextPlaybackPosition{0};
    quint64 jackSubbeatLengthInMicroseconds{0};
    quint64 jackLatency{0};
    bool isPaused{true};

    jack_time_t current_usecs{0};
    jack_time_t refreshThingsAfter{0};
    quint64 jackPlayheadReturn{0};
    quint64 jackSubbeatLengthInMicrosecondsReturn{0};

    juce::MidiBuffer missingBitsBuffer[ZynthboxTrackCount];
    int process(jack_nframes_t nframes) {
        // const std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        void *buffer[ZynthboxTrackCount];
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            buffer[track] = jack_port_get_buffer(jackPort[track], nframes);
            jack_midi_clear_buffer(buffer[track]);
        }
#ifdef DEBUG_SYNCTIMER_JACK
        quint64 stepCount = 0;
        QList<int> commandValues;
        QList<int> noteValues;
        QList<int> velocities;
        QList<quint64> framePositions;
        QList<quint64> frameSteps;
        quint64 eventCount = 0;
#endif

        jack_nframes_t current_frames;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(jackClient, &current_frames, &current_usecs, &next_usecs, &period_usecs);
        // Things get refreshed 50ms after they've been marked for refreshing
        refreshThingsAfter = current_usecs + 5000;
        const quint64 microsecondsPerFrame = (next_usecs - current_usecs) / nframes;

        double thisStepBpm{jackPlayheadBpm};
        double thisStepSubbeatLengthInMicroseconds{double(timerThread->subbeatCountToNanoseconds(jackPlayheadBpm, 1)) / 1000.0};

        // Setting here because we need the this-process value, not the next-process
        jackPlayheadReturn = jackPlayhead;
        jackSubbeatLengthInMicrosecondsReturn = thisStepSubbeatLengthInMicroseconds;

        if (!isPaused) {
            if (jackPlayhead == 0) {
                // first run for this playback session, let's do a touch of setup
                jackNextPlaybackPosition = current_usecs;
                jackBar = jackBeat = jackBeatTick = jackTick = 0;
                // We need to send out a beat clock tick on the first position as well, so let's make sure we do that
                jackMidiBeatTick = TicksPerMidiBeatClock - 1;
                transportManager->restartTransport();
            }
            jackMostRecentNextUsecs = next_usecs;
        }
        if (stepNextPlaybackPosition == 0) {
            stepNextPlaybackPosition = current_usecs;
        }

        jack_time_t currentStepUsecsStart{0};
        jack_time_t currentStepUsecsEnd = qMin(period_usecs, float(stepNextPlaybackPosition - current_usecs));
        double updatedJackBeatsPerMinute{0};
        jack_nframes_t firstAvailableFrame{0};
        jack_nframes_t relativePosition{0};
        int errorCode{0};
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            // In case there were any missing events from the last run... we do that first, and then we get onto the rest of the events
            // This is going to be an extremely rare case, and if it happens there's likely something more substantial wrong, but best safe.
            if (missingBitsBuffer[track].isEmpty() == false) {
                for (const juce::MidiMessageMetadata &juceMessage : qAsConst(missingBitsBuffer[track])) {
                    jack_midi_event_write(buffer[track], relativePosition,
                        const_cast<jack_midi_data_t*>(juceMessage.data), // this might seems odd, but it's really only because juce's internal store is const here, and the data types are otherwise the same
                        size_t(juceMessage.numBytes) // this changes signedness, but from a lesser space (int) to a larger one (unsigned long)
                    );
                }
                missingBitsBuffer[track].clear();
            }
        }
        // As long as the next playback position is before this period is supposed to end, and we have frames for it, let's post some events
        while (stepNextPlaybackPosition < next_usecs && firstAvailableFrame < nframes) {
            StepData *stepData = stepReadHead;
            // Next roll for next time (also do it now, as we're reading out of it)
            stepReadHead = stepReadHead->next;
            // Counting total steps, for determining delays and the like at a global level
            ++jackCumulativePlayhead;
            // If the events are in the past, they need to be scheduled as soon as we can, so just put those on position 0, and if we are here, that means that ending up in the future is a rounding error, so clamp that
            if (stepNextPlaybackPosition <= current_usecs) {
                relativePosition = firstAvailableFrame;
                ++firstAvailableFrame;
            } else {
                relativePosition = std::clamp<jack_nframes_t>((stepNextPlaybackPosition - current_usecs) / microsecondsPerFrame, firstAvailableFrame, nframes - 1);
                firstAvailableFrame = relativePosition;
            }
            // Make sure there's a midi beat pulse going out if one is needed
            ++jackMidiBeatTick;
            bool writeBeatTick{false};
            if (jackMidiBeatTick == TicksPerMidiBeatClock) {
                jackMidiBeatTick = 0;
            }

            // In case we're cycling through stuff we've already played, let's just... not do anything with that
            // Basically that just means nobody else has attempted to do stuff with the step since we last played it
            if (!stepData->played) {
                stepData->played = true;
                // First, let's get the midi messages sent out
                for (int track = 0; track < ZynthboxTrackCount; ++track) {
                    if (writeBeatTick) {
                        jack_midi_event_write(buffer[track], relativePosition, &jackMidiBeatMessage, 1);
                    }
                    for (const juce::MidiMessageMetadata &juceMessage : qAsConst(stepData->trackBuffer[track])) {
                        if (firstAvailableFrame >= nframes) {
                            qWarning() << Q_FUNC_INFO << "First available frame is in the future - that's a problem";
                            break;
                        }
                        errorCode = jack_midi_event_write(buffer[track], relativePosition,
                            const_cast<jack_midi_data_t*>(juceMessage.data), // this might seems odd, but it's really only because juce's internal store is const here, and the data types are otherwise the same
                            size_t(juceMessage.numBytes) // this changes signedness, but from a lesser space (int) to a larger one (unsigned long)
                        );
                        if (errorCode == ENOBUFS) {
                            qWarning() << Q_FUNC_INFO << "Ran out of space while writing events - scheduling the event there's not enough space for to be fired first next round";
                            // Schedule the rest of the buffer for immediate dispatch on next go-around
                            missingBitsBuffer[track].addEvent(juceMessage.getMessage(), 0);
                        } else {
                            if (errorCode != 0) {
                                qWarning() << Q_FUNC_INFO << "Error writing midi event:" << -errorCode << strerror(-errorCode);
                            }
#ifdef DEBUG_SYNCTIMER_JACK
                            ++eventCount;
                            commandValues << juceMessage.data[0]; noteValues << juceMessage.data[1]; velocities << juceMessage.data[2];
#endif
                        }
                    }
                }

                // Then do direct-control samplersynth things
                for (ClipCommand *clipCommand : qAsConst(stepData->clipCommands)) {
                    // Using the protected function, which only we (and SamplerSynth) can use, to ensure less locking
                    samplerSynth->handleClipCommand(clipCommand, firstAvailableFrame + current_frames);
                    sentOutClipsRing.write(clipCommand, 0);
                }

                // Do playback control things as the last thing, otherwise we might end up affecting things
                // currently happening (like, if we stop playback on the last step of a thing, we still want
                // notes on that step to have been played and so on)
                for (TimerCommand *command : qAsConst(stepData->timerCommands)) {
                    Q_EMIT q->timerCommand(command);
                    switch (command->operation) {
                        case TimerCommand::StartPlaybackOperation:
                            startPlayback(command, firstAvailableFrame + current_frames, stepNextPlaybackPosition);
                            // Start playback does in fact happen here, but anything scheduled for step 0 of playback will happen on /next/ step.
                            // Consequently, we'll need to kind of lie a little bit, since playback actually will start next step, not this step.
                            jackPlayheadAtStart = firstAvailableFrame + current_frames + (thisStepSubbeatLengthInMicroseconds / microsecondsPerFrame);
                            break;
                        case TimerCommand::StopPlaybackOperation:
                            stopPlayback(firstAvailableFrame + current_frames, stepNextPlaybackPosition);
                            break;
                        case TimerCommand::StartClipLoopOperation:
                        case TimerCommand::StopClipLoopOperation:
                            {
                                ClipCommand *clipCommand = static_cast<ClipCommand *>(command->variantParameter.value<void*>());
                                if (clipCommand) {
                                    samplerSynth->handleClipCommand(clipCommand, firstAvailableFrame + current_frames);
                                    sentOutClipsRing.write(clipCommand, 0);
                                } else {
                                    qWarning() << Q_FUNC_INFO << "Failed to retrieve clip command from clip based timer command";
                                }
                                command->variantParameter.clear();
                            }
                            break;
                        case TimerCommand::SamplerChannelEnabledStateOperation:
                            samplerSynth->setChannelEnabled(command->parameter, command->parameter2);
                            break;
                        case TimerCommand::ClipCommandOperation:
                            {
                                ClipCommand *clipCommand = static_cast<ClipCommand *>(command->dataParameter);
                                if (clipCommand) {
                                    samplerSynth->handleClipCommand(clipCommand, firstAvailableFrame + current_frames);
                                    sentOutClipsRing.write(clipCommand, 0);
                                } else {
                                    qWarning() << Q_FUNC_INFO << "Failed to retrieve clip command from clip based timer command";
                                }
                                command->dataParameter = nullptr;
                            }
                            break;
                        case TimerCommand::SetBpmOperation:
                            {
                                const quint64 newBpm{std::clamp<quint64>(quint64(command->parameter), 50, 200)};
                                setBpm(newBpm);
                                thisStepBpm = newBpm;
                            }
                            break;
                        case TimerCommand::GuiMessageOperation:
                            Q_EMIT timerThread->timerMessage(command->variantParameter.toString(), command->parameter, command->parameter2, command->parameter3, command->parameter4, command->bigParameter);
                            break;
                        case TimerCommand::RegisterCASOperation:
                        case TimerCommand::UnregisterCASOperation:
                            {
                                ClipAudioSource *clip = static_cast<ClipAudioSource*>(command->dataParameter);
                                if (clip) {
                                    if (command->operation == TimerCommand::RegisterCASOperation) {
                                        samplerSynth->registerClip(clip);
                                    } else {
                                        samplerSynth->unregisterClip(clip);
                                    }
                                } else {
                                    qWarning() << Q_FUNC_INFO << "Failed to retrieve clip from clip registration timer command";
                                }
                            }
                            break;
                        case TimerCommand::ChannelRecorderStartOperation:
                            if (command->parameter == 1) {
                                AudioLevels::instance()->handleTimerCommand(firstAvailableFrame + current_frames, command);
                            } else {
                                AudioLevels::instance()->startRecording(firstAvailableFrame + current_frames);
                            }
                            break;
                        case TimerCommand::ChannelRecorderStopOperation:
                            if (command->parameter == 1) {
                                AudioLevels::instance()->handleTimerCommand(firstAvailableFrame + current_frames, command);
                            } else {
                                AudioLevels::instance()->stopRecording(firstAvailableFrame + current_frames);
                            }
                            break;
                        case TimerCommand::MidiRecorderStartOperation:
                            MidiRecorder::instance()->startRecording(command->parameter, false, stepNextPlaybackPosition);
                            break;
                        case TimerCommand::MidiRecorderStopOperation:
                            MidiRecorder::instance()->stopRecording(command->parameter, stepNextPlaybackPosition);
                            break;
                        case TimerCommand::SendMidiMessageOperation:
                            {
                                if (-1 < command->parameter && command->parameter < ZynthboxTrackCount) {
                                    size_t size = 3;
                                    const jack_midi_data_t message[3]{jack_midi_data_t(command->parameter2), jack_midi_data_t(command->parameter3), jack_midi_data_t(command->parameter4)};
                                    if (-1 < command->parameter4 && command->parameter4 < 256) {
                                        size = 3;
                                    } else if (-1 < command->parameter3 && command->parameter3 < 256) {
                                        size = 2;
                                    } else {
                                        size = 1;
                                    }
                                    errorCode = jack_midi_event_write(buffer[command->parameter], relativePosition, message, size);
                                    if (errorCode == ENOBUFS) {
                                        qWarning() << Q_FUNC_INFO << "Ran out of space while writing events - scheduling the event there's not enough space for to be fired first next round";
                                        // Schedule the rest of the buffer for immediate dispatch on next go-around
                                        missingBitsBuffer[command->parameter].addEvent(message, int(size), 0);
                                    } else {
                                        if (errorCode != 0) {
                                            qWarning() << Q_FUNC_INFO << "Error writing midi event:" << -errorCode << strerror(-errorCode);
                                        }
                                    }
                                }
                                break;
                            }
                        case TimerCommand::StartPartOperation:
                        case TimerCommand::StopPartOperation:
                        case TimerCommand::InvalidOperation:
                        case TimerCommand::AutomationOperation:
                        case TimerCommand::PassthroughClientOperation:
                        default:
                            break;
                    }
                }
            }
            // Update our internal BPM state, based on what we had on the previous step
            if (jackPlayheadBpm != thisStepBpm) {
                // update the playhead's BPM
                jackPlayheadBpm = thisStepBpm;
                // update the subbeat length in ms
                thisStepSubbeatLengthInMicroseconds = timerThread->subbeatCountToNanoseconds(jackPlayheadBpm, 1) / 1000;
            }
            // Add the amount of the BPM value appropriate to this step's duration inside the current period
            updatedJackBeatsPerMinute += jackPlayheadBpm * double(currentStepUsecsEnd - currentStepUsecsStart) / period_usecs;
            // qDebug() << Q_FUNC_INFO << "After a step between" << currentStepUsecsStart << "and" << currentStepUsecsEnd << "the updated jack bpm is" << updatedJackBeatsPerMinute;
            const quint64 nextStepUsecsEnd = qMin(float(currentStepUsecsEnd + thisStepSubbeatLengthInMicroseconds), period_usecs);
            currentStepUsecsStart = currentStepUsecsEnd;
            currentStepUsecsEnd = nextStepUsecsEnd;
            // Update our timecode data
            ++jackTick;
            ++jackBeatTick;
            if (jackBeatTick == BeatSubdivisions) {
                jackBeatTick = 0;
                ++jackBeat;
                if (jackBeat == BeatsPerBar) {
                    jackBeat = 0;
                    ++jackBar;
                    jackBarStartTick = jackTick;
                }
            }
            if (!isPaused) {
                // Next roll for next time
                ++jackPlayhead;
                jackNextPlaybackPosition += thisStepSubbeatLengthInMicroseconds;
#ifdef DEBUG_SYNCTIMER_JACK
                    ++stepCount;
#endif
            }
            // Now roll to the next step's playback position
            stepNextPlaybackPosition += thisStepSubbeatLengthInMicroseconds;
        }
        // Finally, update with whatever is left
        updatedJackBeatsPerMinute += jackPlayheadBpm * double(currentStepUsecsEnd - currentStepUsecsStart) / period_usecs;
        jackBeatsPerMinute = std::round(updatedJackBeatsPerMinute * 100.0) / 100.0; // Round to within the nearest two decimal points - otherwise we run into precision issues
        // qDebug() << Q_FUNC_INFO << "Final updated jack beats per minute:" << jackBeatsPerMinute;
#ifdef DEBUG_SYNCTIMER_JACK
        if (eventCount > 0) {
            for (int track = 0; track < ZynthboxTrackCount; ++track) {
                if (uint32_t lost = jack_midi_get_lost_event_count(buffer[track])) {
                    qDebug() << "Lost some notes:" << lost;
                }
            }
            qDebug() << Q_FUNC_INFO << "We advanced jack playback by" << stepCount << "steps, and are now at position" << jackPlayhead << "and we filled up jack with" << eventCount << "events" << nframes << jackSubbeatLengthInMicroseconds << frameSteps << framePositions << commandValues << noteValues << velocities;
        } else {
            qDebug() << Q_FUNC_INFO << "We advanced jack playback by" << stepCount << "steps, and are now at position" << jackPlayhead << "and scheduled no notes";
        }
#endif
        // const std::chrono::duration<double, std::milli> ms_double = std::chrono::high_resolution_clock::now() - t1;
        // if (ms_double.count() > 0.2) {
        //     qDebug() << Q_FUNC_INFO << ms_double.count() << "ms after" << belowThreshold << "runs under 0.2ms";
        //     belowThreshold = 0;
        // } else {
        //     ++belowThreshold;
        // }

        return 0;
    }
    int belowThreshold{0};
    int xrun() {
#ifdef DEBUG_SYNCTIMER_JACK
        qDebug() << "SyncTimer detected XRun";
#endif
        return 0;
    }

    void startPlayback(TimerCommand *command, jack_nframes_t currentFrame, jack_time_t currentFrameUsecs) {
        if (timerThread->isPaused()) {
            SegmentHandler *handler = SegmentHandler::instance();
            if (command->parameter == 1) {
                handler->startPlayback(command->parameter2, command->bigParameter);
            } else {
                qDebug() << Q_FUNC_INFO << "Starting metronome and playback";
                PlayGridManager *pgm = PlayGridManager::instance();
                const QObjectList sequenceModels{pgm->getSequenceModels()};
                for (QObject *object : qAsConst(sequenceModels)) {
                    SequenceModel *sequence{qobject_cast<SequenceModel*>(object)};
                    if (sequence) {
                        sequence->prepareSequencePlayback();
                    } else {
                        qWarning() << Q_FUNC_INFO << "Sequence for track" << i << "could not be fetched, and playback could not be prepared";
                    }
                }
                QObject *clipToRecord = pgm->zlSketchpad()->property("clipToRecord").value<QObject*>();
                if (clipToRecord) {
                    MidiRecorder::instance()->startRecording(pgm->currentMidiChannel(), true, currentFrameUsecs);
                    AudioLevels::instance()->startRecording(currentFrame);
                }
                QMetaObject::invokeMethod(pgm->zlSketchpad(), "startPlayback", Qt::DirectConnection);
                q->start();
                qDebug() << Q_FUNC_INFO << "Metronome and playback started";
            }
        } else {
            qDebug() << Q_FUNC_INFO << "Attempted to start playback without playback running";
        }
    }
    void stopPlayback(jack_nframes_t currentFrame, jack_time_t currentFrameUsecs) {
        if (timerThread->isPaused()) {
            qDebug() << Q_FUNC_INFO << "Attempted to stop playback when playback was already stopped";
        } else {
            PlayGridManager *pgm = PlayGridManager::instance();
            if (SegmentHandler::instance()->songMode()) {
                qDebug() << Q_FUNC_INFO << "Stopping metronome and playback in song mode";
                QMetaObject::invokeMethod(pgm->zlSketchpad(), "stopAllPlayback", Qt::DirectConnection);
                SegmentHandler::instance()->stopPlayback();
                q->stop();
                for (int chan = 0; chan < 10; ++chan) {
                    // One All Notes Off message for each track (not midi channel)
                    q->sendMidiMessageImmediately(3, 176 + chan, 123, 0);
                }
                qDebug() << Q_FUNC_INFO << "Stopped metronome and playback in song mode";
            } else {
                qDebug() << Q_FUNC_INFO << "Stopping metronome and playback";
                const QObjectList sequenceModels{pgm->getSequenceModels()};
                for (QObject *object : qAsConst(sequenceModels)) {
                    SequenceModel *sequence{qobject_cast<SequenceModel*>(object)};
                    if (sequence) {
                        sequence->stopSequencePlayback();
                    } else {
                        qWarning() << Q_FUNC_INFO << "Sequence for track" << i << "could not be fetched, and playback could not be stopped";
                    }
                }
                const bool isRecording = pgm->zlSketchpad()->property("isRecording").toBool();
                if (isRecording) {
                    if (MidiRecorder::instance()->isRecording()) {
                        // Don't stop again if we've already been stopped
                        MidiRecorder::instance()->stopRecording(-1, currentFrameUsecs);
                    }
                    if (AudioLevels::instance()->isRecording()) {
                        AudioLevels::instance()->stopRecording(currentFrame);
                    }
                    QMetaObject::invokeMethod(pgm->zlSketchpad(), "stopRecording", Qt::QueuedConnection);
                }
                QMetaObject::invokeMethod(pgm->zlSketchpad(), "stopAllPlayback", Qt::DirectConnection);
                pgm->stopMetronome();
                q->stop();
                for (int chan = 0; chan < 10; ++chan) {
                    // One All Notes Off message for each track (not midi channel)
                    q->sendMidiMessageImmediately(3, 176 + chan, 123, 0);
                }
                qDebug() << Q_FUNC_INFO << "Metronome and playback stopped";
            }
        }
    }

    quint64 scheduleAheadAmount{0};
    void updateScheduleAheadAmount() {
        scheduleAheadAmount = (timerThread->nanosecondsToSubbeatCount(timerThread->getBpm(), jackLatency * (float)1000000)) + 1;
        QMetaObject::invokeMethod(q, "scheduleAheadAmountChanged", Qt::QueuedConnection);
    }
};

void StepData::insertMidiBuffer(const juce::MidiBuffer &buffer, int sketchpadTrack) {
    trackBuffer[sketchpadTrack].addEvents(buffer, 0, -1, trackBuffer[sketchpadTrack].getLastEventTime());
    quint64 timestamp{d->jackCumulativePlayhead};
    if (d->stepReadHead != this) {
        if (d->stepReadHead->index < index) {
            timestamp += index - d->stepReadHead->index;
        } else {
            timestamp += StepRingCount - index + d->stepReadHead->index;
        }
    }
    for (const juce::MidiMessageMetadata &message : buffer) {
        if (message.numBytes == 3 && 0x7F < message.data[0] && message.data[0] < 0xA0) {
            const int channel{message.data[0] & 0xf};
            if (message.data[0] < 0x90) {
                d->tracks[sketchpadTrack].registerDeactivation(channel, message.data[1], timestamp);
            } else {
                d->tracks[sketchpadTrack].registerActivation(channel, message.data[1]);
            }
        }
    }
}

static int client_process(jack_nframes_t nframes, void* arg) {
    // Just roll empty, we're not really processing anything for SyncTimer here, MidiRouter does that explicitly
    static_cast<SyncTimerPrivate*>(arg)->process(nframes);
    return 0;
}
static int client_xrun(void* arg) {
    return static_cast<SyncTimerPrivate*>(arg)->xrun();
}
void client_latency_callback(jack_latency_callback_mode_t mode, void *arg)
{
    if (mode == JackPlaybackLatency) {
        SyncTimerPrivate *d = static_cast<SyncTimerPrivate*>(arg);
        jack_latency_range_t range;
        jack_port_get_latency_range (d->jackPort[0], JackPlaybackLatency, &range);
        if (range.max != d->jackLatency) {
            jack_nframes_t bufferSize = jack_get_buffer_size(d->jackClient);
            jack_nframes_t sampleRate = jack_get_sample_rate(d->jackClient);
            quint64 newLatency = (1000 * (double)qMax(bufferSize, range.max)) / (double)sampleRate;
            if (newLatency != d->jackLatency) {
                d->jackLatency = newLatency;
                d->updateScheduleAheadAmount();
                qDebug() << "Latency changed, max is now" << range.max << "That means we will now suggest scheduling things" << d->q->scheduleAheadAmount() << "steps into the future";
            }
        }
    }
}

SyncTimer::SyncTimer(QObject *parent)
    : QObject(parent)
    , d(new SyncTimerPrivate(this))
{
    d->jackSubbeatLengthInMicroseconds = timerThread->subbeatCountToNanoseconds(timerThread->getBpm(), 1) / 1000;
    connect(timerThread, &SyncTimerThread::pausedChanged, this, [this](){
        d->isPaused = timerThread->isPaused();
    }, Qt::DirectConnection);
    // Open the client.
    jack_status_t real_jack_status{};
    d->jackClient = jack_client_open("SyncTimer", JackNullOption, &real_jack_status);
    if (d->jackClient) {
        // Register the MIDI output ports.
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            d->jackPort[track] = jack_port_register(d->jackClient, QString("Track%1").arg(track).toUtf8(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
        }
        if (d->jackPort[0]) {
            // Set the process callback.
            if (jack_set_process_callback(d->jackClient, client_process, static_cast<void*>(d)) == 0) {
                jack_set_xrun_callback(d->jackClient, client_xrun, static_cast<void*>(d));
                jack_set_latency_callback (d->jackClient, client_latency_callback, static_cast<void*>(d));
                // Activate the client.
                if (jack_activate(d->jackClient) == 0) {
                    qInfo() << "Successfully created and set up the SyncTimer's Jack client";
                    zl_set_jack_client_affinity(d->jackClient);
                    jack_latency_range_t range;
                    jack_port_get_latency_range (d->jackPort[0], JackPlaybackLatency, &range);
                    jack_nframes_t bufferSize = jack_get_buffer_size(d->jackClient);
                    jack_nframes_t sampleRate = jack_get_sample_rate(d->jackClient);
                    d->jackLatency = (1000 * (double)qMax(bufferSize, range.max)) / (double)sampleRate;
                    d->updateScheduleAheadAmount();
                    qDebug() << "SyncTimer: Buffer size is supposed to be" << bufferSize << "but our maximum latency is" << range.max << "and we should be using that one to calculate how far out things should go, as that should include the amount of extra buffers alsa might (and likely does) use.";
                    qDebug() << "SyncTimer: However, as that is sometimes zero, we use the highest of the two. That means we will now suggest scheduling things" << scheduleAheadAmount() << "steps into the future";
                } else {
                    qWarning() << "SyncTimer: Failed to activate SyncTimer Jack client";
                }
            } else {
                qWarning() << "SyncTimer: Failed to set the SyncTimer Jack processing callback";
            }
        } else {
            qWarning() << "SyncTimer: Could not register SyncTimer Jack output port";
        }
    } else {
        qWarning() << "SyncTimer: Could not create SyncTimer Jack client.";
    }
}

SyncTimer::~SyncTimer() {
    delete d;
}

void SyncTimer::queueClipToStartOnChannel(ClipAudioSource *clip, int midiChannel)
{
    ClipCommand *command = ClipCommand::channelCommand(clip, midiChannel);
    command->midiNote = 60;
    command->changeVolume = true;
    command->volume = 1.0;
    command->changeLooping = true;
    command->looping = true;
    // When explicity starting a clip in a looping state, we want to /restart/ the loop, not start multiple loops (to run multiple at the same time, sample-trig can do that for us)
    command->stopPlayback = true;
    command->startPlayback = true;

    const quint64 nextZeroBeat = timerThread->isPaused() ? 0 : (BeatSubdivisions * 4) - (d->cumulativeBeat % (BeatSubdivisions * 4));
//     qDebug() << "Queueing up" << clip << "to start, with jack and timer zero beats at" << nextZeroBeat << "at beats" << d->cumulativeBeat << "meaning we want positions" << (d->cumulativeBeat + nextZeroBeat < d->jackPlayhead ? nextZeroBeat + BeatSubdivisions * 4 : nextZeroBeat);
    scheduleClipCommand(command, d->cumulativeBeat + nextZeroBeat < d->jackPlayhead ? nextZeroBeat + BeatSubdivisions * 4 : nextZeroBeat);
}

void SyncTimer::queueClipToStopOnChannel(ClipAudioSource *clip, int midiChannel)
{
    // First, remove any references to the clip that we're wanting to stop
    for (quint64 step = 0; step < StepRingCount; ++step) {
        StepData *stepData = &d->stepRing[step];
        if (!stepData->played) {
            QMutableListIterator<ClipCommand *> stepIterator(stepData->clipCommands);
            while (stepIterator.hasNext()) {
                ClipCommand *stepCommand = stepIterator.next();
                if (stepCommand->clip == clip) {
                    deleteClipCommand(stepCommand);
                    stepIterator.remove();
                    break;
                }
            }
        }
    }

    // Then stop it, now, because it should be now
    ClipCommand *command = ClipCommand::channelCommand(clip, midiChannel);
    command->midiNote = 60;
    command->stopPlayback = true;
    StepData *stepData{d->delayedStep(0)};
    stepData->clipCommands << command;
}

void SyncTimer::queueClipToStart(ClipAudioSource *clip) {
    queueClipToStartOnChannel(clip, -1);
}

void SyncTimer::queueClipToStop(ClipAudioSource *clip) {
    queueClipToStopOnChannel(clip, -1);
}

void SyncTimer::startWithCountin(quint64 bars, bool songMode)
{
    ClipCommand *command{nullptr};
    TimerCommand *messageCommand{nullptr};
    // How long should the message be shown for, in ms (we add 50, to ensure a slight overlap)
    quint64 showDuration = 50 + quint64(subbeatCountToSeconds(getBpm(), BeatSubdivisions) * 1000.0f);
    for (quint64 beat = 0; beat < 4 * bars; ++beat) {
        if (beat % 4 == 0) {
            command = ClipCommand::globalCommand(d->metronomeTick);
        } else {
            command = ClipCommand::globalCommand(d->metronomeTock);
        }
        command->startPlayback = true;
        command->changeVolume = true;
        command->volume = 1.0;
        scheduleClipCommand(command, beat * BeatSubdivisions);
        messageCommand = getTimerCommand();
        messageCommand->operation = TimerCommand::GuiMessageOperation;
        messageCommand->parameter = 1; // Set to 1 to make the UI know this is a count-in message
        messageCommand->parameter2 = (beat % 4) + 1; // The current beat of the countin
        messageCommand->parameter3 = (beat / 4) + 1; // The current bar of the countin
        messageCommand->parameter4 = bars; // How many bars did we get asked to count
        messageCommand->bigParameter = showDuration;
        scheduleTimerCommand(beat * BeatSubdivisions, messageCommand);
        // qDebug() << Q_FUNC_INFO << "Scheduled gui message operation for countin message with" << messageCommand->parameter << messageCommand->parameter2 << messageCommand->parameter3 << messageCommand->parameter4 << messageCommand->bigParameter;
    }
    TimerCommand *startCommand = getTimerCommand();
    startCommand->operation = TimerCommand::StartPlaybackOperation;
    if (songMode) {
        startCommand->parameter = 1;
        startCommand->parameter2 = 0;
        startCommand->bigParameter = 0;
    }
    scheduleTimerCommand(bars * 4 * BeatSubdivisions - 1, startCommand);
}

void SyncTimer::start() {
    if (timerThread->isPaused()) {
        qDebug() << "#### Starting timer with previously set BPM" << getBpm();
#ifdef DEBUG_SYNCTIMER_TIMING
        d->intervals.clear();
        d->lastRound = frame_clock::now();
#endif
        d->stepReadHeadOnStart = (*d->stepReadHead).index;
        timerThread->resume();
    }
}

void SyncTimer::stop() {
    cerr << "#### Stopping timer" << endl;

    if(!timerThread->isPaused()) {
        timerThread->pause();
    }

    d->beat = 0;
    d->cumulativeBeat = 0;
    d->jackPlayhead = 0;

    // A touch of hackery to ensure we end immediately, and leave a clean state
    // We want to fire off all the off notes immediately, and none of the on notes
    juce::MidiBuffer onlyOffs[ZynthboxTrackCount];
    // We also want to fire off all the clip commands (so they happen, but without making noises)
    QList<ClipCommand*> clipCommands;
    // We also want to clean up the step, so timer commands still happen at the expected times, without the other two happening
    for (quint64 step = 0; step < StepRingCount; ++step) {
        StepData *stepData = &d->stepRing[(step + d->stepReadHead->index) % StepRingCount];
        if (!stepData->played) {
            // First, collect all the queued midi messages, but in strict order, and only off notes...
            for (int track = 0; track < ZynthboxTrackCount; ++track) {
                for (const juce::MidiMessageMetadata& message : stepData->trackBuffer[track]) {
                    if (message.getMessage().isNoteOff()) {
                        onlyOffs[track].addEvent(message.getMessage(), 0);
                    }
                }
                stepData->trackBuffer[track].clear();
            }
            // Now for the clip commands
            for (ClipCommand *clipCommand : qAsConst(stepData->clipCommands)) {
                // Actually run all the commands (so we don't end up in a weird state), but also
                // set all the volumes to 0 so we don't make the users' ears bleed
                clipCommand->changeVolume = true;
                clipCommand->volume = 0;
                clipCommands << clipCommand;
            }
            stepData->clipCommands.clear();
        }
    }
    // And now everything has been marked as sent out, let's re-schedule the things that actually want to go out
    for (int track = 0; track < ZynthboxTrackCount; ++track) {
        if (!onlyOffs[track].isEmpty()) {
            sendMidiBufferImmediately(onlyOffs[track], track);
        }
        // Since we're doing a bit of jiggery-pokery with the order of things, we can expect there to be some off notes without matching on notes, so... let's just not do that
        d->tracks[track].clearActivations();
    }
    for (ClipCommand *clipCommand : qAsConst(clipCommands)) {
        scheduleClipCommand(clipCommand, 0);
        Q_EMIT clipCommandSent(clipCommand);
    }

    // Make sure we're actually informing about any clips that have been sent out, in case we
    // hit somewhere between a jack roll and a synctimer tick
    while (d->sentOutClipsRing.readHead->processed == false) {
        Q_EMIT clipCommandSent(d->sentOutClipsRing.read());
    }
#ifdef DEBUG_SYNCTIMER_TIMING
    qDebug() << d->intervals;
#endif
}

int SyncTimer::getInterval(int bpm) {
    // Calculate interval
    return 60000 / (bpm * BeatSubdivisions);
}

float SyncTimer::subbeatCountToSeconds(quint64 bpm, quint64 beats) const
{
    return double(timerThread->subbeatCountToNanoseconds(qBound(quint64(BPM_MINIMUM), bpm, quint64(BPM_MAXIMUM)), beats)) / (float)1000000000;
}

quint64 SyncTimer::secondsToSubbeatCount(quint64 bpm, float seconds) const
{
    return timerThread->nanosecondsToSubbeatCount(qBound(quint64(BPM_MINIMUM), bpm, quint64(BPM_MAXIMUM)), floor(seconds * (float)1000000000));
}

int SyncTimer::getMultiplier() const {
    return BeatSubdivisions;
}

quint64 SyncTimer::getBpm() const
{
    return timerThread->getBpm();
}

void SyncTimer::setBpm(quint64 bpm)
{
    d->recentlyRequestedBpm = bpm;
    TimerCommand *timerCommand = getTimerCommand();
    timerCommand->operation = TimerCommand::SetBpmOperation;
    timerCommand->parameter = bpm;
    scheduleTimerCommand(0, timerCommand);
}

void SyncTimer::increaseBpm()
{
    setBpm(qMin(d->recentlyRequestedBpm + 1, quint64(BPM_MAXIMUM)));
}

void SyncTimer::decreaseBpm()
{
    setBpm(qMax(d->recentlyRequestedBpm - 1, quint64(BPM_MINIMUM)));
}

quint64 SyncTimer::scheduleAheadAmount() const
{
    return d->scheduleAheadAmount;
}

void SyncTimer::setMetronomeTicks(ClipAudioSource* tick, ClipAudioSource* tock)
{
    d->metronomeTick = tick;
    d->metronomeTock = tock;
    if (d->metronomeTick == nullptr || d->metronomeTock == nullptr) {
        setAudibleMetronome(false);
    }
}

bool SyncTimer::audibleMetronome() const
{
    return d->audibleMetronome;
}

void SyncTimer::setAudibleMetronome(const bool& value)
{
    if (d->audibleMetronome != value) {
        d->audibleMetronome = value;
        Q_EMIT audibleMetronomeChanged();
    }
}

int SyncTimer::beat() const {
    return d->beat;
}

quint64 SyncTimer::cumulativeBeat() const {
    return d->cumulativeBeat;
}

const quint64 & SyncTimer::jackPlayheadAtStart() const
{
    return d->jackPlayheadAtStart;
}

const quint64 &SyncTimer::jackPlayhead() const
{
    if (timerThread->isPaused()) {
        return (*d->stepReadHead).index;
    }
    return d->jackPlayhead;
}

const quint64 &SyncTimer::jackPlayheadUsecs() const
{
    if (timerThread->isPaused()) {
        return d->stepNextPlaybackPosition;
    }
    return d->jackNextPlaybackPosition;
}

const quint64 &SyncTimer::jackSubbeatLengthInMicroseconds() const
{
    return d->jackSubbeatLengthInMicroseconds;
}

void SyncTimer::scheduleClipCommand(ClipCommand *command, quint64 delay)
{
    StepData *stepData{d->delayedStep(delay)};
    bool foundExisting{false};
    for (ClipCommand *existingCommand : qAsConst(stepData->clipCommands)) {
        if (existingCommand->equivalentTo(command)) {
            if (command->changeLooping) {
                existingCommand->looping = command->looping;
                existingCommand->changeLooping = true;
            }
            if (command->changePitch) {
                existingCommand->pitchChange = command->pitchChange;
                existingCommand->changePitch = true;
            }
            if (command->changeSpeed) {
                existingCommand->speedRatio = command->speedRatio;
                existingCommand->changeSpeed = true;
            }
            if (command->changeGainDb) {
                existingCommand->gainDb = command->gainDb;
                existingCommand->changeGainDb = true;
            }
            if (command->changeVolume) {
                existingCommand->volume = command->volume;
                existingCommand->changeVolume = true;
            }
            if (command->startPlayback) {
                existingCommand->startPlayback = true;
            }
            foundExisting = true;
        }
    }
    if (foundExisting) {
        deleteClipCommand(command);
    } else {
        stepData->clipCommands << command;
    }
}

int SyncTimer::currentTrack() const
{
    return d->currentTrack;
}

void SyncTimer::setCurrentTrack(const int& newTrack)
{
    if (d->currentTrack != std::clamp(newTrack, 0, 9)) {
        d->currentTrack = std::clamp(newTrack, 0, 9);
        Q_EMIT currentTrackChanged();
    }
}

void SyncTimer::scheduleTimerCommand(quint64 delay, TimerCommand *command)
{
    if (d->timerCommandBundleStarts == 0) {
        StepData *stepData{d->delayedStep(delay)};
        stepData->timerCommands << command;
    } else {
        d->bundledTimerCommands[command] = delay;
    }
}

void SyncTimer::startTimerCommandBundle()
{
    d->timerCommandBundleStarts += 1;
}

void SyncTimer::endTimerCommandBundle(quint64 startDelay)
{
    if (d->timerCommandBundleStarts > 0) {
        d->timerCommandBundleStarts -= 1;
    }
    if (d->timerCommandBundleStarts == 0) {
        // If we are at zero, submit any and all bundled commands properly
        // Operate using an offset from a specific step to be ultra certain. To ensure we can handle very extreme
        // duration work, we take the next-next from current, and count everything from there.
        StepData *logicalFirstStep{d->delayedStep(startDelay)};
        QHashIterator<TimerCommand*, quint64> bundleIterator{d->bundledTimerCommands};
        while (bundleIterator.hasNext()) {
            bundleIterator.next();
            StepData *addToStep{logicalFirstStep};
            const quint64 delay{bundleIterator.value()};
            if (delay > StepRingCount) {
                qCritical() << Q_FUNC_INFO << "Attempting to add a timer command further into the future than our Step Ring size. This is going to cause fairly serious problems, and we are going to need to increase the size of the ring. The ring size is" << StepRingCount << "and the requested delay was" << delay;
            }
            for (quint64 delayCounter = 0; delayCounter < delay; ++delayCounter) {
                addToStep = addToStep->next;
            }
            addToStep->ensureFresh();
            addToStep->timerCommands << bundleIterator.key();
        }
        d->bundledTimerCommands.clear();
    }
}

void SyncTimer::scheduleTimerCommand(quint64 delay, int operation, int parameter1, int parameter2, int parameter3, const QVariant &variantParameter, int parameter4)
{
    TimerCommand* timerCommand = getTimerCommand();
    timerCommand->operation = static_cast<TimerCommand::Operation>(operation);
    timerCommand->parameter = parameter1;
    timerCommand->parameter2 = parameter2;
    timerCommand->parameter3 = parameter3;
    timerCommand->parameter4 = parameter4;
    if (variantParameter.isValid()) {
        timerCommand->variantParameter = variantParameter;
    }
    scheduleTimerCommand(delay, timerCommand);
}

int SyncTimer::nextAvailableChannel(const int& sketchpadTrack, quint64 delay)
{
    int availableChannel{-1};
    const int theTrack{d->sketchpadTrack(sketchpadTrack)};
    const quint64 availableFrom{d->jackCumulativePlayhead + delay};
    for (int channel = 0; channel < 16; ++channel) {
        if (channel == MidiRouter::instance()->masterChannel()) {
            continue;
        }
        if (d->tracks[theTrack].channelAvailableAfter[channel] < availableFrom) {
            availableChannel = channel;
            break;
        }
    }
    // This is a panic moment, and we have to decide what to do: Decision becomes, use the oldest channel for the newest events
    if (availableChannel == -1) {
        // qDebug() << Q_FUNC_INFO << "We failed to secure a 'normal' channel to use, so let's find whatever is oldest and use that";
        int oldestChannel{-1};
        quint64 oldestTimestamp{UINT64_MAX};
        for (int channel = 0; channel < 16; ++channel) {
            if (oldestTimestamp > d->tracks[theTrack].channelAvailableAfter[channel]) {
                oldestTimestamp = d->tracks[theTrack].channelAvailableAfter[channel];
                oldestChannel = channel;
            }
        }
        availableChannel = oldestChannel;
        if (availableChannel == -1) {
            // qWarning() << Q_FUNC_INFO << "No available channels on track" << theTrack << "so we'll put the events onto literally any channel that isn't the master channel";
            if (MidiRouter::instance()->masterChannel() == 0) {
                availableChannel = 1;
            } else {
                availableChannel = 0;
            }
        }
    }
    // qDebug() << Q_FUNC_INFO << "Using channel" << availableChannel << "as next available channel for" << theTrack << "after delay" << delay;
    // Since we now say we're using the channel, mark it as unavailable forever (this gets updated when registering and deregistering activations)
    d->tracks[theTrack].channelAvailableAfter[availableChannel] = UINT64_MAX;
    return availableChannel;
}

void SyncTimer::scheduleNote(unsigned char midiNote, unsigned char midiChannel, bool setOn, unsigned char velocity, quint64 duration, quint64 delay, int sketchpadTrack)
{
    StepData *stepData{d->delayedStep(delay)};
    juce::MidiBuffer &addToThis = stepData->trackBuffer[d->sketchpadTrack(sketchpadTrack)];
    unsigned char note[3];
    if (setOn) {
        note[0] = 0x90 + midiChannel;
    } else {
        note[0] = 0x80 + midiChannel;
    }
    note[1] = midiNote;
    note[2] = velocity;
    const int onOrOff = setOn ? 1 : 0;
    addToThis.addEvent(note, 3, onOrOff);
    if (setOn) {
        d->tracks[d->sketchpadTrack(sketchpadTrack)].registerActivation(midiChannel, midiNote);
        if (duration > 0) {
            // Schedule an off note for that position
            scheduleNote(midiNote, midiChannel, false, 64, 0, delay + duration);
        }
    } else {
        d->tracks[d->sketchpadTrack(sketchpadTrack)].registerDeactivation(midiChannel, midiNote, d->jackCumulativePlayhead + delay);
    }
}

void SyncTimer::scheduleMidiBuffer(const juce::MidiBuffer& buffer, quint64 delay, int sketchpadTrack)
{
//     qDebug() << Q_FUNC_INFO << "Adding buffer with" << buffer.getNumEvents() << "notes, with delay" << delay << "giving us ring step" << d->delayedStep(delay) << "at ring playhead" << d->stepReadHead << "with cumulative beat" << d->cumulativeBeat;
    StepData *stepData{d->delayedStep(delay)};
    stepData->insertMidiBuffer(buffer, d->sketchpadTrack(sketchpadTrack));
}

void SyncTimer::sendNoteImmediately(unsigned char midiNote, unsigned char midiChannel, bool setOn, unsigned char velocity, int sketchpadTrack)
{
    StepData *stepData{d->delayedStep(0, true, true)};
    if (setOn) {
        stepData->insertMidiBuffer(juce::MidiBuffer(juce::MidiMessage::noteOn(midiChannel + 1, midiNote, juce::uint8(velocity))), d->sketchpadTrack(sketchpadTrack));
    } else {
        stepData->insertMidiBuffer(juce::MidiBuffer(juce::MidiMessage::noteOff(midiChannel + 1, midiNote)), d->sketchpadTrack(sketchpadTrack));
    }
}

void SyncTimer::sendMidiMessageImmediately(int size, int byte0, int byte1, int byte2, int sketchpadTrack)
{
    StepData *stepData{d->delayedStep(0, true, true)};
    if (size ==1) {
        stepData->insertMidiBuffer(juce::MidiBuffer(juce::MidiMessage(byte0)), d->sketchpadTrack(sketchpadTrack));
    } else if (size == 2) {
        stepData->insertMidiBuffer(juce::MidiBuffer(juce::MidiMessage(byte0, byte1)), d->sketchpadTrack(sketchpadTrack));
    } else if (size == 3) {
        stepData->insertMidiBuffer(juce::MidiBuffer(juce::MidiMessage(byte0, byte1, byte2)), d->sketchpadTrack(sketchpadTrack));
    } else {
        qWarning() << Q_FUNC_INFO << "Midi event is outside of bounds" << size;
    }
}

void SyncTimer::sendProgramChangeImmediately(int midiChannel, int program, int sketchpadTrack)
{
    sendMidiMessageImmediately(2, 192 + std::clamp(midiChannel, 0, 16), std::clamp(program, 0, 127), sketchpadTrack);
}

void SyncTimer::sendCCMessageImmediately(int midiChannel, int control, int value, int sketchpadTrack)
{
    sendMidiMessageImmediately(3, 176 + std::clamp(midiChannel, 0, 16), std::clamp(control, 0, 127), std::clamp(value, 0, 127), sketchpadTrack);
}

void SyncTimer::sendMidiBufferImmediately(const juce::MidiBuffer& buffer, int sketchpadTrack)
{
    StepData *stepData{d->delayedStep(0, true, true)};
    stepData->insertMidiBuffer(buffer, d->sketchpadTrack(sketchpadTrack));
}

bool SyncTimer::timerRunning() {
    return !timerThread->isPaused();
}

static int returnedCommands{0};
ClipCommand * SyncTimer::getClipCommand()
{
    // Before fetching commands, check whether there's anything that needs refreshing and do that first
    // Might seem a little heavy to put that here, but it's the most central location, and in reality
    // it is a fairly low-impact operation, so it's not really particularly bad.
    while (d->freshClipCommands.writeHead->processed == true && d->clipCommandsToDelete.readHead->processed == false && d->clipCommandsToDelete.readHead->timestamp < d->current_usecs) {
        ClipCommand *refreshedCommand = d->clipCommandsToDelete.read();
        ClipCommand::clear(refreshedCommand);
        d->freshClipCommands.write(refreshedCommand, 0);
    }
    ClipCommand *command{nullptr};
    if (d->freshClipCommands.readHead->processed == false) {
        command = d->freshClipCommands.read();
    }
    ++returnedCommands;
    if (command == nullptr) {
        qDebug() << Q_FUNC_INFO << "We're returning a null command here somehow... During our full runtime, this is attempt number:" << returnedCommands;
    }
    return command;
}

void SyncTimer::deleteClipCommand(ClipCommand* command)
{
    d->clipCommandsToDelete.write(command, d->refreshThingsAfter);
}

TimerCommand * SyncTimer::getTimerCommand()
{
    // Before fetching commands, check whether there's anything that needs refreshing and do that first
    // Might seem a little heavy to put that here, but it's the most central location, and in reality
    // it is a fairly low-impact operation, so it's not really particularly bad.
    while (d->freshTimerCommands.writeHead->processed == true  && d->timerCommandsToDelete.readHead->processed == false && d->timerCommandsToDelete.readHead->timestamp < d->current_usecs) {
        TimerCommand* refreshedCommand = d->timerCommandsToDelete.read();
        TimerCommand::clear(refreshedCommand);
        d->freshTimerCommands.write(refreshedCommand, 0);
    }
    TimerCommand *command{nullptr};
    if (d->freshTimerCommands.readHead->processed == false) {
        command = d->freshTimerCommands.read();
    }
    return command;
}

void SyncTimer::deleteTimerCommand(TimerCommand* command)
{
    d->timerCommandsToDelete.write(command, d->refreshThingsAfter);
}

void SyncTimer::scheduleStartPlayback(quint64 delay, bool startInSongMode, int startOffset, quint64 duration)
{
    TimerCommand *command = getTimerCommand();
    command->operation = TimerCommand::StartPlaybackOperation;
    if (startInSongMode) {
        command->parameter = 1;
        command->parameter2 = startOffset;
        command->bigParameter = duration;
    }
    scheduleTimerCommand(delay, command);
}

void SyncTimer::scheduleStopPlayback(quint64 delay)
{
    TimerCommand *command = getTimerCommand();
    command->operation = TimerCommand::StopPlaybackOperation;
    scheduleTimerCommand(delay, command);
}

void SyncTimer::process(jack_nframes_t /*nframes*/, void */*buffer*/, quint64 *jackPlayhead, quint64 *jackSubbeatLengthInMicroseconds)
{
//     d->process(nframes, buffer, jackPlayhead, jackSubbeatLengthInMicroseconds);
    *jackPlayhead = d->jackPlayheadReturn;
    *jackSubbeatLengthInMicroseconds = d->jackSubbeatLengthInMicrosecondsReturn;
}

void SyncTimer::setPosition(jack_position_t* position) const
{
    position->bar = d->jackBar;
    position->beat = d->jackBeat;
    position->tick = d->jackBeatTick;
    position->bar_start_tick = d->jackBarStartTick;
    position->beats_per_bar = BeatsPerBar;
    position->beat_type = BeatsPerBar;
    position->ticks_per_beat = BeatSubdivisions;
    position->beats_per_minute = d->jackBeatsPerMinute;
}

#include "SyncTimer.moc"
