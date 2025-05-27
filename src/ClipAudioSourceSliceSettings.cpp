#include "ClipAudioSourceSliceSettings.h"

#include <QDebug>

#include "JUCEHeaders.h"
#include "ClipAudioSourceSubvoiceSettings.h"
#include "GainHandler.h"
#include "SyncTimer.h"

#define DEBUG_SLICE false
#define IF_DEBUG_SLICE if (DEBUG_SLICE)

class ClipAudioSourceSliceSettingsPrivate {
public:
    ClipAudioSourceSliceSettingsPrivate(ClipAudioSourceSliceSettings *q)
        : q(q)
    {
        gainHandler = new GainHandler(q);
        juce::ADSR::Parameters parameters = grainADSR.getParameters();
        parameters.attack = 0.01f;
        parameters.decay = 0.0f;
        parameters.sustain = 1.0f;
        parameters.release = 0.01f;
        grainADSR.setParameters(parameters);
        // Sub-voices
        for (int subvoiceIndex = 0; subvoiceIndex < 16; ++subvoiceIndex) {
            ClipAudioSourceSubvoiceSettings *newSubvoice = new ClipAudioSourceSubvoiceSettings(q);
            subvoiceSettings << QVariant::fromValue<QObject*>(newSubvoice);
            subvoiceSettingsActual << newSubvoice;
        }
    }
    ClipAudioSource *clip{nullptr};
    ClipAudioSourceSliceSettings *q{nullptr};
    int index{-1};

    float startPositionInSeconds{0};
    int startPositionInSamples{0};
    bool snapLengthToBeat{false};
    float lengthInSeconds{-1};
    int lengthInSamples{-1};
    float lengthInBeats{-1};
    double loopCrossfadeAmount{0.0};
    ClipAudioSource::CrossfadingDirection loopStartCrossfadeDirection{ClipAudioSource::CrossfadeOutie};
    ClipAudioSource::CrossfadingDirection stopCrossfadeDirection{ClipAudioSource::CrossfadeInnie};
    int loopFadeAdjustment{0};
    int stopFadeAdjustment{0};
    ClipAudioSource::PlaybackStyle playbackStyle{ClipAudioSource::NonLoopingPlaybackStyle};
    bool looping{false};
    float loopDelta{0.0f};
    int loopDeltaSamples{0};
    float loopDelta2{0.0f};
    int loopDelta2Samples{0};

    ClipAudioSource::TimeStretchStyle timeStretchStyle{ClipAudioSource::TimeStretchOff};
    float pitchChange = 0;
    float pitchChangePrecalc = 1.0f;

    GainHandler *gainHandler{nullptr};
    float pan{0.0f};

    int rootNote{60};
    int keyZoneStart{0};
    int keyZoneEnd{127};
    int velocityMinimum{1};
    int velocityMaximum{127};

    int exclusivityGroup{-1};

