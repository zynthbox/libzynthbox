#pragma once

#include "ClipAudioSource.h"

class SamplerSynthSoundPrivate;
class SamplerSynthSound {
public:
    explicit SamplerSynthSound(ClipAudioSource *clip);
    ~SamplerSynthSound();
    ClipAudioSource *clip() const;
    juce::AudioBuffer<float>* audioData() const noexcept;
    const int &length() const;
    int startPosition(int slice = 0) const;
    int stopPosition(int slice = 0) const;
    int rootMidiNote() const;
    double sourceSampleRate() const;
    // The ratio between the loaded sample's sample rate, and the one in jack
    double sampleRateRatio() const;
    bool isValid{false};

    jack_port_t *leftPort{nullptr};
    jack_port_t *rightPort{nullptr};
    jack_default_audio_sample_t *leftBuffer{nullptr};
    jack_default_audio_sample_t *rightBuffer{nullptr};
private:
    SamplerSynthSoundPrivate *d{nullptr};
};
