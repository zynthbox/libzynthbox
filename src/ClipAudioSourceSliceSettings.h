#pragma once

#include "ClipAudioSource.h"

class ClipAudioSourceSliceSettingsPrivate;
class ClipAudioSourceSubvoiceSettings;
class GainHandler;
class ClipAudioSourceSliceSettings : public QObject
{
    Q_OBJECT
    /**
     * \brief The index of the slice in its associated clip
     * The root slice will be -1, everything else is the index inside the slices list
     */
    Q_PROPERTY(int index READ index CONSTANT)
    /**
     * \brief A convenience function to check whether this is a root slice (can also check whether index == -1)
     */
    Q_PROPERTY(bool isRootSlice READ isRootSlice CONSTANT)
    /**
     * \brief The way in which SamplerSynth will perform playback
     * Setting this to various modes will result in other properties changing as well
     * For example, setting it to GranularLoopingPlaybackStyle or GranularOneshotPlaybackStyle
     * will set the granular property to true, and setting it to OneshotPlaybackStyle or
     * LoopingPlaybackStyle will set the granular property to false. Setting it to one of the Looping
     * playback styles will set the looping property to true, and to false for the non-Looping styles.
     * @default InheritPlaybackStyle
     */
    Q_PROPERTY(ClipAudioSource::PlaybackStyle playbackStyle READ playbackStyle WRITE setPlaybackStyle NOTIFY playbackStyleChanged)
    Q_PROPERTY(QString playbackStyleLabel READ playbackStyleLabel NOTIFY playbackStyleChanged)
    /**
     * \brief The effective playback style for this slice (if the style is set to inherit, it will return the root slice's style)
     */
    Q_PROPERTY(ClipAudioSource::PlaybackStyle effectivePlaybackStyle READ effectivePlaybackStyle NOTIFY playbackStyleChanged)
    /**
     * \brief The start position in seconds
     * @see startPositionSamples
     */
    Q_PROPERTY(float startPositionSeconds READ startPositionSeconds WRITE setStartPositionSeconds NOTIFY startPositionChanged)
    /**
     * \brief The start position in samples
     * @see startPositionSeconds
     */
    Q_PROPERTY(int startPositionSamples READ startPositionSamples WRITE setStartPositionSamples NOTIFY startPositionChanged)
    /**
     * \brief Whether the playback length should be locked to a number of beats (quarter notes) per the clip's BPM
     */
    Q_PROPERTY(bool snapLengthToBeat READ snapLengthToBeat WRITE setSnapLengthToBeat NOTIFY snapLengthToBeatChanged)
    /**
     * \brief The duration in a number of beats (or quarter notes)
     * @see lengthSamples
     * @see lengthSeconds
     */
    Q_PROPERTY(float lengthBeats READ lengthBeats WRITE setLengthBeats NOTIFY lengthChanged)
    /**
     * \brief The duration in samples
     * @see lengthBeats
     * @see lengthSeconds
     */
    Q_PROPERTY(int lengthSamples READ lengthSamples WRITE setLengthSamples NOTIFY lengthChanged)
    /**
     * \brief The duration in seconds
     * @see lengthBeats
     * @see lengthSamples
     */
    Q_PROPERTY(float lengthSeconds READ lengthSeconds NOTIFY lengthChanged)

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
    Q_PROPERTY(ClipAudioSource::CrossfadingDirection loopStartCrossfadeDirection READ loopStartCrossfadeDirection WRITE setLoopStartCrossfadeDirection NOTIFY loopStartCrossfadeDirectionChanged)
    /**
     * \brief Whether the crossfade is done using data from inside or outside the loop area
     * When this is set to...
     * - CrossfadeInnie, the fade will start at the position inside the loop backward from the stop point represented by loopCrossfadeAmount, and end at the stop point
     * - CrossfadeOutie, the fade will start at the stop position, and end loopCrossfadeAmount ahead of the stop position
     * @note If loopStartCrossfadeDirection and stopCrossfadeDirection are set to the same value, the crossfade will cause the loop's playback to change duration over time
     * @default CrossfadeInnie
     */
    Q_PROPERTY(ClipAudioSource::CrossfadingDirection stopCrossfadeDirection READ stopCrossfadeDirection WRITE setStopCrossfadeDirection NOTIFY stopCrossfadeDirectionChanged)
    /**
     * \brief Whether or not this sample should be looped for playback (or single-shot so it auto-stops)
     * @note This property forward the actual state as defined by playbackStyle
     * This can be overridden by the play function, where looping can be forced
     * @see ClipAudioSource::play(bool, int)
     * @default false
     */
    Q_PROPERTY(bool looping READ looping NOTIFY loopingChanged)
    /**
     * \brief The position of the loop point where looping playback will start from after the first run (in seconds, delta from playback start)
     * This will not be clamped for interaction purposes, but for playback the value will be blocked at the stop position
     * @minimum 0.0
     * @default 0.0
     */
    Q_PROPERTY(float loopDeltaSeconds READ loopDeltaSeconds WRITE setLoopDeltaSeconds NOTIFY loopDeltaChanged)
    Q_PROPERTY(int loopDeltaSamples READ loopDeltaSamples WRITE setLoopDeltaSamples NOTIFY loopDeltaChanged)
    /**
     * \brief The position of the loop point where looping playback will stop at once released (in seconds, delta from playback stop)
     * TODO Once implemented, this will function as a fade-out point, to allow for long tails to go past the stop point after looping for sustain
     * This will not be clamped for interaction purposes, but for playback the value will be blocked at the end of the sample
     * @minimum 0.0
     * @default 0.0
     */
    Q_PROPERTY(float loopDelta2Seconds READ loopDelta2Seconds WRITE setLoopDelta2Seconds NOTIFY loopDelta2Changed)
    Q_PROPERTY(int loopDelta2Samples READ loopDelta2Samples WRITE setLoopDelta2Samples NOTIFY loopDelta2Changed)

