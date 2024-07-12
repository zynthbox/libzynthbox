/*
  ==============================================================================

    ClipAudioSource.cpp
    Created: 9 Aug 2021 7:44:30pm
    Author:  root

  ==============================================================================
*/

#include "ClipAudioSource.h"
#include "ClipAudioSourcePositionsModel.h"

#include <QDateTime>
#include <QDebug>

#include <unistd.h>

#include "JUCEHeaders.h"
#include "../tracktion_engine/examples/common/Utilities.h"
#include "Helper.h"
#include "ClipCommand.h"
#include "JackPassthroughAnalyser.h"
#include "JackPassthroughCompressor.h"
#include "JackPassthroughFilter.h"
#include "MidiRouterDeviceModel.h"
#include "SamplerSynth.h"
#include "SyncTimer.h"
#include "Plugin.h"

namespace tracktion_engine {
#include <tracktion_engine/3rd_party/soundtouch/include/BPMDetect.h>
};

#define DEBUG_CLIP false
#define IF_DEBUG_CLIP if (DEBUG_CLIP)

#define equaliserBandCount 6
static constexpr float maxGainDB{24.0f};

using namespace std;

class ClipAudioSource::Private : public juce::Timer {
public:
  Private(ClipAudioSource *qq) : q(qq) {
    juce::ADSR::Parameters parameters = grainADSR.getParameters();
    parameters.attack = 0.01f;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.01f;
    grainADSR.setParameters(parameters);
    // Equaliser
    for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
        JackPassthroughFilter *newBand = new JackPassthroughFilter(equaliserBand, q);
        QObject::connect(newBand, &JackPassthroughFilter::activeChanged, q, [this](){ bypassUpdater(); });
        QObject::connect(newBand, &JackPassthroughFilter::soloedChanged, q, [this](){ bypassUpdater(); });
        QObject::connect(newBand, &JackPassthroughFilter::dataChanged, q, &ClipAudioSource::equaliserDataChanged);
        equaliserSettings[equaliserBand] = newBand;
    }
    for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
        if (equaliserBand > 0) {
            equaliserSettings[equaliserBand]->setPrevious(equaliserSettings[equaliserBand - 1]);
        }
        if (equaliserBand < 5) {
            equaliserSettings[equaliserBand]->setNext(equaliserSettings[equaliserBand + 1]);
        }
    }
    // A bit awkward perhaps, but... this is a variadic template, and there's no indexed access, just a template one, so... alright
    equaliserSettings[0]->setDspObjects(&filterChain[0].get<0>(), &filterChain[1].get<0>());
    equaliserSettings[1]->setDspObjects(&filterChain[0].get<1>(), &filterChain[1].get<1>());
    equaliserSettings[2]->setDspObjects(&filterChain[0].get<2>(), &filterChain[1].get<2>());
    equaliserSettings[3]->setDspObjects(&filterChain[0].get<3>(), &filterChain[1].get<3>());
    equaliserSettings[4]->setDspObjects(&filterChain[0].get<4>(), &filterChain[1].get<4>());
    equaliserSettings[5]->setDspObjects(&filterChain[0].get<5>(), &filterChain[1].get<5>());
    equaliserFrequencies.resize(300);
    for (size_t i = 0; i < equaliserFrequencies.size(); ++i) {
        equaliserFrequencies[i] = 20.0 * std::pow(2.0, i / 30.0);
    }
    equaliserMagnitudes.resize(300);
    // Compressor
    compressorSettings = new JackPassthroughCompressor(q);
    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
        sideChainGain[channelIndex] = new jack_default_audio_sample_t[8192](); // TODO This is an awkward assumption, there has to be a sensible way to do this - jack should know this, right?
    }
  }
  ClipAudioSource *q;
  const te::Engine &getEngine() const { return *engine; };
  te::WaveAudioClip::Ptr clip{nullptr};

  te::Engine *engine{nullptr};
  std::unique_ptr<te::Edit> edit;
  bool isRendering{false};
  SyncTimer *syncTimer;
  juce::File givenFile;
  juce::String chosenPath;
  juce::String fileName;
  QString filePath;
  float startPositionInSeconds = 0;
  int startPositionInSamples = 0;
  bool snapLengthToBeat{false};
  float lengthInSeconds = -1;
  int lengthInSamples = -1;
  float lengthInBeats = -1;
  ClipAudioSource::PlaybackStyle playbackStyle{ClipAudioSource::NonLoopingPlaybackStyle};
  bool looping{false};
  float loopDelta{0.0f};
  int loopDeltaSamples{0};
  float loopDelta2{0.0f};
  int loopDelta2Samples{0};
  float gain{1.0f};
  float volumeAbsolute{-1.0f}; // This is a cached value
  TimeStretchStyle timeStretchLive{ClipAudioSource::TimeStretchOff};
  float pitchChange = 0;
  float pitchChangePrecalc = 1.0f;
  bool autoSynchroniseSpeedRatio{false};
  float speedRatio = 1.0;
  float bpm{0};
  float pan{0.0f};
  double sampleRate{0.0f};
  double currentLeveldB{-400.0};
  double prevLeveldB{-400.0};
  int id{0};
  int sketchpadTrack{-1};
  int laneAffinity{0};
  ClipAudioSourcePositionsModel *positionsModel{nullptr};
  // Default is 16, but we also need to generate the positions, so that is set up in the ClipAudioSource ctor
  int slices{0};
  QVariantList slicePositions;
  QList<double> slicePositionsCache;
  int sliceBaseMidiNote{60};
  int keyZoneStart{0};
  int keyZoneEnd{127};
  int rootNote{60};
  juce::ADSR adsr;

  bool granular{false};
  float grainPosition{0.0f};
  float grainSpray{1.0f};
  float grainScan{0.0f};
  float grainInterval{10};
  float grainIntervalAdditional{10};
  float grainSize{100};
  float grainSizeAdditional{50};
  float grainPanMinimum{-1.0f};
  float grainPanMaximum{1.0f};
  float grainPitchMinimum1{1.0};
  float grainPitchMaximum1{1.0};
  float grainPitchMinimum2{1.0};
  float grainPitchMaximum2{1.0};
  float grainPitchPriority{0.5};
  float grainSustain{0.3f};
  float grainTilt{0.5f};
  juce::ADSR grainADSR;
  void updateGrainADSR() {
    // Sustain is 0.0 through 1.0, defines how much of the base period should be given to sustain
    // The time as known by the envelope is in seconds, and we're holding milliseconds, so... divide by a thousand
    const float remainingPeriod = (grainSize * (1.0f - grainSustain)) / 1000.0f;
    juce::ADSR::Parameters parameters;
    // Tilt is 0.0 through 1.0, defines how much of the period should be attack and how much should be
    // release (0.0 is all attack no release, 0.5 is even split, 1.0 is all release)
    parameters.attack = remainingPeriod * grainTilt;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = remainingPeriod * (1.0f - grainTilt);
    grainADSR.setParameters(parameters);
  }

  bool equaliserEnabled{false};
  JackPassthroughFilter* equaliserSettings[equaliserBandCount];
  JackPassthroughFilter *soloedFilter{nullptr};
  bool updateMagnitudes{true};
  std::vector<double> equaliserMagnitudes;
  std::vector<double> equaliserFrequencies;
  QList<JackPassthroughAnalyser*> equaliserInputAnalysers{nullptr,nullptr};
  QList<JackPassthroughAnalyser*> equaliserOutputAnalysers{nullptr,nullptr};
  dsp::ProcessorChain<dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>> filterChain[2];
  void bypassUpdater() {
      for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
          soloedFilter = nullptr;
          for (JackPassthroughFilter *filter : equaliserSettings) {
              if (filter->soloed()) {
                  soloedFilter = filter;
                  break;
              }
          }
          // A bit awkward perhaps, but... this is a variadic template, and there's no indexed access, just a template one, so... alright
          filterChain[channelIndex].setBypassed<0>((soloedFilter == equaliserSettings[0]) == false && equaliserSettings[0]->active() == false);
          filterChain[channelIndex].setBypassed<1>((soloedFilter == equaliserSettings[1]) == false && equaliserSettings[1]->active() == false);
          filterChain[channelIndex].setBypassed<2>((soloedFilter == equaliserSettings[2]) == false && equaliserSettings[2]->active() == false);
          filterChain[channelIndex].setBypassed<3>((soloedFilter == equaliserSettings[3]) == false && equaliserSettings[3]->active() == false);
          filterChain[channelIndex].setBypassed<4>((soloedFilter == equaliserSettings[4]) == false && equaliserSettings[4]->active() == false);
          filterChain[channelIndex].setBypassed<5>((soloedFilter == equaliserSettings[5]) == false && equaliserSettings[5]->active() == false);
      }
  }

  bool compressorEnabled{false};
  JackPassthroughCompressor *compressorSettings{nullptr};
  QString compressorSidechannelLeft, compressorSidechannelRight;
  bool compressorSidechannelEmpty[2]{true, true};
  jack_port_t *sideChainInput[2]{nullptr};
  jack_default_audio_sample_t *sideChainGain[2]{nullptr};

  qint64 nextPositionUpdateTime{0};
  double firstPositionProgress{0};
  qint64 nextGainUpdateTime{0};
  double progress{0};
  bool isPlaying{false};

  void syncAudioLevel() {
    if (nextGainUpdateTime < QDateTime::currentMSecsSinceEpoch()) {
      prevLeveldB = currentLeveldB;

      currentLeveldB = Decibels::gainToDecibels(positionsModel->peakGain());

      // Now we give the level bar fading charcteristics.
      // And, the below coversions, decibelsToGain and gainToDecibels,
      // take care of 0dB, which will never fade!...but a gain of 1.0 (0dB) will.

      const auto prevLevel{Decibels::decibelsToGain(prevLeveldB)};

      if (prevLeveldB > currentLeveldB)
        currentLeveldB = Decibels::gainToDecibels(prevLevel * 0.94);

      // Only notify when the value actually changes by some noticeable kind of amount
      if (abs(currentLeveldB - prevLeveldB) > 0.1) {
        // Because emitting from a thread that's not the object's own is a little dirty, so make sure it's done queued
        QMetaObject::invokeMethod(q, &ClipAudioSource::audioLevelChanged, Qt::QueuedConnection);
      }
      nextGainUpdateTime = QDateTime::currentMSecsSinceEpoch() + 30;
    }
  }
  void updateBpmDependentValues() {
    if (autoSynchroniseSpeedRatio && bpm > 0) {
      q->setSpeedRatio(syncTimer->getBpm() / bpm);
    } else {
      q->setSpeedRatio(1.0);
    }
    // Reset the length in beats to match
    // lengthInBeats = double(d->syncTimer->secondsToSubbeatCount(bpm, lengthInSeconds)) / double(syncTimer->getMultiplier());
    // Q_EMIT q->lengthChanged();
  }
