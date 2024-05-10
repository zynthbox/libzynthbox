#pragma once

#include <QObject>
#include <QCoreApplication>
#include <QList>
#include <QVariant>
#include <jack/types.h>

using namespace std;

namespace juce {
    class MidiBuffer;
}
struct ClipCommand;
struct TimerCommand;
class ClipAudioSource;
class SyncTimerPrivate;
/**
 * \brief A sequencer into which can be scheduled midi events, TimerCommand and ClipCommand instances
 */
class SyncTimer : public QObject {
  Q_OBJECT
  Q_PROPERTY(int currentTrack READ currentTrack WRITE setCurrentTrack NOTIFY currentTrackChanged)
  Q_PROPERTY(bool timerRunning READ timerRunning NOTIFY timerRunningChanged)
  Q_PROPERTY(quint64 bpm READ getBpm WRITE setBpm NOTIFY bpmChanged)
  Q_PROPERTY(quint64 scheduleAheadAmount READ scheduleAheadAmount NOTIFY scheduleAheadAmountChanged)
  Q_PROPERTY(bool audibleMetronome READ audibleMetronome WRITE setAudibleMetronome NOTIFY audibleMetronomeChanged)
public:
  static SyncTimer* instance() {
    static SyncTimer* instance{nullptr};
    if (!instance) {
      instance = new SyncTimer(qApp);
    }
    return instance;
  };
  explicit SyncTimer(QObject *parent = nullptr);
  virtual ~SyncTimer();

  /**
   * \brief Fired at each position in the timer
   * You will receive a tick at a rate equal to what is returned by getMultiplier (this is currently 96ppqn,
   * but you should not assume things about this and instead operate on the assumption that it is whatever
   * that function returns).
   * @param beat The beat inside the current note (a number from 0 through 4*getMultiplier())
   */
  Q_SIGNAL void timerTick(int beat);
  void queueClipToStart(ClipAudioSource *clip);
  void queueClipToStop(ClipAudioSource *clip);
  void queueClipToStartOnChannel(ClipAudioSource *clip, int midiChannel);
  void queueClipToStopOnChannel(ClipAudioSource *clip, int midiChannel);
  /**
   * \brief Plays a number of full bar of metronome ticks (four), and starts playback on the next bar
   * @note This does not change the audible metronome state, and that will require turning on explicitly
   * @param bars The number of bars to count in (default 1, passing 0 will just start playback)
   * @param songMode If true, playback will be started in song mode
   */
  Q_INVOKABLE void startWithCountin(quint64 bars = 1, bool songMode = false);
  void start();
  void stop();
  int getInterval(int bpm);
  /**
   * \brief Convert a number of subbeat to seconds, given a specific bpm rate
   * @note The number of subbeats is relative to the multiplier (so a multiplier of 32 would give you 128 beats for a note)
   * @param bpm The number of beats per minute used as the basis of the calculation
   * @param beats The number of subbeats to convert to a duration in seconds
   * @return A floating point precision amount of seconds for the given number of subbeats at the given bpm rate
   */
  Q_INVOKABLE float subbeatCountToSeconds(quint64 bpm, quint64 beats) const;
  /**
   * \brief Convert an amount of seconds to the nearest number of subbeats, given a specific bpm rate
   * @note The number of subbeats is relative to the multiplier (so a multiplier of 32 would give you 128 beats for a note)
   * @param bpm The number of beats per minute used as the basis of the calculation
   * @param seconds The number of seconds to convert to an amount of subbeats
   * @return The number of beats that most closely matches the given number of seconds at the given bpm rate
   */
  Q_INVOKABLE quint64 secondsToSubbeatCount(quint64 bpm, float seconds) const;
  /**
   * \brief The timer's beat multiplier (that is, the number of subbeats per quarter note)
   * @return The number of subbeats per quarter note
   */
  Q_INVOKABLE int getMultiplier() const;
  /**
   * \brief The timer's current bpm rate
   * @return The number of beats per minute currently used as the basis for the timer's operation
   */
  Q_INVOKABLE quint64 getBpm() const;
  /**
   * \brief Sets the timer's bpm rate
   * @param bpm The bpm you wish the timer to operate at
   */
  Q_INVOKABLE void setBpm(quint64 bpm);
  Q_INVOKABLE void increaseBpm();
  Q_INVOKABLE void decreaseBpm();
  Q_SIGNAL void bpmChanged();

