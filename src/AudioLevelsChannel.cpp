#include "AudioLevelsChannel.h"
#include "DiskWriter.h"
#include "AudioLevels.h"
#include "JackPassthroughAnalyser.h"
#include "JackPassthroughCompressor.h"
#include "JackPassthroughFilter.h"
#include "JackThreadAffinitySetter.h"
#include "MidiRouter.h"
#include "MidiRouterDeviceModel.h"

#include <cmath>
#include <QDebug>
#include <QVariantList>

#include <jack/jack.h>
#include <jack/midiport.h>

#define equaliserBandCount 6

class AudioLevelsChannel::Private {
public:
    Private(AudioLevelsChannel *q)
        : q(q)
    {}
    ~Private() {}
    AudioLevelsChannel *q{nullptr};
    ZynthboxBasics::Track sketchpadTrack{ZynthboxBasics::NoTrack};

    void setupEqualiserAndCompressor() {
        // Equaliser
        const float sampleRate = jack_get_sample_rate(q->jackClient);
        for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
            JackPassthroughFilter *newBand = new JackPassthroughFilter(equaliserBand, q);
            newBand->setSampleRate(sampleRate);
            QObject::connect(newBand, &JackPassthroughFilter::activeChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::soloedChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::dataChanged, q, &AudioLevelsChannel::equaliserDataChanged);
            equaliserSettings[equaliserBand] = newBand;
        }
        for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
            if (equaliserBand > 0) {
                equaliserSettings[equaliserBand]->setPrevious(equaliserSettings[equaliserBand - 1]);
            }
            if (equaliserBand < 5) {
                equaliserSettings[equaliserBand]->setNext(equaliserSettings[equaliserBand + 1]);
            }
        }
        // A bit awkward perhaps, but... this is a variadic template, and there's no indexed access, just a template one, so... alright
        equaliserSettings[0]->setDspObjects(&filterChain[0].get<0>(), &filterChain[1].get<0>());
        equaliserSettings[1]->setDspObjects(&filterChain[0].get<1>(), &filterChain[1].get<1>());
        equaliserSettings[2]->setDspObjects(&filterChain[0].get<2>(), &filterChain[1].get<2>());
        equaliserSettings[3]->setDspObjects(&filterChain[0].get<3>(), &filterChain[1].get<3>());
        equaliserSettings[4]->setDspObjects(&filterChain[0].get<4>(), &filterChain[1].get<4>());
        equaliserSettings[5]->setDspObjects(&filterChain[0].get<5>(), &filterChain[1].get<5>());
        equaliserFrequencies.resize(300);
        for (size_t i = 0; i < equaliserFrequencies.size(); ++i) {
            equaliserFrequencies[i] = 20.0 * std::pow(2.0, i / 30.0);
        }
        equaliserMagnitudes.resize(300);
        // Compressor
        static const QString channelNames[2]{"Left", "Right"};
        compressorSettings = new JackPassthroughCompressor(q);
        compressorSettings->setSampleRate(sampleRate);
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            sideChainInput[channelIndex] = jack_port_register(q->jackClient, QString("%1sidechainInput%2").arg(q->clientName).arg(channelNames[channelIndex]).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
            sideChainGain[channelIndex] = new jack_default_audio_sample_t[8192](); // TODO This is an awkward assumption, there has to be a sensible way to do this - jack should know this, right?
        }
    }

    bool equaliserEnabled{false};
    JackPassthroughFilter* equaliserSettings[equaliserBandCount];
    JackPassthroughFilter *soloedFilter{nullptr};
    bool updateMagnitudes{true};
    std::vector<double> equaliserMagnitudes;
    std::vector<double> equaliserFrequencies;
    QList<JackPassthroughAnalyser*> equaliserInputAnalysers{nullptr,nullptr};
    QList<JackPassthroughAnalyser*> equaliserOutputAnalysers{nullptr,nullptr};

    bool compressorEnabled{false};
    JackPassthroughCompressor *compressorSettings{nullptr};
    QString compressorSidechannelLeft, compressorSidechannelRight;
    bool compressorSidechannelEmpty[2]{true, true};
    void updateSidechannelLeftConnections() {
        // First disconnect anything currently connected to the left sidechannel input port
        jack_port_disconnect(q->jackClient, sideChainInput[0]);
        // Then connect up the new sidechain input
        static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        const QStringList portsToConnect{model->audioInSourceToJackPortNames(compressorSidechannelLeft, {}, sketchpadTrack)};
        for (const QString &port : portsToConnect) {
            connectPorts(port, QString("%1sidechainInputLeft").arg(q->clientName));
        }
        compressorSidechannelEmpty[0] = portsToConnect.isEmpty();
    }
    void updateSidechannelRightConnections() {
        // First disconnect anything currently connected to the right sidechannel input port
        jack_port_disconnect(q->jackClient, sideChainInput[1]);
        // Then connect up the new sidechain input
        static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        const QStringList portsToConnect{model->audioInSourceToJackPortNames(compressorSidechannelRight, {}, sketchpadTrack)};
        for (const QString &port : portsToConnect) {
            connectPorts(port, QString("%1sidechainInputRight").arg(q->clientName));
        }
        compressorSidechannelEmpty[1] = portsToConnect.isEmpty();
    }
    jack_port_t *sideChainInput[2]{nullptr};
    jack_default_audio_sample_t *sideChainGain[2]{nullptr};
    dsp::ProcessorChain<dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>, dsp::IIR::Filter<float>> filterChain[2];
    void bypassUpdater() {
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            soloedFilter = nullptr;
            for (JackPassthroughFilter *filter : equaliserSettings) {
                if (filter->soloed()) {
                    soloedFilter = filter;
                    break;
                }
            }
            // A bit awkward perhaps, but... this is a variadic template, and there's no indexed access, just a template one, so... alright
            filterChain[channelIndex].setBypassed<0>((soloedFilter == equaliserSettings[0]) == false && equaliserSettings[0]->active() == false);
            filterChain[channelIndex].setBypassed<1>((soloedFilter == equaliserSettings[1]) == false && equaliserSettings[1]->active() == false);
            filterChain[channelIndex].setBypassed<2>((soloedFilter == equaliserSettings[2]) == false && equaliserSettings[2]->active() == false);
            filterChain[channelIndex].setBypassed<3>((soloedFilter == equaliserSettings[3]) == false && equaliserSettings[3]->active() == false);
            filterChain[channelIndex].setBypassed<4>((soloedFilter == equaliserSettings[4]) == false && equaliserSettings[4]->active() == false);
            filterChain[channelIndex].setBypassed<5>((soloedFilter == equaliserSettings[5]) == false && equaliserSettings[5]->active() == false);
        }
    }

    void connectPorts(const QString &from, const QString &to) {
        int result = jack_connect(q->jackClient, from.toUtf8(), to.toUtf8());
        if (result == 0 || result == EEXIST) {
            // successful connection or connection already exists
            // qDebug() << Q_FUNC_INFO << "Successfully connected" << from << "to" << to << "(or connection already existed)";
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to connect" << from << "with" << to << "with error code" << result;
            // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
        }
    }
};

