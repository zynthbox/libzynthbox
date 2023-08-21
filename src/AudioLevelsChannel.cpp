#include "AudioLevelsChannel.h"
#include "DiskWriter.h"

#include <cmath>
#include <QDebug>
#include <QVariantList>

AudioLevelsChannel::AudioLevelsChannel(jack_client_t *client, const QString &clientName)
    : clientName(clientName)
    , m_diskRecorder(new DiskWriter)
{
    jackClient = client;
    leftPort = jack_port_register(jackClient, QString("%1-left_in").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
    rightPort = jack_port_register(jackClient, QString("%1-right_in").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
    qInfo() << Q_FUNC_INFO << "Successfully created and set up" << clientName;
}

AudioLevelsChannel::~AudioLevelsChannel()
{
    delete m_diskRecorder;

}

int AudioLevelsChannel::process(jack_nframes_t nframes, jack_nframes_t current_frames, jack_nframes_t next_frames, jack_time_t /*current_usecs*/, jack_time_t /*next_usecs*/, float /*period_usecs*/)
{
    if (enabled) {
        leftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(leftPort, nframes);
        rightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(rightPort, nframes);
        if (!leftBuffer || !rightBuffer) {
            qWarning() << Q_FUNC_INFO << clientName << "has incorrect ports and things are unhappy - how to fix, though...";
            enabled = false;
            bufferReadSize = 0;
        } else {
            if (m_diskRecorder->isRecording()) {
                jack_nframes_t firstFrame{0};
                jack_nframes_t recordingLength{0};
                if (firstRecordingFrame < current_frames) {
                    recordingLength = nframes;
                } else if (firstRecordingFrame < next_frames) {
                    firstFrame = firstRecordingFrame - current_frames;
                    recordingLength = nframes - firstFrame;
                    qDebug() << Q_FUNC_INFO << clientName << "First frame of recording is within out limits, but not before this period. Likely this means this is our first period for recording, and we have set the first frame to" << firstFrame << "and the length of the recording to" << recordingLength << "for current_frames" << current_frames << "and next_frames" << next_frames;
                } else {
                    recordingLength = 0;
                }
                if (recordingLength > 0 && lastRecordingFrame < next_frames) {
                    recordingLength = recordingLength - ((next_frames) - lastRecordingFrame);
                    qDebug() << Q_FUNC_INFO << clientName << "The last recording frame is within this period, and we have reset the recording length to" << recordingLength;
                }
                if (recordingLength > 0 && m_diskRecorder->isRecording()) {
                    recordingPassthroughBuffer[0] = leftBuffer + firstFrame;
                    recordingPassthroughBuffer[1] = rightBuffer + firstFrame;
                    m_diskRecorder->processBlock(recordingPassthroughBuffer, (int)recordingLength);
                }
                if (lastRecordingFrame < next_frames) {
                    qDebug() << Q_FUNC_INFO << clientName << "We've passed the last data to the recorder - tell it to stop.";
                    m_diskRecorder->stop();
                    lastRecordingFrame = ULONG_LONG_MAX;
                }
            }
            bufferReadSize = nframes;
        }
    }
    return 0;
}

DiskWriter * AudioLevelsChannel::diskRecorder()
{
    return m_diskRecorder;
}