  /**
   * \brief Returns the number of timer ticks you should schedule midi events for to ensure they won't get missed
   * To ensure that jack doesn't miss one of your midi notes, you should schedule at least this many ticks ahead
   * when you are inserting midi notes into the schedule. The logic is that this is the amount of ticks which will
   * fit inside the length of buffer jack uses.
   * If you are working out yourself, the formula for working out the full buffer length (latency) would be:
   * (Frames [or buffer]/Sample Rate) * Period = Theoretical (or Math-derived) Latency in ms
   * and you will want one more than will fit inside that period (so that if you end up with exactly the right
   * conditions, you will have enough to schedule a note on both the first and last frame of a single buffer)
   * @return The number of ticks you should schedule midi notes ahead for
   */
  Q_INVOKABLE quint64 scheduleAheadAmount() const;
  Q_SIGNAL void scheduleAheadAmountChanged();

  /**
   * \brief Set the CAS instances used for the metronome's click sounds
   * @note This must be called before the audible metronome can be enabled (and will disable it if this function is called with either of the two set to nullptr)
   * @param tick The sound used for the 1st beat
   * @param tock The sound used for the 2nd through 4th beats
   */
  Q_INVOKABLE void setMetronomeTicks(ClipAudioSource *tick, ClipAudioSource *tock);
  /**
   * \brief Whether or not there is an audible metronome when the timer is running
   *
   * The metronome clicks will be on sketchpad channel -2 (the un-effected global channel), and not included
   * in the recordings made within sketchpad. If you record the system output using other tools, it is just
   * a part of the audio output signal and consequently you will end up having it in the recording.
   */
  bool audibleMetronome() const;
  void setAudibleMetronome(const bool &value);
  Q_SIGNAL void audibleMetronomeChanged();

  /**
   * \brief The current beat, where that makes useful sense
   * @returns An integer from 0 through 128
   */
  int beat() const;
  /**
   * \brief The number of ticks since the timer was most recently started
   * @returns The number of times the timer has fired since it was most recently started
   */
  Q_INVOKABLE quint64 cumulativeBeat() const;

  /**
   * \brief The jack playhead for the most recent playback start event
   * @return The jack frame timestamp for the point at which playback was most recently started
   */
  const quint64 &jackPlayheadAtStart() const;
  /**
   * \brief Used only for playback purposes, for synchronising the sampler synth loop playback
   * In short - you probably don't need this, unless you need to sync specifically with jack's internal playback position
   * (which is the most recent tick for stuff put into a jack buffer)
   * @returns The internal jack playback position in timer ticks
   */
  const quint64 &jackPlayhead() const;
  /**
   * \brief Used for playback purposes, for synchronising the sampler synth loop playback
   * In short - you probably don't need this, unless you need to sync specifically with jack's internal playback position
   * (which is the usecs position of the jack playhead)
   * @returns The internal jack playback position in usecs
   */
  const quint64 &jackPlayheadUsecs() const;
  /**
   * \brief The current length of a subbeat in microseconds (as used by jack)
   * @return The current length of a subbeat in microseconds
   */
  const quint64 &jackSubbeatLengthInMicroseconds() const;

  /**
   * \brief The Zynthbox Sketchpad's currently selected track
   * @returns The track index (0 through 9)
   */
  int currentTrack() const;
  /**
   * \brief Set the current track for Zynthbox' Sketchpad
   * @param newTrack The index of the track to be set as current (will be clamped to the range 0 through 9)
   */
  void setCurrentTrack(const int &newTrack);
  Q_SIGNAL void currentTrackChanged();