private:
  void timerCallback() override;
};

ClipAudioSource::ClipAudioSource(const char *filepath, bool muted, QObject *parent)
    : QObject(parent)
    , d(new Private(this)) {
  moveToThread(Plugin::instance()->qmlEngine()->thread());
  d->syncTimer = SyncTimer::instance();
  d->engine = Plugin::instance()->getTracktionEngine();
  d->id = Plugin::instance()->nextClipId();
  Plugin::instance()->addCreatedClipToMap(this);

  connect(this, &ClipAudioSource::grainSizeChanged, this, [this]() { d->updateGrainADSR(); });
  connect(this, &ClipAudioSource::grainSustainChanged, this, [this]() { d->updateGrainADSR(); });
  connect(this, &ClipAudioSource::grainTiltChanged, this, [this]() { d->updateGrainADSR(); });

  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Opening file:" << filepath;

  d->givenFile = juce::File(filepath);

  const File editFile = File::createTempFile("editFile");

  d->edit = te::createEmptyEdit(*d->engine, editFile);
  d->clip = Helper::loadAudioFileAsClip(*d->edit, d->givenFile);

  d->fileName = d->givenFile.getFileName();
  d->filePath = QString::fromUtf8(filepath);
  d->lengthInSeconds = d->edit->getLength();

  if (d->clip) {
    d->clip->setAutoTempo(false);
    d->clip->setAutoPitch(false);
    d->clip->setTimeStretchMode(te::TimeStretcher::defaultMode);
    d->sampleRate = d->clip->getAudioFile().getSampleRate();
    d->adsr.setSampleRate(d->sampleRate);
    d->lengthInSamples = d->clip->getAudioFile().getLengthInSamples();
  }
  // Initially set the length in seconds to the full duration of the sample,
  // let the user set it to something else later on if they want to

  if (muted) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Clip marked to be muted";
    setVolume(-100.0f);
  }
  d->startTimerHz(30);

  d->positionsModel = new ClipAudioSourcePositionsModel(this);
  d->positionsModel->moveToThread(Plugin::instance()->qmlEngine()->thread());
  // We don't connect to this, as we are already syncing explicitly in timerCallback
  // connect(d->positionsModel, &ClipAudioSourcePositionsModel::peakGainChanged, this, [&](){ d->syncAudioLevel(); });
  connect(d->positionsModel, &QAbstractItemModel::dataChanged, this, [&](const QModelIndex& topLeft, const QModelIndex& /*bottomRight*/, const QVector< int >& roles = QVector<int>()){
    if (topLeft.row() == 0 && roles.contains(ClipAudioSourcePositionsModel::PositionProgressRole)) {
      syncProgress();
    }
  });
  SamplerSynth::instance()->registerClip(this);

  connect(this, &ClipAudioSource::slicePositionsChanged, this, [&](){
    d->slicePositionsCache.clear();
    for (const QVariant &position : d->slicePositions) {
        d->slicePositionsCache << position.toDouble();
    }
  });
  setSlices(16);

  connect(d->syncTimer, &SyncTimer::bpmChanged, this, [this](){ d->updateBpmDependentValues(); } );
  connect(this, &ClipAudioSource::bpmChanged, this, [this](){ d->updateBpmDependentValues(); } );
  connect(this, &ClipAudioSource::autoSynchroniseSpeedRatioChanged, this, [this](){ d->updateBpmDependentValues(); } );
}

ClipAudioSource::~ClipAudioSource() {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Destroying Clip";
  SamplerSynth::instance()->unregisterClip(this);
  Plugin::instance()->removeCreatedClipFromMap(this);
  Helper::callFunctionOnMessageThread(
    [&]() {
      d->stopTimer();
      d->edit.reset();
    }, true);
}

void ClipAudioSource::syncProgress() {
  if (d->nextPositionUpdateTime < QDateTime::currentMSecsSinceEpoch()) {
    double newPosition = d->startPositionInSeconds / getDuration();
    if (d->positionsModel && d->positionsModel->firstProgress() > -1.0f) {
      newPosition = d->positionsModel->firstProgress();
      if (d->isPlaying == false) {
        d->isPlaying = true;
        Q_EMIT isPlayingChanged();
      }
    } else {
      if (d->isPlaying == true) {
        d->isPlaying = false;
        Q_EMIT isPlayingChanged();
      }
    }
    if (abs(d->firstPositionProgress - newPosition) > 0.001) {
      d->firstPositionProgress = newPosition;
      d->progress = d->firstPositionProgress * getDuration();
      Q_EMIT positionChanged();
      Q_EMIT progressChanged();
      // This really wants to be 16, so we can get to 60 updates per second, but that tears to all heck without compositing, so... for now
      // (tested with higher rates, but it tears, so while it looks like an arbitrary number, afraid it's as high as we can go)
      // d->nextPositionUpdateTime = QDateTime::currentMSecsSinceEpoch() + 100; // 10 updates per second, this is loooow...
      // If it turns out this is a problem, we can reinstate the old code above, or perhaps do it on-demand...
      // (it will a problem be for rpi4 but is that a problem-problem if we're more properly rpi5, and it's really purely visual?)
      d->nextPositionUpdateTime = QDateTime::currentMSecsSinceEpoch() + 16;
    }
  }
}

