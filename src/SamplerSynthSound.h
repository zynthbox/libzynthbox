#pragma once

#include "ClipAudioSource.h"
#include "JUCEHeaders.h"

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
    const double &sourceSampleRate() const;
    // The amount of stretch applied to the sample compared to the source version (will be 1.0 if time stretching is disabled)
    const double &stretchRate() const;
    // The ratio between the loaded sample's sample rate, and the one in jack
    const double &sampleRateRatio() const;
    bool isValid{false};

    jack_port_t *leftPort{nullptr};
    jack_port_t *rightPort{nullptr};
    jack_default_audio_sample_t *leftBuffer{nullptr};
    jack_default_audio_sample_t *rightBuffer{nullptr};

    tracktion_engine::TracktionThumbnail *thumbnail();
private:
    SamplerSynthSoundPrivate *d{nullptr};
};
