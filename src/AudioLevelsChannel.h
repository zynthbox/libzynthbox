#pragma once

#include "TimerCommand.h"
#include "GainHandler.h"
#include "ZynthboxBasics.h"

#include "JUCEHeaders.h"
#include <jack/jack.h>
#include <QObject>
#include <QString>
#include <limits.h>

#define CHANNELS_COUNT 10

class DiskWriter;
class JackPassthroughAnalyser;
class AudioLevelsChannel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* gainHandler READ gainHandler CONSTANT)
    Q_PROPERTY(float panAmount READ panAmount WRITE setPanAmount NOTIFY panAmountChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    /**
     * \brief Whether or not the equaliser will be applied to incoming audio
     * @default false
     */
    Q_PROPERTY(bool equaliserEnabled READ equaliserEnabled WRITE setEqualiserEnabled NOTIFY equaliserEnabledChanged)
    /**
     * \brief A list of the settings container objects for each of the equaliser bands
     */
    Q_PROPERTY(QVariantList equaliserSettings READ equaliserSettings NOTIFY equaliserSettingsChanged)

    /**
     * \brief Whether or not the compressor will be applied to incoming audio (post-equaliser)
     * @default false
     */
    Q_PROPERTY(bool compressorEnabled READ compressorEnabled WRITE setCompressorEnabled NOTIFY compressorEnabledChanged)
    /**
     * \brief The sources used for the left channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelLeft READ compressorSidechannelLeft WRITE setCompressorSidechannelLeft NOTIFY compressorSidechannelLeftChanged)
    /**
     * \brief The sources used for the right channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelRight READ compressorSidechannelRight WRITE setCompressorSidechannelRight NOTIFY compressorSidechannelRightChanged)
    /**
     * \brief The settings container object for the compressor
     */
    Q_PROPERTY(QObject* compressorSettings READ compressorSettings NOTIFY compressorSettingsChanged)
public:
    explicit AudioLevelsChannel(jack_client_t *client, const QString &clientName, const ZynthboxBasics::Track &sketchpadTrack, juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, QObject *parent = nullptr);
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
    float peakA{0};
    float peakB{0};
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

    bool equaliserEnabled() const;
    void setEqualiserEnabled(const bool &equaliserEnabled);
    Q_SIGNAL void equaliserEnabledChanged();
    QVariantList equaliserSettings() const;
    Q_SIGNAL void equaliserSettingsChanged();
    Q_INVOKABLE QObject *equaliserNearestToFrequency(const float &frequency) const;
    Q_SIGNAL void equaliserDataChanged();
    const std::vector<double> &equaliserMagnitudes() const;
    const std::vector<double> &equaliserFrequencies() const;
    void equaliserCreateFrequencyPlot(QPolygonF &p, const QRect bounds, float pixelsPerDouble);
    void setEqualiserInputAnalysers(QList<JackPassthroughAnalyser*> &equaliserInputAnalysers) const;
    void setEqualiserOutputAnalysers(QList<JackPassthroughAnalyser*> &equaliserOutputAnalysers) const;

    bool compressorEnabled() const;
    void setCompressorEnabled(const bool &compressorEnabled);
    Q_SIGNAL void compressorEnabledChanged();
    QString compressorSidechannelLeft() const;
    void setCompressorSidechannelLeft(const QString &compressorSidechannelLeft);
    Q_SIGNAL void compressorSidechannelLeftChanged();
    QString compressorSidechannelRight() const;
    void setCompressorSidechannelRight(const QString &compressorSidechannelRight);
    Q_SIGNAL void compressorSidechannelRightChanged();
    QObject *compressorSettings() const;
    Q_SIGNAL void compressorSettingsChanged();
private:
    class Private;
    Private *d{nullptr};
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