    /**
     * \brief The pan value denoting how much of a source signal is sent to the left and right channels
     * @default 0.0f
     */
    Q_PROPERTY(float pan READ pan WRITE setPan NOTIFY panChanged)

    /**
     * \brief Whether to play using a live time stretching method for pitch changes during polyphonic playback
     * When set to something other than off, pitch bending will be done using a time stretching system, instead
     * of the standard pitch bending (which is done by speeding up or slowing down the sample)
     * This is orthogonal to the offline time stretching done by the speedRatio property
     * TODO This requires implementing in SamplerSynthSound, but for now we simply have the setting here, because this is where it will live
     * @default TimeStretchOff
     */
    Q_PROPERTY(ClipAudioSource::TimeStretchStyle timeStretchStyle READ timeStretchStyle WRITE setTimeStretchStyle NOTIFY timeStretchStyleChanged)
    /**
     * \brief The pitch adjustment (a floating point number of semitones) to adjust the sample offline
     */
    Q_PROPERTY(float pitch READ pitch WRITE setPitch NOTIFY pitchChanged)

    /**
     * \brief An object for adjusting the slice's gain relative to the clip's (as defined by the root slice, which is only relative to itself)
     * @default no adjustment
     */
    Q_PROPERTY(QObject* gainHandler READ gainHandler NOTIFY gainHandlerChanged)

