/*
  ==============================================================================

    AudioLevels.cpp
    Created: 8 Feb 2022
    Author:  Anupam Basak <anupam.basak27@gmail.com>
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#include "AudioLevels.h"
#include "DiskWriter.h"
#include "JackThreadAffinitySetter.h"
#include "SyncTimer.h"
#include "TimerCommand.h"
#include "ZynthboxBasics.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QVariantList>

#define DebugAudioLevels false

struct RecordPort {
    QString portName;
    int channel{-1};
};

class AudioLevelsPrivate {
public:
    AudioLevelsPrivate() {
        for(int i = 0; i < CHANNELS_COUNT; ++i) {
            channelsToRecord << false;
            levels.append(QVariant::fromValue<float>(0));
        }
    }
    ~AudioLevelsPrivate() {
        if (jackClient) {
            jack_client_close(jackClient);
        }
        qDeleteAll(audioLevelsChannels);
    }
    QList<AudioLevelsChannel*> audioLevelsChannels;
    QVariantList tracks;
    DiskWriter* globalPlaybackWriter{nullptr};
    DiskWriter* portsRecorder{nullptr};
    QList<RecordPort> recordPorts;
    QList<DiskWriter*> channelWriters{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    QVariantList channelsToRecord;
    QVariantList levels;
    QTimer analysisTimer;
    QTimer isRecordingChangedThrottle;
    jack_client_t* jackClient{nullptr};
    bool initialized{false};
    quint64 startTimestamp{0};
    quint64 stopTimestamp{0};

    void connectPorts(const QString &from, const QString &to) {
        int result = jack_connect(jackClient, from.toUtf8(), to.toUtf8());
        if (result == 0 || result == EEXIST) {
            if (DebugAudioLevels) { qDebug() << Q_FUNC_INFO << (result == EEXIST ? "Retaining existing connection from" : "Successfully created new connection from" ) << from << "to" << to; }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to connect" << from << "with" << to << "with error code" << result;
            // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
        }
    }
    void disconnectPorts(const QString &from, const QString &to) {
        // Don't attempt to connect already connected ports
        int result = jack_disconnect(jackClient, from.toUtf8(), to.toUtf8());
        if (result == 0) {
            if (DebugAudioLevels) { qDebug() << Q_FUNC_INFO << "Successfully disconnected" << from << "from" << to; }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to disconnect" << from << "from" << to << "with error code" << result;
        }
    }

    const QStringList recorderPortNames{QLatin1String{"AudioLevels:SystemRecorder-left_in"}, QLatin1String{"AudioLevels:SystemRecorder-right_in"}};
    void disconnectPort(const QString& portName, int channel) {
        jackClient = audioLevelsChannels[2]->jackClient;
        disconnectPorts(portName, recorderPortNames[channel]);
        jackClient = audioLevelsChannels[0]->jackClient;
    }
    void connectPort(const QString& portName, int channel) {
        jackClient = audioLevelsChannels[2]->jackClient;
        connectPorts(portName, recorderPortNames[channel]);
        jackClient = audioLevelsChannels[0]->jackClient;
    }
};

AudioLevels *AudioLevels::instance()  {
    AudioLevels* sin = singletonInstance.load(std::memory_order_acquire);
    if (!sin) {
        std::lock_guard<std::mutex> myLock(singletonMutex);
        sin = singletonInstance.load(std::memory_order_relaxed);
        if (!sin) {
            sin = new AudioLevels(QCoreApplication::instance());
            singletonInstance.store(sin, std::memory_order_release);
      }
    }
    return sin;
}

std::atomic<AudioLevels*> AudioLevels::singletonInstance;
std::mutex AudioLevels::singletonMutex;

static int audioLevelsProcess(jack_nframes_t nframes, void* arg) {
    const AudioLevelsPrivate* d = static_cast<AudioLevelsPrivate*>(arg);
    if (d->initialized) {
        jack_nframes_t current_frames;
        jack_time_t current_usecs;
        jack_time_t next_usecs;
        float period_usecs;
        jack_get_cycle_times(d->jackClient, &current_frames, &current_usecs, &next_usecs, &period_usecs);
        jack_nframes_t next_frames{current_frames + nframes};
        for (AudioLevelsChannel *channel : qAsConst(d->audioLevelsChannels)) {
            channel->process(nframes, current_frames, next_frames, current_usecs, next_usecs, period_usecs);
        }
    }
    return 0;
}

AudioLevels::AudioLevels(QObject *parent)
  : QObject(parent)
  , d(new AudioLevelsPrivate)
{
    static const QStringList audioLevelClientNames{
        "SystemCapture",
        "SystemPlayback",
        "SystemRecorder",
        "Channel1",
        "Channel2",
        "Channel3",
        "Channel4",
        "Channel5",
        "Channel6",
        "Channel7",
        "Channel8",
        "Channel9",
        "Channel10",
    };
    m_formatManager.registerBasicFormats();
    jack_status_t real_jack_status{};
    int result{0};
    d->jackClient = jack_client_open("AudioLevels", JackNullOption, &real_jack_status);
    if (d->jackClient) {
        // Set the process callback.
        result = jack_set_process_callback(d->jackClient, audioLevelsProcess, d);
        if (result == 0) {
            // Activate the client.
            result = jack_activate(d->jackClient);
            if (result == 0) {
                qInfo() << Q_FUNC_INFO << "Successfully created and set up AudioLevels Jack client";
                zl_set_jack_client_affinity(d->jackClient);
                int channelIndex{0};
                for (const QString &clientName : audioLevelClientNames) {
                    AudioLevelsChannel *channel = new AudioLevelsChannel(d->jackClient, clientName, m_formatManager, m_thumbnailsCache);
                    if (channelIndex == 0) {
                        d->jackClient = channel->jackClient;
                        d->connectPorts("system:capture_1", "AudioLevels:SystemCapture-left_in");
                        d->connectPorts("system:capture_2", "AudioLevels:SystemCapture-right_in");
                    } else if (channelIndex == 1) {
                        d->globalPlaybackWriter = channel->diskRecorder();
                    } else if (channelIndex == 2) {
                        d->portsRecorder = channel->diskRecorder();
                    } else {
                        const int sketchpadChannelIndex{channelIndex - 3};
                        d->tracks << QVariant::fromValue<QObject*>(channel);
                        /**
                        * Do not assign items by index like this : d->channelWriters[sketchpadChannelIndex] = channel->diskRecorder;
                        * Assigning items by index causes size() to report 0 even though items are added.
                        * This might be causing a crash sometimes during initialization
                        *
                        * Instead of assigning by index, create a qlist with 10 elements having nullptr as value
                        * and replace nullptr with DiskWriter pointer when initializing
                        */
                        d->channelWriters.replace(sketchpadChannelIndex, channel->diskRecorder());
                    }
                    d->audioLevelsChannels << channel;
                    ++channelIndex;
                }
                d->analysisTimer.setInterval(50);
                connect(&d->analysisTimer, &QTimer::timeout, this, &AudioLevels::timerCallback);
                d->analysisTimer.start();
                d->isRecordingChangedThrottle.setInterval(10);
                d->isRecordingChangedThrottle.setSingleShot(true);
                connect(&d->isRecordingChangedThrottle, &QTimer::timeout, this, &AudioLevels::isRecordingChanged);
                for (AudioLevelsChannel *channel : d->audioLevelsChannels) {
                    channel->enabled = true;
                    connect(channel->diskRecorder(), &DiskWriter::isRecordingChanged, &d->isRecordingChangedThrottle, QOverload<>::of(&QTimer::start), Qt::QueuedConnection);
                }
                d->initialized = true;
            } else {
                qWarning() << Q_FUNC_INFO << "Failed to activate AudioLevels Jack client with the return code" << result;
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to set AudioLevels Jack processing callback for with the return code" << result;
        }
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to open AudioLevels Jack client with status" << real_jack_status;
    }
}