  /**
   * \brief The timer tick for a given jack playhead value (valid while timer is running)
   * Use this to convert a jack playhead value (as returned by jackPlayhead()) to a timer tick
   * @note This is not kept perpetually (we only keep 32768 of these values - technically, the same amount as the step command ring)
   * @param jackPlayhead A number of jack frames (potentially a very large number, number of jack frames since the server was most recently started)
   * @param remainder The number of frames that were past for the jack playhead, compared to when the timer tick actually happened
   * @return The timer tick that was valid for that frame
   */
  const quint64 timerTickForJackPlayhead(const quint64 &jackPlayhead, quint64 *remainder = nullptr) const;

  /**
   * \brief Schedule an audio clip to have one or more commands run on it on the next tick of the timer
   * If a command with the associated clip is already scheduled at the position and the given midiNote you're attempting to schedule it into,
   * this function will change the existing to match any new settings (that is, things marked to be done on the command
   * will be marked to be done on the existing command).
   * @note This function will take ownership of the command, and you should expect it to no longer exist after (especially if the above happens)
   * @note If you want the clip to loop (or not), set this on the clip itself along with the other clip properties
   * @param command The audio clip command you wish to fire on at the specified time
   * @param delay A delay in number of timer ticks counting from the current position
   */
  void scheduleClipCommand(ClipCommand *command, quint64 delay);
  /**
    * \brief Fired whenever a scheduled clip command has been sent to SamplerSynth
    * @param clipCommand The clip command which has just been sent to SamplerSynth
    */
  Q_SIGNAL void clipCommandSent(ClipCommand *clipCommand);

  /**
   * \brief Call this function to start collecting timer commands to be submitted all at the same time, end with endTimerCommandBundle()
   * The logic here is that to ensure timer commands are added at the precise moment we really want it,
   * if there is too much back and forth between QML and C++, this could take an inordinate amount of time,
   * and to reduce the effect of that, we instead allow you to send a bunch of commands, the same way you
   * would normally do it using scheduleTimerCommand, and then submit all of them at the same time, reducing
   * the roundtripping during the actual submission step.
   * @note Ensure that you have the same number of start and stop calls, as it is reference counted
   * @see endTimerCommandBundle()
   */
  Q_INVOKABLE void startTimerCommandBundle();
  /**
   * \brief Call this function to submit the commands collected after calling startTimerCommandBundle()
   * The start delay can be used to pick a specific step on which to start, but the default is selected
   * (yes, it's a seemingly magic number: not the current step, and also not the next, just to be sure)
   * to try and ensure we don't end up attempting to add data to a step which has now been played. This
   * is usually the safer option, but you can adjust it manually if you need it closer to the function
   * being called.
   * @param startDelay An initial extra delay which is used to pick the first step to insert data into
   * @see startTimerCommandBundle()
   * @see scheduleTimerCommand(quint, int, int, int, int, const QVariant&)
   */
  Q_INVOKABLE void endTimerCommandBundle(quint64 startDelay = 2);
  /**
   * \brief Schedule a playback command into the playback schedule to be sent with the given delay
   * @note This function will take ownership of the command, and you should expect it to no longer exist after
   * @param delay A delay in number of timer ticks counting from the current position (cumulativeBeat)
   * @param operation A number signifying the operation to schedule (see TimerCommand::Operation)
   * @param parameter1 An integer optionally used by the command's handler to perform its work
   * @param parameter2 A second integer optionally used by the command's handler to perform its work
   * @param parameter2 A third integer optionally used by the command's handler to perform its work
   * @param variantParameter A QVariant used by the parameter's handler, if an integer is insufficient
   */
  Q_INVOKABLE void scheduleTimerCommand(quint64 delay, int operation, int parameter1 = 0, int parameter2 = 0, int parameter3 = 0, const QVariant &variantParameter = QVariant(), int parameter4 = 0);

  /**
   * \brief Schedule a playback command into the playback schedule to be sent with the given delay
   * Scheduled commands will be fired on the step, unless the timer is stopped, at which point they
   * will be deleted and no longer be used. Unlike clip commands, they will not be combined, and instead
   * are simply added to the end of the command list for the given step.
   * @note This function will take ownership of the command, and you should expect it to no longer exist after
   * @param delay A delay in number of timer ticks counting from the current position (cumulativeBeat)
   * @param command A TimerCommand instance to be executed at the given time
   */
  void scheduleTimerCommand(quint64 delay, TimerCommand* command);

