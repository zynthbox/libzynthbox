/*
  ==============================================================================

    ClipAudioSource.h
    Created: 9 Aug 2021 6:25:01pm
    Author:  Anupam Basak <anupam.basak27@gmail.com>

  ==============================================================================
*/

#pragma once

#include <QObject>

#include <iostream>

#include <jack/jack.h>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

class SyncTimer;
class ClipAudioSourceSubvoiceSettings;
class ClipAudioSourcePositionsModel;
class JackPassthroughAnalyser;
class JackPassthroughFilter;
class JackPassthroughCompressor;
namespace tracktion_engine {
    class AudioFile;
    class Engine;
}
using namespace std;

//==============================================================================
class ClipAudioSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(int id READ id WRITE setId NOTIFY idChanged)
    /**
     * \brief How far along in a processing operation the clip is (for example timestretching)
     * The values are to be interpreted as:
     * * [0.0:0.0]: Processing has started, but we don't know how far along it is
     * * ]0.0:1.0[: Processing is under way and we know how far along we are
     * * [1.0:1.0]: Processing is being finished up
     * * -1: There is no processing currently ongoing
     */
    Q_PROPERTY(float processingProgress READ processingProgress NOTIFY processingProgressChanged)
    /**
     * \brief A description of what is currently happening
     */
    Q_PROPERTY(QString processingDescription READ processingDescription NOTIFY processingDescriptionChanged)
    /**
     * \brief The way in which SamplerSynth will perform playback
     * @default NonLoopingPlaybackStyle
     * Setting this to various modes will result in other properties changing as well
     * For example, setting it to GranularLoopingPlaybackStyle or GranularOneshotPlaybackStyle
     * will set the granular property to true, and setting it to OneshotPlaybackStyle or
     * LoopingPlaybackStyle will set the granular property to false. Setting it to one of the Looping
     * playback styles will set the looping property to true, and to false for the non-Looping styles.
     */
    Q_PROPERTY(PlaybackStyle playbackStyle READ playbackStyle WRITE setPlaybackStyle NOTIFY playbackStyleChanged)
    Q_PROPERTY(QString playbackStyleLabel READ playbackStyleLabel NOTIFY playbackStyleChanged)
    /**
     * \brief Whether to play using a live time stretching method for pitch changes during polyphonic playback
     * When set to something other than off, pitch bending will be done using a time stretching system, instead
     * of the standard pitch bending (which is done by speeding up or slowing down the sample)
     * This is orthogonal to the offline time stretching done by the speedRatio property
     */
    Q_PROPERTY(TimeStretchStyle timeStretchStyle READ timeStretchStyle WRITE setTimeStretchStyle NOTIFY timeStretchStyleChanged)
    /**
     * \brief The ptch adjustment (a floating point number of semitones) to adjust the sample offline
     * This is orthogonal to the live time stretching done by setting timeStretchLive
     */
    Q_PROPERTY(float pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    /**
     * \brief Whether to automatically synchronise the speed ratio between the clip's BPM and the playback one, to stretch the playback duration to match the same number of quarter notes between the two
     */
    Q_PROPERTY(bool autoSynchroniseSpeedRatio READ autoSynchroniseSpeedRatio WRITE setAutoSynchroniseSpeedRatio NOTIFY autoSynchroniseSpeedRatioChanged)
    /**
     * \brief The playback speed adjustment (a floating point number) for adjusting the sample offline
     * This is orthogonal to the live time stretching done by setting timeStretchLive
     */
    Q_PROPERTY(float speedRatio READ speedRatio WRITE setSpeedRatio NOTIFY speedRatioChanged)
    /**
     * \brief The clip's own BPM (used to calculate the speed ratio if required)
     * @note If set to 0, we will use the current song's BPM
     */
    Q_PROPERTY(float bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    /**
     * \brief The start position of the root slice in seconds
     * @see startPositionSamples
     */
    Q_PROPERTY(float startPositionSeconds READ getStartPosition WRITE setStartPosition NOTIFY startPositionChanged)
    /**
     * \brief The start position of the root slice in samples
     * @see startPositionSeconds
     */
    Q_PROPERTY(int startPositionSamples READ getStartPositionSamples WRITE setStartPositionSamples NOTIFY startPositionChanged)
    /**
     * \brief Whether the playback length should be locked to a number of beats (quarter notes) per the clip's BPM
     */
    Q_PROPERTY(bool snapLengthToBeat READ snapLengthToBeat WRITE setSnapLengthToBeat NOTIFY snapLengthToBeatChanged)
    /**
     * \brief The duration of the root slice in a number of beats (or quarter notes)
     * @see lengthSamples
     * @see lengthSeconds
     */
    Q_PROPERTY(float lengthBeats READ getLengthBeats WRITE setLengthBeats NOTIFY lengthChanged)
    /**
     * \brief The duration of the root slice in samples
     * @see lengthBeats
     * @see lengthSeconds
     */
    Q_PROPERTY(int lengthSamples READ getLengthSamples WRITE setLengthSamples NOTIFY lengthChanged)
    /**
     * \brief The duration of the root slice in seconds
     * @see lengthBeats
     * @see lengthSamples
     */
    Q_PROPERTY(float lengthSeconds READ getLengthSeconds NOTIFY lengthChanged)

    /**
     * \brief The amount of the loop duration which is used for crossfading (between none, and half of the loop's duration)
     * @default 0
     * @minimum 0
     * @maximum 0.5
     */
    Q_PROPERTY(double loopCrossfadeAmount READ loopCrossfadeAmount WRITE setLoopCrossfadeAmount NOTIFY loopCrossfadeAmountChanged)
    /**
     * \brief Whether the crossfade is done using data inside or outside the loop area
     * When this is set to...
     * - CrossfadeInnie, the fade will start from the loop start point, and end at the amount inside the loop represented by loopCrossfadeAmount
     * - CrossfadeOutie, the fade will start backward from the loop start point by the amount represented by loopCrossfadeAmount, and stop at the loop start point
     * @note If loopStartCrossfadeDirection and stopCrossfadeDirection are set to the same value, the crossfade will cause the loop's playback to change duration over time
     * @default CrossfadeOutie
     */
    Q_PROPERTY(CrossfadingDirection loopStartCrossfadeDirection READ loopStartCrossfadeDirection WRITE setLoopStartCrossfadeDirection NOTIFY loopStartCrossfadeDirectionChanged)
    /**
     * \brief Whether the crossfade is done using data from inside or outside the loop area
     * When this is set to...
     * - CrossfadeInnie, the fade will start at the position inside the loop backward from the stop point represented by loopCrossfadeAmount, and end at the stop point
     * - CrossfadeOutie, the fade will start at the stop position, and end loopCrossfadeAmount ahead of the stop position
     * @note If loopStartCrossfadeDirection and stopCrossfadeDirection are set to the same value, the crossfade will cause the loop's playback to change duration over time
     * @default CrossfadeInnie
     */
    Q_PROPERTY(CrossfadingDirection stopCrossfadeDirection READ stopCrossfadeDirection WRITE setStopCrossfadeDirection NOTIFY stopCrossfadeDirectionChanged)

    /**
     * \brief Whether or not this sample should be looped for playback (or single-shot so it auto-stops)
     * This can be overridden by the play function, where looping can be forced
     * @see ClipAudioSource::play(bool, int)
     * @default false
     */
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)
    /**
     * \brief The position of the loop point where looping playback will start from after the first run (in seconds, delta from playback start)
     * This will not be clamped for interaction purposes, but for playback the value will be blocked at the stop position
     * @minimum 0.0
     * @default 0.0
     */
    Q_PROPERTY(float loopDelta READ loopDelta WRITE setLoopDelta NOTIFY loopDeltaChanged)
    Q_PROPERTY(int loopDeltaSamples READ loopDeltaSamples WRITE setLoopDeltaSamples NOTIFY loopDeltaChanged)
    /**
     * \brief The position of the loop point where looping playback will stop at once released (in seconds, delta from playback stop)
     * TODO Once implemented, this will function as a fade-out point, to allow for long tails to go past the stop point after looping for sustain
     * This will not be clamped for interaction purposes, but for playback the value will be blocked at the end of the sample
     * @minimum 0.0
     * @default 0.0
     */
    Q_PROPERTY(float loopDelta2 READ loopDelta2 WRITE setLoopDelta2 NOTIFY loopDelta2Changed)
    Q_PROPERTY(int loopDelta2Samples READ loopDelta2Samples WRITE setLoopDelta2Samples NOTIFY loopDelta2Changed)
    /**
     * \brief The duration of the underlying file
     */
    Q_PROPERTY(float durationSeconds READ getDuration NOTIFY durationChanged)
    Q_PROPERTY(int durationSamples READ getDurationSamples NOTIFY durationChanged)
    /**
     * \brief The sketchpad track this clip is associated with
     * @note changing this while the clip is playing will potentially cause some weird sounds to happen, so probably try and avoid that
     * @default -1 (global playback)
     * @minimum -1
     * @maximum 9
     */
    Q_PROPERTY(int sketchpadTrack READ sketchpadTrack WRITE setSketchpadTrack NOTIFY sketchpadTrackChanged)
    /**
     * \brief The lane the clip should be played one (equivalent to the sample slot in SketchPad)
     * @default 0
     * @minimum 0
     * @maximum 4
     */
    Q_PROPERTY(int laneAffinity READ laneAffinity WRITE setLaneAffinity NOTIFY laneAffinityChanged)
    /**
     * \brief The gain of the clip in absolute style (from 0.0 through 1.0, equivalent to dB -100 through 24)
     */
    Q_PROPERTY(float gainAbsolute READ gainAbsolute WRITE setGainAbsolute NOTIFY gainChanged)
    /**
     * \brief The gain of the clip as a dB value
     */
    Q_PROPERTY(float gainDb READ getGainDB NOTIFY gainChanged)
    /**
     * \brief The current audio level in dB as a float (might be anywhere from -200 to 30, but can exist above that level as well)
     */
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)
    /**
     * \brief Whether or not there is at least one active position in the playback model which is active
     */
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    /**
     * \brief The current playback progress (of the first position in the positions model) as a floating point amount of seconds
      */
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
    /**
     * \brief The current playback position (of the first position in the positions model) as a floating point value from 0 through 1
     */
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    /**
     * \brief A model which contains the current positions at which the clip is being played back in SamplerSynth
     */
    Q_PROPERTY(QObject* playbackPositions READ playbackPositions CONSTANT)
    /**
     * \brief Defines how many of the subvoices should be activated when starting playback
     * @default 0
     * @minimum 0
     * @maximum 16
     */
    Q_PROPERTY(int subvoiceCount READ subvoiceCount WRITE setSubvoiceCount NOTIFY subvoiceCountChanged)
    /**
     * \brief A list containing the 16 entries for subvoices, whether active or not
     */
    Q_PROPERTY(QVariantList subvoiceSettings READ subvoiceSettings CONSTANT)
    /**
     * \brief How many slices should the Clip have
     * Setting this to a lower number than the current will remove entries from the positions list
     * and you will not be able to restore that value by returning it to the previous, larger value.
     * The default value is 16, meaning you have slices with the index 0 through 15
     * @default 16
     */
    Q_PROPERTY(int slices READ slices WRITE setSlices NOTIFY slicesChanged)
    /**
     * \brief The starting positions of each slice in Clip
     * Logically, all slices end at the start position of the next slice, and the last ends at the end of the clip
     * The values are double values, from 0 through 1 (usually there will not be a 1, as the last slice would then have length 0)
     */
    Q_PROPERTY(QVariantList slicePositions READ slicePositions WRITE setSlicePositions NOTIFY slicePositionsChanged)
    /**
     * \brief The midi note used to calculate the rotating positions of slices (this midi note will be slice 0)
     * @default 60
     */
    Q_PROPERTY(int sliceBaseMidiNote READ sliceBaseMidiNote WRITE setSliceBaseMidiNote NOTIFY sliceBaseMidiNoteChanged)
    /**
     * \brief The first midi note this clip should be used for (by default, a clip's keyzone is all midi notes)
     * @default 0
     */
    Q_PROPERTY(int keyZoneStart READ keyZoneStart WRITE setKeyZoneStart NOTIFY keyZoneStartChanged)
    /**
     * \brief The last midi note this clip should be used for (by default, a clip's keyzone is all midi notes)
     * @default 127
     */
    Q_PROPERTY(int keyZoneEnd READ keyZoneEnd WRITE setKeyZoneEnd NOTIFY keyZoneEndChanged)
    /**
     * \brief The midi note this clip plays at un-pitched when used by the sampler synth
     * @default 60
     */
    Q_PROPERTY(int rootNote READ rootNote WRITE setRootNote NOTIFY rootNoteChanged)
    /**
     * \brief The pan value denoting how much of a source signal is sent to the left and right channels
     * @default 0.0f
     */
    Q_PROPERTY(float pan READ pan WRITE setPan NOTIFY panChanged)
    /**
     * \brief The attack part of an ADSR envelope (number of seconds from the start of the clip playback start point)
     */
    Q_PROPERTY(float adsrAttack READ adsrAttack WRITE setADSRAttack NOTIFY adsrParametersChanged)
    /**
     * \brief The decay part of an ADSR envelope (duration of the decay part in seconds)
     */
    Q_PROPERTY(float adsrDecay READ adsrDecay WRITE setADSRDecay NOTIFY adsrParametersChanged)
    /**
     * \brief The sustain part of an ADSR envelope (amount of volume at the sustain point, from 0 through 1)
     */
    Q_PROPERTY(float adsrSustain READ adsrSustain WRITE setADSRSustain NOTIFY adsrParametersChanged)
    /**
     * \brief The release part of an ADSR envelope (the duration of the release in seconds)
     */
    Q_PROPERTY(float adsrRelease READ adsrRelease WRITE setADSRRelease NOTIFY adsrParametersChanged)
    /**
     * \brief Whether or not this clip should be played using a granular synthesis method
     */
    Q_PROPERTY(bool granular READ granular WRITE setGranular NOTIFY granularChanged)
    /**
     * \brief The position of sample playback as a fraction of the sample window
     * @default 0.0f
     * @minimum 0.0f
     * @maximum 1.0f
     */
    Q_PROPERTY(float grainPosition READ grainPosition WRITE setGrainPosition NOTIFY grainPositionChanged)
    /**
     * \brief The width of the window from the grain position as a fraction of the sample window that grains will be picked from
     * This is counted from the position (not centered around it), but will wrap the window. For example, if this is set to 0.3,
     * and grainPosition is set to 0.9, the grains will be positioned inside either the last 0.1 of the sample window, or the
     * first 0.2. Note also that grains will not be positioned at the end if they do not have the space to fully complete
     * playback there.
     * @default 1.0f
     * @minimum 0.0f
     * @maximum 1.0f
     */
    Q_PROPERTY(float grainSpray READ grainSpray WRITE setGrainSpray NOTIFY grainSprayChanged)
    /**
     * \brief The speed with which the grain position is moved for any playing notes
     * 1.0 means the position is moved at the speed of the playing note, 0.0 means no movement, 2.0 means
     * double speed (and so on). Negative numbers means the position moves backwards (with the same speeds
     * as when moving forwards, just backwards)
     * @default 0.0f
     * @minimum -100.0f
     * @maximum 100.0f
     */
    Q_PROPERTY(float grainScan READ grainScan WRITE setGrainScan NOTIFY grainScanChanged)
    /**
     * \brief The minimum interval for grains in milliseconds (default is 0, meaning the sample window duration)
     * @default 0 meaning the full duration of the sample window
     * @minimum 0
     */
    Q_PROPERTY(float grainInterval READ grainInterval WRITE setGrainInterval NOTIFY grainIntervalChanged)
    /**
     * \brief The maximum additional time for grain intervals (in ms, default is 0, meaning no variance)
     * @default 0 meaning no interval variance
     * @minimum 0
     */
    Q_PROPERTY(float grainIntervalAdditional READ grainIntervalAdditional WRITE setGrainIntervalAdditional NOTIFY grainIntervalAdditionalChanged)
    /**
     * \brief The minimum size of the grains in ms (default is 1 ms, minimum is 1ms)
     * @default 1
     * @minimum 1
     */
    Q_PROPERTY(float grainSize READ grainSize WRITE setGrainSize NOTIFY grainSizeChanged)
    /**
     * \brief The maximum additional duration for grains (in ms, default is 0, meaning no variance)
     * @default 0
     * @minimum 0
     */
    Q_PROPERTY(float grainSizeAdditional READ grainSizeAdditional WRITE setGrainSizeAdditional NOTIFY grainSizeAdditionalChanged)
    /**
     * \brief The lower end of the pan allowance for individual grains, relative to the clip's global pan (from -1 (left pan) through 1 (right pan))
     * @default 0
     * @minimum -1.0f
     * @maximum 1.0f
     * @note If this is set higher than the maximum pan, the maximum pan will be pushed up to match
     */
    Q_PROPERTY(float grainPanMinimum READ grainPanMinimum WRITE setGrainPanMinimum NOTIFY grainPanMinimumChanged)
    /**
     * \brief The upper end of the pan allowance for individual grains, relative to the clip's global pan (from -1 (left pan) through 1 (right pan))
     * @default 0
     * @minimum -1.0f
     * @maximum 1.0f
     * @note If this is set lower than the current mimimum pan, the mimimum pan will be pushed down to match
     */
    Q_PROPERTY(float grainPanMaximum READ grainPanMaximum WRITE setGrainPanMaximum NOTIFY grainPanMaximumChanged)
    /**
     * \brief The lower bound of the first set of the potential pitch adjustments for an individual grain, relative to the note's pitch (from -2 (reverse playback at double speed) through 2 (forward playback at double speed)
     * The concept here is to allow people the ability to send grain playback both forward and backward through, at a speed relative
     * to the note they are spawned from. If you want to limit it to one direction, just set the two pairs to the same values.
     * @default 1
     * @minimum -2.0f
     * @maximum 2.0f
     */
    Q_PROPERTY(float grainPitchMinimum1 READ grainPitchMinimum1 WRITE setGrainPitchMinimum1 NOTIFY grainPitchMinimum1Changed)
    /**
     * \brief The upper bound of the first set of the potential pitch adjustments for an individual grain, relative to the note's pitch (from -2 (reverse playback at double speed) through 2 (forward playback at double speed)
     * The concept here is to allow people the ability to send grain playback both forward and backward through, at a speed relative
     * to the note they are spawned from. If you want to limit it to one direction, just set the two pairs to the same values.
     * @default 1
     * @minimum -2.0f
     * @maximum 2.0f
     */
    Q_PROPERTY(float grainPitchMaximum1 READ grainPitchMaximum1 WRITE setGrainPitchMaximum1 NOTIFY grainPitchMaximum1Changed)
    /**
     * \brief The lower bound of the second set of the potential pitch adjustments for an individual grain, relative to the note's pitch (from -2 (reverse playback at double speed) through 2 (forward playback at double speed)
     * The concept here is to allow people the ability to send grain playback both forward and backward through, at a speed relative
     * to the note they are spawned from. If you want to limit it to one direction, just set the two pairs to the same values.
     * @default 1
     * @minimum -2.0f
     * @maximum 2.0f
     */
    Q_PROPERTY(float grainPitchMinimum2 READ grainPitchMinimum2 WRITE setGrainPitchMinimum2 NOTIFY grainPitchMinimum2Changed)
    /**
     * \brief The upper bound of the second set of the potential pitch adjustments for an individual grain, relative to the note's pitch (from -2 (reverse playback at double speed) through 2 (forward playback at double speed)
     * The concept here is to allow people the ability to send grain playback both forward and backward through, at a speed relative
     * to the note they are spawned from. If you want to limit it to one direction, just set the two pairs to the same values.
     * @default 1
     * @minimum -2.0f
     * @maximum 2.0f
     */
    Q_PROPERTY(float grainPitchMaximum2 READ grainPitchMaximum2 WRITE setGrainPitchMaximum2 NOTIFY grainPitchMaximum2Changed)
    /**
     * \brief The priority of one grain pitch value set over the other (0 only picks from the first, 1 only picks from the second, 0.5 is an even split)
     * @default 0.5f
     * @minimum 0.0f
     * @maximum 1.0f
     */
    Q_PROPERTY(float grainPitchPriority READ grainPitchPriority WRITE setGrainPitchPriority NOTIFY grainPitchPriorityChanged)
    /**
     * \brief The amount of the grain's minimum size should be given to sustain
     * The rest of the minimum period will be shared between attack and release (the envelope is an attack/sustain/release with no attack)
     * @default 0.3f
     * @minimum 0.0f
     * @maximum 1.0f
     */
    Q_PROPERTY(float grainSustain READ grainSustain WRITE setGrainSustain NOTIFY grainSustainChanged)
    /**
     * \brief How much of the grain's mimimum size should be attack and how much should be release (0.0 is all attack no release, 0.5 is even split, 1.0 is all release)
     * @default 0.5f
     * @minimum 0.0f
     * @minimum 1.0f
     */
    Q_PROPERTY(float grainTilt READ grainTilt WRITE setGrainTilt NOTIFY grainTiltChanged)

    /**
     * \brief Whether or not the equaliser will be applied to incoming audio
     * @default false
     */
    Q_PROPERTY(bool equaliserEnabled READ equaliserEnabled WRITE setEqualiserEnabled NOTIFY equaliserEnabledChanged)
    /**
     * \brief A list of the settings container objects for each of the equaliser bands
     */
    Q_PROPERTY(QVariantList equaliserSettings READ equaliserSettings NOTIFY equaliserSettingsChanged)

    /**
     * \brief Whether or not the compressor will be applied to incoming audio (post-equaliser)
     * @default false
     */
    Q_PROPERTY(bool compressorEnabled READ compressorEnabled WRITE setCompressorEnabled NOTIFY compressorEnabledChanged)
    /**
     * \brief The sources used for the left channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelLeft READ compressorSidechannelLeft WRITE setCompressorSidechannelLeft NOTIFY compressorSidechannelLeftChanged)
    /**
     * \brief The sources used for the right channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelRight READ compressorSidechannelRight WRITE setCompressorSidechannelRight NOTIFY compressorSidechannelRightChanged)
    /**
     * \brief The settings container object for the compressor
     */
    Q_PROPERTY(QObject* compressorSettings READ compressorSettings NOTIFY compressorSettingsChanged)
