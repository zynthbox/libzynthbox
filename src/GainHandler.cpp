#include "GainHandler.h"

#include "JUCEHeaders.h"

class GainHandler::Private {
public:
    Private(GainHandler *q)
        : q(q)
    { }
    GainHandler *q{nullptr};
    float minimumDecibel{-24.0};
    float maximumDecibel{24};
    float maximumGain{15.84893192461113};
    float gain{1.0};
    bool muted{false};

    // This allows us to force-set the gain when required (such as when changing the dB range)
    void setGain(const float &newGain) {
        gain = qMax(0.0f, qMin(newGain, maximumGain));
        Q_EMIT q->gainChanged();
    }
};

GainHandler::GainHandler(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    connect(this, &GainHandler::gainChanged, &GainHandler::operationalGainChanged);
    connect(this, &GainHandler::mutedChanged, &GainHandler::operationalGainChanged);
}

GainHandler::~GainHandler()
{
    delete d;
}

float GainHandler::minimumDecibel() const
{
    return d->minimumDecibel;
}

void GainHandler::setMinimumDecibel(const float& minimumDecibel)
{
    if (d->minimumDecibel != minimumDecibel) {
        d->minimumDecibel = minimumDecibel;
        Q_EMIT minimumDecibelChanged();
        d->setGain(d->gain);
    }
}

float GainHandler::maximumDecibel() const
{
    return d->maximumDecibel;
}

void GainHandler::setMaximumDecibel(const float& maximumDecibel)
{
    if (d->maximumDecibel != maximumDecibel) {
        d->maximumDecibel = maximumDecibel;
        d->maximumGain = juce::Decibels::decibelsToGain(maximumDecibel, d->minimumDecibel);
        Q_EMIT maximumDecibelChanged();
        d->setGain(d->gain);
    }
}

float GainHandler::absoluteGainAtZeroDb() const
{
    return juce::jmap(0.0f, d->minimumDecibel, d->maximumDecibel, 0.0f, 1.0f);
}

float GainHandler::gain() const
{
    return d->gain;
}

float GainHandler::gainDb() const
{
    return juce::Decibels::gainToDecibels(d->gain, d->minimumDecibel);
}

float GainHandler::gainAbsolute() const
{
    return d->gain == 0 ? 0 : juce::jmap(juce::Decibels::gainToDecibels(d->gain, d->minimumDecibel), d->minimumDecibel, d->maximumDecibel, 0.0f, 1.0f);
}

void GainHandler::setGain(const float& gain)
{
    if (d->gain != gain) {
        d->setGain(gain);
    }
}

void GainHandler::setGainDb(const float& db)
{
    setGain(db == d->minimumDecibel ? 0 : juce::Decibels::decibelsToGain(db, d->minimumDecibel));
}

void GainHandler::setGainAbsolute(const float& gainAbsolute)
{
    setGain(gainAbsolute == 0 ? 0 : juce::Decibels::decibelsToGain(juce::jmap(gainAbsolute, 0.0f, 1.0f, d->minimumDecibel, d->maximumDecibel), d->minimumDecibel));
}

bool GainHandler::muted() const
{
    return d->muted;
}

void GainHandler::setMuted(const bool& muted)
{
    if (d->muted != muted) {
        d->muted = muted;
        Q_EMIT mutedChanged();
    }
}

const float &GainHandler::operationalGain() const
{
    static const float mutedGain{0.0f};
    return d->muted ? mutedGain : d->gain;
}
