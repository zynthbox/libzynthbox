/*
  ==============================================================================

    ClipAudioSource.cpp
    Created: 9 Aug 2021 7:44:30pm
    Author:  root

  ==============================================================================
*/

#include "ClipAudioSource.h"
#include "ClipAudioSourceSliceSettings.h"
#include "ClipAudioSourceSubvoiceSettings.h"
#include "ClipAudioSourcePositionsModel.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>

#include <unistd.h>

#include "JUCEHeaders.h"
#include "../tracktion_engine/examples/common/Utilities.h"
#include "Helper.h"
#include "ClipCommand.h"
#include "GainHandler.h"
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

// This gives us a nice, total chunk of 8 potential levels for any
// multi-sampled instrument with 128 recorded notes. It'll be a huge
// sample, but as a top-level potential, that seems reasonable
#define SLICE_COUNT 1024

#define equaliserBandCount 6

using namespace std;

class ClipAudioSource::Private : public juce::Timer {
public:
  Private(ClipAudioSource *qq) : q(qq) {
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
    // The slices are created in the CAS ctor, otherwise we're missing some important information that isn't there until the clip's been loaded
  }
  ClipAudioSource *q;
  const te::Engine &getEngine() const { return *engine; };

  te::Engine *engine{nullptr};
  SyncTimer *syncTimer;
  juce::File givenFile;
  tracktion_engine::AudioFile *audioFile{nullptr};
  juce::String chosenPath;
  juce::String fileName;
  QString filePath;
  bool autoSynchroniseSpeedRatio{false};
  float speedRatio = 1.0;
  float bpm{0};
  double sampleRate{0.0f};
  double currentLeveldB{-400.0};
  double prevLeveldB{-400.0};
  int id{0};
  float processingProgress{-1.0f};
  QString processingDescription;
  int sketchpadTrack{-1};
  int sketchpadSlot{0};
  bool registerForPolyphonicPlayback{true};
  int laneAffinity{0};
  ClipAudioSourcePositionsModel *positionsModel{nullptr};

  ClipAudioSourceSliceSettings *rootSlice{nullptr};
  int sliceCount{0};
  bool slicesContiguous{false};
  QVariantList sliceSettings;
  QList<ClipAudioSourceSliceSettings*> sliceSettingsActual;
  int selectedSlice{-1};
  SamplePickingStyle slicePickingStyle{ClipAudioSource::AllPickingStyle};

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

  bool shouldSyncProgress{false};
  void syncProgress() {
    shouldSyncProgress = false;
    if (nextPositionUpdateTime < QDateTime::currentMSecsSinceEpoch()) {
      double newPosition = rootSlice->startPositionSeconds() / q->getDuration();
      if (positionsModel && positionsModel->firstProgress() > -1.0f) {
        newPosition = positionsModel->firstProgress();
        if (isPlaying == false) {
          isPlaying = true;
          Q_EMIT q->isPlayingChanged();
        }
      } else {
        if (isPlaying == true) {
          isPlaying = false;
          Q_EMIT q->isPlayingChanged();
        }
      }
      if (abs(firstPositionProgress - newPosition) > 0.001) {
        firstPositionProgress = newPosition;
        progress = firstPositionProgress * q->getDuration();
        Q_EMIT q->positionChanged();
        Q_EMIT q->progressChanged();
        // This really wants to be 16, so we can get to 60 updates per second, but that tears to all heck without compositing, so... for now
        // (tested with higher rates, but it tears, so while it looks like an arbitrary number, afraid it's as high as we can go)
        // d->nextPositionUpdateTime = QDateTime::currentMSecsSinceEpoch() + 100; // 10 updates per second, this is loooow...
        // If it turns out this is a problem, we can reinstate the old code above, or perhaps do it on-demand...
        // (it will a problem be for rpi4 but is that a problem-problem if we're more properly rpi5, and it's really purely visual?)
        nextPositionUpdateTime = QDateTime::currentMSecsSinceEpoch() + 16;
      }
    }
  }
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
  }