    /**
     * \brief The midi note this clip plays at un-pitched when used by the sampler synth
     * @note When set to -1, the root note will be inherited from the clip
     * @default -1
     */
    Q_PROPERTY(int rootNote READ rootNote WRITE setRootNote NOTIFY rootNoteChanged)
    /**
     * \brief The first midi note this slice should be used for (by default, a slice's keyzone is all midi notes)
     * @default 0
     */
    Q_PROPERTY(int keyZoneStart READ keyZoneStart WRITE setKeyZoneStart NOTIFY keyZoneStartChanged)
    /**
     * \brief The last midi note this slice should be used for (by default, a slice's keyzone is all midi notes)
     * @default 127
     */
    Q_PROPERTY(int keyZoneEnd READ keyZoneEnd WRITE setKeyZoneEnd NOTIFY keyZoneEndChanged)
    /**
     * \brief The minimum velocity for which this slice should be activated
     * @note For stopping, all slices which match the keyzone will be stopped
     * Setting this may cause the maximum to change as well (pushing it up as required)
     * @default 1
     * @minimum 1
     * @maximum 127
     */
    Q_PROPERTY(int velocityMinimum READ velocityMinimum WRITE setVelocityMinimum NOTIFY velocityMinimumChanged)
    /**
     * \brief The maximum velocity for which this slice should be activated
     * @note For stopping, all slices which match the keyzone will be stopped
     * Setting this may change the minimum to change as well (pushing it down as required)
     * @default 127
     * @minimum 1
     * @maximum 127
     */
    Q_PROPERTY(int velocityMaximum READ velocityMaximum WRITE setVelocityMaximum NOTIFY velocityMaximumChanged)

    /**
     * \brief The sample-level exclusivity group for the slice
     * Group -1 is "no exclusivity", and the default
     * Any value above that is an exclusivity group, and any slice in some group
     * will stop the playback of any other slice with the same group.
     * @default -1
     * @minimum -1
     * @maximum 1024
     */
    Q_PROPERTY(int exclusivityGroup READ exclusivityGroup WRITE setExclusivityGroup NOTIFY exclusivityGroupChanged)

    /**
     * \brief If this is set to true, the root slice's voice settings will be used in place of the slice's own
     * @default true
     */
    Q_PROPERTY(bool inheritSubvoices READ inheritSubvoices WRITE setInheritSubvoices NOTIFY inheritSubvoicesChanged)
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
     * @note This property forwards the actual state as defined by playbackStyle
     */
    Q_PROPERTY(bool granular READ granular NOTIFY granularChanged)
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
    explicit ClipAudioSourceSliceSettings(const int &index, ClipAudioSource *parent = nullptr);
    ~ClipAudioSourceSliceSettings() override;

    void cloneFrom(const ClipAudioSourceSliceSettings *other);
    void clear();

    int index() const;
    bool isRootSlice() const;

    ClipAudioSource::PlaybackStyle playbackStyle() const;
    ClipAudioSource::PlaybackStyle effectivePlaybackStyle() const;
    QString playbackStyleLabel() const;
    void setPlaybackStyle(const ClipAudioSource::PlaybackStyle &playbackStyle);
    Q_SIGNAL void playbackStyleChanged();

    void setStartPositionSeconds(const float &startPositionInSeconds);
    void setStartPositionSamples(const int &startPositionInSamples);
    float startPositionSeconds() const;
    int startPositionSamples() const;
    Q_SIGNAL void startPositionChanged();

    float stopPositionSeconds() const;
    int stopPositionSamples() const;

    void setLooping(bool looping);
    bool looping() const;
    Q_SIGNAL void loopingChanged();

    float loopDeltaSeconds() const;
    int loopDeltaSamples() const;
    void setLoopDeltaSeconds(const float &newLoopDelta);
    void setLoopDeltaSamples(const int &newLoopDeltaSamples);
    Q_SIGNAL void loopDeltaChanged();

    float loopDelta2Seconds() const;
    int loopDelta2Samples() const;
    void setLoopDelta2Seconds(const float &newLoopDelta2);
    void setLoopDelta2Samples(const int &newLoopDelta2Samples);
    Q_SIGNAL void loopDelta2Changed();

    bool snapLengthToBeat() const;
    void setSnapLengthToBeat(const bool &snapLengthToBeat);
    Q_SIGNAL void snapLengthToBeatChanged();
    void setLengthBeats(const float &beat);
    void setLengthSamples(const int &lengthInSamples);
    /**
    * \brief The length of the clip in beats
    * @return The length of the clip in beats (that is, in quarter notes)
    */
    float lengthBeats() const;
    /**
    * \brief The length of the clip in samples
    * @return The length of the clip in samples
    */
    int lengthSamples() const;
    /*
    * \brief Get the length of the clip in seconds
    * @return the length of the clip in seconds
    */
    float lengthSeconds() const;
    Q_SIGNAL void lengthChanged();