    // Subvoices (extra voices which are launched at the same time as the sound usually is, with a number of adjustments to some settings, specifically pan, pitch, and gain)
    bool inheritSubvoices{true};
    int subvoiceCount{0};
    QVariantList subvoiceSettings;
    QList<ClipAudioSourceSubvoiceSettings*> subvoiceSettingsActual;

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
    void updateCrossfadeAmounts() {
        QList<int> newLoopFadeAdjustment, newStopFadeAdjustment;
        const double loopStartPositionInSamples{double(startPositionInSamples + loopDelta)};
        const double loopStopPositionInSamples{double(q->stopPositionSamples())};
        const int fadeDurationSamples{int((loopStopPositionInSamples - loopStartPositionInSamples) * loopCrossfadeAmount)};
        if (loopStartCrossfadeDirection == ClipAudioSource::CrossfadeInnie) {
            loopFadeAdjustment = fadeDurationSamples;
        } else {
            loopFadeAdjustment = -fadeDurationSamples;
        }
        if (stopCrossfadeDirection == ClipAudioSource::CrossfadeInnie) {
            stopFadeAdjustment = -fadeDurationSamples;
        } else {
            stopFadeAdjustment = fadeDurationSamples;
        }
    }
    void setPlaybackStyleDependentsFromState(const ClipAudioSource::PlaybackStyle &playbackStyle) {
        switch (playbackStyle) {
        case ClipAudioSource::InheritPlaybackStyle:
            // Do nothing except be loud and angry, we should never hit this!
            qWarning() << Q_FUNC_INFO << "We have been asked to set the depends based on the inherited style - this should never happen!";
            break;
        case ClipAudioSource::LoopingPlaybackStyle:
            q->setLooping(true);
            q->setGranular(false);
            break;
        case ClipAudioSource::OneshotPlaybackStyle:
            q->setLooping(false);
            q->setGranular(false);
            break;
        case ClipAudioSource::GranularNonLoopingPlaybackStyle:
            q->setLooping(false);
            q->setGranular(true);
            break;
        case ClipAudioSource::GranularLoopingPlaybackStyle:
            q->setLooping(true);
            q->setGranular(true);
            break;
        case ClipAudioSource::WavetableStyle:
            // WavetableStyle is functionally the same as LoopingPlaybackStyle, but is informative to allow the UI
            // to do a bit of supportive work (in short: treat the length as a window size, lock the loop delta
            // to 0, and moving the start point as multiples of the window size)
            q->setLooping(true);
            q->setGranular(false);
            q->setLoopDeltaSamples(0);
            if (lengthInSamples > (clip->getDurationSamples() / 4)) {
                q->setLengthSamples(clip->getDurationSamples() / 32);
            }
            // TODO Maybe we should do something clever like make some reasonable assumptions here if length is unreasonably large for a wavetable and then lock the wavetable position to a multiple of that when switching? Or is that too destructive?
            break;
        case ClipAudioSource::NonLoopingPlaybackStyle:
        default:
            q->setLooping(false);
            q->setGranular(false);
            break;
        }
    }
    void updatePlaybackStyleDependents() {
        if (q != clip->rootSliceActual() && playbackStyle == ClipAudioSource::InheritPlaybackStyle) {
            setPlaybackStyleDependentsFromState(clip->rootSliceActual()->playbackStyle());
        } else {
            setPlaybackStyleDependentsFromState(playbackStyle);
        }
    }
};

ClipAudioSourceSliceSettings::ClipAudioSourceSliceSettings(const int& index, ClipAudioSource* parent)
    : QObject(parent)
    , d(new ClipAudioSourceSliceSettingsPrivate(this))
{
    d->clip = parent;
    d->index = index;

    d->adsr.setSampleRate(d->clip->sampleRate());
    d->adsr.setParameters({0.0f, 0.0f, 1.0f, 0.0f});

    // If this isn't the root slice, make sure we also update the playback style dependents (in case we're inheriting the style)
    if (d->clip->rootSliceActual() != nullptr && d->clip->rootSliceActual() != this) {
        connect(d->clip->rootSliceActual(), &ClipAudioSourceSliceSettings::playbackStyleChanged, this, [this](){ d->updatePlaybackStyleDependents(); });
    }
    connect(this, &ClipAudioSourceSliceSettings::playbackStyleChanged, this, [this](){ d->updatePlaybackStyleDependents(); });

    connect(this, &ClipAudioSourceSliceSettings::grainSizeChanged, this, [this]() { d->updateGrainADSR(); });
    connect(this, &ClipAudioSourceSliceSettings::grainSustainChanged, this, [this]() { d->updateGrainADSR(); });
    connect(this, &ClipAudioSourceSliceSettings::grainTiltChanged, this, [this]() { d->updateGrainADSR(); });

    connect(this, &ClipAudioSourceSliceSettings::loopCrossfadeAmountChanged, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::loopStartCrossfadeDirectionChanged, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::stopCrossfadeDirectionChanged, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::startPositionChanged, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::loopDeltaChanged, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::loopDelta2Changed, this, [this](){ d->updateCrossfadeAmounts(); });
    connect(this, &ClipAudioSourceSliceSettings::lengthChanged, this, [this](){ d->updateCrossfadeAmounts(); });
}

ClipAudioSourceSliceSettings::~ClipAudioSourceSliceSettings() {
    delete d;
}

void ClipAudioSourceSliceSettings::cloneFrom(const ClipAudioSourceSliceSettings *other)
{
}

void ClipAudioSourceSliceSettings::clear()
{
}

int ClipAudioSourceSliceSettings::index() const
{
    return d->index;
}