inline float AudioLevels::convertTodbFS(float raw) {
    if (raw <= 0) {
        return -200;
    }

    const float fValue = 20 * log10f(raw);
    if (fValue < -200) {
        return -200;
    }

    return fValue;
}

inline float addFloat(const float& db1, const float &db2) {
   return 10 * log10f(pow(10, db1/10) + pow(10, db2/10));
}

float AudioLevels::add(float db1, float db2) {
    return addFloat(db1, db2);
}

QVariantList AudioLevels::tracks() const
{
    return d->tracks;
}

void AudioLevels::timerCallback() {
    int channelIndex{0};
    for (AudioLevelsChannel *channel : d->audioLevelsChannels) {
        if (channel->enabled && channel->leftPort && channel->rightPort) {
            // channel->peakA = qMax(0.0f, channel->peakA - 0.01f);
            // channel->peakB = qMax(0.0f, channel->peakB - 0.01f);
            const float peakDbA{convertTodbFS(channel->peakA)},
                        peakDbB{convertTodbFS(channel->peakB)};
            if (channelIndex == 0) {
                captureA = peakDbA <= -200 ? -200 : peakDbA;
                captureB = peakDbB <= -200 ? -200 : peakDbB;
            } else if (channelIndex == 1) {
                playbackA = peakDbA <= -200 ? -200 : peakDbA;
                playbackB = peakDbB <= -200 ? -200 : peakDbB;
                playback = add(peakDbA, peakDbB);
                channel->peakAHoldSignal = (channel->peakA >= channel->peakAHoldSignal) ? channel->peakA : channel->peakAHoldSignal * 0.9f;
                channel->peakBHoldSignal = (channel->peakB >= channel->peakBHoldSignal) ? channel->peakB : channel->peakBHoldSignal * 0.9f;
                playbackAHold = convertTodbFS(channel->peakAHoldSignal);
                playbackBHold = convertTodbFS(channel->peakBHoldSignal);
            } else if (channelIndex == 2) {
                recordingA = peakDbA <= -200 ? -200 : peakDbA;
                recordingB = peakDbB <= -200 ? -200 : peakDbB;
            } else {
                const int sketchpadChannelIndex{channelIndex - 3};
                channelsA[sketchpadChannelIndex] = peakDbA <= -200 ? -200 : peakDbA;
                channelsB[sketchpadChannelIndex] = peakDbB <= -200 ? -200 : peakDbB;
                d->levels[sketchpadChannelIndex].setValue<float>(qMax(channelsA[sketchpadChannelIndex], channelsB[sketchpadChannelIndex]));
            }
        }
        ++channelIndex;
    }
    Q_EMIT audioLevelsChanged();
}