    void setLoopCrossfadeAmount(const double &loopCrossfadeAmount);
    double loopCrossfadeAmount() const;
    Q_SIGNAL void loopCrossfadeAmountChanged();
    void setLoopStartCrossfadeDirection(const ClipAudioSource::CrossfadingDirection &loopStartCrossfadeDirection);
    ClipAudioSource::CrossfadingDirection loopStartCrossfadeDirection() const;
    Q_SIGNAL void loopStartCrossfadeDirectionChanged();
    void setStopCrossfadeDirection(const ClipAudioSource::CrossfadingDirection &stopCrossfadeDirection);
    ClipAudioSource::CrossfadingDirection stopCrossfadeDirection() const;
    Q_SIGNAL void stopCrossfadeDirectionChanged();
    int loopFadeAdjustment() const;
    int stopFadeAdjustment() const;

    void setTimeStretchStyle(const ClipAudioSource::TimeStretchStyle &timeStretchStyle);
    ClipAudioSource::TimeStretchStyle timeStretchStyle() const;
    Q_SIGNAL void timeStretchStyleChanged();

    float pitch() const;
    float pitchChangePrecalc() const;
    void setPitch(const float &pitch);
    Q_SIGNAL void pitchChanged();

    QObject *gainHandler() const;
    GainHandler *gainHandlerActual() const;
    Q_SIGNAL void gainHandlerChanged();

    /**
    * @brief Get the current pan value in degress
    * @return A float denoting current pan value ranging from -1.0(Pan left) to +1.0(Pan right). Default : 0(No panning)
    */
    float pan() const;
    /**
    * @brief Sets how much of a source signal is sent to the left and right channels
    * M/S Panning is implemented as per the following algo :
    * <code>
      mSignal = 0.5 * (left + right);
      sSignal = left - right;
      float pan; // [-1; +1]
      left  = 0.5 * (1.0 + pan) * mSignal + sSignal;
      right = 0.5 * (1.0 - pan) * mSignal - sSignal;
      </code>
    * @note Source : https://forum.juce.com/t/how-do-stereo-panning-knobs-work/25773/9
    * @param pan The pan value you wish to set ranging from  ranging from -1.0(Pan left) to +1.0(Pan right)
    */
    void setPan(const float &pan);
    Q_SIGNAL void panChanged();

    int rootNote() const;
    void setRootNote(const int &rootNote);
    Q_SIGNAL void rootNoteChanged();

    int keyZoneStart() const;
    void setKeyZoneStart(const int &keyZoneStart);
    Q_SIGNAL void keyZoneStartChanged();

    int keyZoneEnd() const;
    void setKeyZoneEnd(const int &keyZoneEnd);
    Q_SIGNAL void keyZoneEndChanged();

    int velocityMinimum() const;
    void setVelocityMinimum(const int &velocityMinimum);
    Q_SIGNAL void velocityMinimumChanged();

    int velocityMaximum() const;
    void setVelocityMaximum(const int &velocityMaximum);
    Q_SIGNAL void velocityMaximumChanged();

    int exclusivityGroup() const;
    void setExclusivityGroup(const int &exclusivityGroup);
    Q_SIGNAL void exclusivityGroupChanged();

    bool inheritSubvoices() const;
    void setInheritSubvoices(const bool &inheritSubvoices);
    Q_SIGNAL void inheritSubvoicesChanged();
    int subvoiceCount() const;
    void setSubvoiceCount(const int &subvoiceCount);
    Q_SIGNAL void subvoiceCountChanged();
    QVariantList subvoiceSettings() const;
    const QList<ClipAudioSourceSubvoiceSettings*> &subvoiceSettingsActual() const;
    /**
     * These two are used by SamplerSynthVoice to pull
     * out the appropriate subvoice settings as implied
     * by the state of the inheritSubvoices property
     */
    int subvoiceCountPlayback() const;
    const QList<ClipAudioSourceSubvoiceSettings*> &subvoiceSettingsPlayback() const;

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
    ClipAudioSourceSliceSettingsPrivate *d{nullptr};
};