bool ClipAudioSourceSliceSettings::isRootSlice() const
{
    return d->index == -1;
}

ClipAudioSource::PlaybackStyle ClipAudioSourceSliceSettings::playbackStyle() const
{
    return d->playbackStyle;
}

ClipAudioSource::PlaybackStyle ClipAudioSourceSliceSettings::effectivePlaybackStyle() const
{
    if (d->playbackStyle == ClipAudioSource::InheritPlaybackStyle) {
        d->clip->rootSliceActual()->playbackStyle();
    }
    return d->playbackStyle;
}

QString ClipAudioSourceSliceSettings::playbackStyleLabel() const
{
    switch (d->playbackStyle) {
        case ClipAudioSource::InheritPlaybackStyle: {
            static const QLatin1String label{"Inherit"};
            return label;
            break; }
        case ClipAudioSource::LoopingPlaybackStyle: {
            static const QLatin1String label{"Looping"};
            return label;
            break; }
        case ClipAudioSource::OneshotPlaybackStyle: {
            static const QLatin1String label{"One-shot"};
            return label;
            break; }
        case ClipAudioSource::GranularNonLoopingPlaybackStyle: {
            static const QLatin1String label{"Granular Non-looping"};
            return label;
            break; }
        case ClipAudioSource::GranularLoopingPlaybackStyle: {
            static const QLatin1String label{"Granular Looping"};
            return label;
            break; }
        case ClipAudioSource::WavetableStyle: {
            static const QLatin1String label{"Wavetable"};
            return label;
            break; }
        case ClipAudioSource::NonLoopingPlaybackStyle:
        default: {
            static const QLatin1String label{"Non-looping"};
            return label;
            break; }
    }
}