  /**
   * \brief Emitted when a timer command is found in the schedule
   *
   * @note This is called from the jack process call, and must complete in an extremely short amount of time
   * If you cannot guarantee a quick operation, use a queued connection
   */
  Q_SIGNAL void timerCommand(TimerCommand *command);

  /**
   * \brief Get the next channel available on the given track
   * The returned channel will never include the master channel (which is always available for scheduling)
   * The channel returned by the function will be marked as busy. Schedule events into SyncTimer using this
   * channel to update the internal state back to available after some time.
   * @param sketchpadTrack The track to get channel availability for (if -1, we'll assume the current track)
   * @param delay The time after which the channel needs to be available (if 0, we're working with right now)
   * @return A number from 0 to 15, representing a midi channel available for scheduling events onto without clashing (if there are channels available, otherwise the oldest active channel will be overridden)
   */
  int nextAvailableChannel(const int &sketchpadTrack = -1, quint64 delay = 0);

  /**
   * \brief Schedule a note message to be sent on the next tick of the timer
   * @note This is not thread-safe in itself - when the timer is running, don't call this function outside of a callback
   * @param midiNote The note you wish to change the state of
   * @param midiChannel The channel you wish to change the given note on
   * @param setOn Whether or not you are turning the note on
   * @param velocity The velocity of the note (only matters if you're turning it on)
   * @param duration An optional duration for on notes (0 means don't schedule a release, higher will schedule an off at the durationth beat from the start of the note)
   * @param delay A delay in numbers of timer ticks counting from the current position
   * @param sketchpadTrack The sketchpad track to schedule this to (-1 will send to the current track, -2 will send to the master track)
   */
  void scheduleNote(unsigned char midiNote, unsigned char midiChannel, bool setOn, unsigned char velocity, quint64 duration, quint64 delay, int sketchpadTrack = -1);

  /**
   * \brief Schedule a buffer of midi messages (the Juce type) to be sent with the given delay
   * @note This is not thread-safe in itself - when the timer is running, don't call this function outside of a callback
   * @param buffer The buffer that you wish to add to the schedule
   * @param delay The delay (if any) you wish to add
   * @param sketchpadTrack The sketchpad track to schedule this to (-1 will send to the current track, -2 will send to the master track)
   */
  void scheduleMidiBuffer(const juce::MidiBuffer& buffer, quint64 delay, int sketchpadTrack = -1);

  /**
   * \brief Send a note message immediately (ensuring it goes through the step sequencer output)
   * @param midiNote The note you wish to change the state of
   * @param midiChannel The channel you wish to change the given note on
   * @param setOn Whether or not you are turning the note on
   * @param velocity The velocity of the note (only matters if you're turning it on)
   * @param sketchpadTrack The sketchpad track to send this to (-1 will send to the current track, -2 will send to the master track)
   */
  void sendNoteImmediately(unsigned char midiNote, unsigned char midiChannel, bool setOn, unsigned char velocity, int sketchpadTrack = -1);

  /**
   * \brief Send all-note-off messages to all channels on the given track
   * @param sketchpadTrack The track to send note-off messages to
   */
  Q_INVOKABLE void sendAllNotesOffImmediately(int sketchpadTrack = -1);
  /**
   * \brief Send all-note-off messages to all channels on all tracks
   */
  Q_INVOKABLE void sendAllNotesOffEverywhereImmediately();
  /**
   * \brief Send all-sounds-off messages to all channels on the given track
   * @param sketchpadTrack The track to send note-off messages to
   */
  Q_INVOKABLE void sendAllSoundsOffImmediately(int sketchpadTrack = -1);
  /**
   * \brief Send all-sounds-off messages to all channels on all tracks
   */
  Q_INVOKABLE void sendAllSoundsOffEverywhereImmediately();

