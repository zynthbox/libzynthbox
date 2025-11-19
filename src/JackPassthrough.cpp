/*
  ==============================================================================

    JackPassthrough.cpp
    Created: 26 Sep 2022
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#include "JackPassthrough.h"
#include "GainHandler.h"
#include "JackPassthroughAnalyser.h"
#include "JackPassthroughCompressor.h"
#include "JackPassthroughFilter.h"
#include "JackThreadAffinitySetter.h"
#include "MidiRouter.h"
#include "MidiRouterDeviceModel.h"

#include <QDebug>
#include <QGlobalStatic>
#include <QPolygonF>

#include <jack/jack.h>
#include <jack/midiport.h>

#define equaliserBandCount 6

struct JackPassthroughAggregate {
public:
    JackPassthroughAggregate(jack_client_t *client)
        : client(client)
    {}
    ~JackPassthroughAggregate() {
        if (client) {
            jack_client_close(client);
        }
    }
    jack_client_t *client{nullptr};
    QList<JackPassthroughPrivate*> passthroughs;
};

typedef QHash<QString, JackPassthroughAggregate*> JackClientHash;
Q_GLOBAL_STATIC(JackClientHash, jackPassthroughClients)

class JackPassthroughPrivate {
public:
    JackPassthroughPrivate(const QString &clientName, const bool &dryOutPortsEnabled, const bool &wetOutFx1PortsEnabled, const bool &wetOutFx2PortsEnabled, const bool &wetInPortsEnabled, const float &minimumDB, const float &maximumDB, JackPassthrough *q);
    ~JackPassthroughPrivate() {
        JackPassthroughAggregate *aggregate{nullptr};
        QString key;
        for (JackPassthroughAggregate *needle : jackPassthroughClients->values()) {
            if (needle->passthroughs.contains(this)) {
                key = jackPassthroughClients->key(needle);
                aggregate = needle;
                return;
            }
        }
        if (aggregate) {
            aggregate->passthroughs.removeAll(this);
            if (aggregate->passthroughs.count() == 0) {
                jackPassthroughClients->remove(key);
                delete aggregate;
            }
        }
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            delete sideChainGain[channelIndex];
        }
    }
    void registerPorts();
    JackPassthrough *q{nullptr};
    ZynthboxBasics::Track sketchpadTrack{ZynthboxBasics::NoTrack};
    QString actualClientName;
    QString portPrefix;
    GainHandler *dryGainHandler{nullptr};
    GainHandler *wetFx1GainHandler{nullptr};
    GainHandler *wetFx2GainHandler{nullptr};
    GainHandler *wetDryMixGainHandler{nullptr};
    float dryAmount{1.0f};
    float wetFx1Amount{1.0f};
    float wetFx2Amount{1.0f};
    float dryWetMixAmount{-1.0f};
    float panAmount{0.0f};
    bool bypass{false};
    bool muted{false};

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
        if (createPorts) {
            jack_port_disconnect(client, sideChainInput[0]);
        }
        // Then connect up the new sidechain input
        static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        const QStringList portsToConnect{model->audioInSourceToJackPortNames(compressorSidechannelLeft, {}, sketchpadTrack)};
        if (createPorts) {
            for (const QString &port : portsToConnect) {
                connectPorts(port, QString("%1:%2sidechainInputLeft").arg(actualClientName).arg(portPrefix));
            }
        }
        compressorSidechannelEmpty[0] = portsToConnect.isEmpty();
    }
    void updateSidechannelRightConnections() {
        // First disconnect anything currently connected to the right sidechannel input port
        if (createPorts) {
            jack_port_disconnect(client, sideChainInput[1]);
        }
        // Then connect up the new sidechain input
        static MidiRouterDeviceModel *model = qobject_cast<MidiRouterDeviceModel*>(MidiRouter::instance()->model());
        const QStringList portsToConnect{model->audioInSourceToJackPortNames(compressorSidechannelRight, {}, sketchpadTrack)};
        if (createPorts) {
            for (const QString &port : portsToConnect) {
                connectPorts(port, QString("%1:%2sidechainInputRight").arg(actualClientName).arg(portPrefix));
            }
        }
        compressorSidechannelEmpty[1] = portsToConnect.isEmpty();
    }

    bool createPorts{false};

    bool dryOutPortsEnabled{true};
    bool wetOutFx1PortsEnabled{true};
    bool wetOutFx2PortsEnabled{true};
    bool wetInPortsEnabled{false};
    jack_default_audio_sample_t channelSampleLeft;
    jack_default_audio_sample_t channelSampleRight;

    jack_client_t *client{nullptr};
    jack_port_t *inputLeft{nullptr};
    jack_port_t *inputRight{nullptr};
    jack_port_t *wetInputLeft{nullptr};
    jack_port_t *wetInputRight{nullptr};
    jack_port_t *dryOutLeft{nullptr};
    jack_port_t *dryOutRight{nullptr};
    jack_port_t *wetOutFx1Left{nullptr};
    jack_port_t *wetOutFx1Right{nullptr};
    jack_port_t *wetOutFx2Left{nullptr};
    jack_port_t *wetOutFx2Right{nullptr};
    jack_default_audio_sample_t *inputLeftBuffer{nullptr};
    jack_default_audio_sample_t *inputRightBuffer{nullptr};
    jack_default_audio_sample_t *wetInputLeftBuffer{nullptr};
    jack_default_audio_sample_t *wetInputRightBuffer{nullptr};
    jack_default_audio_sample_t *dryOutLeftBuffer{nullptr};
    jack_default_audio_sample_t *dryOutRightBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx1LeftBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx1RightBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx2LeftBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx2RightBuffer{nullptr};

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

    int process(jack_nframes_t nframes) {
        if (createPorts && inputLeft && inputRight) {
            inputLeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(inputLeft, nframes);
            inputRightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(inputRight, nframes);
            if (wetInPortsEnabled) {
                wetInputLeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetInputLeft, nframes);
                wetInputRightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetInputRight, nframes);
            }
            if (dryOutPortsEnabled) {
                dryOutLeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(dryOutLeft, nframes);
                dryOutRightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(dryOutRight, nframes);
            }
            if (wetOutFx1PortsEnabled) {
                wetOutFx1LeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetOutFx1Left, nframes);
                wetOutFx1RightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetOutFx1Right, nframes);
            }
            if (wetOutFx2PortsEnabled) {
                wetOutFx2LeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetOutFx2Left, nframes);
                wetOutFx2RightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(wetOutFx2Right, nframes);
            }

            if (bypass) {
                if (wetOutFx1PortsEnabled) {
                    memset(wetOutFx1LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    memset(wetOutFx1RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                }
                if (wetOutFx2PortsEnabled) {
                    memset(wetOutFx2LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    memset(wetOutFx2RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                }
            }
            if (muted) {
                if (dryOutPortsEnabled) {
                    memset(dryOutLeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    memset(dryOutRightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                }
                if (wetOutFx1PortsEnabled) {
                    memset(wetOutFx1LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    memset(wetOutFx1RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                }
                if (wetOutFx2PortsEnabled) {
                    memset(wetOutFx2LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    memset(wetOutFx2RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                }
            } else {
                jack_default_audio_sample_t *inputBuffers[2]{inputLeftBuffer, inputRightBuffer};
                if (wetInPortsEnabled) {
                    // If the wet inputs are enabled, it means we mix down before applying eq/compressor and outputting the result to all enabled outputs
                    // First inline-adjust the input buffer according to the dry level and pan amount (if that is anything other than 1.0, or if the dry amount is 0, in which case dry should be zeroed anyway)
                    if (dryAmount == 0) {
                        memset(inputLeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                        memset(inputRightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                    } else if (dryAmount != 1.0 || panAmount != 0.0) {
                        const float dryAmountLeft{dryAmount * std::min(1 - panAmount, 1.0f)};
                        const float dryAmountRight{dryAmount * std::min(1 + panAmount, 1.0f)};
                        juce::FloatVectorOperations::multiply(inputLeftBuffer, dryAmountLeft, int(nframes));
                        juce::FloatVectorOperations::multiply(inputRightBuffer, dryAmountRight, int(nframes));
                    }
                    // Adjust the wet input buffer according to the wet fx1 level and pan amount (if that is anything other than 1.0) and apply directly to the matching input buffers
                    if (wetFx1Amount == 0 || bypass) {
                        // Do nothing if wet is 1 (as this just means it shouldn't be used)
                    } else {
                        const float wetAmountLeft{wetFx1Amount * std::min(1 - panAmount, 1.0f)};
                        const float wetAmountRight{wetFx1Amount * std::min(1 + panAmount, 1.0f)};
                        juce::FloatVectorOperations::addWithMultiply(inputLeftBuffer, wetInputLeftBuffer, wetAmountLeft, int(nframes));
                        juce::FloatVectorOperations::addWithMultiply(inputRightBuffer, wetInputRightBuffer, wetAmountRight, int(nframes));
                    }
                }
                if (equaliserEnabled) {
                    for (JackPassthroughFilter *filter : equaliserSettings) {
                        filter->updateCoefficients();
                    }
                    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                        juce::AudioBuffer<float> bufferWrapper(&inputBuffers[channelIndex], 1, int(nframes));
                        juce::dsp::AudioBlock<float> block(bufferWrapper);
                        juce::dsp::ProcessContextReplacing<float> context(block);
                        if (equaliserInputAnalysers[channelIndex]) {
                            equaliserInputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
                        }
                        filterChain[channelIndex].process(context);
                        if (equaliserOutputAnalysers[channelIndex]) {
                            equaliserOutputAnalysers[channelIndex]->addAudioData(bufferWrapper, 0, 1);
                        }
                    }
                }
                if (compressorEnabled) {
                    float sidechainPeaks[2]{0.0f, 0.0f};
                    float outputPeaks[2]{0.0f, 0.0f};
                    float maxGainReduction[2]{0.0f, 0.0f};
                    compressorSettings->updateParameters();
                    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                        // If we're not using a sidechannel for input, use what we're fed instead
                        jack_default_audio_sample_t *sideChainInputBuffer = compressorSidechannelEmpty[channelIndex] ? inputBuffers[channelIndex] : (jack_default_audio_sample_t *)jack_port_get_buffer(sideChainInput[channelIndex], nframes);
                        compressorSettings->compressors[channelIndex].getGainFromSidechainSignal(sideChainInputBuffer, sideChainGain[channelIndex], int(nframes));
                        juce::FloatVectorOperations::multiply(inputBuffers[channelIndex], sideChainGain[channelIndex], int(nframes));
                        // These three are essentially visualisation, so let's try and make sure we don't do the work unless someone's looking
                        if (compressorSettings->hasObservers()) {
                            sidechainPeaks[channelIndex] = juce::Decibels::decibelsToGain(compressorSettings->compressors[channelIndex].getMaxLevelInDecibels());
                            maxGainReduction[channelIndex] = juce::Decibels::decibelsToGain(juce::Decibels::gainToDecibels(juce::FloatVectorOperations::findMinimum(sideChainGain[channelIndex], int(nframes)) - compressorSettings->compressors[channelIndex].getMakeUpGain()));
                            outputPeaks[channelIndex] = juce::AudioBuffer<float>(&inputBuffers[channelIndex], 1, int(nframes)).getMagnitude(0, 0, int(nframes));
                        }
                    }
                    compressorSettings->updatePeaks(sidechainPeaks[0], sidechainPeaks[1], maxGainReduction[0], maxGainReduction[1], outputPeaks[0], outputPeaks[1]);
                } else if (compressorSettings) { // just to avoid doing any unnecessary hoop-jumping during construction
                    compressorSettings->setPeaks(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                }
                if (wetInPortsEnabled) {
                    // If the wet input ports are enabled, our mixdown is done before applying the effects, so we should just copy whatever is in the processed input buffer to all enabled outputs
                    // Usually there would be only one output buffer actually enabled, but technically there's nothing to stop you having more, so...
                    if (dryOutPortsEnabled) {
                        memcpy(dryOutLeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        memcpy(dryOutRightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                    }
                    if (wetOutFx1PortsEnabled) {
                        memcpy(wetOutFx1LeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        memcpy(wetOutFx1RightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                    }
                    if (wetOutFx2PortsEnabled) {
                        memcpy(wetOutFx2LeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        memcpy(wetOutFx2RightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                    }
                } else {
                    bool outputDry{true};
                    bool outputWetFx1{true};
                    bool outputWetFx2{true};
                    if (dryOutPortsEnabled) {
                        if (panAmount == 0 && dryAmount == 0) {
                            outputDry = false;
                            memset(dryOutLeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                            memset(dryOutRightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                        } else if (panAmount == 0 && dryAmount == 1) {
                            outputDry = false;
                            memcpy(dryOutLeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                            memcpy(dryOutRightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        }
                    }
                    if (wetOutFx1PortsEnabled && bypass == false) {
                        if (panAmount == 0 && wetFx1Amount == 0) {
                            outputWetFx1 = false;
                            memset(wetOutFx1LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                            memset(wetOutFx1RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                        } else if (panAmount == 0 && wetFx1Amount == 1) {
                            outputWetFx1 = false;
                            memcpy(wetOutFx1LeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                            memcpy(wetOutFx1RightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        }
                    }
                    if (wetOutFx2PortsEnabled && bypass == false) {
                        if (panAmount == 0 && wetFx2Amount == 0) {
                            outputWetFx2 = false;
                            memset(wetOutFx2LeftBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                            memset(wetOutFx2RightBuffer, 0, nframes * sizeof(jack_default_audio_sample_t));
                        } else if (panAmount == 0 && wetFx2Amount == 1) {
                            outputWetFx2 = false;
                            memcpy(wetOutFx2LeftBuffer, inputLeftBuffer, nframes * sizeof(jack_default_audio_sample_t));
                            memcpy(wetOutFx2RightBuffer, inputRightBuffer, nframes * sizeof(jack_default_audio_sample_t));
                        }
                    }
                    if (panAmount != 0 || outputDry || outputWetFx1 || outputWetFx2) {
                        if (dryOutPortsEnabled && outputDry) {
                            const float dryAmountLeft{dryAmount * std::min(1 - panAmount, 1.0f)};
                            const float dryAmountRight{dryAmount * std::min(1 + panAmount, 1.0f)};
                            juce::FloatVectorOperations::multiply(dryOutLeftBuffer, inputLeftBuffer, dryAmountLeft, int(nframes));
                            juce::FloatVectorOperations::multiply(dryOutRightBuffer, inputRightBuffer, dryAmountRight, int(nframes));
                        }
                        if (wetOutFx1PortsEnabled && outputWetFx1 && bypass == false) {
                            const float wetFx1AmountLeft{wetFx1Amount * std::min(1 - panAmount, 1.0f)};
                            const float wetFx1AmountRight{wetFx1Amount * std::min(1 + panAmount, 1.0f)};
                            juce::FloatVectorOperations::multiply(wetOutFx1LeftBuffer, inputLeftBuffer, wetFx1AmountLeft, int(nframes));
                            juce::FloatVectorOperations::multiply(wetOutFx1RightBuffer, inputRightBuffer, wetFx1AmountRight, int(nframes));
                        }
                        if (wetOutFx2PortsEnabled && outputWetFx2 && bypass == false) {
                            const float wetFx2AmountLeft{wetFx2Amount * std::min(1 - panAmount, 1.0f)};
                            const float wetFx2AmountRight{wetFx2Amount * std::min(1 + panAmount, 1.0f)};
                            juce::FloatVectorOperations::multiply(wetOutFx2LeftBuffer, inputLeftBuffer, wetFx2AmountLeft, int(nframes));
                            juce::FloatVectorOperations::multiply(wetOutFx2RightBuffer, inputRightBuffer, wetFx2AmountRight, int(nframes));
                        }
                    }
                }
            }
        }
        return 0;
    }

    void connectPorts(const QString &from, const QString &to) {
        int result = jack_connect(client, from.toUtf8(), to.toUtf8());
        if (result == 0 || result == EEXIST) {
            // successful connection or connection already exists
            // qDebug() << Q_FUNC_INFO << "Successfully connected" << from << "to" << to << "(or connection already existed)";
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to connect" << from << "with" << to << "with error code" << result;
            // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
        }
    }
};

static int jackPassthroughProcess(jack_nframes_t nframes, void* arg) {
    JackPassthroughAggregate *aggregate = static_cast<JackPassthroughAggregate*>(arg);
    for (JackPassthroughPrivate *passthrough : qAsConst(aggregate->passthroughs)) {
        if (passthrough) {
            passthrough->process(nframes);
        }
    }
    return 0;
}

JackPassthroughPrivate::JackPassthroughPrivate(const QString &clientName, const bool &dryOutPortsEnabled, const bool &wetOutFx1PortsEnabled, const bool &wetOutFx2PortsEnabled, const bool &wetInPortsEnabled, const float &minimumDB, const float &maximumDB, JackPassthrough *q)
    : q(q)
{
    jack_status_t real_jack_status{};
    actualClientName = clientName;
    this->dryOutPortsEnabled = dryOutPortsEnabled;
    this->wetOutFx1PortsEnabled = wetOutFx1PortsEnabled;
    this->wetOutFx2PortsEnabled = wetOutFx2PortsEnabled;
    this->wetInPortsEnabled = wetInPortsEnabled;

    dryGainHandler = new GainHandler(q);
    dryGainHandler->setMinimumDecibel(minimumDB);
    dryGainHandler->setMaximumDecibel(maximumDB);
    QObject::connect(dryGainHandler, &GainHandler::operationalGainChanged, q, [this](){ dryAmount = dryGainHandler->operationalGain(); });
    QObject::connect(dryGainHandler, &GainHandler::operationalGainChanged, q, &JackPassthrough::dryAmountChanged);
    wetFx1GainHandler = new GainHandler(q);
    wetFx1GainHandler->setMinimumDecibel(minimumDB);
    wetFx1GainHandler->setMaximumDecibel(maximumDB);
    QObject::connect(wetFx1GainHandler, &GainHandler::operationalGainChanged, q, [this](){ wetFx1Amount = wetFx1GainHandler->operationalGain(); });
    QObject::connect(wetFx1GainHandler, &GainHandler::operationalGainChanged, q, &JackPassthrough::wetFx1AmountChanged);
    wetFx2GainHandler = new GainHandler(q);
    wetFx2GainHandler->setMinimumDecibel(minimumDB);
    wetFx2GainHandler->setMaximumDecibel(maximumDB);
    QObject::connect(wetFx2GainHandler, &GainHandler::operationalGainChanged, q, [this](){ wetFx2Amount = wetFx2GainHandler->operationalGain(); });
    QObject::connect(wetFx2GainHandler, &GainHandler::operationalGainChanged, q, &JackPassthrough::wetFx2AmountChanged);
    // Set respective output amount to 0 if ports are not enabled
    if (!dryOutPortsEnabled) {
        dryGainHandler->setGainAbsolute(0.0f);
    }
    if (!wetOutFx1PortsEnabled) {
        wetFx1GainHandler->setGainAbsolute(0.0f);
    }
    if (!wetOutFx2PortsEnabled) {
         wetFx2GainHandler->setGainAbsolute(0.0f);
    }
    // Calculation assistance tool for doing the wet/dry mix management
    wetDryMixGainHandler = new GainHandler(q);
    wetDryMixGainHandler->setMaximumDecibel(0.0f);

    if (clientName.contains(":")) {
        const QStringList splitName{clientName.split(":")};
        actualClientName = splitName[0];
        portPrefix = QString("%1-").arg(splitName[1]);
    }
    JackPassthroughAggregate *aggregate{nullptr};
    if (jackPassthroughClients->contains(actualClientName)) {
        aggregate = jackPassthroughClients->value(actualClientName);
        client = aggregate->client;
    } else {
        client = jack_client_open(actualClientName.toUtf8(), JackNullOption, &real_jack_status);
        if (client) {
            aggregate = new JackPassthroughAggregate(client);
            jackPassthroughClients->insert(actualClientName, aggregate);
            // Set the process callback.
            if (jack_set_process_callback(client, jackPassthroughProcess, aggregate) == 0) {
                if (jack_activate(client) == 0) {
                    // Success! Now we just kind of sit here and do the thing until we're done or whatever
                    zl_set_jack_client_affinity(client);
                } else {
                    qWarning() << "JackPasstrough Client: Failed to activate the Jack client for" << clientName;
                }
            } else {
                qWarning() << "JackPasstrough Client: Failed to set Jack processing callback for" << clientName;
            }
        } else {
            qWarning() << "JackPasstrough Client: Failed to create Jack client for" << clientName;
        }
    }
    if (createPorts) {
        registerPorts();
    }
    if (aggregate) {
        // Equaliser
        const float sampleRate = jack_get_sample_rate(client);
        for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
            JackPassthroughFilter *newBand = new JackPassthroughFilter(equaliserBand, q);
            newBand->setSampleRate(sampleRate);
            QObject::connect(newBand, &JackPassthroughFilter::activeChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::soloedChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::dataChanged, q, &JackPassthrough::equaliserDataChanged);
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
            sideChainGain[channelIndex] = new jack_default_audio_sample_t[8192](); // TODO This is an awkward assumption, there has to be a sensible way to do this - jack should know this, right?
        }
    }
}

void JackPassthroughPrivate::registerPorts()
{
    JackPassthroughAggregate *aggregate{jackPassthroughClients->value(actualClientName)};
    if (aggregate) {
        bool dryOutPortsRegistrationFailed{false};
        bool wetOutFx1PortsRegistrationFailed{false};
        bool wetOutFx2PortsRegistrationFailed{false};
        inputLeft = jack_port_register(client, QString("%1inputLeft").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        inputRight = jack_port_register(client, QString("%1inputRight").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (wetInPortsEnabled) {
            wetInputLeft = jack_port_register(client, QString("%1wetInputLeft").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
            wetInputRight = jack_port_register(client, QString("%1wetInputRight").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        }
        if (dryOutPortsEnabled) {
            dryOutLeft = jack_port_register(client, QString("%1dryOutLeft").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            dryOutRight = jack_port_register(client, QString("%1dryOutRight").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            dryOutPortsRegistrationFailed = dryOutLeft == NULL || dryOutRight == NULL;
        }
        if (wetOutFx1PortsEnabled) {
            wetOutFx1Left = jack_port_register(client, QString("%1wetOutFx1Left").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            wetOutFx1Right = jack_port_register(client, QString("%1wetOutFx1Right").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            wetOutFx1PortsRegistrationFailed = wetOutFx1Left == NULL || wetOutFx1Right == NULL;
        }
        if (wetOutFx2PortsEnabled) {
            wetOutFx2Left = jack_port_register(client, QString("%1wetOutFx2Left").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            wetOutFx2Right = jack_port_register(client, QString("%1wetOutFx2Right").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            wetOutFx2PortsRegistrationFailed = wetOutFx2Left == NULL || wetOutFx2Right == NULL;
        }
        if (inputLeft != NULL && inputRight != NULL && !dryOutPortsRegistrationFailed && !wetOutFx1PortsRegistrationFailed && !wetOutFx2PortsRegistrationFailed) {
            if (!aggregate->passthroughs.contains(this)) {
                aggregate->passthroughs << this;
            }
        } else {
            qWarning() << "JackPasstrough Client: Failed to register ports for" << actualClientName << portPrefix;
        }
        // Compressor
        static const QString channelNames[2]{"Left", "Right"};
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            sideChainInput[channelIndex] = jack_port_register(client, QString("%1sidechainInput%2").arg(portPrefix).arg(channelNames[channelIndex]).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        }
    }
}


JackPassthrough::JackPassthrough(const QString &clientName, QObject *parent, const bool &dryOutPortsEnabled, const bool &wetOutFx1PortsEnabled, const bool &wetOutFx2PortsEnabled, const bool &wetInPortsEnabled, const float &minimumDB, const float &maximumDB)
    : QObject(parent)
    , d(new JackPassthroughPrivate(clientName, dryOutPortsEnabled, wetOutFx1PortsEnabled, wetOutFx2PortsEnabled, wetInPortsEnabled, minimumDB, maximumDB, this))
{
}

JackPassthrough::~JackPassthrough()
{
    delete d;
}

void JackPassthrough::setSketchpadTrack(const ZynthboxBasics::Track& sketchpadTrack)
{
    d->sketchpadTrack = sketchpadTrack;
}

const bool & JackPassthrough::bypass() const
{
    return d->bypass;
}

void JackPassthrough::setBypass(const bool& bypass)
{
    if (d->bypass != bypass) {
        d->bypass = bypass;
        Q_EMIT bypassChanged();
    }
}

const bool &JackPassthrough::muted() const
{
    return d->muted;
}

void JackPassthrough::setMuted(const bool &newValue)
{
    if (d->muted != newValue) {
        d->muted = newValue;
        Q_EMIT mutedChanged();
    }
}

float JackPassthrough::dryAmount() const
{
    return d->dryGainHandler->gain();
}

void JackPassthrough::setDryAmount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->dryGainHandler->gain() != newValue) {
        d->dryGainHandler->setGain(newValue);
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT dryAmountChanged();
    }
}

float JackPassthrough::wetFx1Amount() const
{
    return d->wetFx1GainHandler->gain();
}

void JackPassthrough::setWetFx1Amount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->wetFx1GainHandler->gain() != newValue) {
        d->wetFx1GainHandler->setGain(newValue);
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT wetFx1AmountChanged();
    }
}

float JackPassthrough::wetFx2Amount() const
{
    return d->wetFx2GainHandler->gain();
}

void JackPassthrough::setWetFx2Amount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->wetFx2GainHandler->gain() != newValue) {
        d->wetFx2GainHandler->setGain(newValue);
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT wetFx2AmountChanged();
    }
}

QObject * JackPassthrough::dryGainHandler() const
{
    return d->dryGainHandler;
}

QObject * JackPassthrough::wetFx1GainHandler() const
{
    return d->wetFx1GainHandler;
}

QObject * JackPassthrough::wetFx2GainHandler() const
{
    return d->wetFx2GainHandler;
}

float JackPassthrough::dryWetMixAmount() const
{
    return d->dryWetMixAmount;
}

void JackPassthrough::setDryWetMixAmount(const float &newValue)
{
    if (d->dryWetMixAmount != newValue) {
        d->dryWetMixAmount = newValue;
        if (newValue >= 0.0f && newValue < 1.0f) {
            d->dryGainHandler->setGain(1.0f);
            d->wetDryMixGainHandler->setGainAbsolute(newValue);
            d->wetFx1GainHandler->setGain(d->wetDryMixGainHandler->gain());
            d->wetFx2GainHandler->setGain(d->wetDryMixGainHandler->gain());
        } else if (newValue == 1.0f) {
            d->dryGainHandler->setGain(1.0f);
            d->wetFx1GainHandler->setGain(1.0f);
            d->wetFx2GainHandler->setGain(1.0f);
        } else if (newValue > 1.0f && newValue <= 2.0f) {
            d->wetDryMixGainHandler->setGainAbsolute(2.0f - newValue);
            d->dryGainHandler->setGain(d->wetDryMixGainHandler->gain());
            d->wetFx1GainHandler->setGain(1.0f);
            d->wetFx2GainHandler->setGain(1.0f);
        }
        Q_EMIT dryWetMixAmountChanged();
    }
}

float JackPassthrough::panAmount() const
{
    return d->panAmount;
}

void JackPassthrough::setPanAmount(const float &newValue)
{
    if (d->panAmount != newValue) {
        d->panAmount = newValue;
        Q_EMIT panAmountChanged();
    }
}

bool JackPassthrough::equaliserEnabled() const
{
    return d->equaliserEnabled;
}

void JackPassthrough::setEqualiserEnabled(const bool& equaliserEnabled)
{
    if (d->equaliserEnabled != equaliserEnabled) {
        d->equaliserEnabled = equaliserEnabled;
        Q_EMIT equaliserEnabledChanged();
    }
}

QVariantList JackPassthrough::equaliserSettings() const
{
    QVariantList settings;
    for (JackPassthroughFilter *filter : d->equaliserSettings) {
        settings.append(QVariant::fromValue<QObject*>(filter));
    }
    return settings;
}

QObject * JackPassthrough::equaliserNearestToFrequency(const float& frequency) const
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

const std::vector<double> & JackPassthrough::equaliserMagnitudes() const
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

const std::vector<double> & JackPassthrough::equaliserFrequencies() const
{
    return d->equaliserFrequencies;
}

void JackPassthrough::equaliserCreateFrequencyPlot(QPolygonF &p, const QRect bounds, float pixelsPerDouble)
{
    equaliserMagnitudes(); // Just make sure our magnitudes are updated
    const auto xFactor = static_cast<double>(bounds.width()) / d->equaliserFrequencies.size();
    for (size_t i = 0; i < d->equaliserFrequencies.size(); ++i) {
        p <<  QPointF(float (bounds.x() + i * xFactor), float(d->equaliserMagnitudes[i] > 0 ? bounds.center().y() - pixelsPerDouble * std::log(d->equaliserMagnitudes[i]) / std::log (2.0) : bounds.bottom()));
    }
}

void JackPassthrough::setEqualiserInputAnalysers(QList<JackPassthroughAnalyser*>& equaliserInputAnalysers) const
{
    d->equaliserInputAnalysers = equaliserInputAnalysers;
}

void JackPassthrough::setEqualiserOutputAnalysers(QList<JackPassthroughAnalyser*>& equaliserOutputAnalysers) const
{
    d->equaliserOutputAnalysers = equaliserOutputAnalysers;
}

bool JackPassthrough::compressorEnabled() const
{
    return d->compressorEnabled;
}

void JackPassthrough::setCompressorEnabled(const bool& compressorEnabled)
{
    if (d->compressorEnabled != compressorEnabled) {
        d->compressorEnabled = compressorEnabled;
        Q_EMIT compressorEnabledChanged();
    }
}

QString JackPassthrough::compressorSidechannelLeft() const
{
    return d->compressorSidechannelLeft;
}

void JackPassthrough::setCompressorSidechannelLeft(const QString& compressorSidechannelLeft)
{
    if (d->compressorSidechannelLeft != compressorSidechannelLeft) {
        d->compressorSidechannelLeft = compressorSidechannelLeft;
        Q_EMIT compressorSidechannelLeftChanged();
        d->updateSidechannelLeftConnections();
    }
}

QString JackPassthrough::compressorSidechannelRight() const
{
    return d->compressorSidechannelRight;
}

void JackPassthrough::setCompressorSidechannelRight(const QString& compressorSidechannelRight)
{
    if (d->compressorSidechannelRight != compressorSidechannelRight) {
        d->compressorSidechannelRight = compressorSidechannelRight;
        Q_EMIT compressorSidechannelRightChanged();
        d->updateSidechannelRightConnections();
    }
}

QObject * JackPassthrough::compressorSettings() const
{
    return d->compressorSettings;
}

bool JackPassthrough::createPorts() const
{
    return d->createPorts;
}

void JackPassthrough::setCreatePorts(const bool& createPorts)
{
    // qDebug() << Q_FUNC_INFO << d->actualClientName << d->portPrefix << createPorts;
    if (d->createPorts != createPorts) {
        // And now, actually create/remove the ports
        if (createPorts) {
            d->registerPorts();
        } else {
            JackPassthroughAggregate *aggregate{jackPassthroughClients->value(d->actualClientName)};
            aggregate->passthroughs.removeAll(d);
            if (d->inputLeft) {
                jack_port_unregister(d->client, d->inputLeft);
                d->inputLeft = nullptr;
            }
            if (d->inputRight) {
                jack_port_unregister(d->client, d->inputRight);
                d->inputRight = nullptr;
            }
            if (d->wetInputLeft) {
                jack_port_unregister(d->client, d->wetInputLeft);
                d->wetInputLeft = nullptr;
            }
            if (d->wetInputRight) {
                jack_port_unregister(d->client, d->wetInputRight);
                d->wetInputRight = nullptr;
            }
            if (d->dryOutLeft) {
                jack_port_unregister(d->client, d->dryOutLeft);
                d->dryOutLeft = nullptr;
            }
            if (d->dryOutRight) {
                jack_port_unregister(d->client, d->dryOutRight);
                d->dryOutRight = nullptr;
            }
            if (d->wetOutFx1Left) {
                jack_port_unregister(d->client, d->wetOutFx1Left);
                d->wetOutFx1Left = nullptr;
            }
            if (d->wetOutFx1Right) {
                jack_port_unregister(d->client, d->wetOutFx1Right);
                d->wetOutFx1Right = nullptr;
            }
            if (d->wetOutFx2Left) {
                jack_port_unregister(d->client, d->wetOutFx2Left);
                d->wetOutFx2Left = nullptr;
            }
            if (d->wetOutFx2Right) {
                jack_port_unregister(d->client, d->wetOutFx2Right);
                d->wetOutFx2Right = nullptr;
            }
            // Compressor
            for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                if (d->sideChainInput[channelIndex]) {
                    jack_port_unregister(d->client, d->sideChainInput[channelIndex]);
                    d->sideChainInput[channelIndex] = nullptr;
                }
            }
        }
        d->createPorts = createPorts;
        d->updateSidechannelLeftConnections();
        d->updateSidechannelRightConnections();
        Q_EMIT createPortsChanged();
    }
}
