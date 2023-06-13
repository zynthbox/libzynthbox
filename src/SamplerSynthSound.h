#pragma once

#include "ClipAudioSource.h"

class SamplerSynthSoundPrivate;
class SamplerSynthSound {
public:
    explicit SamplerSynthSound(ClipAudioSource *clip);
    ~SamplerSynthSound();
    ClipAudioSource *clip() const;
    juce::AudioBuffer<float>* audioData() const noexcept;
    int length() const;
    int startPosition(int slice = 0) const;
    int stopPosition(int slice = 0) const;
    int rootMidiNote() const;
    double sourceSampleRate() const;
    bool isValid{false};
private:
    SamplerSynthSoundPrivate *d{nullptr};
};
