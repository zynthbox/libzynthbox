#pragma once

#include "ClipAudioSource.h"
#include "JUCEHeaders.h"
#include <QDebug>

class SamplerSynthSoundPrivate;
class SamplerSynthSound {
public:
    explicit SamplerSynthSound(ClipAudioSource *clip);
    ~SamplerSynthSound();
    ClipAudioSource *clip() const;
    juce::AudioBuffer<float>* audioData() const noexcept;
    const int &length() const;
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

#define SamplerSoundListSize 512
class SamplerSoundList {
public:
    struct alignas(64) Entry {
        Entry *previous{nullptr};
        Entry *next{nullptr};
        SamplerSynthSound *samplerSound{nullptr};
        ClipAudioSource *audioSource{nullptr};
        bool empty{true};
    };

    SamplerSoundList () {
        Entry* entryPrevious{&ringData[SamplerSoundListSize - 1]};
        for (quint64 i = 0; i < SamplerSoundListSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~SamplerSoundList() {}
    void insert(SamplerSynthSound *samplerSound, ClipAudioSource *audioSource) {
        for (Entry &entry : ringData) {
            if (entry.empty == true && entry.samplerSound == nullptr && entry.audioSource == nullptr) {
                entry.samplerSound = samplerSound;
                entry.audioSource = audioSource;
                entry.empty = false;
                return;
            }
        }
        qWarning() << Q_FUNC_INFO << "There is unprocessed data stored at the write location: id" << writeHead->samplerSound << writeHead->audioSource << "This likely means the buffer size is too small, which will require attention at the api level.";
    }
    void find(SamplerSynthSound *samplerSound, ClipAudioSource **audioSource) {
        for (const Entry &entry : ringData) {
            if (entry.empty == false && entry.samplerSound == samplerSound) {
                *audioSource = entry.audioSource;
                break;
            }
        }
        return;
    }
    void find(ClipAudioSource *audioSource, SamplerSynthSound **samplerSound) {
        for (const Entry &entry : ringData) {
            if (entry.empty == false && entry.audioSource == audioSource) {
                *samplerSound = entry.samplerSound;
                break;
            }
        }
        return;
    }
    /**
     * \brief Attempt to read the data out of the ring, until there are no more unprocessed entries
     * @return Whether or not the read was valid
     */
    bool remove(SamplerSynthSound *samplerSound, ClipAudioSource *audioSource) {
        for (Entry &entry : ringData) {
            if (entry.empty == false && entry.samplerSound == samplerSound && entry.audioSource == audioSource) {
                entry.samplerSound = nullptr;
                entry.audioSource = nullptr;
                entry.empty = true;
                return true;
            }
        }
        return false;
    }
    Entry ringData[SamplerSoundListSize];
    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
    QString name;
};
