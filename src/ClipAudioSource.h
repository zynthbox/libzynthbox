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

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

class SyncTimer;
class ClipAudioSourcePositionsModel;
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
    Q_PROPERTY(float gainAbsolute READ gainAbsolute WRITE setGainAbsolute NOTIFY gainAbsoluteChanged)
    /**
     * \brief The volume of the clip in an absolute style (from 0.0 through 1.0)
     */
    Q_PROPERTY(float volumeAbsolute READ volumeAbsolute WRITE setVolumeAbsolute NOTIFY volumeAbsoluteChanged)
    /**
     * \brief The current audio level in dB as a float (might be anywhere from -200 to 30, but can exist above that level as well)
     */
    Q_PROPERTY(float audioLevel READ audioLevel NOTIFY audioLevelChanged)
    /**
     * \brief The current playback progress as a float
     */
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
    /**
     * \brief The current playback position (of the first position in the positions model) in seconds
     */
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    /**
     * \brief A model which contains the current positions at which the clip is being played back in SamplerSynth
     */
    Q_PROPERTY(QObject* playbackPositions READ playbackPositions CONSTANT)
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
     * \brief The midi note this clip plays at un-pitched when used by the sampler synth (only used for trig mode, not slice)
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
public:
  explicit ClipAudioSource(const char *filepath, bool muted = false, QObject *parent = nullptr);
  ~ClipAudioSource() override;

  void syncProgress();
  void setStartPosition(float startPositionInSeconds);
  float getStartPosition(int slice = -1) const;
  float getStopPosition(int slice = -1) const;
  void setLooping(bool looping);
  bool looping() const;
  Q_SIGNAL void loopingChanged();
  float loopDelta() const;
  void setLoopDelta(const float &newLoopDelta);
  Q_SIGNAL void loopDeltaChanged();
  void setLength(float beat, int bpm);
  /**
   * \brief The length of the clip in beats
   * @return The length of the clip in beats (that is, in quarter notes)
   */
  float getLengthInBeats() const;
  void setPitch(float pitchChange, bool immediate = false);
  void setSpeedRatio(float speedRatio, bool immediate = false);
  void setGain(float db);
  float getGain() const;
  float getGainDB() const;
  float gainAbsolute() const;
  void setGainAbsolute(const float &gainAbsolute);
  Q_SIGNAL void gainAbsoluteChanged();

  void setVolume(float vol);
  /**
   * \brief Set the volume by "slider position" (0.0 through 1.0)
   * @param vol The volume you wish to set, using tracktion's slider position notation (0.0 through 1.0)
   */
  void setVolumeAbsolute(float vol);
  /**
   * \brief Get the volume in "slider position" (0.0 through 1.0)
   * @return A number from 0.0 through 1.0 - that is, tracktion's slider position notation
   */
  float volumeAbsolute() const;
  Q_SIGNAL void volumeAbsoluteChanged();
  /**
   * \brief Starts playing, by default by forcing looping and on the global channel
   * Using the channel logic from SamplerSynth, -1 is the global channel (set lane
   * affinity to 1 for effected, and 0 for no effects), and 0-9 are channels 1
   * through 10 inclusive
   * @param forceLooping Plays with looping, and also force stops playback on the same lane/channel. This will override the sample's loop setting
   * @param midiChannel Pick the SketchPad track to play on
   */
  void play(bool forceLooping = true, int midiChannel = -1);
  // Midi channel logic as play(), except defaulting to stop all the things everywhere
  void stop(int midiChannel = -3);
  float getDuration() const;
  const char *getFileName() const;
  const char *getFilePath() const;
  void updateTempoAndPitch();

  double sampleRate() const;

  tracktion_engine::AudioFile getPlaybackFile() const;
  Q_SIGNAL void playbackFileChanged();

  int id() const;
  void setId(int id);
  Q_SIGNAL void idChanged();

  int laneAffinity() const;
  void setLaneAffinity(const int& newValue);
  Q_SIGNAL void laneAffinityChanged();

  float audioLevel() const;
  Q_SIGNAL void audioLevelChanged();

  float progress() const;
  Q_SIGNAL void progressChanged();

  double position() const;
  Q_SIGNAL void positionChanged();

  QObject *playbackPositions();
  ClipAudioSourcePositionsModel *playbackPositionsModel();

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
private:
  class Private;
  Private *d;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipAudioSource)
};
