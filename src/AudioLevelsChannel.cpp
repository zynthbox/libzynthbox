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

int AudioLevelsChannel::process(jack_nframes_t nframes)
{
    if (enabled) {
        leftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(leftPort, nframes);
        rightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(rightPort, nframes);
        if (!leftBuffer || !rightBuffer) {
            qWarning() << Q_FUNC_INFO << clientName << "has incorrect ports and things are unhappy - how to fix, though...";
            enabled = false;
            bufferReadSize = 0;
        } else {
            bufferReadSize = nframes;
            if (m_diskRecorder->isRecording()) {
                recordingPassthroughBuffer[0] = leftBuffer;
                recordingPassthroughBuffer[1] = rightBuffer;
                m_diskRecorder->processBlock(recordingPassthroughBuffer, (int)nframes);
            }
        }
    }
    return 0;
}

DiskWriter * AudioLevelsChannel::diskRecorder()
{
    return m_diskRecorder;
}
