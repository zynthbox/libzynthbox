#pragma once

#include <QObject>
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

    void setStartTick(quint64 startTick);

    void startNote (ClipCommand *clipCommand);
    void stopNote (float velocity, bool allowTailOff);

    void handleControlChange(jack_nframes_t time, int control, int value);
    void handleAftertouch(jack_nframes_t time, int pressure);
    void handlePitchChange(jack_nframes_t time, float pitchValue);

    void process(jack_default_audio_sample_t *leftBuffer, jack_default_audio_sample_t *rightBuffer, jack_nframes_t nframes, jack_nframes_t current_frames, jack_time_t current_usecs, jack_time_t next_usecs, float period_usecs);

    jack_nframes_t availableAfter{0};
    ClipCommand *mostRecentStartCommand{nullptr};
    bool isPlaying{false};
    bool isTailingOff{false};
private:
    SamplerSynthVoicePrivate *d{nullptr};
};