void ClipAudioSourceSliceSettings::setPlaybackStyle(const ClipAudioSource::PlaybackStyle& playbackStyle)
{
    if (d->playbackStyle != playbackStyle) {
        d->playbackStyle = playbackStyle;
        Q_EMIT playbackStyleChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setLooping(bool looping) {
    if (d->looping != looping) {
        d->looping = looping;
        Q_EMIT loopingChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

bool ClipAudioSourceSliceSettings::looping() const
{
    return d->looping;
}

float ClipAudioSourceSliceSettings::loopDeltaSeconds() const
{
    return d->loopDelta;
}

int ClipAudioSourceSliceSettings::loopDeltaSamples() const
{
    return d->loopDeltaSamples;
}

void ClipAudioSourceSliceSettings::setLoopDeltaSeconds(const float& newLoopDelta)
{
    if (d->loopDelta != newLoopDelta) {
        d->loopDelta = newLoopDelta;
        d->loopDeltaSamples = newLoopDelta * d->clip->sampleRate();
        Q_EMIT loopDeltaChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setLoopDeltaSamples(const int& newLoopDeltaSamples)
{
    if (d->loopDeltaSamples != newLoopDeltaSamples) {
        d->loopDeltaSamples = newLoopDeltaSamples;
        d->loopDelta = double(newLoopDeltaSamples) / d->clip->sampleRate();
        Q_EMIT loopDeltaChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::loopDelta2Seconds() const
{
    return d->loopDelta2;
}

int ClipAudioSourceSliceSettings::loopDelta2Samples() const
{
    return d->loopDelta2Samples;
}

void ClipAudioSourceSliceSettings::setLoopDelta2Seconds(const float& newLoopDelta2)
{
    if (d->loopDelta2 != newLoopDelta2) {
        d->loopDelta2 = newLoopDelta2;
        d->loopDelta2Samples = newLoopDelta2 * d->clip->sampleRate();
        Q_EMIT loopDelta2Changed();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setLoopDelta2Samples(const int& newLoopDelta2Samples)
{
    if (d->loopDelta2Samples != newLoopDelta2Samples) {
        d->loopDelta2Samples = newLoopDelta2Samples;
        d->loopDelta2 = double(newLoopDelta2Samples) / d->clip->sampleRate();
        Q_EMIT loopDelta2Changed();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setStartPositionSeconds(const float &startPositionInSeconds) {
    setStartPositionSamples(startPositionInSeconds * d->clip->sampleRate());
}

void ClipAudioSourceSliceSettings::setStartPositionSamples(const int &startPositionInSamples)
{
    if (d->startPositionInSamples != startPositionInSamples) {
        d->startPositionInSamples = jmax(0, startPositionInSamples);
        d->startPositionInSeconds = double(startPositionInSamples) / d->clip->sampleRate();
        Q_EMIT startPositionChanged();
        Q_EMIT d->clip->sliceDataChanged();
        IF_DEBUG_SLICE qDebug() << Q_FUNC_INFO << "Setting Start Position to" << d->startPositionInSeconds << "seconds, meaning" << d->startPositionInSamples << "samples of" << d->clip->getDurationSamples();
    }
}

float ClipAudioSourceSliceSettings::startPositionSeconds() const
{
    return d->startPositionInSeconds;
}

int ClipAudioSourceSliceSettings::startPositionSamples() const
{
    return d->startPositionInSamples;
}

float ClipAudioSourceSliceSettings::stopPositionSeconds() const
{
    return d->startPositionInSeconds + d->lengthInSeconds;
}

int ClipAudioSourceSliceSettings::stopPositionSamples() const
{
    return d->startPositionInSamples + d->lengthInSamples;
}

void ClipAudioSourceSliceSettings::setTimeStretchStyle(const ClipAudioSource::TimeStretchStyle &timeStretchStyle)
{
    if (d->timeStretchStyle != timeStretchStyle) {
        d->timeStretchStyle = timeStretchStyle;
        Q_EMIT timeStretchStyleChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

ClipAudioSource::TimeStretchStyle ClipAudioSourceSliceSettings::timeStretchStyle() const
{
    return d->timeStretchStyle;
}

void ClipAudioSourceSliceSettings::setPitch(const float &pitchChange) {
    IF_DEBUG_SLICE qDebug() << Q_FUNC_INFO << "Setting Pitch to" << pitchChange;
    d->pitchChange = pitchChange;
    d->pitchChangePrecalc = std::pow(2.0, d->pitchChange / 12.0) /* * sampleRate() / sampleRate() */; // should this perhaps be a sound sample rate over playback sample rate thing?
    Q_EMIT pitchChanged();
    Q_EMIT d->clip->sliceDataChanged();
}

float ClipAudioSourceSliceSettings::pitch() const
{
    return d->pitchChange;
}

float ClipAudioSourceSliceSettings::pitchChangePrecalc() const
{
    return d->pitchChangePrecalc;
}

QObject * ClipAudioSourceSliceSettings::gainHandler() const
{
    return d->gainHandler;
}

GainHandler * ClipAudioSourceSliceSettings::gainHandlerActual() const
{
    return d->gainHandler;
}

void ClipAudioSourceSliceSettings::setSnapLengthToBeat(const bool& snapLengthToBeat)
{
    if (d->snapLengthToBeat != snapLengthToBeat) {
        d->snapLengthToBeat = snapLengthToBeat;
        Q_EMIT snapLengthToBeatChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

bool ClipAudioSourceSliceSettings::snapLengthToBeat() const
{
    return d->snapLengthToBeat;
}

void ClipAudioSourceSliceSettings::setLengthBeats(const float &beat) {
    // IF_DEBUG_SLICE qDebug() << Q_FUNC_INFO << "Interval:" << SyncTimer::instance()->getInterval(bpm);
    float lengthInSeconds = SyncTimer::instance()->subbeatCountToSeconds(quint64(d->clip->bpm() == 0 ? SyncTimer::instance()->getBpm() : d->clip->bpm()), quint64((beat * SyncTimer::instance()->getMultiplier())));
    // qDebug() << Q_FUNC_INFO << beat << d->bpm << SyncTimer::instance()->getBpm() << lengthInSeconds << d->sampleRate << lengthInSeconds * d->sampleRate;
    if (lengthInSeconds != d->lengthInSeconds) {
        // IF_DEBUG_SLICE qDebug() << Q_FUNC_INFO << "Setting Length to" << lengthInSeconds;
        d->lengthInSeconds = lengthInSeconds;
        d->lengthInSamples = lengthInSeconds * d->clip->sampleRate();
        d->lengthInBeats = beat;
        Q_EMIT lengthChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setLengthSamples(const int &lengthInSamples)
{
    if (d->lengthInSamples != lengthInSamples) {
        d->lengthInSamples = lengthInSamples;
        d->lengthInSeconds = double(lengthInSamples) / d->clip->sampleRate();
        d->lengthInBeats = double(SyncTimer::instance()->secondsToSubbeatCount(d->clip->bpm() == 0 ? SyncTimer::instance()->getBpm() : d->clip->bpm(), d->lengthInSeconds)) / double(SyncTimer::instance()->getMultiplier());
        Q_EMIT lengthChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::lengthBeats() const {
    return d->lengthInBeats;
}

int ClipAudioSourceSliceSettings::lengthSamples() const
{
    return d->lengthInSamples;
}

float ClipAudioSourceSliceSettings::lengthSeconds() const
{
    return double(d->lengthInSamples) / d->clip->sampleRate();
}

double ClipAudioSourceSliceSettings::loopCrossfadeAmount() const
{
  return d->loopCrossfadeAmount;
}

void ClipAudioSourceSliceSettings::setLoopCrossfadeAmount(const double& loopCrossfadeAmount)
{
  if (d->loopCrossfadeAmount != loopCrossfadeAmount) {
    d->loopCrossfadeAmount = std::clamp(loopCrossfadeAmount, 0.0, 0.5);
    Q_EMIT loopCrossfadeAmountChanged();
    Q_EMIT d->clip->sliceDataChanged();
  }
}

ClipAudioSource::CrossfadingDirection ClipAudioSourceSliceSettings::loopStartCrossfadeDirection() const
{
  return d->loopStartCrossfadeDirection;
}

void ClipAudioSourceSliceSettings::setLoopStartCrossfadeDirection(const ClipAudioSource::CrossfadingDirection& loopStartCrossfadeDirection)
{
  if (d->loopStartCrossfadeDirection != loopStartCrossfadeDirection) {
    d->loopStartCrossfadeDirection = loopStartCrossfadeDirection;
    Q_EMIT loopStartCrossfadeDirectionChanged();
    Q_EMIT d->clip->sliceDataChanged();
  }
}

ClipAudioSource::CrossfadingDirection ClipAudioSourceSliceSettings::stopCrossfadeDirection() const
{
  return d->stopCrossfadeDirection;
}

void ClipAudioSourceSliceSettings::setStopCrossfadeDirection(const ClipAudioSource::CrossfadingDirection& stopCrossfadeDirection)
{
  if (d->stopCrossfadeDirection != stopCrossfadeDirection) {
    d->stopCrossfadeDirection = stopCrossfadeDirection;
    Q_EMIT stopCrossfadeDirectionChanged();
    Q_EMIT d->clip->sliceDataChanged();
  }
}

int ClipAudioSourceSliceSettings::loopFadeAdjustment() const
{
  return d->loopFadeAdjustment;
}

int ClipAudioSourceSliceSettings::stopFadeAdjustment() const
{
  return d->stopFadeAdjustment;
}

float ClipAudioSourceSliceSettings::pan() const
{
    return d->pan;
}

void ClipAudioSourceSliceSettings::setPan(const float &pan)
{
    if (d->pan != pan) {
        IF_DEBUG_SLICE cerr << "Setting pan : " << pan;
        d->pan = pan;
        Q_EMIT panChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

int ClipAudioSourceSliceSettings::rootNote() const
{
    if (d->rootNote == -1 && d->index != -1) {
        d->clip->rootSliceActual()->rootNote();
    }
    return d->rootNote;
}

void ClipAudioSourceSliceSettings::setRootNote(const int &rootNote)
{
    if (d->rootNote != rootNote) {
        if (d->index == -1) {
            d->rootNote = std::clamp(rootNote, 0, 127);
        } else {
            d->rootNote = std::clamp(rootNote, -1, 127);
        }
        Q_EMIT rootNoteChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

int ClipAudioSourceSliceSettings::keyZoneStart() const
{
    return d->keyZoneStart;
}

void ClipAudioSourceSliceSettings::setKeyZoneStart(const int &keyZoneStart)
{
    if (d->keyZoneStart != keyZoneStart) {
        d->keyZoneStart = std::clamp(keyZoneStart, -1, 127);
        Q_EMIT keyZoneStartChanged();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->keyZoneEnd < d->keyZoneStart) {
            setKeyZoneEnd(d->keyZoneStart);
        }
    }
}

int ClipAudioSourceSliceSettings::keyZoneEnd() const
{
    return d->keyZoneEnd;
}

void ClipAudioSourceSliceSettings::setKeyZoneEnd(const int &keyZoneEnd)
{
  if (d->keyZoneEnd != keyZoneEnd) {
    d->keyZoneEnd = std::clamp(keyZoneEnd, -1, 127);
    Q_EMIT keyZoneEndChanged();
    Q_EMIT d->clip->sliceDataChanged();
    if (d->keyZoneStart > d->keyZoneEnd) {
      setKeyZoneStart(d->keyZoneEnd);
    }
  }
}

int ClipAudioSourceSliceSettings::velocityMinimum() const
{
    return d->velocityMinimum;
}

void ClipAudioSourceSliceSettings::setVelocityMinimum(const int& velocityMinimum)
{
    if (d->velocityMinimum != velocityMinimum) {
        d->velocityMinimum = std::clamp(velocityMinimum, 1, 127);
        Q_EMIT velocityMinimumChanged();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->velocityMinimum > d->velocityMaximum) {
            setVelocityMaximum(d->velocityMinimum);
        }
    }
}

int ClipAudioSourceSliceSettings::velocityMaximum() const
{
    return d->velocityMaximum;
}

void ClipAudioSourceSliceSettings::setVelocityMaximum(const int& velocityMaximum)
{
    if (d->velocityMaximum != velocityMaximum) {
        d->velocityMaximum = std::clamp(velocityMaximum, 1, 127);
        Q_EMIT velocityMaximumChanged();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->velocityMinimum > d->velocityMaximum) {
            setVelocityMinimum(d->velocityMaximum);
        }
    }
}

int ClipAudioSourceSliceSettings::exclusivityGroup() const
{
    return d->exclusivityGroup;
}

void ClipAudioSourceSliceSettings::setExclusivityGroup(const int& exclusivityGroup)
{
    if (d->exclusivityGroup != exclusivityGroup) {
        d->exclusivityGroup = std::clamp(exclusivityGroup, -1, 1024);
        Q_EMIT exclusivityGroupChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

bool ClipAudioSourceSliceSettings::inheritSubvoices() const
{
    return d->inheritSubvoices;
}

void ClipAudioSourceSliceSettings::setInheritSubvoices(const bool& inheritSubvoices)
{
    if (d->inheritSubvoices != inheritSubvoices) {
        d->inheritSubvoices = inheritSubvoices;
        Q_EMIT inheritSubvoicesChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

int ClipAudioSourceSliceSettings::subvoiceCount() const
{
  return d->subvoiceCount;
}

void ClipAudioSourceSliceSettings::setSubvoiceCount(const int& subvoiceCount)
{
  if (d->subvoiceCount != subvoiceCount) {
    d->subvoiceCount = subvoiceCount;
    Q_EMIT subvoiceCountChanged();
    Q_EMIT d->clip->sliceDataChanged();
  }
}

QVariantList ClipAudioSourceSliceSettings::subvoiceSettings() const
{
  return d->subvoiceSettings;
}

const QList<ClipAudioSourceSubvoiceSettings*> & ClipAudioSourceSliceSettings::subvoiceSettingsActual() const
{
  return d->subvoiceSettingsActual;
}

int ClipAudioSourceSliceSettings::subvoiceCountPlayback() const
{
    if (d->inheritSubvoices) {
        return d->clip->rootSliceActual()->subvoiceCount();
    }
    return d->subvoiceCount;
}

const QList<ClipAudioSourceSubvoiceSettings *> & ClipAudioSourceSliceSettings::subvoiceSettingsPlayback() const
{
    if (d->inheritSubvoices) {
        return d->clip->rootSliceActual()->subvoiceSettingsActual();
    }
    return d->subvoiceSettingsActual;
}

float ClipAudioSourceSliceSettings::adsrAttack() const
{
    return d->adsr.getParameters().attack;
}

void ClipAudioSourceSliceSettings::setADSRAttack(const float& newValue)
{
    juce::ADSR::Parameters params = d->adsr.getParameters();
    if (params.attack != newValue) {
        params.attack = newValue;
        d->adsr.setParameters(params);
        Q_EMIT adsrParametersChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::adsrDecay() const
{
    return d->adsr.getParameters().decay;
}

void ClipAudioSourceSliceSettings::setADSRDecay(const float& newValue)
{
    juce::ADSR::Parameters params = d->adsr.getParameters();
    if (params.decay != newValue) {
        params.decay = newValue;
        d->adsr.setParameters(params);
        Q_EMIT adsrParametersChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::adsrSustain() const
{
    return d->adsr.getParameters().sustain;
}

void ClipAudioSourceSliceSettings::setADSRSustain(const float& newValue)
{
    juce::ADSR::Parameters params = d->adsr.getParameters();
    if (params.sustain != newValue) {
        params.sustain = newValue;
        d->adsr.setParameters(params);
        Q_EMIT adsrParametersChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::adsrRelease() const
{
    return d->adsr.getParameters().release;
}

void ClipAudioSourceSliceSettings::setADSRRelease(const float& newValue)
{
    juce::ADSR::Parameters params = d->adsr.getParameters();
    if (params.release != newValue) {
        params.release = newValue;
        d->adsr.setParameters(params);
        Q_EMIT adsrParametersChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

void ClipAudioSourceSliceSettings::setADSRParameters(const juce::ADSR::Parameters& parameters)
{
    d->adsr.setParameters(parameters);
    Q_EMIT adsrParametersChanged();
    Q_EMIT d->clip->sliceDataChanged();
}

const juce::ADSR::Parameters & ClipAudioSourceSliceSettings::adsrParameters() const
{
    return d->adsr.getParameters();
}

const juce::ADSR & ClipAudioSourceSliceSettings::adsr() const
{
    return d->adsr;
}

bool ClipAudioSourceSliceSettings::granular() const
{
    return d->granular;
}

void ClipAudioSourceSliceSettings::setGranular(const bool& newValue)
{
    if (d->granular != newValue) {
        d->granular = newValue;
        Q_EMIT granularChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainPosition() const
{
    return d->grainPosition;
}

void ClipAudioSourceSliceSettings::setGrainPosition(const float& newValue)
{
    if (d->grainPosition != newValue) {
        d->grainPosition = newValue;
        Q_EMIT grainPositionChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainSpray() const
{
    return d->grainSpray;
}

void ClipAudioSourceSliceSettings::setGrainSpray(const float& newValue)
{
    if (d->grainSpray != newValue) {
        d->grainSpray = newValue;
        Q_EMIT grainSprayChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainScan() const
{
    return d->grainScan;
}

void ClipAudioSourceSliceSettings::setGrainScan(const float& newValue)
{
    if (d->grainScan != newValue) {
        d->grainScan = newValue;
        Q_EMIT grainScanChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainInterval() const
{
    return d->grainInterval;
}

void ClipAudioSourceSliceSettings::setGrainInterval(const float& newValue)
{
    const float adjustedValue{qMax(0.0f, newValue)};
    if (d->grainInterval != adjustedValue) {
        d->grainInterval = adjustedValue;
        Q_EMIT grainIntervalChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainIntervalAdditional() const
{
    return d->grainIntervalAdditional;
}

void ClipAudioSourceSliceSettings::setGrainIntervalAdditional(const float& newValue)
{
    const float adjustedValue{qMax(0.0f, newValue)};
    if (d->grainIntervalAdditional != adjustedValue) {
        d->grainIntervalAdditional = adjustedValue;
        Q_EMIT grainIntervalAdditionalChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainSize() const
{
    return d->grainSize;
}

void ClipAudioSourceSliceSettings::setGrainSize(const float& newValue)
{
    const float adjustedValue{qMax(1.0f, newValue)};
    if (d->grainSize != adjustedValue) {
        d->grainSize = adjustedValue;
        Q_EMIT grainSizeChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainSizeAdditional() const
{
    return d->grainSizeAdditional;
}

void ClipAudioSourceSliceSettings::setGrainSizeAdditional(const float& newValue)
{
    if (d->grainSizeAdditional != newValue) {
        d->grainSizeAdditional = newValue;
        Q_EMIT grainSizeAdditionalChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainPanMinimum() const
{
    return d->grainPanMinimum;
}

void ClipAudioSourceSliceSettings::setGrainPanMinimum(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -1.0f, 1.0f)};
    if (d->grainPanMinimum != adjustedValue) {
        d->grainPanMinimum = adjustedValue;
        Q_EMIT grainPanMinimumChanged();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPanMaximum < adjustedValue) {
            d->grainPanMaximum = adjustedValue;
            Q_EMIT grainPanMaximumChanged();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPanMaximum() const
{
    return d->grainPanMaximum;
}

void ClipAudioSourceSliceSettings::setGrainPanMaximum(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -1.0f, 1.0f)};
    if (d->grainPanMaximum != adjustedValue) {
        d->grainPanMaximum = adjustedValue;
        Q_EMIT grainPanMaximumChanged();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPanMinimum > adjustedValue) {
            d->grainPanMinimum = adjustedValue;
            Q_EMIT grainPanMinimumChanged();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPitchMinimum1() const
{
    return d->grainPitchMinimum1;
}

void ClipAudioSourceSliceSettings::setGrainPitchMinimum1(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
    if (d->grainPitchMinimum1 != adjustedValue) {
        d->grainPitchMinimum1 = adjustedValue;
        Q_EMIT grainPitchMinimum1Changed();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPitchMaximum1 < adjustedValue) {
            d->grainPitchMaximum1 = adjustedValue;
            Q_EMIT grainPitchMaximum1Changed();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPitchMaximum1() const
{
    return d->grainPitchMaximum1;
}

void ClipAudioSourceSliceSettings::setGrainPitchMaximum1(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
    if (d->grainPitchMaximum1 != adjustedValue) {
        d->grainPitchMaximum1 = adjustedValue;
        Q_EMIT grainPitchMaximum1Changed();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPitchMinimum1 > adjustedValue) {
            d->grainPitchMinimum1 = adjustedValue;
            Q_EMIT grainPitchMinimum1Changed();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPitchMinimum2() const
{
    return d->grainPitchMinimum2;
}

void ClipAudioSourceSliceSettings::setGrainPitchMinimum2(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
    if (d->grainPitchMinimum2 != adjustedValue) {
        d->grainPitchMinimum2 = adjustedValue;
        Q_EMIT grainPitchMinimum2Changed();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPitchMaximum2 < adjustedValue) {
            d->grainPitchMaximum2 = adjustedValue;
            Q_EMIT grainPitchMaximum2Changed();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPitchMaximum2() const
{
    return d->grainPitchMaximum2;
}

void ClipAudioSourceSliceSettings::setGrainPitchMaximum2(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, -2.0f, 2.0f)};
    if (d->grainPitchMaximum2 != adjustedValue) {
        d->grainPitchMaximum2 = adjustedValue;
        Q_EMIT grainPitchMaximum2Changed();
        Q_EMIT d->clip->sliceDataChanged();
        if (d->grainPitchMinimum2 > adjustedValue) {
            d->grainPitchMinimum2 = adjustedValue;
            Q_EMIT grainPitchMinimum2Changed();
            Q_EMIT d->clip->sliceDataChanged();
        }
    }
}

float ClipAudioSourceSliceSettings::grainPitchPriority() const
{
    return d->grainPitchPriority;
}

void ClipAudioSourceSliceSettings::setGrainPitchPriority(const float& newValue)
{
    const float adjustedValue{std::clamp(newValue, 0.0f, 1.0f)};
    if (d->grainPitchPriority != adjustedValue) {
        d->grainPitchPriority = adjustedValue;
        Q_EMIT grainPitchPriorityChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainSustain() const
{
    return d->grainSustain;
}

void ClipAudioSourceSliceSettings::setGrainSustain(const float& newValue)
{
    if (d->grainSustain != newValue) {
        d->grainSustain = newValue;
        Q_EMIT grainSustainChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

float ClipAudioSourceSliceSettings::grainTilt() const
{
    return d->grainTilt;
}

void ClipAudioSourceSliceSettings::setGrainTilt(const float& newValue)
{
    if (d->grainTilt != newValue) {
        d->grainTilt = newValue;
        Q_EMIT grainTiltChanged();
        Q_EMIT d->clip->sliceDataChanged();
    }
}

const juce::ADSR & ClipAudioSourceSliceSettings::grainADSR() const
{
    return d->grainADSR;
}
