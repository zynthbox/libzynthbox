#include "ClipAudioSourceSubvoiceSettings.h"

#include "JUCEHeaders.h"

static constexpr float maxGainDB{24.0f};

class ClipAudioSourceSubvoiceSettingsPrivate{
public:
    ClipAudioSourceSubvoiceSettingsPrivate() {}
    float pitch{0.0f};
    float pitchChangePrecalc{1.0f};
    float pan{0.0f};
    float gain{1.0f};
};

ClipAudioSourceSubvoiceSettings::ClipAudioSourceSubvoiceSettings(QObject* parent)
    : QObject(parent)
    , d(new ClipAudioSourceSubvoiceSettingsPrivate)
{
}

ClipAudioSourceSubvoiceSettings::~ClipAudioSourceSubvoiceSettings()
{
    delete d;
}

float ClipAudioSourceSubvoiceSettings::pan() const
{
    return d->pan;
}

void ClipAudioSourceSubvoiceSettings::setPan(const float& pan)
{
    if (d->pan != pan) {
        d->pan = pan;
        Q_EMIT panChanged();
    }
}

float ClipAudioSourceSubvoiceSettings::pitch() const
{
    return d->pitch;
}

float ClipAudioSourceSubvoiceSettings::pitchChangePrecalc() const
{
    return d->pitchChangePrecalc;
}

void ClipAudioSourceSubvoiceSettings::setPitch(const float& pitch)
{
    if (d->pitch != pitch) {
        d->pitch = pitch;
        d->pitchChangePrecalc = std::pow(2.0, d->pitch / 12.0) /* * sampleRate() / sampleRate() */; // should this perhaps be a sound sample rate over playback sample rate thing?
        Q_EMIT pitchChanged();
    }
}

float ClipAudioSourceSubvoiceSettings::gain() const
{
    return d->gain;
}

float ClipAudioSourceSubvoiceSettings::gainDb() const
{
    return juce::Decibels::gainToDecibels(d->gain);
}

float ClipAudioSourceSubvoiceSettings::gainAbsolute() const
{
    return juce::jmap(juce::Decibels::gainToDecibels(d->gain, -maxGainDB), -maxGainDB, maxGainDB, 0.0f, 1.0f);
}

void ClipAudioSourceSubvoiceSettings::setGain(const float& gain)
{
    if (d->gain != gain && 0.0f <= gain && gain <= 15.84893192461113) {
        d->gain = gain;
        Q_EMIT gainChanged();
    }
}

void ClipAudioSourceSubvoiceSettings::setGainDb(const float& gainDb)
{
    setGain(juce::Decibels::decibelsToGain(gainDb));
}

void ClipAudioSourceSubvoiceSettings::setGainAbsolute(const float& gainAbsolute)
{
    setGain(juce::Decibels::decibelsToGain(juce::jmap(gainAbsolute, 0.0f, 1.0f, -maxGainDB, maxGainDB), -maxGainDB));
}
