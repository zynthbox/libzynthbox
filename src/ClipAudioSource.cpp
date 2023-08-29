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
#include "SamplerSynth.h"
#include "SyncTimer.h"
#include "Plugin.h"

#define DEBUG_CLIP true
#define IF_DEBUG_CLIP if (DEBUG_CLIP)

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
  }
  ClipAudioSource *q;
  const te::Engine &getEngine() const { return *engine; };
  te::WaveAudioClip::Ptr getClip() {
    if (edit) {
      if (auto track = Helper::getOrInsertAudioTrackAt(*edit, 0)) {
        if (auto clip = dynamic_cast<te::WaveAudioClip *>(track->getClips()[0])) {
          return *clip;
        }
      }
    }

    return {};
  }

  te::Engine *engine{nullptr};
  std::unique_ptr<te::Edit> edit;
  bool isRendering{false};
  SyncTimer *syncTimer;
  juce::File givenFile;
  juce::String chosenPath;
  juce::String fileName;
  QString filePath;
  float startPositionInSeconds = 0;
  float lengthInSeconds = -1;
  float lengthInBeats = -1;
  bool looping{false};
  float loopDelta{0.0f};
  float gainDB{0.0f};
  float gain{1.0f};
  float gainAbsolute{0.5};
  float volumeAbsolute{-1.0f}; // This is a cached value
  float pitchChange = 0;
  float speedRatio = 1.0;
  float pan{0.0f};
  double sampleRate{0.0f};
  double currentLeveldB{-400.0};
  double prevLeveldB{-400.0};
  int id{0};
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

  qint64 nextPositionUpdateTime{0};
  double firstPositionProgress{0};
  qint64 nextGainUpdateTime{0};
  double progress{0};

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
  auto clip = Helper::loadAudioFileAsClip(*d->edit, d->givenFile);

  d->fileName = d->givenFile.getFileName();
  d->filePath = QString::fromUtf8(filepath);
  // Initially set the length in seconds to the full duration of the sample,
  // let the user set it to something else later on if they want to
  d->lengthInSeconds = d->edit->getLength();

  if (clip) {
    clip->setAutoTempo(false);
    clip->setAutoPitch(false);
    clip->setTimeStretchMode(te::TimeStretcher::defaultMode);
    d->sampleRate = clip->getAudioFile().getSampleRate();
    d->adsr.setSampleRate(d->sampleRate);
  }

  if (muted) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Clip marked to be muted";
    setVolume(-100.0f);
  }
  d->startTimerHz(30);

  d->positionsModel = new ClipAudioSourcePositionsModel(this);
  d->positionsModel->moveToThread(Plugin::instance()->qmlEngine()->thread());
  connect(d->positionsModel, &ClipAudioSourcePositionsModel::peakGainChanged, this, [&](){ d->syncAudioLevel(); });
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
    }
    if (abs(d->firstPositionProgress - newPosition) > 0.001) {
      d->firstPositionProgress = newPosition;
      d->progress = d->firstPositionProgress * getDuration();
      Q_EMIT positionChanged();
      Q_EMIT progressChanged();
      /// TODO This really wants to be 16, so we can get to 60 updates per second, but that tears to all heck without compositing, so... for now
      // (tested with higher rates, but it tears, so while it looks like an arbitrary number, afraid it's as high as we can go)
      d->nextPositionUpdateTime = QDateTime::currentMSecsSinceEpoch() + 100; // 10 updates per second, this is loooow...
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

void ClipAudioSource::setLoopDelta(const float& newLoopDelta)
{
  if (d->loopDelta != newLoopDelta) {
    d->loopDelta = newLoopDelta;
    Q_EMIT loopDeltaChanged();
  }
}

void ClipAudioSource::setStartPosition(float startPositionInSeconds) {
  d->startPositionInSeconds = jmax(0.0f, startPositionInSeconds);
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Start Position to" << d->startPositionInSeconds;
}

float ClipAudioSource::getStartPosition(int slice) const
{
    if (slice > -1 && slice < d->slicePositionsCache.length()) {
        return d->startPositionInSeconds + (d->lengthInSeconds * d->slicePositionsCache[slice]);
    } else {
        return d->startPositionInSeconds;
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

void ClipAudioSource::setPitch(float pitchChange, bool immediate) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Pitch to" << pitchChange;
  d->pitchChange = pitchChange;
  if (immediate) {
    if (auto clip = d->getClip()) {
      clip->setPitchChange(d->pitchChange);
    }
  } else {
    updateTempoAndPitch();
  }
  d->isRendering = true;
}

void ClipAudioSource::setSpeedRatio(float speedRatio, bool immediate) {
  IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Speed to" << speedRatio;
  d->speedRatio = speedRatio;
  if (immediate) {
    if (auto clip = d->getClip()) {
      clip->setSpeedRatio(d->speedRatio);
    }
  } else {
    updateTempoAndPitch();
  }
  d->isRendering = true;
}

void ClipAudioSource::setGain(float db) {
  if (auto clip = d->getClip()) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting gain:" << db;
    clip->setGainDB(db);
    d->gainDB = clip->getGainDB();
    d->gain = clip->getGain();
    setGainAbsolute(std::pow(2, (d->gainDB - 24) / 24));
  }
}

float ClipAudioSource::getGainDB() const
{
  return d->gainDB;
}

float ClipAudioSource::getGain() const
{
  return d->gain;
}

float ClipAudioSource::gainAbsolute() const
{
  return d->gainAbsolute;
}

// The logic here is that we want it to almost linearly assign dB values from 0 through 24,
// and also on the negative side, however we want it to also suddenly and precipitously fall
// off towards -100, while having 0.5 be 0dB. This allows us to do that.
void ClipAudioSource::setGainAbsolute(const float& gainAbsolute)
{
  const float adjusted{std::clamp(gainAbsolute, 0.0f, 1.0f)};
  if (abs(d->gainAbsolute - adjusted) > 0.0001) {
    d->gainAbsolute = adjusted;
    Q_EMIT gainAbsoluteChanged();
    setGain((std::log2(adjusted) * 24) + 24);
  }
}

void ClipAudioSource::setVolume(float vol) {
  if (auto clip = d->getClip()) {
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
  if (auto clip = d->getClip()) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting volume absolutely:" << vol;
    clip->edit.setMasterVolumeSliderPos(qMax(0.0f, qMin(vol, 1.0f)));
    d->volumeAbsolute = clip->edit.getMasterVolumePlugin()->getSliderPos();
    Q_EMIT volumeAbsoluteChanged();
  }
}

float ClipAudioSource::volumeAbsolute() const
{
  if (d->volumeAbsolute < 0) {
    if (auto clip = d->getClip()) {
      d->volumeAbsolute = clip->edit.getMasterVolumePlugin()->getSliderPos();
    }
  }
  return d->volumeAbsolute;
}

void ClipAudioSource::setLength(float beat, int bpm) {
  // IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Interval:" << d->syncTimer->getInterval(bpm);
  float lengthInSeconds = d->syncTimer->subbeatCountToSeconds(quint64(bpm), quint64((beat * d->syncTimer->getMultiplier())));
  // IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Setting Length to" << lengthInSeconds;
  d->lengthInSeconds = lengthInSeconds;
  d->lengthInBeats = beat;
}

float ClipAudioSource::getLengthInBeats() const {
  return d->lengthInBeats;
}

float ClipAudioSource::getDuration() const {
  return d->edit->getLength();
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
    if (const auto& clip = d->getClip()) {
        return clip->getPlaybackFile();
    }
    return te::AudioFile(*d->engine);
}

void ClipAudioSource::updateTempoAndPitch() {
  if (auto clip = d->getClip()) {
    IF_DEBUG_CLIP qDebug() << Q_FUNC_INFO << "Updating speedRatio(" << d->speedRatio << ") and pitch(" << d->pitchChange << ")";
    clip->setSpeedRatio(d->speedRatio);
    clip->setPitchChange(d->pitchChange);
  }
}

void ClipAudioSource::Private::timerCallback() {
  positionsModel->updatePositions();
  syncAudioLevel();

  if (auto clip = getClip()) {
    if (!clip->needsRender() && isRendering) {
        isRendering = false;
        Q_EMIT q->playbackFileChanged();
        adsr.setSampleRate(clip->getAudioFile().getSampleRate());
        adsr.setParameters(adsr.getParameters());
    }
  }
}

void ClipAudioSource::play(bool forceLooping, int midiChannel) {
  auto clip = d->getClip();
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
  if (d->keyZoneStart != keyZoneStart) {
    d->keyZoneStart = keyZoneStart;
    Q_EMIT keyZoneStartChanged();
  }
}

int ClipAudioSource::keyZoneEnd() const
{
  return d->keyZoneEnd;
}

void ClipAudioSource::setKeyZoneEnd(int keyZoneEnd)
{
  if (d->keyZoneEnd != keyZoneEnd) {
    d->keyZoneEnd = keyZoneEnd;
    Q_EMIT keyZoneEndChanged();
  }
}

int ClipAudioSource::rootNote() const
{
  return d->rootNote;
}

void ClipAudioSource::setRootNote(int rootNote)
{
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