ClipAudioSource::PlaybackStyle ClipAudioSource::playbackStyle() const
{
  return d->playbackStyle;
}

QString ClipAudioSource::playbackStyleLabel() const
{
  switch (d->playbackStyle) {
    case LoopingPlaybackStyle: {
      static const QLatin1String label{"Looping"};
      return label;
      break; }
    case OneshotPlaybackStyle: {
      static const QLatin1String label{"One-shot"};
      return label;
      break; }
    case GranularNonLoopingPlaybackStyle: {
      static const QLatin1String label{"Granular Non-looping"};
      return label;
      break; }
    case GranularLoopingPlaybackStyle: {
      static const QLatin1String label{"Granular Looping"};
      return label;
      break; }
    case WavetableStyle: {
      static const QLatin1String label{"Wavetable"};
      return label;
      break; }
    case NonLoopingPlaybackStyle:
    default: {
      static const QLatin1String label{"Non-looping"};
      return label;
      break; }
  }
}

void ClipAudioSource::setPlaybackStyle(const PlaybackStyle& playbackStyle)
{
  if (d->playbackStyle != playbackStyle) {
    d->playbackStyle = playbackStyle;
    Q_EMIT playbackStyleChanged();
    switch (playbackStyle) {
      case LoopingPlaybackStyle:
        setLooping(true);
        setGranular(false);
        break;
      case OneshotPlaybackStyle:
        setLooping(false);
        setGranular(false);
        break;
      case GranularNonLoopingPlaybackStyle:
        setLooping(false);
        setGranular(true);
        break;
      case GranularLoopingPlaybackStyle:
        setLooping(true);
        setGranular(true);
        break;
      case WavetableStyle:
        // WavetableStyle is functionally the same as LoopingPlaybackStyle, but is informative to allow the UI
        // to do a bit of supportive work (in short: treat the length as a window size, lock the loop delta
        // to 0, and moving the start point as multiples of the window size)
        setLooping(true);
        setGranular(false);
        setLoopDeltaSamples(0);
        if (d->lengthInSamples > (d->clip->getAudioFile().getLengthInSamples() / 4)) {
          setLengthSamples(d->clip->getAudioFile().getLengthInSamples() / 32);
        }
        // TODO Maybe we should do something clever like make some reasonable assumptions here if length is unreasonably large for a wavetable and then lock the wavetable position to a multiple of that when switching? Or is that too destructive?
        break;
      case NonLoopingPlaybackStyle:
      default:
        setLooping(false);
        setGranular(false);
        break;
    }
  }
}

void ClipAudioSource::setLooping(bool looping) {
  if (d->looping != looping) {
    d->looping = looping;
    Q_EMIT loopingChanged();
  }
}

bool ClipAudioSource::looping() const
{
  return d->looping;
}

float ClipAudioSource::loopDelta() const
{
  return d->loopDelta;
}

int ClipAudioSource::loopDeltaSamples() const
{
  return d->loopDeltaSamples;
}

void ClipAudioSource::setLoopDelta(const float& newLoopDelta)
{
  if (d->loopDelta != newLoopDelta) {
    d->loopDelta = newLoopDelta;
    d->loopDeltaSamples = newLoopDelta * d->sampleRate;
    Q_EMIT loopDeltaChanged();
  }
}

void ClipAudioSource::setLoopDeltaSamples(const int& newLoopDeltaSamples)
{
  if (d->loopDeltaSamples != newLoopDeltaSamples) {
    d->loopDeltaSamples = newLoopDeltaSamples;
    d->loopDelta = double(newLoopDeltaSamples) / d->sampleRate;
    Q_EMIT loopDeltaChanged();
  }
}

float ClipAudioSource::loopDelta2() const
{
  return d->loopDelta2;
}

int ClipAudioSource::loopDelta2Samples() const
{
  return d->loopDelta2Samples;
}

void ClipAudioSource::setLoopDelta2(const float& newLoopDelta2)
{
  if (d->loopDelta2 != newLoopDelta2) {
    d->loopDelta2 = newLoopDelta2;
    d->loopDelta2Samples = newLoopDelta2 * d->sampleRate;
    Q_EMIT loopDelta2Changed();
  }
}

void ClipAudioSource::setLoopDelta2Samples(const int& newLoopDelta2Samples)
{
  if (d->loopDelta2Samples != newLoopDelta2Samples) {
    d->loopDelta2Samples = newLoopDelta2Samples;
    d->loopDelta2 = double(newLoopDelta2Samples) / d->sampleRate;
    Q_EMIT loopDelta2Changed();
  }
}

void ClipAudioSource::setStartPosition(float startPositionInSeconds) {
  setStartPositionSamples(startPositionInSeconds * d->sampleRate);
}

void ClipAudioSource::setStartPositionSamples(int startPositionInSamples)
{
  if (d->startPositionInSamples != startPositionInSamples) {
    d->startPositionInSamples = jmax(0, startPositionInSamples);
    d->startPositionInSeconds = double(startPositionInSamples) / d->sampleRate;
    Q_EMIT startPositionChanged();
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Start Position to" << d->startPositionInSeconds << "seconds, meaning" << d->startPositionInSamples << "samples of" << d->clip->getAudioFile().getLengthInSamples();
  }
}

float ClipAudioSource::getStartPosition(int slice) const
{
    if (slice > -1 && slice < d->slicePositionsCache.length()) {
        return d->startPositionInSeconds + (d->lengthInSeconds * d->slicePositionsCache[slice]);
    } else {
        return d->startPositionInSeconds;
    }
}

int ClipAudioSource::getStartPositionSamples(int slice) const
{
  if (slice > -1 && slice < d->slicePositionsCache.length()) {
    return d->startPositionInSamples + (d->lengthInSamples * d->slicePositionsCache[slice]);
  } else {
    return d->startPositionInSamples;
  }
}

float ClipAudioSource::getStopPosition(int slice) const
{
    if (slice > -1 && slice + 1 < d->slicePositionsCache.length()) {
        return d->startPositionInSeconds + (d->lengthInSeconds * d->slicePositionsCache[slice + 1]);
    } else {
        return d->startPositionInSeconds + d->lengthInSeconds;
    }
}

int ClipAudioSource::getStopPositionSamples(int slice) const
{
  if (slice > -1 && slice + 1 < d->slicePositionsCache.length()) {
    return d->startPositionInSamples + (d->lengthInSamples * d->slicePositionsCache[slice + 1]);
  } else {
    return d->startPositionInSamples * d->lengthInSamples;
  }
}

float ClipAudioSource::guessBPM(int slice) const
{
  float guessedBPM{0.0f};
  if (auto clip = d->clip) {
    // Set up our basic prerequisite knowledge
    tracktion_engine::AudioFile audioFile = clip->getAudioFile();
    const int numChannels{audioFile.getNumChannels()};
    int startSample = audioFile.getLengthInSamples() * getStartPosition(slice);
    int lastSample = audioFile.getLengthInSamples() * getStopPosition(slice);
    // Pull the samples we want out of the reader and stuff them into the bpm detector
    const int numSamples{numChannels * (lastSample - startSample)};
    juce::int64 numLeft{numSamples};
    const int blockSize{65536};
    const bool useRightChan{numChannels > 1};
    AudioFormatReader *reader{audioFile.getFormat()->createReaderFor(d->givenFile.createInputStream().release(), true)};
    tracktion_engine::soundtouch::BPMDetect bpmDetector(numChannels, audioFile.getSampleRate());
    juce::AudioBuffer<float> fileBuffer(jmin(2, numChannels), lastSample - startSample);
    while (numLeft > 0)
    {
      // Either read our desired block size, or whatever is left, whichever is shorter
      const int numThisTime = (int) juce::jmin (numLeft, (juce::int64) blockSize);
      // Get the data and stuff it into a buffer
      reader->read(&fileBuffer, 0, numThisTime, startSample, true, useRightChan);
      // Create an interleaved selection of samples as we want them
      tracktion_engine::AudioScratchBuffer scratch(1, numThisTime * numChannels);
      float* interleaved = scratch.buffer.getWritePointer(0);
      juce::AudioDataConverters::interleaveSamples(fileBuffer.getArrayOfReadPointers(), interleaved, numThisTime, numChannels);
      // Pass them along to the bpm detector for processing
      bpmDetector.inputSamples(interleaved, numThisTime);
      // Next run...
      startSample += numThisTime;
      numLeft -= numThisTime;
    }
    guessedBPM = bpmDetector.getBpm();
  }
  return guessedBPM;
}

