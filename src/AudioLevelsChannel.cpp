#include "AudioLevelsChannel.h"
#include "DiskWriter.h"

#include <cmath>
#include <QDebug>
#include <QVariantList>

static int audioLevelsChannelProcess(jack_nframes_t nframes, void* arg) {
  return static_cast<AudioLevelsChannel*>(arg)->process(nframes);
}

AudioLevelsChannel::AudioLevelsChannel(const QString &clientName)
    : clientName(clientName)
    , m_diskRecorder(new DiskWriter)
{
    jack_status_t real_jack_status{};
    int result{0};
    jackClient = jack_client_open(clientName.toUtf8(), JackNullOption, &real_jack_status);
    if (jackClient) {
        // Set the process callback.
        result = jack_set_process_callback(jackClient, audioLevelsChannelProcess, this);
        if (result == 0) {
            leftPort = jack_port_register(jackClient, "left_in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
            rightPort = jack_port_register(jackClient, "right_in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
            // Activate the client.
            result = jack_activate(jackClient);
            if (result == 0) {
                qInfo() << Q_FUNC_INFO << "Successfully created and set up" << clientName;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to activate Jack client" << clientName << "with the return code" << result;
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to set Jack processing callback for" << clientName << "with the return code" << result;
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to open Jack client" << clientName << "with status" << real_jack_status;
    }
}

AudioLevelsChannel::~AudioLevelsChannel()
{
    if (jackClient) {
        jack_client_close(jackClient);
    }
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
