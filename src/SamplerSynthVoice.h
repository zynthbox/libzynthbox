#pragma once

#include <QObject>
#include <QDebug>
#include <jack/jack.h>

struct ClipCommand;
class SamplerSynth;
class SamplerSynthSound;
class SamplerSynthVoicePrivate;
class SamplerSynthVoice
{
public:
    explicit SamplerSynthVoice(SamplerSynth *samplerSynth);
    ~SamplerSynthVoice();

    void handleCommand(ClipCommand *clipCommand, jack_nframes_t timestamp);

    void setCurrentCommand(ClipCommand *clipCommand);

    void setModwheel(int modwheelValue);

    void startNote (ClipCommand *clipCommand, jack_nframes_t timestamp);
    void stopNote (float velocity, bool allowTailOff, jack_nframes_t timestamp, float peakGainLeft = -1, float peakGainRight = -1);

    void handleControlChange(jack_nframes_t time, int channel, int control, int value);
    void handleAftertouch(jack_nframes_t time, int channel, int note, int pressure);
    void handlePitchChange(jack_nframes_t time, int channel, int note, float pitchValue);

    void process(jack_default_audio_sample_t *leftBuffer, jack_default_audio_sample_t *rightBuffer, jack_nframes_t nframes, jack_nframes_t current_frames, jack_time_t current_usecs, jack_time_t next_usecs, float period_usecs);

    jack_nframes_t availableAfter{0};
    ClipCommand *mostRecentStartCommand{nullptr};
    bool isPlaying{false};
    bool isTailingOff{false};

protected:
    // Convenience for holding a linked list of voices
    friend class SamplerChannel;
    SamplerSynthVoice *previous{nullptr};
    SamplerSynthVoice *next{nullptr};
private:
    SamplerSynthVoicePrivate *d{nullptr};
};

// We used to have 32 voices per channel, so with the global pool we now have...
#define SamplerVoicePoolSize 384
class SamplerVoicePoolRing {
public:
    struct alignas(64) Entry {
        Entry *previous{nullptr};
        Entry *next{nullptr};
        SamplerSynthVoice *samplerVoice{nullptr};
        bool processed{true};
    };

    SamplerVoicePoolRing () {
        Entry* entryPrevious{&ringData[SamplerVoicePoolSize - 1]};
        for (quint64 i = 0; i < SamplerVoicePoolSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~SamplerVoicePoolRing() {}
    void write(SamplerSynthVoice *samplerVoice) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data stored at the write location: id" << writeHead->samplerVoice << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->samplerVoice = samplerVoice;
        entry->processed = false;
    }
    /**
     * \brief Attempt to read the data out of the ring, until there are no more unprocessed entries
     * @return Whether or not the read was valid
     */
    bool read(SamplerSynthVoice **samplerVoice) {
        if (readHead->processed == false) {
            Entry *entry = readHead;
            readHead = readHead->next;
            *samplerVoice = entry->samplerVoice;
            entry->samplerVoice = nullptr;
            entry->processed = true;
            return true;
        }
        return false;
    }
    Entry ringData[SamplerVoicePoolSize];
    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
    QString name;
};