AudioLevelsChannel::AudioLevelsChannel(jack_client_t *client, const QString &clientName, const ZynthboxBasics::Track &sketchpadTrack, juce::AudioFormatManager& formatManagerToUse, juce::AudioThumbnailCache& cacheToUse, QObject *parent)
    : QObject(parent)
    , clientName(clientName)
    , d(new Private(this))
    , m_diskRecorder(new DiskWriter(this))
    , m_thumbnail(512, formatManagerToUse, cacheToUse)
{
    jackClient = client;
    leftPort = jack_port_register(jackClient, QString("%1-left_in").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    rightPort = jack_port_register(jackClient, QString("%1-right_in").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    leftOutPort = jack_port_register(jackClient, QString("%1-left_out").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    rightOutPort = jack_port_register(jackClient, QString("%1-right_out").arg(clientName).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    m_gainHandler = new GainHandler(this);
    m_gainHandler->setMinimumDecibel(-40.0);
    m_gainHandler->setMaximumDecibel(20.0);
    d->sketchpadTrack = sketchpadTrack;
    d->setupEqualiserAndCompressor();
    qInfo() << Q_FUNC_INFO << "Successfully created and set up" << clientName;
}

AudioLevelsChannel::~AudioLevelsChannel()
{
    delete d;
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
            doRecordingHandling(nframes, current_frames, next_frames);
            bool recordingStarted{false};
            quint64 timestamp{0};
            while (startCommandsRing.readHead->processed == false && startCommandsRing.readHead->timestamp < next_frames) {
                TimerCommand *command = startCommandsRing.read(&timestamp);
                firstRecordingFrame = timestamp;
                recordingStarted = true;
                const double sampleRate = jack_get_sample_rate(jackClient);
                if (m_diskRecorder->isRecording()) {
                    qDebug() << Q_FUNC_INFO << "We have been asked to start a new recording while one is already going on. Stopping the ongoing one first.";
                    m_diskRecorder->stop();
                }
                m_diskRecorder->startRecording(command->variantParameter.toString(), sampleRate);
            }
            if (recordingStarted) {
                doRecordingHandling(nframes, current_frames, next_frames);
            }
            bufferReadSize = nframes;

            // Send all the data from the input buffers into the output buffers
            leftOutBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(leftOutPort, nframes);
            rightOutBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(rightOutPort, nframes);
            const float gainAmount{m_gainHandler->operationalGain()};
            if (m_muted || m_gainHandler->gainAbsolute() == 0) {
                memset(leftOutBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                memset(rightOutBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
            } else if (m_panAmount == 0 && gainAmount == 1) {
                memcpy(leftOutBuffer, leftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                memcpy(rightOutBuffer, rightBuffer, nframes * sizeof(jack_default_audio_sample_t));
            } else {
                const float amountLeft{gainAmount * std::min(1 - m_panAmount, 1.0f)};
                const float amountRight{gainAmount * std::min(1 + m_panAmount, 1.0f)};
                juce::FloatVectorOperations::multiply(leftOutBuffer, leftBuffer, amountLeft, int(nframes));
                juce::FloatVectorOperations::multiply(rightOutBuffer, rightBuffer, amountRight, int(nframes));
            }

            // Analyse the output buffers to get the peak for each channel
            static const float fadePerFrame{0.0001f};
            const float fadeForPeriod{fadePerFrame * float(nframes)};
            const auto leftPeaks = juce::FloatVectorOperations::findMinAndMax(leftOutBuffer, int(nframes));
            const float leftPeak{qMin(1.0f, qMax(abs(leftPeaks.getStart()), abs(leftPeaks.getEnd())))};
            peakA = qMax(leftPeak, peakA - fadeForPeriod);
            const auto rightPeaks = juce::FloatVectorOperations::findMinAndMax(rightOutBuffer, int(nframes));
            const float rightPeak{qMin(1.0f, qMax(abs(rightPeaks.getStart()), abs(rightPeaks.getEnd())))};
            peakB = qMax(rightPeak, peakB - fadeForPeriod);

            jack_default_audio_sample_t *inputBuffers[2]{leftOutBuffer, rightOutBuffer};
            if (d->equaliserEnabled) {
                for (JackPassthroughFilter *filter : d->equaliserSettings) {
                    filter->updateCoefficients();
                }
                for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                    juce::AudioBuffer<float> bufferWrapper(&inputBuffers[channelIndex], 1, int(nframes));
                    juce::dsp::AudioBlock<float> block(bufferWrapper);
                    juce::dsp::ProcessContextReplacing<float> context(block);
                    if (d->equaliserInputAnalysers[channelIndex]) {
                        d->equaliserInputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
                    }
                    d->filterChain[channelIndex].process(context);
                    if (d->equaliserOutputAnalysers[channelIndex]) {
                        d->equaliserOutputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
                    }
                }
            }
            if (d->compressorEnabled) {
                float sidechainPeaks[2]{0.0f, 0.0f};
                float outputPeaks[2]{0.0f, 0.0f};
                float maxGainReduction[2]{0.0f, 0.0f};
                d->compressorSettings->updateParameters();
                for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                    // If we're not using a sidechannel for input, use what we're fed instead
                    jack_default_audio_sample_t *sideChainInputBuffer = d->compressorSidechannelEmpty[channelIndex] ? inputBuffers[channelIndex] : (jack_default_audio_sample_t *)jack_port_get_buffer(d->sideChainInput[channelIndex], nframes);
                    d->compressorSettings->compressors[channelIndex].getGainFromSidechainSignal(sideChainInputBuffer, d->sideChainGain[channelIndex], int(nframes));
                    juce::FloatVectorOperations::multiply(inputBuffers[channelIndex], d->sideChainGain[channelIndex], int(nframes));
                    // These three are essentially visualisation, so let's try and make sure we don't do the work unless someone's looking
                    if (d->compressorSettings->hasObservers()) {
                        sidechainPeaks[channelIndex] = juce::Decibels::decibelsToGain(d->compressorSettings->compressors[channelIndex].getMaxLevelInDecibels());
                        maxGainReduction[channelIndex] = juce::Decibels::decibelsToGain(juce::Decibels::gainToDecibels(juce::FloatVectorOperations::findMinimum(d->sideChainGain[channelIndex], int(nframes)) - d->compressorSettings->compressors[channelIndex].getMakeUpGain()));
                        outputPeaks[channelIndex] = juce::AudioBuffer<float>(&inputBuffers[channelIndex], 1, int(nframes)).getMagnitude(0, 0, int(nframes));
                    }
                }
                d->compressorSettings->updatePeaks(sidechainPeaks[0], sidechainPeaks[1], maxGainReduction[0], maxGainReduction[1], outputPeaks[0], outputPeaks[1]);
            } else if (d->compressorSettings) { // just to avoid doing any unnecessary hoop-jumping during construction
                d->compressorSettings->setPeaks(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
    return 0;
}

DiskWriter * AudioLevelsChannel::diskRecorder()
{
    return m_diskRecorder;
}

tracktion_engine::TracktionThumbnail * AudioLevelsChannel::thumbnail()
{
    return &m_thumbnail;
}

void AudioLevelsChannel::addChangeListener(ChangeListener* listener)
{
    m_thumbnailListenerCount++;
    m_thumbnail.addChangeListener(listener);
}

void AudioLevelsChannel::removeChangeListener(ChangeListener* listener)
{
    m_thumbnailListenerCount--;
    if (m_thumbnailListenerCount < 0) {
        qWarning() << Q_FUNC_INFO << this << "now has a negative amount of listeners, which means something has gone very wrong somewhere.";
    }
    m_thumbnail.removeChangeListener(listener);
}

bool AudioLevelsChannel::thumbnailHAnyListeners() const
{
    return m_thumbnailListenerCount > 0;
}

QObject * AudioLevelsChannel::gainHandler() const
{
    return m_gainHandler;
}

float AudioLevelsChannel::panAmount() const
{
    return m_panAmount;
}

void AudioLevelsChannel::setPanAmount(const float& newValue)
{
    if (m_panAmount != newValue) {
        m_panAmount = newValue;
        Q_EMIT panAmountChanged();
    }
}

bool AudioLevelsChannel::muted() const
{
    return m_muted;
}

void AudioLevelsChannel::setMuted(const bool& newValue)
{
    if (m_muted != newValue) {
        m_muted = newValue;
        Q_EMIT mutedChanged();
    }
}

bool AudioLevelsChannel::equaliserEnabled() const
{
    return d->equaliserEnabled;
}

void AudioLevelsChannel::setEqualiserEnabled(const bool& equaliserEnabled)
{
    if (d->equaliserEnabled != equaliserEnabled) {
        d->equaliserEnabled = equaliserEnabled;
        Q_EMIT equaliserEnabledChanged();
    }
}

QVariantList AudioLevelsChannel::equaliserSettings() const
{
    QVariantList settings;
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
        settings.append(QVariant::fromValue<QObject*>(filter));
    }
    return settings;
}

QObject * AudioLevelsChannel::equaliserNearestToFrequency(const float& frequency) const
{
    JackPassthroughFilter *nearest{nullptr};
    QMap<float, JackPassthroughFilter*> sortedFilters;
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
        sortedFilters.insert(filter->frequency(), filter);
    }
    QMap<float, JackPassthroughFilter*>::const_iterator filterIterator(sortedFilters.constBegin());
    float previousFrequency{0};
    JackPassthroughFilter *previousFilter{nullptr};
    while (filterIterator != sortedFilters.constEnd()) {
        float currentFrequency = filterIterator.key();
        nearest = filterIterator.value();
        if (frequency <= currentFrequency) {
            if (previousFilter) {
                // Between two filters, so test which one we're closer to. If it's nearest to the previous filter, reset nearest to that (otherwise it's already the nearest)
                float halfWayPoint{currentFrequency - ((currentFrequency - previousFrequency) / 2)};
                if (frequency < halfWayPoint) {
                    nearest = previousFilter;
                }
            }
            // We've found our filter, so stop looking :)
            break;
        } else {
            previousFrequency = currentFrequency;
            previousFilter = nearest;
        }
        ++filterIterator;
    }
    return nearest;
}

const std::vector<double> & AudioLevelsChannel::equaliserMagnitudes() const
{
    if (d->updateMagnitudes) {
        // Fill the magnitudes with a flat 1.0 of no change
        std::fill(d->equaliserMagnitudes.begin(), d->equaliserMagnitudes.end(), 1.0f);

        if (d->soloedFilter) {
            // If we've got a soloed band, only show that one
            juce::FloatVectorOperations::multiply(d->equaliserMagnitudes.data(), d->soloedFilter->magnitudes().data(), static_cast<int>(d->equaliserMagnitudes.size()));
        } else {
            for (size_t bandIndex = 0; bandIndex < equaliserBandCount; ++bandIndex) {
                if (d->equaliserSettings[bandIndex]->active()) {
                    juce::FloatVectorOperations::multiply(d->equaliserMagnitudes.data(), d->equaliserSettings[bandIndex]->magnitudes().data(), static_cast<int>(d->equaliserMagnitudes.size()));
                }
            }
        }
    }
    return d->equaliserMagnitudes;
}

const std::vector<double> & AudioLevelsChannel::equaliserFrequencies() const
{
    return d->equaliserFrequencies;
}

void AudioLevelsChannel::equaliserCreateFrequencyPlot(QPolygonF &p, const QRect bounds, float pixelsPerDouble)
{
    equaliserMagnitudes(); // Just make sure our magnitudes are updated
    const auto xFactor = static_cast<double>(bounds.width()) / d->equaliserFrequencies.size();
    for (size_t i = 0; i < d->equaliserFrequencies.size(); ++i) {
        p <<  QPointF(float (bounds.x() + i * xFactor), float(d->equaliserMagnitudes[i] > 0 ? bounds.center().y() - pixelsPerDouble * std::log(d->equaliserMagnitudes[i]) / std::log (2.0) : bounds.bottom()));
    }
}

void AudioLevelsChannel::setEqualiserInputAnalysers(QList<JackPassthroughAnalyser*>& equaliserInputAnalysers) const
{
    d->equaliserInputAnalysers = equaliserInputAnalysers;
}

void AudioLevelsChannel::setEqualiserOutputAnalysers(QList<JackPassthroughAnalyser*>& equaliserOutputAnalysers) const
{
    d->equaliserOutputAnalysers = equaliserOutputAnalysers;
}

bool AudioLevelsChannel::compressorEnabled() const
{
    return d->compressorEnabled;
}

void AudioLevelsChannel::setCompressorEnabled(const bool& compressorEnabled)
{
    if (d->compressorEnabled != compressorEnabled) {
        d->compressorEnabled = compressorEnabled;
        Q_EMIT compressorEnabledChanged();
    }
}

QString AudioLevelsChannel::compressorSidechannelLeft() const
{
    return d->compressorSidechannelLeft;
}

void AudioLevelsChannel::setCompressorSidechannelLeft(const QString& compressorSidechannelLeft)
{
    if (d->compressorSidechannelLeft != compressorSidechannelLeft) {
        d->compressorSidechannelLeft = compressorSidechannelLeft;
        Q_EMIT compressorSidechannelLeftChanged();
        d->updateSidechannelLeftConnections();
    }
}

QString AudioLevelsChannel::compressorSidechannelRight() const
{
    return d->compressorSidechannelRight;
}

void AudioLevelsChannel::setCompressorSidechannelRight(const QString& compressorSidechannelRight)
{
    if (d->compressorSidechannelRight != compressorSidechannelRight) {
        d->compressorSidechannelRight = compressorSidechannelRight;
        Q_EMIT compressorSidechannelRightChanged();
        d->updateSidechannelRightConnections();
    }
}

QObject * AudioLevelsChannel::compressorSettings() const
{
    return d->compressorSettings;
}

void AudioLevelsChannel::doRecordingHandling(jack_nframes_t nframes, jack_nframes_t current_frames, jack_nframes_t next_frames)
{
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
        }
    }
}