const QVariantList AudioLevels::getChannelsAudioLevels() {
    return d->levels;
}

void AudioLevels::setRecordGlobalPlayback(bool shouldRecord)
{
    if (d->globalPlaybackWriter->shouldRecord() != shouldRecord) {
        d->globalPlaybackWriter->setShouldRecord(shouldRecord);
        Q_EMIT recordGlobalPlaybackChanged();
    }
}

bool AudioLevels::recordGlobalPlayback() const
{
    return d->globalPlaybackWriter->shouldRecord();
}

void AudioLevels::setGlobalPlaybackFilenamePrefix(const QString &fileNamePrefix)
{
    d->globalPlaybackWriter->setFilenamePrefix(fileNamePrefix);
}

void AudioLevels::setGlobalPlaybackFilenameSuffix(const QString& fileNameSuffix)
{
    d->globalPlaybackWriter->setFilenameSuffix(fileNameSuffix);
}

void AudioLevels::setChannelToRecord(int channel, bool shouldRecord)
{
    if (channel > -1 && channel < d->channelWriters.count()) {
        d->channelWriters[channel]->setShouldRecord(shouldRecord);
        d->channelsToRecord[channel] = shouldRecord;
        Q_EMIT channelsToRecordChanged();
    }
}

QVariantList AudioLevels::channelsToRecord() const
{
    return d->channelsToRecord;
}

void AudioLevels::setChannelFilenamePrefix(int channel, const QString& fileNamePrefix)
{
    if (channel > -1 && channel < d->channelWriters.count()) {
        d->channelWriters[channel]->setFilenamePrefix(fileNamePrefix);
    }
}

void AudioLevels::setChannelFilenameSuffix(int channel, const QString& fileNameSuffix)
{
    if (channel > -1 && channel < d->channelWriters.count()) {
        d->channelWriters[channel]->setFilenameSuffix(fileNameSuffix);
    }
}

void AudioLevels::setRecordPortsFilenamePrefix(const QString &fileNamePrefix)
{
    d->portsRecorder->setFilenamePrefix(fileNamePrefix);
}

void AudioLevels::setRecordPortsFilenameSuffix(const QString& fileNameSuffix)
{
    d->portsRecorder->setFilenameSuffix(fileNameSuffix);
}


void AudioLevels::addRecordPort(const QString &portName, int channel)
{
    bool addPort{true};
    for (const RecordPort &port : qAsConst(d->recordPorts)) {
        if (port.portName == portName && port.channel == channel) {
            addPort = false;
            break;
        }
    }
    if (addPort) {
        RecordPort port;
        port.portName = portName;
        port.channel = channel;
        d->recordPorts << port;
        d->connectPort(portName, channel);
    }
}

void AudioLevels::removeRecordPort(const QString &portName, int channel)
{
    QMutableListIterator<RecordPort> iterator(d->recordPorts);
    while (iterator.hasNext()) {
        const RecordPort &port = iterator.value();
        if (port.portName == portName && port.channel == channel) {
            d->disconnectPort(port.portName, port.channel);
            iterator.remove();
            break;
        }
    }
}

