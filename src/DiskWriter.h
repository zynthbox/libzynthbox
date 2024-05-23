#pragma once

#include "JUCEHeaders.h"

#include <QObject>
#include <QString>

// that is one left and one right channel
#define DISKWRITER_CHANNEL_COUNT 2

class AudioLevelsChannel;
class DiskWriter : public QObject {
    Q_OBJECT
public:
    explicit DiskWriter(AudioLevelsChannel *audioLevelsChannel);
    ~DiskWriter();

    void startRecording(const QString& fileName, double sampleRate = 44100, int bitRate = 32, int channelCount=DISKWRITER_CHANNEL_COUNT);

    // The input data must be an array with the same number of channels as the writer expects (that is, in our general case DISKWRITER_CHANNEL_COUNT)
    void processBlock(const float** inputChannelData, int numSamples) const;

    void stop();

    const bool &isRecording() const;
    Q_SIGNAL void isRecordingChanged();

    const QString &filenamePrefix() const;
    void setFilenamePrefix(const QString& fileNamePrefix);
    const QString &filenameSuffix() const;
    void setFilenameSuffix(const QString& fileNameSuffix);
    QString fileName() const;
    const bool &shouldRecord() const;
    void setShouldRecord(bool shouldRecord);
private:
    QString m_fileNamePrefix;
    QString m_fileNameSuffix{".wav"};
    bool m_shouldRecord{false};
    bool m_isRecording{false};

    QString m_fileName;
    juce::File m_file;
    juce::TimeSliceThread m_backgroundThread{"AudioLevel Disk Recorder"}; // the thread that will write our audio data to disk
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> m_threadedWriter; // the FIFO used to buffer the incoming data
    double m_sampleRate{0.0};

    AudioLevelsChannel *m_audioLevelsChannel{nullptr};
    CriticalSection m_writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter*> m_activeWriter{nullptr};
};