void ClipAudioSource::setTimeStretchStyle(const TimeStretchStyle &timeStretchLive)
{
  if (d->timeStretchLive != timeStretchLive) {
    d->timeStretchLive = timeStretchLive;
    Q_EMIT timeStretchStyleChanged();
  }
}

ClipAudioSource::TimeStretchStyle ClipAudioSource::timeStretchStyle() const
{
  return d->timeStretchLive;
}

void ClipAudioSource::setPitch(float pitchChange, bool /*immediate*/) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Pitch to" << pitchChange;
  d->pitchChange = pitchChange;
  d->pitchChangePrecalc = std::pow(2.0, d->pitchChange / 12.0) /* * sampleRate() / sampleRate() */; // should this perhaps be a sound sample rate over playback sample rate thing?
  Q_EMIT pitchChanged();
  // TODO Offline pitch shifting seems... to just outright be not working? Not sure i understand, i'm sure it worked at some point, but it looks to be entirely ignored when the rendering happens...
  // d->isRendering = true;
  // if (immediate) {
    // if (auto clip = d->clip) {
      // clip->setPitchChange(d->pitchChange);
    // }
  // } else {
    // updateTempoAndPitch();
  // }
}

float ClipAudioSource::pitch() const
{
  return d->pitchChange;
}

float ClipAudioSource::pitchChangePrecalc() const
{
  return d->pitchChangePrecalc;
}

void ClipAudioSource::setAutoSynchroniseSpeedRatio(const bool& autoSynchroniseSpeedRatio)
{
  if (d->autoSynchroniseSpeedRatio != autoSynchroniseSpeedRatio) {
    d->autoSynchroniseSpeedRatio = autoSynchroniseSpeedRatio;
    Q_EMIT autoSynchroniseSpeedRatioChanged();
  }
}

bool ClipAudioSource::autoSynchroniseSpeedRatio() const
{
  return d->autoSynchroniseSpeedRatio;
}

void ClipAudioSource::setSpeedRatio(float speedRatio, bool immediate) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Speed to" << speedRatio;
  d->speedRatio = speedRatio;
  Q_EMIT speedRatioChanged();
  d->isRendering = true;
  if (immediate) {
    if (auto clip = d->clip) {
      clip->setSpeedRatio(d->speedRatio);
    }
  } else {
    updateTempoAndPitch();
  }
}

float ClipAudioSource::speedRatio() const
{
  return d->speedRatio;
}

void ClipAudioSource::setBpm(const float& bpm)
{
  if (d->bpm != bpm) {
    d->bpm = bpm;
    Q_EMIT bpmChanged();
  }
}

float ClipAudioSource::bpm() const
{
  return d->bpm;
}

float ClipAudioSource::getGain() const
{
  return d->gain;
}

float ClipAudioSource::getGainDB() const
{
  return juce::Decibels::gainToDecibels(d->gain);
}

float ClipAudioSource::gainAbsolute() const
{
  return juce::jmap(juce::Decibels::gainToDecibels(d->gain, -maxGainDB), -maxGainDB, maxGainDB, 0.0f, 1.0f);
}

void ClipAudioSource::setGain(const float &gain)
{
  if (d->gain != gain && 0.0f <= gain && gain <= 15.84893192461113) {
    d->gain = gain;
    Q_EMIT gainChanged();
  }
}

void ClipAudioSource::setGainDb(const float &db) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting gain:" << db;
  setGain(juce::Decibels::decibelsToGain(db, -maxGainDB));
}

void ClipAudioSource::setGainAbsolute(const float &gainAbsolute)
{
  setGain(juce::Decibels::decibelsToGain(juce::jmap(gainAbsolute, 0.0f, 1.0f, -maxGainDB, maxGainDB), -maxGainDB));
}

void ClipAudioSource::setVolume(float vol) {
  if (auto clip = d->clip) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting volume:" << vol;
    // Knowing that -40 is our "be quiet now thanks" volume level, but tracktion thinks it should be -100, we'll just adjust that a bit
    // It means the last step is a bigger jump than perhaps desirable, but it'll still be more correct
    if (vol <= -40.0f) {
      clip->edit.setMasterVolumeSliderPos(0);
    } else {
      clip->edit.setMasterVolumeSliderPos(te::decibelsToVolumeFaderPosition(vol));
    }
    d->volumeAbsolute = clip->edit.getMasterVolumePlugin()->getSliderPos();
    Q_EMIT volumeAbsoluteChanged();
  }
}

void ClipAudioSource::setVolumeAbsolute(float vol)
{
  if (auto clip = d->clip) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting volume absolutely:" << vol;
    clip->edit.setMasterVolumeSliderPos(qMax(0.0f, qMin(vol, 1.0f)));
    d->volumeAbsolute = clip->edit.getMasterVolumePlugin()->getSliderPos();
    Q_EMIT volumeAbsoluteChanged();
  }
}

float ClipAudioSource::volumeAbsolute() const
{
  if (d->volumeAbsolute < 0) {
    if (auto clip = d->clip) {
      d->volumeAbsolute = clip->edit.getMasterVolumePlugin()->getSliderPos();
    }
  }
  return d->volumeAbsolute;
}

void ClipAudioSource::setSnapLengthToBeat(const bool& snapLengthToBeat)
{
  if (d->snapLengthToBeat != snapLengthToBeat) {
    d->snapLengthToBeat = snapLengthToBeat;
    Q_EMIT snapLengthToBeatChanged();
  }
}

bool ClipAudioSource::snapLengthToBeat() const
{
  return d->snapLengthToBeat;
}

void ClipAudioSource::setLengthBeats(float beat) {
  // IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Interval:" << d->syncTimer->getInterval(bpm);
  float lengthInSeconds = d->syncTimer->subbeatCountToSeconds(quint64(d->bpm), quint64((beat * d->syncTimer->getMultiplier())));
  if (lengthInSeconds != d->lengthInSeconds) {
    // IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Length to" << lengthInSeconds;
    d->lengthInSeconds = lengthInSeconds;
    d->lengthInSamples = lengthInSeconds * d->sampleRate;
    d->lengthInBeats = beat;
    Q_EMIT lengthChanged();
  }
}

void ClipAudioSource::setLengthSamples(int lengthInSamples)
{
  if (d->lengthInSamples != lengthInSamples) {
    d->lengthInSamples = lengthInSamples;
    d->lengthInSeconds = double(lengthInSamples) / d->sampleRate;
    d->lengthInBeats = double(d->syncTimer->secondsToSubbeatCount(d->bpm, d->lengthInSeconds)) / double(d->syncTimer->getMultiplier());
    Q_EMIT lengthChanged();
  }
}

float ClipAudioSource::getLengthBeats() const {
  return d->lengthInBeats;
}

int ClipAudioSource::getLengthSamples() const
{
  return d->lengthInSamples;
}

float ClipAudioSource::getLengthSeconds() const
{
  return double(d->lengthInSamples) / d->sampleRate;
}


float ClipAudioSource::getDuration() const {
  return d->edit->getLength();
}

int ClipAudioSource::getDurationSamples() const
{
  if (d->clip) {
    return d->clip->getAudioFile().getLengthInSamples();
  }
  return 0;
}