  /**
   * \brief Send a raw midi message with the given values at the next possible opportunity
   * @param size How many of the given values should be sent (between 1 and 3)
   * @param byte0 The first byte of the message
   * @param byte1 The second byte of the message
   * @param byte2 The third byte of the message
   * @param sketchpadTrack The sketchpad track to send this to (-1 will send to the current track, -2 will send to the master track)
   */
  Q_INVOKABLE void sendMidiMessageImmediately(int size, int byte0, int byte1 = 0, int byte2 = 0, int sketchpadTrack = -1);
  /**
   * \brief Send a program change to the given channel (will be sent to all external devices)
   * @param midiChannel The midi channel to send the message to
   * @param program The new value (will be clamped to 0 through 127)
   * @param sketchpadTrack The sketchpad track to send this to (-1 will send to the current track, -2 will send to the master track)
   */
  Q_INVOKABLE void sendProgramChangeImmediately(int midiChannel, int program, int sketchpadTrack = -1);
  /**
   * \brief Send a control change message to the given channel (will be sent to all external devices)
   * @param midiChannel The midi channel to send the message to
   * @param control The control (a conceptual knob, will be clamped between to 0 through 127)
   * @param value The value of the control (will be clamped to 0 through 127)
   * @param sketchpadTrack The sketchpad track to send this to (-1 will send to the current track, -2 will send to the master track)
   */
  Q_INVOKABLE void sendCCMessageImmediately(int midiChannel, int control, int value, int sketchpadTrack = -1);

  /**
   * \brief Send a set of midi messages out immediately (ensuring they go through the step sequencer output)
   * @param buffer The buffer that you wish to send out immediately
   * @param sketchpadTrack The sketchpad track to send this to (-1 will send to the current track, -2 will send to the master track)
   */
  void sendMidiBufferImmediately(const juce::MidiBuffer& buffer, int sketchpadTrack = -1);

  /**
   * \brief A convenience getter which returns what should be used to schedule things onto the current track
   * @return A value to be used to indicate the current track when performing scheduling operations
   */
  const int currentSketchpadTrack() const { return -1; };
  /**
   * \brief Convenience getter which returns what should be used to schedule things onto the master control track
   * @see scheduleMidiBuffer
   * @return A value to be used to indicate the master control track when performing scheduling operations
   */
  const int masterSketchpadTrack() const { return -2; };

  bool timerRunning();
  Q_SIGNAL void timerRunningChanged();

  /**
   * \brief Emitted when a GuiMessageOperation is found in the schedule
   */
  Q_SIGNAL void timerMessage(const QString& message, const int &parameter, const int &parameter2, const int &parameter3, const int &parameter4, const quint64 &bigParameter);

  Q_SLOT ClipCommand *getClipCommand();
  Q_SLOT void deleteClipCommand(ClipCommand *command);
  Q_SLOT TimerCommand *getTimerCommand();
  Q_SLOT void deleteTimerCommand(TimerCommand *command);

  /**
   * \brief Schedule start of playback at the given delay
   * @param delay The amount of ticks to delay the start of playback
   * @param startInSongMode If true, playback will be started in song mode
   * @param startOffset In song mode, use this to set the start offset position (or leave it alone)
   * @param duration In song mode, use this to set for how long playback should commence before stopping
   * @see SegmentHandler::startPlayback(qint64, quint64)
   */
  Q_INVOKABLE void scheduleStartPlayback(quint64 delay, bool startInSongMode = false, int startOffset = 0, quint64 duration = 0);
  /**
   * \brief Schedule stop of playback at the given delay
   * @param delay The amount of ticks to delay the stop of playback
   */
  Q_INVOKABLE void scheduleStopPlayback(quint64 delay);
  Q_SIGNAL void pleaseStartPlayback();
  Q_SIGNAL void pleaseStopPlayback();
protected:
  // This allows MidiRouter to process SyncTimer explicitly (this way we avoid having to pass through jack, which already has plenty of clients to worry about)
  friend class MidiRouterPrivate;
  void process(jack_nframes_t nframes, void *buffer, quint64 *jackPlayhead, quint64 *jackSubbeatLengthInMicroseconds);
  // This allows TransportManager to call us, so we avoid some back and forth since SyncTimer has all the information needed to set the position
  friend class TransportManagerPrivate;
  void setPosition(jack_position_t *position) const;
private:
  SyncTimerPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(SyncTimer*)