void AudioLevels::clearRecordPorts()
{
    for (const RecordPort &port : qAsConst(d->recordPorts)) {
        d->disconnectPort(port.portName, port.channel);
    }
    d->recordPorts.clear();
}

void AudioLevels::setShouldRecordPorts(bool shouldRecord)
{
    if (d->portsRecorder->shouldRecord() != shouldRecord) {
        d->portsRecorder->setShouldRecord(shouldRecord);
        Q_EMIT shouldRecordPortsChanged();
    }
}

bool AudioLevels::shouldRecordPorts() const
{
    return d->portsRecorder->shouldRecord();
}

QString AudioLevels::getTimestampedFilename(const QString& prefix, const QString& suffix)
{
    const QString timestamp{QDateTime::currentDateTime().toString(Qt::ISODate)};
    return QString("%1-%2%3").arg(prefix).arg(timestamp).arg(suffix);
}

void AudioLevels::startRecording(quint64 startTimestamp)
{
    // If we've been passed a timestamp, use that, otherwise just set to the most recent jack playhead timestamp
    if (startTimestamp > 0) {
        d->startTimestamp = startTimestamp;
    } else {
        d->startTimestamp = SyncTimer::instance()->jackPlayhead();
    }
    d->stopTimestamp = ULONG_LONG_MAX;
    // Inform all the channels they should only be recording from (and including) that given timestamp
    for (AudioLevelsChannel *channel : qAsConst(d->audioLevelsChannels)) {
        channel->firstRecordingFrame = d->startTimestamp;
        channel->lastRecordingFrame = d->stopTimestamp;
    }
    const QString timestamp{QDateTime::currentDateTime().toString(Qt::ISODate)};
    const double sampleRate = jack_get_sample_rate(d->jackClient);
    // Doing this in two goes, because when we ask recording to start, it will very extremely start,
    // and we kind of want to at least get pretty close to them starting at the same time, so let's
    // do this bit of the ol' filesystem work first
    QString dirPath = d->globalPlaybackWriter->filenamePrefix().left(d->globalPlaybackWriter->filenamePrefix().lastIndexOf('/'));
    if (d->globalPlaybackWriter->shouldRecord() && !QDir().exists(dirPath)) {
        QDir().mkpath(dirPath);
    }
    dirPath = d->portsRecorder->filenamePrefix().left(d->portsRecorder->filenamePrefix().lastIndexOf('/'));
    if (d->portsRecorder->shouldRecord() && !QDir().exists(dirPath)) {
        QDir().mkpath(dirPath);
    }
    for (DiskWriter *channelWriter : d->channelWriters) {
        dirPath = channelWriter->filenamePrefix().left(channelWriter->filenamePrefix().lastIndexOf('/'));
        if (channelWriter->shouldRecord() && !QDir().exists(dirPath)) {
            QDir().mkpath(dirPath);
        }
    }
    if (d->globalPlaybackWriter->shouldRecord()) {
        if (d->globalPlaybackWriter->filenamePrefix().endsWith(".wav")) {
            // If prefix already ends with `.wav` do not add timestamp and suffix to filename
            d->globalPlaybackWriter->startRecording(d->globalPlaybackWriter->filenamePrefix(), sampleRate);
        } else {
            const QString filename = QString("%1-%2%3").arg(d->globalPlaybackWriter->filenamePrefix()).arg(timestamp).arg(d->globalPlaybackWriter->filenameSuffix());
            d->globalPlaybackWriter->startRecording(filename, sampleRate);
        }
    }
    if (d->portsRecorder->shouldRecord()) {
        if (d->portsRecorder->filenamePrefix().endsWith(".wav")) {
            // If prefix already ends with `.wav` do not add timestamp and suffix to filename
            d->portsRecorder->startRecording(d->portsRecorder->filenamePrefix(), sampleRate, 16, d->recordPorts.count());
        } else {
            const QString filename = QString("%1-%2%3").arg(d->portsRecorder->filenamePrefix()).arg(timestamp).arg(d->portsRecorder->filenameSuffix());
            d->portsRecorder->startRecording(filename, sampleRate, 16, d->recordPorts.count());
        }
    }
    for (DiskWriter *channelWriter : d->channelWriters) {
        if (channelWriter->shouldRecord()) {
            if (channelWriter->filenamePrefix().endsWith(".wav")) {
                // If prefix already ends with `.wav` do not add timestamp and suffix to filename
                channelWriter->startRecording(channelWriter->filenamePrefix(), sampleRate);
            } else {
                const QString filename = QString("%1-%2%3").arg(channelWriter->filenamePrefix()).arg(timestamp).arg(channelWriter->filenameSuffix());
                channelWriter->startRecording(filename, sampleRate);
            }
        }
    }
    Q_EMIT isRecordingChanged();
}