private:
  void timerCallback() override {
    // Calling this from a timer will lead to a bad time, make sure it happens somewhere more reasonable (like on the object's own thread, which in this case is the qml engine thread)
    QMetaObject::invokeMethod(positionsModel, &ClipAudioSourcePositionsModel::updatePositions, Qt::QueuedConnection);
    syncAudioLevel();
    if (shouldSyncProgress) {
      syncProgress();
    }
  }
};

ClipAudioSource::ClipAudioSource(const char *filepath, const int &sketchpadTrack, const int &sketchpadSlot, const bool &registerForPolyphonicPlayback, bool muted, QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
  moveToThread(Plugin::instance()->qmlEngine()->thread());
  d->syncTimer = SyncTimer::instance();
  d->engine = Plugin::instance()->getTracktionEngine();
  d->id = Plugin::instance()->nextClipId();
  d->sketchpadTrack = sketchpadTrack;
  d->sketchpadSlot = sketchpadSlot;
  d->registerForPolyphonicPlayback = registerForPolyphonicPlayback;
  Plugin::instance()->addCreatedClipToMap(this);

  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Opening file:" << filepath;

  d->givenFile = juce::File(filepath);
  d->fileName = d->givenFile.getFileName();
  d->filePath = QString::fromUtf8(filepath);
  d->audioFile = new tracktion_engine::AudioFile(*d->engine, d->givenFile);
  d->sampleRate = d->audioFile->getSampleRate();

  // Slices
  d->rootSlice = new ClipAudioSourceSliceSettings(-1, this);
  d->rootSlice->setLengthSamples(d->audioFile->getLengthInSamples());

  if (muted) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Clip marked to be muted";
    d->rootSlice->gainHandlerActual()->setGainAbsolute(0.0f);
  }

  d->positionsModel = new ClipAudioSourcePositionsModel(this);
  d->positionsModel->moveToThread(Plugin::instance()->qmlEngine()->thread());
  // We don't connect to this, as we are already syncing explicitly in timerCallback
  // connect(d->positionsModel, &ClipAudioSourcePositionsModel::peakGainChanged, this, [&](){ d->syncAudioLevel(); });
  connect(d->positionsModel, &QAbstractItemModel::dataChanged, this, [&](const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/, const QVector< int >& /*roles = QVector<int>()*/){
    d->shouldSyncProgress = true;
  });
  SamplerSynth::instance()->registerClip(this);

  connect(d->syncTimer, &SyncTimer::bpmChanged, this, [this](){ d->updateBpmDependentValues(); } );
  connect(this, &ClipAudioSource::bpmChanged, this, [this](){ d->updateBpmDependentValues(); } );
  connect(this, &ClipAudioSource::autoSynchroniseSpeedRatioChanged, this, [this](){ d->updateBpmDependentValues(); } );

  // Make sure we do this last, so everything's actually done getting set up...
  d->startTimerHz(60);
}

ClipAudioSource::~ClipAudioSource() {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Destroying Clip";
  SamplerSynth::instance()->unregisterClip(this);
  Plugin::instance()->removeCreatedClipFromMap(this);
  Helper::callFunctionOnMessageThread(
    [&]() {
      d->stopTimer();
      delete d->audioFile;
    }, true);
}