public:
  explicit ClipAudioSource(const char *filepath, bool muted = false, QObject *parent = nullptr);
  ~ClipAudioSource() override;

  enum SamplePickingStyle {
    SameOrFirstPickingStyle,
    SamePickingStyle,
    FirstPickingStyle,
    AllPickingStyle,
  };
  Q_ENUM(SamplePickingStyle)

  enum PlaybackStyle {
    NonLoopingPlaybackStyle,
    LoopingPlaybackStyle,
    OneshotPlaybackStyle,
    GranularNonLoopingPlaybackStyle,
    GranularLoopingPlaybackStyle,
    WavetableStyle,
  };
  Q_ENUM(PlaybackStyle)

  void syncProgress();
  void setStartPosition(float startPositionInSeconds);
  void setStartPositionSamples(int startPositionInSamples);
  float getStartPosition(int slice = -1) const;
  int getStartPositionSamples(int slice = -1) const;
  Q_SIGNAL void startPositionChanged();

  float getStopPosition(int slice = -1) const;
  int getStopPositionSamples(int slice = -1) const;

  PlaybackStyle playbackStyle() const;
  QString playbackStyleLabel() const;
  void setPlaybackStyle(const PlaybackStyle &playbackStyle);
  Q_SIGNAL void playbackStyleChanged();

  enum LoopStyle {
    ForwardLoop,
    BackwardLoop,
    PingPongLoop,
  };
  Q_ENUM(LoopStyle)
  void setLooping(bool looping);
  bool looping() const;
  Q_SIGNAL void loopingChanged();

  float loopDelta() const;
  int loopDeltaSamples() const;
  void setLoopDelta(const float &newLoopDelta);
  void setLoopDeltaSamples(const int &newLoopDeltaSamples);
  Q_SIGNAL void loopDeltaChanged();

  float loopDelta2() const;
  int loopDelta2Samples() const;
  void setLoopDelta2(const float &newLoopDelta2);
  void setLoopDelta2Samples(const int &newLoopDelta2Samples);
  Q_SIGNAL void loopDelta2Changed();

  bool snapLengthToBeat() const;
  void setSnapLengthToBeat(const bool &snapLengthToBeat);
  Q_SIGNAL void snapLengthToBeatChanged();
  void setLengthBeats(float beat);
  void setLengthSamples(int lengthInSamples);
  /**
   * \brief The length of the clip in beats
   * @return The length of the clip in beats (that is, in quarter notes)
   */
  float getLengthBeats() const;
  /**
   * \brief The length of the clip in samples
   * @return The length of the clip in samples
   */
  int getLengthSamples() const;
  /*
   * \brief Get the length of the clip in seconds
   * @return the length of the clip in seconds
   */
  float getLengthSeconds() const;
  Q_SIGNAL void lengthChanged();

  void setLoopCrossfadeAmount(const double &loopCrossfadeAmount);
  double loopCrossfadeAmount() const;
  Q_SIGNAL void loopCrossfadeAmountChanged();
  enum CrossfadingDirection {
    CrossfadeInnie,
    CrossfadeOutie,
  };
  Q_ENUM(CrossfadingDirection)
  void setLoopStartCrossfadeDirection(const CrossfadingDirection &loopStartCrossfadeDirection);
  CrossfadingDirection loopStartCrossfadeDirection() const;
  Q_SIGNAL void loopStartCrossfadeDirectionChanged();
  void setStopCrossfadeDirection(const CrossfadingDirection &stopCrossfadeDirection);
  CrossfadingDirection stopCrossfadeDirection() const;
  Q_SIGNAL void stopCrossfadeDirectionChanged();
  int loopFadeAdjustment(const int &slice) const;
  int stopFadeAdjustment(const int &slice) const;

  /**
   * \brief Attempt to guess the beats per minute of the given slice
   * @param slice The slice to detect the BPM inside of
   * @return The guessed BPM
   */
  Q_INVOKABLE float guessBPM(int slice = -1) const;

  enum TimeStretchStyle {
    TimeStretchOff,
    TimeStretchStandard,
    TimeStretchBetter,
  };
  Q_ENUM(TimeStretchStyle)
  void setTimeStretchStyle(const TimeStretchStyle &timeStretchLive);
  TimeStretchStyle timeStretchStyle() const;
  Q_SIGNAL void timeStretchStyleChanged();

  void setPitch(float pitchChange, bool immediate = false);
  float pitch() const;
  float pitchChangePrecalc() const;
  Q_SIGNAL void pitchChanged();

  void setAutoSynchroniseSpeedRatio(const bool &autoSynchroniseSpeedRatio);
  bool autoSynchroniseSpeedRatio() const;
  Q_SIGNAL void autoSynchroniseSpeedRatioChanged();
  void setSpeedRatio(float speedRatio, bool immediate = false);
  float speedRatio() const;
  Q_SIGNAL void speedRatioChanged();

  void setBpm(const float &bpm);
  float bpm() const;
  Q_SIGNAL void bpmChanged();

  float getGain() const;
  float getGainDB() const;
  float gainAbsolute() const;
  void setGain(const float &gain);
  void setGainDb(const float &db);
  void setGainAbsolute(const float &gainAbsolute);
  Q_SIGNAL void gainChanged();

  /**
   * \brief Starts playing, by default by forcing looping and on the global channel
   * Using the channel logic from SamplerSynth, -1 is the global channel (set lane
   * affinity to 1 for effected, and 0 for no effects), and 0-9 are channels 1
   * through 10 inclusive
   * @param forceLooping Plays with looping, and also force stops playback on the same lane/channel. This will override the sample's loop setting
   * @param midiChannel Pick the SketchPad track to play on
   */
  Q_INVOKABLE void play(bool forceLooping = true, int midiChannel = -1);
  // Midi channel logic as play(), except defaulting to stop all the things everywhere
  Q_INVOKABLE void stop(int midiChannel = -3);
  // The duration of the sample itself, in seconds
  float getDuration() const;
  // The duration of the sample itself, in samples
  Q_INVOKABLE int getDurationSamples() const;
  Q_SIGNAL void durationChanged();

  const char *getFileName() const;
  const char *getFilePath() const;

  double sampleRate() const;

  tracktion_engine::AudioFile getPlaybackFile() const;
  Q_SIGNAL void playbackFileChanged();

  int id() const;
  void setId(int id);
  Q_SIGNAL void idChanged();

  void setProcessingProgress(const float &processingProgress);
  void startProcessing(const QString &description = {});
  void endProcessing();
  const float &processingProgress() const;
  Q_SIGNAL void processingProgressChanged();
  void setProcessingDescription(const QString &processingDescription);
  const QString &processingDescription() const;
  Q_SIGNAL void processingDescriptionChanged();

  int sketchpadTrack() const;
  void setSketchpadTrack(const int &newValue);
  Q_SIGNAL void sketchpadTrackChanged();

  int laneAffinity() const;
  void setLaneAffinity(const int& newValue);
  Q_SIGNAL void laneAffinityChanged();

  float audioLevel() const;
  Q_SIGNAL void audioLevelChanged();

  bool isPlaying() const;
  Q_SIGNAL void isPlayingChanged();

  float progress() const;
  Q_SIGNAL void progressChanged();

  double position() const;
  Q_SIGNAL void positionChanged();

  QObject *playbackPositions();
  ClipAudioSourcePositionsModel *playbackPositionsModel();

  int subvoiceCount() const;
  void setSubvoiceCount(const int &subvoiceCount);
  Q_SIGNAL void subvoiceCountChanged();
  QVariantList subvoiceSettings() const;
  const QList<ClipAudioSourceSubvoiceSettings*> &subvoiceSettingsActual();

  int slices() const;
  void setSlices(int slices);
  Q_SIGNAL void slicesChanged();

  QVariantList slicePositions() const;
  void setSlicePositions(const QVariantList &slicePositions);
  /**
   * \brief Get the position of a specific slice
   * @param slice The slice you wish to get the position of (slices are 0-indexed)
   * @return A double precision value between 0 and 1 (for an invalid slice, 0 will be returned)
   */
  double slicePosition(int slice) const;
  /**
   * \brief Set the position of a specific slice
   * @param slice The slice you wish to change the position of (slices are 0-indexed)
   * @param position The new position (from 0 through 1). The value will be clamped to fit inside the area available for this slice (that is, between the positions of slice - 1 and slice + 1)
   */
  void setSlicePosition(int slice, float position);
  Q_SIGNAL void slicePositionsChanged();

  int sliceBaseMidiNote() const;
  void setSliceBaseMidiNote(int sliceBaseMidiNote);
  Q_SIGNAL void sliceBaseMidiNoteChanged();
  /**
   * \brief Get the appropriate slice for the given midi note, based on the current slice base midi note
   * @param midiNote The midi note you wish to get a slice index for
   * @return The slice index matching the given midi note
   */
  int sliceForMidiNote(int midiNote) const;

  int keyZoneStart() const;
  void setKeyZoneStart(int keyZoneStart);
  Q_SIGNAL void keyZoneStartChanged();

  int keyZoneEnd() const;
  void setKeyZoneEnd(int keyZoneEnd);
  Q_SIGNAL void keyZoneEndChanged();

  int rootNote() const;
  void setRootNote(int rootNote);
  Q_SIGNAL void rootNoteChanged();

  /**
   * @brief Get the current pan value in degress
   * @return A float denoting current pan value ranging from -1.0(Pan left) to +1.0(Pan right). Default : 0(No panning)
   */
  float pan();
  /**
   * @brief Sets how much of a source signal is sent to the left and right channels
   * M/S Panning is implemented as per the following algo :
   * <code>
   * mSignal = 0.5 * (left + right);
     sSignal = left - right;
     float pan; // [-1; +1]
     left  = 0.5 * (1.0 + pan) * mSignal + sSignal;
     right = 0.5 * (1.0 - pan) * mSignal - sSignal;
     </code>
     @note Source : https://forum.juce.com/t/how-do-stereo-panning-knobs-work/25773/9
   * @param pan The pan value you wish to set ranging from  ranging from -1.0(Pan left) to +1.0(Pan right)
   */
  void setPan(float pan);
  Q_SIGNAL void panChanged();

  float adsrAttack() const;
  void setADSRAttack(const float& newValue);
  float adsrDecay() const;
  void setADSRDecay(const float& newValue);
  float adsrSustain() const;
  void setADSRSustain(const float& newValue);
  float adsrRelease() const;
  void setADSRRelease(const float& newValue);
  const juce::ADSR::Parameters &adsrParameters() const;
  void setADSRParameters(const juce::ADSR::Parameters &parameters);
  const juce::ADSR &adsr() const;
  Q_SIGNAL void adsrParametersChanged();

  bool granular() const;
  void setGranular(const bool &newValue);
  Q_SIGNAL void granularChanged();
  float grainPosition() const;
  void setGrainPosition(const float &newValue);
  Q_SIGNAL void grainPositionChanged();
  float grainSpray() const;
  void setGrainSpray(const float &newValue);
  Q_SIGNAL void grainSprayChanged();
  float grainScan() const;
  void setGrainScan(const float &newValue);
  Q_SIGNAL void grainScanChanged();
  float grainInterval() const;
  void setGrainInterval(const float &newValue);
  Q_SIGNAL void grainIntervalChanged();
  float grainIntervalAdditional() const;
  void setGrainIntervalAdditional(const float &newValue);
  Q_SIGNAL void grainIntervalAdditionalChanged();
  float grainSize() const;
  void setGrainSize(const float &newValue);
  Q_SIGNAL void grainSizeChanged();
  float grainSizeAdditional() const;
  void setGrainSizeAdditional(const float &newValue);
  Q_SIGNAL void grainSizeAdditionalChanged();
  float grainPanMinimum() const;
  void setGrainPanMinimum(const float &newValue);
  Q_SIGNAL void grainPanMinimumChanged();
  float grainPanMaximum() const;
  void setGrainPanMaximum(const float &newValue);
  Q_SIGNAL void grainPanMaximumChanged();
  float grainPitchMinimum1() const;
  void setGrainPitchMinimum1(const float &newValue);
  Q_SIGNAL void grainPitchMinimum1Changed();
  float grainPitchMaximum1() const;
  void setGrainPitchMaximum1(const float &newValue);
  Q_SIGNAL void grainPitchMaximum1Changed();
  float grainPitchMinimum2() const;
  void setGrainPitchMinimum2(const float &newValue);
  Q_SIGNAL void grainPitchMinimum2Changed();
  float grainPitchMaximum2() const;
  void setGrainPitchMaximum2(const float &newValue);
  Q_SIGNAL void grainPitchMaximum2Changed();
  float grainPitchPriority() const;
  void setGrainPitchPriority(const float &newValue);
  Q_SIGNAL void grainPitchPriorityChanged();
  float grainSustain() const;
  void setGrainSustain(const float &newValue);
  Q_SIGNAL void grainSustainChanged();
  float grainTilt() const;
  void setGrainTilt(const float &newValue);
  Q_SIGNAL void grainTiltChanged();
  const juce::ADSR &grainADSR() const;

  bool equaliserEnabled() const;
  void setEqualiserEnabled(const bool &equaliserEnabled);
  Q_SIGNAL void equaliserEnabledChanged();
  QVariantList equaliserSettings() const;
  Q_SIGNAL void equaliserSettingsChanged();
  Q_INVOKABLE QObject *equaliserNearestToFrequency(const float &frequency) const;
  Q_SIGNAL void equaliserDataChanged();
  const std::vector<double> &equaliserMagnitudes() const;
  const std::vector<double> &equaliserFrequencies() const;
  void equaliserCreateFrequencyPlot(QPolygonF &p, const QRect bounds, float pixelsPerDouble);
  void setEqualiserInputAnalysers(QList<JackPassthroughAnalyser*> &equaliserInputAnalysers) const;
  const QList<JackPassthroughAnalyser*> &equaliserInputAnalysers() const;
  void setEqualiserOutputAnalysers(QList<JackPassthroughAnalyser*> &equaliserOutputAnalysers) const;
  const QList<JackPassthroughAnalyser*> &equaliserOutputAnalysers() const;

  bool compressorEnabled() const;
  void setCompressorEnabled(const bool &compressorEnabled);
  Q_SIGNAL void compressorEnabledChanged();
  QString compressorSidechannelLeft() const;
  void setCompressorSidechannelLeft(const QString &compressorSidechannelLeft);
  Q_SIGNAL void compressorSidechannelLeftChanged();
  QString compressorSidechannelRight() const;
  void setCompressorSidechannelRight(const QString &compressorSidechannelRight);
  Q_SIGNAL void compressorSidechannelRightChanged();
  void setSidechainPorts(jack_port_t *leftPort, jack_port_t *rightPort);
  void reconnectSidechainPorts(jack_client_t* jackClient);
  QObject *compressorSettings() const;
  Q_SIGNAL void compressorSettingsChanged();
  void finaliseProcess(jack_default_audio_sample_t** inputBuffers, jack_default_audio_sample_t** outputBuffers, size_t bufferLenth) const;
private:
  class Private;
  Private *d;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipAudioSource)
};
Q_DECLARE_METATYPE(ClipAudioSource::PlaybackStyle)
Q_DECLARE_METATYPE(ClipAudioSource::TimeStretchStyle)
Q_DECLARE_METATYPE(ClipAudioSource::CrossfadingDirection)