const char *ClipAudioSource::getFileName() const {
  return static_cast<const char *>(d->fileName.toUTF8());
}

double ClipAudioSource::sampleRate() const
{
  return d->sampleRate;
}

const char *ClipAudioSource::getFilePath() const {
    return d->filePath.toUtf8();
}

tracktion_engine::AudioFile ClipAudioSource::getPlaybackFile() const {
    if (const auto& clip = d->clip) {
        return clip->getPlaybackFile();
    }
    return te::AudioFile(*d->engine);
}

void ClipAudioSource::updateTempoAndPitch() {
  if (auto clip = d->clip) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Updating speedRatio(" << d->speedRatio << ") and pitch(" << d->pitchChange << ")";
    // clip->setPitchChange(d->pitchChange);
    clip->setSpeedRatio(d->speedRatio);
  }
}

void ClipAudioSource::Private::timerCallback() {
  // Calling this from a timer will lead to a bad time, make sure it happens somewhere more reasonable (like on the object's own thread, which in this case is the qml engine thread)
  QMetaObject::invokeMethod(positionsModel, &ClipAudioSourcePositionsModel::updatePositions, Qt::QueuedConnection);
  syncAudioLevel();

  if (clip) {
    if (!clip->needsRender() && isRendering) {
        isRendering = false;
        Q_EMIT q->playbackFileChanged();
        adsr.setSampleRate(clip->getAudioFile().getSampleRate());
        adsr.setParameters(adsr.getParameters());
    }
  }
}

void ClipAudioSource::play(bool forceLooping, int midiChannel) {
  auto clip = d->clip;
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Starting clip " << this << d->filePath << " which is really " << clip.get() << " in a " << (forceLooping ? "looping" : "non-looping") << " manner from " << d->startPositionInSeconds << " and for " << d->lengthInSeconds << " seconds at volume " << (clip  && clip->edit.getMasterVolumePlugin().get() ? clip->edit.getMasterVolumePlugin()->volume : 0);

  ClipCommand *command = ClipCommand::channelCommand(this, midiChannel);
  command->midiNote = 60;
  command->changeVolume = true;
  command->volume = 1.0f;
  command->changeLooping = true;
  if (forceLooping) {
    command->looping = true;
    command->stopPlayback = true; // this stops any current loop plays, and immediately starts a new one
  } else {
    command->looping = d->looping;
  }
  command->startPlayback = true;
  d->syncTimer->scheduleClipCommand(command, 0);
}

void ClipAudioSource::stop(int midiChannel) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Stopping clip" << this << "on channel" << midiChannel << "path:" << d->filePath;
  if (midiChannel > -3) {
    ClipCommand *command = ClipCommand::channelCommand(this, midiChannel);
    command->midiNote = 60;
    command->stopPlayback = true;
    d->syncTimer->scheduleClipCommand(command, 0);
  } else {
    // Less than the best thing - having to do this to ensure we stop the ones looper
    // queued for starting as well, otherwise they'll get missed for stopping... We'll
    // want to handle this more precisely later, but for now this should do the trick.
    ClipCommand *command = ClipCommand::globalCommand(this);
    command->stopPlayback = true;
    d->syncTimer->scheduleClipCommand(command, 0);
    for (int i = 0; i < 10; ++i) {
      command = ClipCommand::channelCommand(this, i);
      command->midiNote = 60;
      command->stopPlayback = true;
      d->syncTimer->scheduleClipCommand(command, 0);
    }
  }
}

int ClipAudioSource::id() const
{
    return d->id;
}

void ClipAudioSource::setId(int id)
{
    if (d->id != id) {
        d->id = id;
        Q_EMIT idChanged();
    }
}

int ClipAudioSource::sketchpadTrack() const
{
  return d->sketchpadTrack;
}

void ClipAudioSource::setSketchpadTrack(const int& newValue)
{
  const int adjusted{std::clamp(newValue, -1, 9)};
  if (d->sketchpadTrack != adjusted) {
    d->sketchpadTrack = adjusted;
    Q_EMIT sketchpadTrackChanged();
  }
}

int ClipAudioSource::laneAffinity() const
{
  return d->laneAffinity;
}

void ClipAudioSource::setLaneAffinity(const int& newValue)
{
  const int adjusted{std::clamp(newValue, 0, 4)};
  if (d->laneAffinity != adjusted) {
    d->laneAffinity = adjusted;
    Q_EMIT laneAffinityChanged();
  }
}

float ClipAudioSource::audioLevel() const
{
  return d->currentLeveldB;
}

bool ClipAudioSource::isPlaying() const
{
  return d->isPlaying;
}

float ClipAudioSource::progress() const
{
  return d->progress;
}

double ClipAudioSource::position() const
{
  return d->firstPositionProgress;
}

QObject *ClipAudioSource::playbackPositions()
{
    return d->positionsModel;
}

ClipAudioSourcePositionsModel *ClipAudioSource::playbackPositionsModel()
{
    return d->positionsModel;
}

int ClipAudioSource::slices() const
{
    return d->slices;
}

void ClipAudioSource::setSlices(int slices)
{
    if (d->slices != slices) {
        if (slices == 0) {
            // Special casing clearing, because simple case, why not make it fast
            d->slicePositions.clear();
            Q_EMIT slicePositionsChanged();
        } else if (d->slices > slices) {
            // Just remove the slices that are too many
            while (d->slicePositions.length() > slices) {
                d->slicePositions.removeLast();
            }
            Q_EMIT slicePositionsChanged();
        } else if (d->slices < slices) {
            // Fit the new number of slices evenly into the available space
            double lastSlicePosition{0.0f};
            if (d->slicePositions.count() > 0) {
                lastSlicePosition = d->slicePositions.last().toDouble();
            }
            double positionIncrement{(1.0f - lastSlicePosition) / (slices - d->slices)};
            double newPosition{lastSlicePosition + positionIncrement};
            if (d->slicePositions.count() == 0) {
                d->slicePositions << QVariant::fromValue<double>(0.0f);
            }
            while (d->slicePositions.length() < slices) {
                d->slicePositions << QVariant::fromValue<double>(newPosition);
                newPosition += positionIncrement;
            }
            Q_EMIT slicePositionsChanged();
        }
        d->slices = slices;
        Q_EMIT slicesChanged();
    }
}

QVariantList ClipAudioSource::slicePositions() const
{
    return d->slicePositions;
}

void ClipAudioSource::setSlicePositions(const QVariantList &slicePositions)
{
    if (d->slicePositions != slicePositions) {
        d->slicePositions = slicePositions;
        Q_EMIT slicePositionsChanged();
        d->slices = slicePositions.length();
        Q_EMIT slicesChanged();
    }
}

double ClipAudioSource::slicePosition(int slice) const
{
    double position{0.0f};
    if (slice > -1 && slice < d->slicePositionsCache.length()) {
        position = d->slicePositionsCache[slice];
    }
    return position;
}

void ClipAudioSource::setSlicePosition(int slice, float position)
{
    if (slice > -1 && slice < d->slicePositions.length()) {
        d->slicePositions[slice] = position;
        Q_EMIT slicePositionsChanged();
    }
}

int ClipAudioSource::sliceBaseMidiNote() const
{
    return d->sliceBaseMidiNote;
}

void ClipAudioSource::setSliceBaseMidiNote(int sliceBaseMidiNote)
{
    if (d->sliceBaseMidiNote != sliceBaseMidiNote) {
        d->sliceBaseMidiNote = sliceBaseMidiNote;
        Q_EMIT sliceBaseMidiNoteChanged();
    }
}

int ClipAudioSource::sliceForMidiNote(int midiNote) const
{
    return ((d->slices - (d->sliceBaseMidiNote % d->slices)) + midiNote) % d->slices;
}

int ClipAudioSource::keyZoneStart() const
{
  return d->keyZoneStart;
}

