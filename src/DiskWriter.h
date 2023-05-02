#pragma once

#include "JUCEHeaders.h"

#include <QString>

// that is one left and one right channel
#define DISKWRITER_CHANNEL_COUNT 2

class DiskWriter {
public:
    explicit DiskWriter();
    ~DiskWriter();

    void startRecording(const QString& fileName, double sampleRate = 44100, int bitRate = 16, int channelCount=DISKWRITER_CHANNEL_COUNT);

    // The input data must be an array with the same number of channels as the writer expects (that is, in our general case DISKWRITER_CHANNEL_COUNT)
    void processBlock(const float** inputChannelData, int numSamples) const;

    void stop();

    const bool &isRecording() const;
    const QString &filenamePrefix() const;
    void setFilenamePrefix(const QString& fileNamePrefix);
    const bool &shouldRecord() const;
    void setShouldRecord(bool shouldRecord);
private:
    QString m_fileNamePrefix;
    bool m_shouldRecord{false};
    bool m_isRecording{false};

    juce::File m_file;
    juce::TimeSliceThread m_backgroundThread{"AudioLevel Disk Recorder"}; // the thread that will write our audio data to disk
    std::unique_ptr<AudioFormatWriter::ThreadedWriter> m_threadedWriter; // the FIFO used to buffer the incoming data
    double m_sampleRate = 0.0;

    CriticalSection m_writerLock;
    std::atomic<AudioFormatWriter::ThreadedWriter*> m_activeWriter { nullptr };
};
