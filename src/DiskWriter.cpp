#include "DiskWriter.h"

#include <QDebug>

DiskWriter::DiskWriter()
{
    m_backgroundThread.startThread();
}

DiskWriter::~DiskWriter() {
    stop();
}

void DiskWriter::startRecording(const QString& fileName, double sampleRate, int bitRate, int channelCount) {
    m_fileName = fileName;
    m_file = juce::File(fileName.toStdString());
    m_sampleRate = sampleRate;
    if (m_sampleRate > 0) {
        // In case there's a file there already, get rid of it - at this point, the user should have been made aware, so we can be ruthless
        m_file.deleteFile();
        // Create our file stream, so we have somewhere to write data to
        if (auto fileStream = std::unique_ptr<FileOutputStream>(m_file.createOutputStream())) {
            // Now create a WAV writer, which will be writing to our output stream
            WavAudioFormat wavFormat;
            if (auto writer = wavFormat.createWriterFor(fileStream.get(), sampleRate, quint32(qMin(channelCount, DISKWRITER_CHANNEL_COUNT)), bitRate, {}, 0)) {
                fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)
                // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                // write the data to disk on our background thread.
                m_threadedWriter.reset(new AudioFormatWriter::ThreadedWriter(writer, m_backgroundThread, 32768));

                // And now, swap over our active writer pointer so that the audio callback will start using it..
                const ScopedLock sl (m_writerLock);
                m_activeWriter = m_threadedWriter.get();
                m_isRecording = true;
            }
        }
    }
}

// The input data must be an array with the same number of channels as the writer expects (that is, in our general case DISKWRITER_CHANNEL_COUNT)
void DiskWriter::processBlock(const float** inputChannelData, int numSamples) const {
    const ScopedLock sl (m_writerLock);
    if (m_activeWriter.load() != nullptr) {
        if (m_activeWriter.load()->write(inputChannelData, numSamples) == false) {
            qWarning() << Q_FUNC_INFO << "Attempted to write data, but did not have the space to do so. This will result in a glitchy recording, and means we should be using a larger buffer.";
        }
    }
}

void DiskWriter::stop() {
    // First, clear this pointer to stop the audio callback from using our writer object..
    {
        const ScopedLock sl(m_writerLock);
        m_activeWriter = nullptr;
        m_sampleRate = 0;
        m_isRecording = false;
    }

    // Now we can delete the writer object. It's done in this order because the deletion could
    // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
    // the audio callback while this happens.
    m_threadedWriter.reset();
}

const bool &DiskWriter::isRecording() const {
    return m_isRecording;
}

const QString &DiskWriter::filenamePrefix() const {
    return m_fileNamePrefix;
}

void DiskWriter::setFilenamePrefix(const QString& fileNamePrefix) {
    m_fileNamePrefix = fileNamePrefix;
}

const bool &DiskWriter::shouldRecord() const {
    return m_shouldRecord;
}

void DiskWriter::setShouldRecord(bool shouldRecord) {
    m_shouldRecord = shouldRecord;
}