void ClipAudioSource::setKeyZoneStart(int keyZoneStart)
{
  keyZoneStart = std::clamp(keyZoneStart, -1, 127);
  if (d->keyZoneStart != keyZoneStart) {
    d->keyZoneStart = keyZoneStart;
    Q_EMIT keyZoneStartChanged();
    if (d->keyZoneEnd < d->keyZoneStart) {
      setKeyZoneEnd(d->keyZoneStart);
    }
  }
}

int ClipAudioSource::keyZoneEnd() const
{
  return d->keyZoneEnd;
}

void ClipAudioSource::setKeyZoneEnd(int keyZoneEnd)
{
  keyZoneEnd = std::clamp(keyZoneEnd, -1, 127);
  if (d->keyZoneEnd != keyZoneEnd) {
    d->keyZoneEnd = keyZoneEnd;
    Q_EMIT keyZoneEndChanged();
    if (d->keyZoneStart > d->keyZoneEnd) {
      setKeyZoneStart(d->keyZoneEnd);
    }
  }
}

int ClipAudioSource::rootNote() const
{
  return d->rootNote;
}

void ClipAudioSource::setRootNote(int rootNote)
{
  rootNote = std::clamp(rootNote, -1, 127);
  if (d->rootNote != rootNote) {
    d->rootNote = rootNote;
    Q_EMIT rootNoteChanged();
  }
}

float ClipAudioSource::pan() {
    return d->pan;
}

void ClipAudioSource::setPan(float pan) {
  if (d->pan != pan) {
    IF_DEBUG_CLIP cerr << "Setting pan : " << pan;
    d->pan = pan;
    Q_EMIT panChanged();
  }
}

float ClipAudioSource::adsrAttack() const
{
  return d->adsr.getParameters().attack;
}

void ClipAudioSource::setADSRAttack(const float& newValue)
{
  juce::ADSR::Parameters params = d->adsr.getParameters();
  if (params.attack != newValue) {
    params.attack = newValue;
    d->adsr.setParameters(params);
    Q_EMIT adsrParametersChanged();
  }
}

float ClipAudioSource::adsrDecay() const
{
  return d->adsr.getParameters().decay;
}

void ClipAudioSource::setADSRDecay(const float& newValue)
{
  juce::ADSR::Parameters params = d->adsr.getParameters();
  if (params.decay != newValue) {
    params.decay = newValue;
    d->adsr.setParameters(params);
    Q_EMIT adsrParametersChanged();
  }
}

float ClipAudioSource::adsrSustain() const
{
  return d->adsr.getParameters().sustain;
}

void ClipAudioSource::setADSRSustain(const float& newValue)
{
  juce::ADSR::Parameters params = d->adsr.getParameters();
  if (params.sustain != newValue) {
    params.sustain = newValue;
    d->adsr.setParameters(params);
    Q_EMIT adsrParametersChanged();
  }
}

float ClipAudioSource::adsrRelease() const
{
  return d->adsr.getParameters().release;
}

void ClipAudioSource::setADSRRelease(const float& newValue)
{
  juce::ADSR::Parameters params = d->adsr.getParameters();
  if (params.release != newValue) {
    params.release = newValue;
    d->adsr.setParameters(params);
    Q_EMIT adsrParametersChanged();
  }
}

void ClipAudioSource::setADSRParameters(const juce::ADSR::Parameters& parameters)
{
  d->adsr.setParameters(parameters);
  Q_EMIT adsrParametersChanged();
}

const juce::ADSR::Parameters & ClipAudioSource::adsrParameters() const
{
  return d->adsr.getParameters();
}

const juce::ADSR & ClipAudioSource::adsr() const
{
  return d->adsr;
}

bool ClipAudioSource::granular() const
{
  return d->granular;
}

void ClipAudioSource::setGranular(const bool& newValue)
{
  if (d->granular != newValue) {
    d->granular = newValue;
    Q_EMIT granularChanged();
  }
}

float ClipAudioSource::grainPosition() const
{
  return d->grainPosition;
}

void ClipAudioSource::setGrainPosition(const float& newValue)
{
  if (d->grainPosition != newValue) {
    d->grainPosition = newValue;
    Q_EMIT grainPositionChanged();
  }
}

float ClipAudioSource::grainSpray() const
{
  return d->grainSpray;
}

void ClipAudioSource::setGrainSpray(const float& newValue)
{
  if (d->grainSpray != newValue) {
    d->grainSpray = newValue;
    Q_EMIT grainSprayChanged();
  }
}

float ClipAudioSource::grainScan() const
{
  return d->grainScan;
}

void ClipAudioSource::setGrainScan(const float& newValue)
{
  if (d->grainScan != newValue) {
    d->grainScan = newValue;
    Q_EMIT grainScanChanged();
  }
}

float ClipAudioSource::grainInterval() const
{
  return d->grainInterval;
}

void ClipAudioSource::setGrainInterval(const float& newValue)
{
  const float adjustedValue{qMax(0.0f, newValue)};
  if (d->grainInterval != adjustedValue) {
    d->grainInterval = adjustedValue;
    Q_EMIT grainIntervalChanged();
  }
}

float ClipAudioSource::grainIntervalAdditional() const
{
  return d->grainIntervalAdditional;
}

void ClipAudioSource::setGrainIntervalAdditional(const float& newValue)
{
  const float adjustedValue{qMax(0.0f, newValue)};
  if (d->grainIntervalAdditional != adjustedValue) {
    d->grainIntervalAdditional = adjustedValue;
    Q_EMIT grainIntervalAdditionalChanged();
  }
}

float ClipAudioSource::grainSize() const
{
  return d->grainSize;
}

void ClipAudioSource::setGrainSize(const float& newValue)
{
  const float adjustedValue{qMax(1.0f, newValue)};
  if (d->grainSize != adjustedValue) {
    d->grainSize = adjustedValue;
    Q_EMIT grainSizeChanged();
  }
}

float ClipAudioSource::grainSizeAdditional() const
{
  return d->grainSizeAdditional;
}

void ClipAudioSource::setGrainSizeAdditional(const float& newValue)
{
  if (d->grainSizeAdditional != newValue) {
    d->grainSizeAdditional = newValue;
    Q_EMIT grainSizeAdditionalChanged();
  }
}

float ClipAudioSource::grainPanMinimum() const
{
  return d->grainPanMinimum;
}

void ClipAudioSource::setGrainPanMinimum(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -1.0f, 1.0f)};
  if (d->grainPanMinimum != adjustedValue) {
    d->grainPanMinimum = adjustedValue;
    Q_EMIT grainPanMinimumChanged();
    if (d->grainPanMaximum < adjustedValue) {
      d->grainPanMaximum = adjustedValue;
      Q_EMIT grainPanMaximumChanged();
    }
  }
}

float ClipAudioSource::grainPanMaximum() const
{
  return d->grainPanMaximum;
}

void ClipAudioSource::setGrainPanMaximum(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -1.0f, 1.0f)};
  if (d->grainPanMaximum != adjustedValue) {
    d->grainPanMaximum = adjustedValue;
    Q_EMIT grainPanMaximumChanged();
    if (d->grainPanMinimum > adjustedValue) {
      d->grainPanMinimum = adjustedValue;
      Q_EMIT grainPanMinimumChanged();
    }
  }
}

float ClipAudioSource::grainPitchMinimum1() const
{
  return d->grainPitchMinimum1;
}

void ClipAudioSource::setGrainPitchMinimum1(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
  if (d->grainPitchMinimum1 != adjustedValue) {
    d->grainPitchMinimum1 = adjustedValue;
    Q_EMIT grainPitchMinimum1Changed();
    if (d->grainPitchMaximum1 < adjustedValue) {
      d->grainPitchMaximum1 = adjustedValue;
      Q_EMIT grainPitchMaximum1Changed();
    }
  }
}

float ClipAudioSource::grainPitchMaximum1() const
{
  return d->grainPitchMaximum1;
}

