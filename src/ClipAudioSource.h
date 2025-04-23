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
class ClipAudioSourceSliceSettings;
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
     * \brief The slot the sample is loaded into on its sketchpad track
     * @default 0
     * @minimum 0
     * @maximum 5
     */
    Q_PROPERTY(int sketchpadSlot READ sketchpadSlot WRITE setSketchpadSlot NOTIFY sketchpadSlotChanged)
    /**
     * \brief The lane the clip should be played one (for samples, the sample slot index in SketchPad, for sketch slots add 5 to the index)
     * @default 0
     * @minimum 0
     * @maximum 9
     */
    Q_PROPERTY(int laneAffinity READ laneAffinity WRITE setLaneAffinity NOTIFY laneAffinityChanged)

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
     * \brief The slice object which contains the settings for the clip itself
     */
    Q_PROPERTY(QObject* rootSlice READ rootSlice CONSTANT)
    /**
     * \brief The maximum number of slices you can have in any one clip, not counting the root slice
     */
    Q_PROPERTY(int sliceCountMaximum READ sliceCountMaximum CONSTANT)
    /**
     * \brief How many slices are active in the clip
     * @default 0
     * @minimum 0
     * @maximum sliceCountMaximum
     */
    Q_PROPERTY(int sliceCount READ sliceCount WRITE setSliceCount NOTIFY sliceCountChanged)
    /**
     * \brief Whether slices should be contiguous or not (in which case they are free-form)
     * @default false
     */
    Q_PROPERTY(bool slicesContiguous READ slicesContiguous WRITE setSlicesContiguous NOTIFY slicesContiguousChanged)
    /**
     * \brief A list containing all potential slices, whether active or not
     */
    Q_PROPERTY(QVariantList sliceSettings READ sliceSettings CONSTANT)
    /**
     * \brief The method by which samples should be picked
     */
    Q_PROPERTY(SamplePickingStyle slicePickingStyle READ slicePickingStyle WRITE setSlicePickingStyle NOTIFY slicePickingStyleChanged)
    /**
     * \brief The index of the currently selected slice (-1 being the root slice)
     * @default -1
     * @minimum -1
     * @maximum sliceCountMaximum - 1
     */
    Q_PROPERTY(int selectedSlice READ selectedSlice WRITE setSelectedSlice NOTIFY selectedSliceChanged)
    /**
     * \brief The object instance for the currently selected slice
     */
    Q_PROPERTY(QObject* selectedSliceObject READ selectedSliceObject NOTIFY selectedSliceChanged)

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
  /**
   * \brief Constructs a new ClipAudioSource instance for the given sample file
   *
   * @param filepath The full path of the file to construct a ClipAudioSource instance for
   * @param sketchpadTrack The sketchpad track this object is associated with (-1 for global, 0 through 9 for the actual tracks)
   * @param sketchpadSlot The slot this object exists in on its given sketchpad track (used for selection purposes during polyphonic, chromatic playback, see SamplePickingStyle)
   * @param registerForPolyphonicPlayback Whether to make this instance available for polyphonic, chromatic playback on the given track (set to false for e.g. loops and metronome samples)
   * @param muted Whether to start the clip off as muted
   * @param parent The QObject parent
   * @return The newly created ClipAudioSource instance based on the given settings
   */
  explicit ClipAudioSource(const char *filepath, const int &sketchpadTrack, const int &sketchpadSlot, const bool &registerForPolyphonicPlayback = true, bool muted = false, QObject *parent = nullptr);
  ~ClipAudioSource() override;

  enum SamplePickingStyle {
    SamePickingStyle,
    FirstPickingStyle,
    AllPickingStyle,
  };
  Q_ENUM(SamplePickingStyle)

  enum PlaybackStyle {
    InheritPlaybackStyle, ///@< Set slices to inherit to use the containing ClipAudioSource's playback style
    NonLoopingPlaybackStyle,
    LoopingPlaybackStyle,
    OneshotPlaybackStyle,
    GranularNonLoopingPlaybackStyle,
    GranularLoopingPlaybackStyle,
    WavetableStyle,
  };
  Q_ENUM(PlaybackStyle)
  enum LoopStyle {
    ForwardLoop,
    BackwardLoop,
    PingPongLoop,
  };
  Q_ENUM(LoopStyle)
  enum CrossfadingDirection {
    CrossfadeInnie,
    CrossfadeOutie,
  };
  Q_ENUM(CrossfadingDirection)
  enum TimeStretchStyle {
    TimeStretchOff,
    TimeStretchStandard,
    TimeStretchBetter,
  };
  Q_ENUM(TimeStretchStyle)

  /**
   * \brief Attempt to guess the beats per minute of the given slice
   * @param slice The slice to detect the BPM inside of
   * @return The guessed BPM
   */
  Q_INVOKABLE float guessBPM(int slice = -1) const;

  void setAutoSynchroniseSpeedRatio(const bool &autoSynchroniseSpeedRatio);
  bool autoSynchroniseSpeedRatio() const;
  Q_SIGNAL void autoSynchroniseSpeedRatioChanged();
  void setSpeedRatio(float speedRatio, bool immediate = false);
  float speedRatio() const;
  Q_SIGNAL void speedRatioChanged();

  void setBpm(const float &bpm);
  float bpm() const;
  Q_SIGNAL void bpmChanged();

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

  const double &sampleRate() const;

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

  int sketchpadSlot() const;
  void setSketchpadSlot(const int &newValue);
  Q_SIGNAL void sketchpadSlotChanged();

  bool registerForPolyphonicPlayback() const;

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

  /**
   * \brief Holds the settings for when performing un-sliced playback
   * When not doing sliced playback, the section being played back is
   * essentially also a slice, as all the data used is what's also
   * relevant to a slice. So, we use that as a container for the data,
   * to avoid too much duplicated functionality.
   * @return The clip's root slice
   */
  QObject* rootSlice() const;
  ClipAudioSourceSliceSettings* rootSliceActual() const;

  int sliceCountMaximum() const;
  int sliceCount() const;
  void setSliceCount(const int &sliceCount);
  Q_SIGNAL void sliceCountChanged();
  bool slicesContiguous() const;
  void setSlicesContiguous(const bool &slicesContiguous);
  Q_SIGNAL void slicesContiguousChanged();
  QVariantList sliceSettings() const;
  const QList<ClipAudioSourceSliceSettings*> &sliceSettingsActual() const;
  int selectedSlice() const;
  void setSelectedSlice(const int &selectedSlice);
  QObject *selectedSliceObject() const;
  Q_SIGNAL void selectedSliceChanged();
  /**
   * \brief Moves all existing entries from the given index up one step, and clears the one at the given index
   * Doing this during playback of slices further up the list results in weird output, so probably let's try and avoid that
   * @param sliceIndex The index of the slice to create
   */
  Q_INVOKABLE void insertSlice(const int &sliceIndex);
  /**
   * \brief Clear a specific slice's settings, move everything above down one step, and reduce sliceCount by 1, effectively "deleting" it from the list, moving all other slices up one step
   * Doing this during playback of slices further up the list results in weird output, so probably let's try and avoid that
   * @param sliceIndex The index of the slice to be deleted - if invalid (not 0 through 127), the call will be ignored
   */
  Q_INVOKABLE void removeSlice(const int &sliceIndex);
  /**
   * \brief The indices of slices for the given midi note
   * @note This list is precalculated and safe for processing-time calls
   * @param midiNote The midi note for which to retrieve a list of slices
   * @return The list of slices relevant to the given midi note
   */
  const QList<int> &sliceIndicesForMidiNote(const int &midiNote) const;
  /**
   * \brief Get the slice for the given index (invalid indices will return the root slice)
   * @param sliceIndex The index to fetch a slice for (for any invalid index, the root slice is returned)
   * @return The slice at the given index in the slice list, or the root slice for any invalid index
   */
  Q_INVOKABLE ClipAudioSourceSliceSettings *sliceFromIndex(const int &sliceIndex) const;
  /**
   * \brief Serialises the slice data into string form for persistence purposes (
   * This is essentially a convenience function for sketchpad.clip, to reduce the round
   * tripping we otherwise do when serialising the slices to a json string.
   * @see stringToSlices()
   */
  Q_INVOKABLE QString slicesToString() const;
  /**
   * \brief Clears the slices, and resets them based on the given dump
   * This is essentially a convenience function for sketchpad.clip, to reduce the round
   * tripping we otherwise do when deserialising the slices based on the clip's json dump.
   * @param data The serialised data as previously produced by slicesToString()
   * @see slicesToString()
   */
  Q_INVOKABLE void stringToSlices(const QString& data);

  SamplePickingStyle slicePickingStyle() const;
  void setSlicePickingStyle(const SamplePickingStyle &slicePickingStyle);
  Q_SIGNAL void slicePickingStyleChanged();

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
