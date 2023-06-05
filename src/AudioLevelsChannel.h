#pragma once

#include <jack/jack.h>
#include <QString>

#define CHANNELS_COUNT 10

class DiskWriter;
class alignas(128) AudioLevelsChannel {
public:
    explicit AudioLevelsChannel(jack_client_t *client, const QString &clientName);
    ~AudioLevelsChannel();
    int process(jack_nframes_t nframes);
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
private:
    const float** recordingPassthroughBuffer{new const float* [2]};
    DiskWriter *m_diskRecorder{nullptr};
};