void ClipAudioSource::setGrainPitchMaximum1(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
  if (d->grainPitchMaximum1 != adjustedValue) {
    d->grainPitchMaximum1 = adjustedValue;
    Q_EMIT grainPitchMaximum1Changed();
    if (d->grainPitchMinimum1 > adjustedValue) {
      d->grainPitchMinimum1 = adjustedValue;
      Q_EMIT grainPitchMinimum1Changed();
    }
  }
}

float ClipAudioSource::grainPitchMinimum2() const
{
  return d->grainPitchMinimum2;
}

void ClipAudioSource::setGrainPitchMinimum2(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
  if (d->grainPitchMinimum2 != adjustedValue) {
    d->grainPitchMinimum2 = adjustedValue;
    Q_EMIT grainPitchMinimum2Changed();
    if (d->grainPitchMaximum2 < adjustedValue) {
      d->grainPitchMaximum2 = adjustedValue;
      Q_EMIT grainPitchMaximum2Changed();
    }
  }
}

float ClipAudioSource::grainPitchMaximum2() const
{
  return d->grainPitchMaximum2;
}

void ClipAudioSource::setGrainPitchMaximum2(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
  if (d->grainPitchMaximum2 != adjustedValue) {
    d->grainPitchMaximum2 = adjustedValue;
    Q_EMIT grainPitchMaximum2Changed();
    if (d->grainPitchMinimum2 > adjustedValue) {
      d->grainPitchMinimum2 = adjustedValue;
      Q_EMIT grainPitchMinimum2Changed();
    }
  }
}

float ClipAudioSource::grainPitchPriority() const
{
  return d->grainPitchPriority;
}

void ClipAudioSource::setGrainPitchPriority(const float& newValue)
{
  const float adjustedValue{std::clamp(newValue, 0.0f, 1.0f)};
  if (d->grainPitchPriority != adjustedValue) {
    d->grainPitchPriority = adjustedValue;
    Q_EMIT grainPitchPriorityChanged();
  }
}

float ClipAudioSource::grainSustain() const
{
  return d->grainSustain;
}

void ClipAudioSource::setGrainSustain(const float& newValue)
{
  if (d->grainSustain != newValue) {
    d->grainSustain = newValue;
    Q_EMIT grainSustainChanged();
  }
}

float ClipAudioSource::grainTilt() const
{
  return d->grainTilt;
}

void ClipAudioSource::setGrainTilt(const float& newValue)
{
  if (d->grainTilt != newValue) {
    d->grainTilt = newValue;
    Q_EMIT grainTiltChanged();
  }
}

const juce::ADSR & ClipAudioSource::grainADSR() const
{
  return d->grainADSR;
}

bool ClipAudioSource::equaliserEnabled() const
{
    return d->equaliserEnabled;
}

void ClipAudioSource::setEqualiserEnabled(const bool& equaliserEnabled)
{
    if (d->equaliserEnabled != equaliserEnabled) {
        d->equaliserEnabled = equaliserEnabled;
        Q_EMIT equaliserEnabledChanged();
    }
}

QVariantList ClipAudioSource::equaliserSettings() const
{
    QVariantList settings;
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
        settings.append(QVariant::fromValue<QObject*>(filter));
    }
    return settings;
}

QObject * ClipAudioSource::equaliserNearestToFrequency(const float& frequency) const
{
    JackPassthroughFilter *nearest{nullptr};
    QMap<float, JackPassthroughFilter*> sortedFilters;
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
        sortedFilters.insert(filter->frequency(), filter);
    }
    QMap<float, JackPassthroughFilter*>::const_iterator filterIterator(sortedFilters.constBegin());
    float previousFrequency{0};
    JackPassthroughFilter *previousFilter{nullptr};
    while (filterIterator != sortedFilters.constEnd()) {
        float currentFrequency = filterIterator.key();
        nearest = filterIterator.value();
        if (frequency <= currentFrequency) {
            if (previousFilter) {
                // Between two filters, so test which one we're closer to. If it's nearest to the previous filter, reset nearest to that (otherwise it's already the nearest)
                float halfWayPoint{currentFrequency - ((currentFrequency - previousFrequency) / 2)};
                if (frequency < halfWayPoint) {
                    nearest = previousFilter;
                }
            }
            // We've found our filter, so stop looking :)
            break;
        } else {
            previousFrequency = currentFrequency;
            previousFilter = nearest;
        }
        ++filterIterator;
    }
    return nearest;
}

const std::vector<double> & ClipAudioSource::equaliserMagnitudes() const
{
    if (d->updateMagnitudes) {
        // Fill the magnitudes with a flat 1.0 of no change
        std::fill(d->equaliserMagnitudes.begin(), d->equaliserMagnitudes.end(), 1.0f);

        if (d->soloedFilter) {
            // If we've got a soloed band, only show that one
            juce::FloatVectorOperations::multiply(d->equaliserMagnitudes.data(), d->soloedFilter->magnitudes().data(), static_cast<int>(d->equaliserMagnitudes.size()));
        } else {
            for (size_t bandIndex = 0; bandIndex < equaliserBandCount; ++bandIndex) {
                if (d->equaliserSettings[bandIndex]->active()) {
                    juce::FloatVectorOperations::multiply(d->equaliserMagnitudes.data(), d->equaliserSettings[bandIndex]->magnitudes().data(), static_cast<int>(d->equaliserMagnitudes.size()));
                }
            }
        }
    }
    return d->equaliserMagnitudes;
}

const std::vector<double> & ClipAudioSource::equaliserFrequencies() const
{
    return d->equaliserFrequencies;
}

void ClipAudioSource::equaliserCreateFrequencyPlot(QPolygonF &p, const QRect bounds, float pixelsPerDouble)
{
    equaliserMagnitudes(); // Just make sure our magnitudes are updated
    const auto xFactor = static_cast<double>(bounds.width()) / d->equaliserFrequencies.size();
    for (size_t i = 0; i < d->equaliserFrequencies.size(); ++i) {
        p <<  QPointF(float (bounds.x() + i * xFactor), float(d->equaliserMagnitudes[i] > 0 ? bounds.center().y() - pixelsPerDouble * std::log(d->equaliserMagnitudes[i]) / std::log (2.0) : bounds.bottom()));
    }
}

void ClipAudioSource::setEqualiserInputAnalysers(QList<JackPassthroughAnalyser*>& equaliserInputAnalysers) const
{
    d->equaliserInputAnalysers = equaliserInputAnalysers;
}

const QList<JackPassthroughAnalyser *> & ClipAudioSource::equaliserInputAnalysers() const
{
  return d->equaliserInputAnalysers;
}

void ClipAudioSource::setEqualiserOutputAnalysers(QList<JackPassthroughAnalyser*>& equaliserOutputAnalysers) const
{
    d->equaliserOutputAnalysers = equaliserOutputAnalysers;
}

const QList<JackPassthroughAnalyser *> & ClipAudioSource::equaliserOutputAnalysers() const
{
  return d->equaliserOutputAnalysers;
}

bool ClipAudioSource::compressorEnabled() const
{
    return d->compressorEnabled;
}

void ClipAudioSource::setCompressorEnabled(const bool& compressorEnabled)
{
    if (d->compressorEnabled != compressorEnabled) {
        d->compressorEnabled = compressorEnabled;
        Q_EMIT compressorEnabledChanged();
    }
}

QString ClipAudioSource::compressorSidechannelLeft() const
{
    return d->compressorSidechannelLeft;
}

void ClipAudioSource::setCompressorSidechannelLeft(const QString& compressorSidechannelLeft)
{
    if (d->compressorSidechannelLeft != compressorSidechannelLeft) {
        d->compressorSidechannelLeft = compressorSidechannelLeft;
        Q_EMIT compressorSidechannelLeftChanged();
        // // First disconnect anything currently connected to the left sidechannel input port
        // jack_port_disconnect(d->client, d->sideChainInput[0]);
        // // Then connect up the new sidechain input
        // static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        // const QStringList portsToConnect{model->audioInSourceToJackPortNames(d->compressorSidechannelLeft, {})};
        // for (const QString &port : portsToConnect) {
        //     d->connectPorts(port, QString("%1:%2sidechainInputLeft").arg(d->actualClientName).arg(d->portPrefix));
        // }
        // d->compressorSidechannelEmpty[0] = portsToConnect.isEmpty();
        // TODO Do this on compressorSidechannelLeftChanged AND when first registering a clip (in case it's already been set up)
    }
}