float ClipAudioSource::guessBPM(int slice) const
{
  float guessedBPM{0.0f};
  // Set up our basic prerequisite knowledge
  const int numChannels{d->audioFile->getNumChannels()};
  const float sliceStartPosition{slice == -1 ? d->rootSlice->startPositionSeconds() : d->sliceSettingsActual.at(slice)->startPositionSeconds()};
  const float sliceStopPosition{slice == -1 ? d->rootSlice->stopPositionSeconds() : d->sliceSettingsActual.at(slice)->stopPositionSeconds()};
  int startSample = d->audioFile->getLengthInSamples() * sliceStartPosition;
  int lastSample = d->audioFile->getLengthInSamples() * sliceStopPosition;
  // Pull the samples we want out of the reader and stuff them into the bpm detector
  const int numSamples{numChannels * (lastSample - startSample)};
  juce::int64 numLeft{numSamples};
  const int blockSize{65536};
  const bool useRightChan{numChannels > 1};
  AudioFormatReader *reader{d->audioFile->getFormat()->createReaderFor(d->givenFile.createInputStream().release(), true)};
  tracktion_engine::soundtouch::BPMDetect bpmDetector(numChannels, d->audioFile->getSampleRate());
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
  return guessedBPM;
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

void ClipAudioSource::setSpeedRatio(float speedRatio, bool /*immediate*/) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Speed to" << speedRatio;
  d->speedRatio = speedRatio;
  Q_EMIT speedRatioChanged();
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

float ClipAudioSource::getDuration() const {
  return d->audioFile->getLength();
}

int ClipAudioSource::getDurationSamples() const
{
  return d->audioFile->getLengthInSamples();
}

const char *ClipAudioSource::getFileName() const {
  return static_cast<const char *>(d->fileName.toUTF8());
}

const double &ClipAudioSource::sampleRate() const
{
  return d->sampleRate;
}

const char *ClipAudioSource::getFilePath() const {
  return d->filePath.toUtf8();
}

tracktion_engine::AudioFile ClipAudioSource::getPlaybackFile() const {
  return tracktion_engine::AudioFile(*d->audioFile);
}

void ClipAudioSource::play(bool forceLooping, int midiChannel) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Starting clip " << this << d->filePath << " which is really " << d->audioFile << " in a " << (forceLooping ? "looping" : "non-looping") << " manner from " << d->rootSlice->startPositionSeconds() << " and for " << d->rootSlice->lengthSeconds() << " seconds";

  ClipCommand *command = ClipCommand::channelCommand(this, midiChannel);
  command->midiNote = 60;
  command->changeVolume = true;
  command->volume = 1.0f;
  command->changeLooping = true;
  if (forceLooping) {
    command->looping = true;
    command->stopPlayback = true; // this stops any current loop plays, and immediately starts a new one
  } else {
    command->looping = d->rootSlice->looping();
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

void ClipAudioSource::setProcessingProgress(const float& processingProgress)
{
  if (d->processingProgress != processingProgress) {
    d->processingProgress = processingProgress;
    Q_EMIT processingProgressChanged();
  }
}

void ClipAudioSource::startProcessing(const QString& description)
{
  setProcessingProgress(0.0);
  setProcessingDescription(description);
}

void ClipAudioSource::endProcessing()
{
  setProcessingProgress(-1.0f);
}

const float & ClipAudioSource::processingProgress() const
{
  return d->processingProgress;
}

void ClipAudioSource::setProcessingDescription(const QString& processingDescription)
{
  if (d->processingDescription != processingDescription) {
    d->processingDescription = processingDescription;
    Q_EMIT processingDescriptionChanged();
  }
}

const QString & ClipAudioSource::processingDescription() const
{
  return d->processingDescription;
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

int ClipAudioSource::sketchpadSlot() const
{
  return d->sketchpadSlot;
}

void ClipAudioSource::setSketchpadSlot(const int& newValue)
{
  const int adjusted{std::clamp(newValue, 0, 4)};
  if (d->sketchpadSlot != adjusted) {
    d->sketchpadSlot = adjusted;
    Q_EMIT sketchpadSlotChanged();
  }
}

bool ClipAudioSource::registerForPolyphonicPlayback() const
{
  return d->registerForPolyphonicPlayback;
}

int ClipAudioSource::laneAffinity() const
{
  return d->laneAffinity;
}

void ClipAudioSource::setLaneAffinity(const int& newValue)
{
  // Samples go into lanes 0 through 4, sketches go into lanes 5 through 9
  const int adjusted{std::clamp(newValue, 0, 9)};
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

QObject * ClipAudioSource::rootSlice() const
{
  return d->rootSlice;
}

ClipAudioSourceSliceSettings * ClipAudioSource::rootSliceActual() const
{
  return d->rootSlice;
}

int ClipAudioSource::sliceCountMaximum() const
{
  return SLICE_COUNT;
}

int ClipAudioSource::sliceCount() const
{
  return d->sliceCount;
}

void ClipAudioSource::setSliceCount(const int& sliceCount)
{
  if (d->sliceCount != sliceCount) {
    const int oldSliceCount{d->sliceCount};
    d->sliceCount = std::clamp(sliceCount, 0, SLICE_COUNT);
    if (oldSliceCount < d->sliceCount) {
      for (int sliceIndex = oldSliceCount; sliceIndex < d->sliceCount; ++sliceIndex) {
        ClipAudioSourceSliceSettings *newSlice = new ClipAudioSourceSliceSettings(sliceIndex, this);
        // A quick heuristic thing, so new slices are created with the basic root loop points...
        newSlice->setStartPositionSamples(d->rootSlice->startPositionSamples());
        newSlice->setLoopDeltaSamples(d->rootSlice->loopDeltaSamples());
        newSlice->setLoopDelta2Samples(d->rootSlice->loopDelta2Samples());
        newSlice->setLengthSamples(d->rootSlice->lengthSamples());
        d->sliceSettings << QVariant::fromValue<QObject*>(newSlice);
        d->sliceSettingsActual << newSlice;
      }
    }
    Q_EMIT sliceCountChanged();
    if (d->selectedSlice >= d->sliceCount) {
      setSelectedSlice(d->sliceCount - 1);
    }
  }
}

bool ClipAudioSource::slicesContiguous() const
{
  return d->slicesContiguous;
}

void ClipAudioSource::setSlicesContiguous(const bool& slicesContiguous)
{
  if (d->slicesContiguous != slicesContiguous) {
    d->slicesContiguous = slicesContiguous;
    Q_EMIT slicesContiguousChanged();
  }
}

QVariantList ClipAudioSource::sliceSettings() const
{
  return d->sliceSettings;
}

const QList<ClipAudioSourceSliceSettings *> & ClipAudioSource::sliceSettingsActual() const
{
  return d->sliceSettingsActual;
}

int ClipAudioSource::selectedSlice() const
{
  return d->selectedSlice;
}

void ClipAudioSource::setSelectedSlice(const int& selectedSlice)
{
  if (d->selectedSlice != selectedSlice) {
    d->selectedSlice = std::clamp(selectedSlice, -1, d->sliceCount - 1);
    Q_EMIT selectedSliceChanged();
  }
}

QObject * ClipAudioSource::selectedSliceObject() const
{
  if (d->selectedSlice == -1) {
    return d->rootSlice;
  }
  return d->sliceSettingsActual.at(d->selectedSlice);
}

void ClipAudioSource::insertSlice(const int& sliceIndex)
{
}

void ClipAudioSource::removeSlice(const int& sliceIndex)
{
  if (0 < d->sliceCount && -1 < sliceIndex && sliceIndex < SLICE_COUNT) {
    for (int currentSliceIndex = sliceIndex; currentSliceIndex < d->sliceCount && currentSliceIndex + 1 < SLICE_COUNT - 1; ++currentSliceIndex) {
      d->sliceSettingsActual.at(currentSliceIndex)->cloneFrom(d->sliceSettingsActual.at(currentSliceIndex + 1));
    }
    // Now everything's been moved down once, set the count to one less, and clear the newly hidden slice
    setSliceCount(d->sliceCount - 1);
    d->sliceSettingsActual.at(d->sliceCount)->clear();
  }
}

const QList<int> & ClipAudioSource::sliceIndicesForMidiNote(const int& midiNote) const
{
  
}

ClipAudioSourceSliceSettings * ClipAudioSource::sliceFromIndex(const int& sliceIndex) const
{
  if (-1 < sliceIndex && sliceIndex < SLICE_COUNT) {
    return d->sliceSettingsActual.at(sliceIndex);
  }
  return d->rootSlice;
}

QString ClipAudioSource::slicesToString() const
{
  static const QMetaEnum timeStretchEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("TimeStretchStyle"));
  static const QMetaEnum playbackStyleEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("PlaybackStyle"));
  static const QMetaEnum crossfadingDirectionEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("CrossfadingDirection"));
  QByteArray output;
  QDataStream stream(&output, QIODevice::WriteOnly);
  QVariantHash settingsHash;
  QVariantList slicesList;
  for (int sliceIndex = 0; sliceIndex < d->sliceCount; ++sliceIndex) {
    ClipAudioSourceSliceSettings *slice{d->sliceSettingsActual.at(sliceIndex)};
    QVariantHash sliceObject;
    sliceObject.insert("pan", slice->pan());
    sliceObject.insert("pitch", slice->pitch());
    sliceObject.insert("gain", slice->gainHandlerActual()->gainAbsolute());
    sliceObject.insert("rootNote", slice->rootNote());
    sliceObject.insert("keyZoneStart", slice->keyZoneStart());
    sliceObject.insert("keyZoneEnd", slice->keyZoneEnd());
    sliceObject.insert("velocityMinimum", slice->velocityMinimum());
    sliceObject.insert("velocityMaximum", slice->velocityMaximum());
    sliceObject.insert("adsrAttack", slice->adsrAttack());
    sliceObject.insert("adsrDecay", slice->adsrDecay());
    sliceObject.insert("adsrSustain", slice->adsrSustain());
    sliceObject.insert("adsrRelease", slice->adsrRelease());
    sliceObject.insert("grainInterval", slice->grainInterval());
    sliceObject.insert("grainIntervalAdditional", slice->grainIntervalAdditional());
    sliceObject.insert("grainPanMaximum", slice->grainPanMaximum());
    sliceObject.insert("grainPanMinimum", slice->grainPanMinimum());
    sliceObject.insert("grainPitchMaximum1", slice->grainPitchMaximum1());
    sliceObject.insert("grainPitchMaximum2", slice->grainPitchMaximum2());
    sliceObject.insert("grainPitchMinimum1", slice->grainPitchMinimum1());
    sliceObject.insert("grainPitchMinimum2", slice->grainPitchMinimum2());
    sliceObject.insert("grainPitchPriority", slice->grainPitchPriority());
    sliceObject.insert("grainPosition", slice->grainPosition());
    sliceObject.insert("grainScan", slice->grainScan());
    sliceObject.insert("grainSize", slice->grainSize());
    sliceObject.insert("grainSizeAdditional", slice->grainSizeAdditional());
    sliceObject.insert("grainSpray", slice->grainSpray());
    sliceObject.insert("grainSustain", slice->grainSustain());
    sliceObject.insert("grainTilt", slice->grainTilt());
    sliceObject.insert("timeStretchStyle", timeStretchEnum.valueToKey(slice->timeStretchStyle()));
    sliceObject.insert("playbackStyle", playbackStyleEnum.valueToKey(slice->playbackStyle()));
    sliceObject.insert("loopCrossfadeAmount", slice->loopCrossfadeAmount());
    sliceObject.insert("loopStartCrossfadeDirection", crossfadingDirectionEnum.valueToKey(slice->loopStartCrossfadeDirection()));
    sliceObject.insert("stopCrossfadeDirection", crossfadingDirectionEnum.valueToKey(slice->stopCrossfadeDirection()));
    sliceObject.insert("startPositionSamples", slice->startPositionSamples());
    sliceObject.insert("lengthSamples", slice->lengthSamples());
    sliceObject.insert("loopDeltaSamples", slice->loopDeltaSamples());
    sliceObject.insert("loopDelta2Samples", slice->loopDelta2Samples());
    sliceObject.insert("subvoiceCount", slice->subvoiceCount());
    QVariantList subvoicesList;
    for (int subvoiceIndex = 0; subvoiceIndex < slice->subvoiceCount(); ++subvoiceIndex) {
      ClipAudioSourceSubvoiceSettings *subvoice{slice->subvoiceSettingsActual().at(subvoiceIndex)};
      QVariantHash subvoiceObject;
      subvoiceObject.insert("pan", subvoice->pan());
      subvoiceObject.insert("pitch", subvoice->pitch());
      subvoiceObject.insert("gain", subvoice->gain());
      subvoicesList.append(subvoiceObject);
    }
    sliceObject.insert("subvoices", subvoicesList);
    slicesList.append(sliceObject);
  }
  settingsHash.insert("count", d->sliceCount);
  settingsHash.insert("contiguous", d->slicesContiguous);
  settingsHash.insert("settings", slicesList);
  stream << settingsHash;
  stream.device()->waitForBytesWritten(-1);
  return output.toBase64();
}

void ClipAudioSource::stringToSlices(const QString& data)
{
  // const int startMsecs = QDateTime::currentMSecsSinceEpoch();
  static const QMetaEnum timeStretchEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("TimeStretchStyle"));
  static const QMetaEnum playbackStyleEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("PlaybackStyle"));
  static const QMetaEnum crossfadingDirectionEnum = ClipAudioSourceSliceSettings::staticMetaObject.enumerator(ClipAudioSourceSliceSettings::staticMetaObject.indexOfEnumerator("CrossfadingDirection"));
  const QByteArray decoded{QByteArray::fromBase64(data.toUtf8())};
  QDataStream stream(decoded);
  QVariantHash settingsObject;
  stream >> settingsObject;
  // qDebug() << Q_FUNC_INFO << "took" << QDateTime::currentMSecsSinceEpoch() - startMsecs << "ms to parse the string, for" << d->filePath << "which is" << data.length() << "bytes";
  if (settingsObject.contains("settings")) {
    QVariantList slicesArray = settingsObject.value("settings").toList();
    // qDebug() << Q_FUNC_INFO << "Root is an object, we can work with this. It contains the keys" << settingsObject.keys() << "and how many objects are in it?" << slicesArray.count();
    setSliceCount(settingsObject.value("count", 0).toInt());
    for (int sliceIndex = 0; sliceIndex < qMin(d->sliceCount, d->sliceSettingsActual.count()); ++sliceIndex) {
      const QVariantHash sliceObject = slicesArray.at(sliceIndex).toHash();
      ClipAudioSourceSliceSettings *slice = d->sliceSettingsActual.at(sliceIndex);
      slice->setPan(sliceObject.value("pan", 0.0).toDouble());
      slice->setPitch(sliceObject.value("pitch", 0.0).toDouble());
      slice->gainHandlerActual()->setGainAbsolute(sliceObject.value("gain", slice->gainHandlerActual()->absoluteGainAtZeroDb()).toDouble());
      slice->setRootNote(sliceObject.value("rootNote", 60).toInt());
      slice->setKeyZoneStart(sliceObject.value("keyZoneStart",0).toInt());
      slice->setKeyZoneEnd(sliceObject.value("keyZoneEnd",127).toInt());
      slice->setVelocityMinimum(sliceObject.value("velocityMinimum", 0).toInt());
      slice->setVelocityMaximum(sliceObject.value("velocityMaximum", 127).toInt());
      slice->setADSRAttack(sliceObject.value("adsrAttack", 0.0).toDouble());
      slice->setADSRDecay(sliceObject.value("adsrDecay", 0.0).toDouble());
      slice->setADSRSustain(sliceObject.value("adsrSustain", 1.0).toDouble());
      slice->setADSRRelease(sliceObject.value("adsrRelease", 0.0).toDouble());
      slice->setGrainInterval(sliceObject.value("grainInterval", 10).toDouble());
      slice->setGrainIntervalAdditional(sliceObject.value("grainIntervalAdditional", 10).toDouble());
      slice->setGrainPanMaximum(sliceObject.value("grainPanMaximum", 1.0).toDouble());
      slice->setGrainPanMinimum(sliceObject.value("grainPanMinimum", -1.0).toDouble());
      slice->setGrainPitchMaximum1(sliceObject.value("grainPitchMaximum1", 1.0).toDouble());
      slice->setGrainPitchMaximum2(sliceObject.value("grainPitchMaximum2", 1.0).toDouble());
      slice->setGrainPitchMinimum1(sliceObject.value("grainPitchMinimum1", 1.0).toDouble());
      slice->setGrainPitchMinimum2(sliceObject.value("grainPitchMinimum2", 1.0).toDouble());
      slice->setGrainPitchPriority(sliceObject.value("grainPitchPriority", 0.5).toDouble());
      slice->setGrainPosition(sliceObject.value("grainPosition", 0.0).toDouble());
      slice->setGrainScan(sliceObject.value("grainScan", 0.0).toDouble());
      slice->setGrainSize(sliceObject.value("grainSize", 100).toDouble());
      slice->setGrainSizeAdditional(sliceObject.value("grainSizeAdditional", 50).toDouble());
      slice->setGrainSpray(sliceObject.value("grainSpray", 1.0).toDouble());
      slice->setGrainSustain(sliceObject.value("grainSustain", 0.3).toDouble());
      slice->setGrainTilt(sliceObject.value("grainTilt", 0.5f).toDouble());
      slice->setTimeStretchStyle(ClipAudioSource::TimeStretchStyle(timeStretchEnum.keyToValue(sliceObject.value("timeStretchStyle","TimeStretchOff").toString().toUtf8())));
      slice->setPlaybackStyle(ClipAudioSource::PlaybackStyle(playbackStyleEnum.keyToValue(sliceObject.value("playbackStyle", "NonLoopingPlaybackStyle").toString().toUtf8())));
      slice->setLoopCrossfadeAmount(sliceObject.value("loopCrossfadeAmount", 0.0).toDouble());
      slice->setLoopStartCrossfadeDirection(ClipAudioSource::CrossfadingDirection(crossfadingDirectionEnum.keyToValue(sliceObject.value("loopStartCrossfadeDirection", "CrossfadeOutie").toString().toUtf8())));
      slice->setStopCrossfadeDirection(ClipAudioSource::CrossfadingDirection(crossfadingDirectionEnum.keyToValue(sliceObject.value("stopCrossfadeDirection", "CrossfadeInnie").toString().toUtf8())));
      slice->setStartPositionSamples(sliceObject.value("startPositionSamples", 0.0).toDouble());
      slice->setLengthSamples(sliceObject.value("lengthSamples", getDurationSamples()).toDouble());
      slice->setLoopDeltaSamples(sliceObject.value("loopDeltaSamples", 0.0).toDouble());
      slice->setLoopDelta2Samples(sliceObject.value("loopDelta2Samples", 0.0).toDouble());
      slice->setSubvoiceCount(sliceObject.value("subvoiceCount", 0).toInt());
      const QVariantList subvoicesArray = sliceObject.value("subvoices").toList();
      for (int subvoiceIndex = 0; subvoiceIndex < slice->subvoiceCount(); ++subvoiceIndex) {
        const QVariantHash &subvoiceObject = subvoicesArray.at(subvoiceIndex).toHash();
        ClipAudioSourceSubvoiceSettings *subvoice = slice->subvoiceSettingsActual().at(subvoiceIndex);
        subvoice->setPan(subvoiceObject.value("pan", 0.0).toDouble());
        subvoice->setPitch(subvoiceObject.value("pitch", 0.0).toDouble());
        subvoice->setGain(subvoiceObject.value("gain", 1.0).toDouble());
      }
    }
    setSlicesContiguous(settingsObject.value("contiguous", false).toBool());
  // } else {
    // qDebug() << Q_FUNC_INFO << "No useful stored information, aborting restore";
  }
  // qDebug() << Q_FUNC_INFO << "took" << QDateTime::currentMSecsSinceEpoch() - startMsecs << "milliseconds for" << d->filePath;
}

ClipAudioSource::SamplePickingStyle ClipAudioSource::slicePickingStyle() const
{
  return d->slicePickingStyle;
}

void ClipAudioSource::setSlicePickingStyle(const ClipAudioSource::SamplePickingStyle& slicePickingStyle)
{
  if (d->slicePickingStyle != slicePickingStyle) {
    d->slicePickingStyle = slicePickingStyle;
    Q_EMIT slicePickingStyleChanged();
  }
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