void AudioLevels::scheduleStartRecording(quint64 delay)
{
    TimerCommand *command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::ChannelRecorderStartOperation;
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
}

QString AudioLevels::scheduleChannelRecorderStart(quint64 delay, int sketchpadTrack, const QString& prefix, const QString &suffix)
{
    TimerCommand *command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::ChannelRecorderStartOperation;
    command->parameter = 1;
    command->parameter2 = sketchpadTrack;
    const QString timestamp{QDateTime::currentDateTime().toString(Qt::ISODate)};
    if (prefix.endsWith(suffix)) {
        // If prefix already ends with `.wav` do not add timestamp and suffix to filename
        command->variantParameter = prefix;
    } else {
        command->variantParameter = QString("%1-%2%3").arg(prefix).arg(timestamp).arg(suffix);
    }
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
    return command->variantParameter.toString();
}

void AudioLevels::stopRecording(quint64 stopTimestamp)
{
    if (stopTimestamp > 0) {
        d->stopTimestamp = stopTimestamp;
    } else {
        d->stopTimestamp = SyncTimer::instance()->jackPlayhead();
    }
    // Inform all the channels they should only be recording from (and including) that given timestamp
    for (AudioLevelsChannel *channel : qAsConst(d->audioLevelsChannels)) {
        channel->lastRecordingFrame = d->stopTimestamp;
    }
}

void AudioLevels::scheduleStopRecording(quint64 delay)
{
    TimerCommand *command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::ChannelRecorderStopOperation;
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
}

void AudioLevels::scheduleChannelRecorderStop(quint64 delay, int sketchpadTrack)
{
    TimerCommand* command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::ChannelRecorderStopOperation;
    command->parameter = 1;
    command->parameter2 = sketchpadTrack;
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
}

void AudioLevels::handleTimerCommand(quint64 timestamp, TimerCommand* command)
{
    if (command->operation == TimerCommand::ChannelRecorderStartOperation) {
        if (command->parameter == 1 && (-1 < command->parameter2 && command->parameter2 < 10)) {
            d->audioLevelsChannels[command->parameter2 + 3]->startCommandsRing.write(command, timestamp);
        }
    } else if (command->operation == TimerCommand::ChannelRecorderStopOperation) {
        if (command->parameter == 1 && (-1 < command->parameter2 && command->parameter2 < 10)) {
            d->audioLevelsChannels[command->parameter2 + 3]->lastRecordingFrame = timestamp;
        }
    }
}

QStringList AudioLevels::recordingFilenames() const
{
    QStringList filenames;
    filenames << (d->globalPlaybackWriter->shouldRecord() ? d->globalPlaybackWriter->fileName() : QString{});
    filenames << (d->portsRecorder->shouldRecord() ? d->portsRecorder->fileName() : QString{});
    for (DiskWriter *channelWriter : d->channelWriters) {
        filenames << (channelWriter->shouldRecord() ? channelWriter->fileName() : QString{});
    }
    return filenames;
}

bool AudioLevels::isRecording() const
{
    bool channelIsRecording{false};
    for (DiskWriter *channelWriter : d->channelWriters) {
        if (channelWriter->isRecording()) {
            channelIsRecording = true;
            break;
        }
    }
    return d->globalPlaybackWriter->isRecording() || d->portsRecorder->isRecording() || channelIsRecording;
}

AudioLevelsChannel * AudioLevels::audioLevelsChannel(const int& sketchpadTrack) const
{
    AudioLevelsChannel *instance{nullptr};
    if (-1 < sketchpadTrack && sketchpadTrack < ZynthboxTrackCount) {
        instance = d->audioLevelsChannels[sketchpadTrack + 3];
    }
    return instance;
}

AudioLevelsChannel * AudioLevels::systemCaptureAudioLevelsChannel() const
{
    return d->audioLevelsChannels[0];
}

AudioLevelsChannel * AudioLevels::globalAudioLevelsChannel() const
{
    return d->audioLevelsChannels[1];
}

AudioLevelsChannel * AudioLevels::portsRecorderAudioLevelsChannel() const
{
    return d->audioLevelsChannels[2];
}