QString ClipAudioSource::compressorSidechannelRight() const
{
    return d->compressorSidechannelRight;
}

void ClipAudioSource::setCompressorSidechannelRight(const QString& compressorSidechannelRight)
{
    if (d->compressorSidechannelRight != compressorSidechannelRight) {
        d->compressorSidechannelRight = compressorSidechannelRight;
        Q_EMIT compressorSidechannelRightChanged();
        // // First disconnect anything currently connected to the right sidechannel input port
        // jack_port_disconnect(d->client, d->sideChainInput[1]);
        // // Then connect up the new sidechain input
        // static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        // const QStringList portsToConnect{model->audioInSourceToJackPortNames(d->compressorSidechannelRight, {})};
        // for (const QString &port : portsToConnect) {
        //     d->connectPorts(port, QString("%1:%2sidechainInputRight").arg(d->actualClientName).arg(d->portPrefix));
        // }
        // d->compressorSidechannelEmpty[1] = portsToConnect.isEmpty();
        // TODO Do this on compressorSidechannelLeftChanged AND when first registering a clip (in case it's already been set up)
    }
}

void connectPorts(jack_client_t* client, const QString &from, const QString &to) {
  int result = jack_connect(client, from.toUtf8(), to.toUtf8());
  if (result == 0 || result == EEXIST) {
    // successful connection or connection already exists
    // qDebug() << Q_FUNC_INFO << "Successfully connected" << from << "to" << to << "(or connection already existed)";
  } else {
    qWarning() << Q_FUNC_INFO << "Failed to connect" << from << "with" << to << "with error code" << result;
    // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
  }
}

void ClipAudioSource::setSidechainPorts(jack_port_t* leftPort, jack_port_t* rightPort)
{
  d->sideChainInput[0] = leftPort;
  d->sideChainInput[1] = rightPort;
}

void ClipAudioSource::reconnectSidechainPorts(jack_client_t* jackClient)
{
  static float sampleRate{0};
  if (sampleRate == 0) {
    sampleRate = jack_get_sample_rate(jackClient);
    d->compressorSettings->setSampleRate(sampleRate);
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
      filter->setSampleRate(sampleRate);
    }
  }
  static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
  // First disconnect anything currently connected to the left sidechannel input port
  jack_port_disconnect(jackClient, d->sideChainInput[0]);
  // Then connect up the new sidechain input
  const QStringList leftPortsToConnect{model->audioInSourceToJackPortNames(d->compressorSidechannelLeft, {})};
  for (const QString &port : leftPortsToConnect) {
    connectPorts(jackClient, port, QString("SamplerSynth:Clip%1-SidechainInputLeft").arg(d->id));
  }
  d->compressorSidechannelEmpty[0] = leftPortsToConnect.isEmpty();
  // First disconnect anything currently connected to the right sidechannel input port
  jack_port_disconnect(jackClient, d->sideChainInput[1]);
  // Then connect up the new sidechain input
  const QStringList rightPortsToConnect{model->audioInSourceToJackPortNames(d->compressorSidechannelRight, {})};
  for (const QString &port : rightPortsToConnect) {
    connectPorts(jackClient, port, QString("SamplerSynth:Clip%1-SidechainInputRight").arg(d->id));
  }
  d->compressorSidechannelEmpty[1] = rightPortsToConnect.isEmpty();
}

QObject * ClipAudioSource::compressorSettings() const
{
    return d->compressorSettings;
}

void ClipAudioSource::finaliseProcess(jack_default_audio_sample_t** inputBuffers, jack_default_audio_sample_t** outputBuffers, size_t bufferLenth) const
{
  if (d->equaliserEnabled) {
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
      filter->updateCoefficients();
    }
    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
      juce::AudioBuffer<float> bufferWrapper(&inputBuffers[channelIndex], 1, int(bufferLenth));
      juce::dsp::AudioBlock<float> block(bufferWrapper);
      juce::dsp::ProcessContextReplacing<float> context(block);
      if (d->equaliserInputAnalysers[channelIndex]) {
        d->equaliserInputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
      }
      d->filterChain[channelIndex].process(context);
      if (d->equaliserOutputAnalysers[channelIndex]) {
        d->equaliserOutputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
      }
    }
  }
  if (d->compressorEnabled) {
    float sidechainPeaks[2]{0.0f, 0.0f};
    float outputPeaks[2]{0.0f, 0.0f};
    float maxGainReduction[2]{0.0f, 0.0f};
    d->compressorSettings->updateParameters();
    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
        // If we're not using a sidechannel for input, use what we're fed instead
        jack_default_audio_sample_t *sideChainInputBuffer = d->compressorSidechannelEmpty[channelIndex] || d->sideChainInput[channelIndex] == nullptr ? inputBuffers[channelIndex] : (jack_default_audio_sample_t *)jack_port_get_buffer(d->sideChainInput[channelIndex], bufferLenth);
        d->compressorSettings->compressors[channelIndex].getGainFromSidechainSignal(sideChainInputBuffer, d->sideChainGain[channelIndex], int(bufferLenth));
        juce::FloatVectorOperations::multiply(inputBuffers[channelIndex], d->sideChainGain[channelIndex], int(bufferLenth));
        // These three are essentially visualisation, so let's try and make sure we don't do the work unless someone's looking
        if (d->compressorSettings->hasObservers()) {
            sidechainPeaks[channelIndex] = juce::Decibels::decibelsToGain(d->compressorSettings->compressors[channelIndex].getMaxLevelInDecibels());
            maxGainReduction[channelIndex] = juce::Decibels::decibelsToGain(juce::Decibels::gainToDecibels(juce::FloatVectorOperations::findMinimum(d->sideChainGain[channelIndex], int(bufferLenth)) - d->compressorSettings->compressors[channelIndex].getMakeUpGain()));
            outputPeaks[channelIndex] = juce::AudioBuffer<float>(&inputBuffers[channelIndex], 1, int(bufferLenth)).getMagnitude(0, 0, int(bufferLenth));
        }
    }
    d->compressorSettings->updatePeaks(sidechainPeaks[0], sidechainPeaks[1], maxGainReduction[0], maxGainReduction[1], outputPeaks[0], outputPeaks[1]);
  } else if (d->compressorSettings) { // just to avoid doing any unnecessary hoop-jumping during construction
    d->compressorSettings->setPeaks(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  // static int throttler{0}; ++throttler; if (throttler > 200) { throttler = 0; };
  for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
    // if (throttler == 0) { qDebug() << Q_FUNC_INFO << "Max thing happening in the input buffer for clip on track/lane" << d->sketchpadTrack << d->laneAffinity << juce::AudioBuffer<float>(&inputBuffers[channelIndex], 1, int(bufferLenth)).getMagnitude(0, 0, int(bufferLenth)); }
    // if (throttler == 0) { qDebug() << Q_FUNC_INFO << "Max thing happening in the output buffer for clip on track/lane" << d->sketchpadTrack << d->laneAffinity << "before" << juce::AudioBuffer<float>(&outputBuffers[channelIndex], 1, int(bufferLenth)).getMagnitude(0, 0, int(bufferLenth)); }
    juce::FloatVectorOperations::add(outputBuffers[channelIndex], inputBuffers[channelIndex], bufferLenth);
    // if (throttler == 0) { qDebug() << Q_FUNC_INFO << "Max thing happening in the output buffer for clip on track/lane" << d->sketchpadTrack << d->laneAffinity << " after" << juce::AudioBuffer<float>(&outputBuffers[channelIndex], 1, int(bufferLenth)).getMagnitude(0, 0, int(bufferLenth)); }
  }
}
