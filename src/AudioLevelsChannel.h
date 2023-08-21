#pragma once

#include <jack/jack.h>
#include <QString>
#include <limits.h>

#define CHANNELS_COUNT 10

class DiskWriter;
class alignas(128) AudioLevelsChannel {
public:
    explicit AudioLevelsChannel(jack_client_t *client, const QString &clientName);
    ~AudioLevelsChannel();
    int process(jack_nframes_t nframes, jack_nframes_t current_frames, jack_nframes_t next_frames, jack_time_t current_usecs, jack_time_t next_usecs, float period_usecs);
    DiskWriter* diskRecorder();

    jack_port_t *leftPort{nullptr};
    jack_default_audio_sample_t *leftBuffer{nullptr};
    jack_port_t *rightPort{nullptr};
    jack_default_audio_sample_t *rightBuffer{nullptr};
    quint32 bufferReadSize{0};
    jack_client_t *jackClient{nullptr};
    float peakAHoldSignal{0};
    float peakBHoldSignal{0};
    int peakA{0};
    int peakB{0};
    bool enabled{false};
    QString clientName;
    quint64 firstRecordingFrame{0};
    quint64 lastRecordingFrame{ULONG_LONG_MAX};
private:
    const float** recordingPassthroughBuffer{new const float* [2]};
    DiskWriter *m_diskRecorder{nullptr};
};
