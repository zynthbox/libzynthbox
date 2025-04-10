#pragma once

#include "TimerCommand.h"
#include "GainHandler.h"

#include "JUCEHeaders.h"
#include <jack/jack.h>
#include <QObject>
#include <QString>
#include <limits.h>

#define CHANNELS_COUNT 10

class DiskWriter;
class AudioLevelsChannel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* gainHandler READ gainHandler CONSTANT)
    Q_PROPERTY(float panAmount READ panAmount WRITE setPanAmount NOTIFY panAmountChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
public:
    explicit AudioLevelsChannel(jack_client_t *client, const QString &clientName, juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, QObject *parent = nullptr);
    ~AudioLevelsChannel();
    int process(jack_nframes_t nframes, jack_nframes_t current_frames, jack_nframes_t next_frames, jack_time_t current_usecs, jack_time_t next_usecs, float period_usecs);
    DiskWriter* diskRecorder();
    tracktion_engine::TracktionThumbnail *thumbnail();
    void addChangeListener(ChangeListener* listener);
    void removeChangeListener(ChangeListener* listener);
    bool thumbnailHAnyListeners() const;

    jack_port_t *leftPort{nullptr};
    jack_default_audio_sample_t *leftBuffer{nullptr};
    jack_port_t *rightPort{nullptr};
    jack_default_audio_sample_t *rightBuffer{nullptr};
    jack_port_t *leftOutPort{nullptr};
    jack_default_audio_sample_t *leftOutBuffer{nullptr};
    jack_port_t *rightOutPort{nullptr};
    jack_default_audio_sample_t *rightOutBuffer{nullptr};
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
    TimerCommandRing startCommandsRing;

    QObject* gainHandler() const;

    float panAmount() const;
    void setPanAmount(const float& newValue);
    Q_SIGNAL void panAmountChanged();

    bool muted() const;
    void setMuted(const bool& newValue);
    Q_SIGNAL void mutedChanged();
private:
    friend class DiskWriter;
    const float** recordingPassthroughBuffer{new const float* [2]};
    DiskWriter *m_diskRecorder{nullptr};
    inline void doRecordingHandling(jack_nframes_t nframes, jack_nframes_t current_frames, jack_nframes_t next_frames);
    tracktion_engine::TracktionThumbnail m_thumbnail;
    int m_thumbnailListenerCount{0};
    int64 m_nextSampleNum{0};
    GainHandler *m_gainHandler{nullptr};
    float m_panAmount{0.0f};
    bool m_muted{false};
};
