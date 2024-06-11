/*
  ==============================================================================

    JackPassthrough.cpp
    Created: 26 Sep 2022
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#include "JackPassthrough.h"
#include "JackPassthroughFilter.h"
#include "JackThreadAffinitySetter.h"
#include "Compressor.h"

#include <QDebug>
#include <QDateTime>
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
    JackPassthroughPrivate(const QString &clientName, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled, JackPassthrough *q);
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
        delete sideChainGainLeft;
        delete sideChainGainRight;
        delete tempBuffer;
    }
    JackPassthrough *q{nullptr};
    QString portPrefix;
    float dryAmount{1.0f};
    float wetFx1Amount{1.0f};
    float wetFx2Amount{1.0f};
    float dryWetMixAmount{-1.0f};
    float panAmount{0.0f};
    bool muted{false};

    bool equaliserEnabled{false};
    JackPassthroughFilter* equaliserSettings[equaliserBandCount];
    JackPassthroughFilter *soloedFilter{nullptr};
    QString equaliserGraphURL;
    qint64 equaliserLastModifiedTime{0};
    bool updateMagnitudes{true};
    std::vector<double> equaliserMagnitudes;
    std::vector<double> equaliserFrequencies;

    bool compressorEnabled{false};
    float compressorThreshold{1.0f};
    QString compressorSidechannelLeft, compressorSidechannelRight;

    bool dryOutPortsEnabled{true};
    bool wetOutFx1PortsEnabled{true};
    bool wetOutFx2PortsEnabled{true};
    jack_default_audio_sample_t channelSampleLeft;
    jack_default_audio_sample_t channelSampleRight;

    jack_client_t *client{nullptr};
    jack_port_t *inputLeft{nullptr};
    jack_port_t *inputRight{nullptr};
    jack_port_t *dryOutLeft{nullptr};
    jack_port_t *dryOutRight{nullptr};
    jack_port_t *wetOutFx1Left{nullptr};
    jack_port_t *wetOutFx1Right{nullptr};
    jack_port_t *wetOutFx2Left{nullptr};
    jack_port_t *wetOutFx2Right{nullptr};
    jack_default_audio_sample_t *inputLeftBuffer{nullptr};
    jack_default_audio_sample_t *inputRightBuffer{nullptr};
    jack_default_audio_sample_t *dryOutLeftBuffer{nullptr};
    jack_default_audio_sample_t *dryOutRightBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx1LeftBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx1RightBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx2LeftBuffer{nullptr};
    jack_default_audio_sample_t *wetOutFx2RightBuffer{nullptr};

    jack_port_t *sideChainInputLeft{nullptr};
    jack_port_t *sideChainInputRight{nullptr};
    jack_default_audio_sample_t *sideChainInputBufferLeft{nullptr};
    jack_default_audio_sample_t *sideChainInputBufferRight{nullptr};
    jack_default_audio_sample_t *sideChainGainLeft{nullptr};
    jack_default_audio_sample_t *sideChainGainRight{nullptr};
    jack_default_audio_sample_t *tempBuffer{nullptr};
    iem::Compressor compressorLeft, compressorRight;

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
        jack_default_audio_sample_t *inputLeftBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(inputLeft, nframes);
        jack_default_audio_sample_t *inputRightBuffer = (jack_default_audio_sample_t *)jack_port_get_buffer(inputRight, nframes);
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
            if (equaliserEnabled) {
                for (JackPassthroughFilter *filter : equaliserSettings) {
                    filter->updateCoefficients();
                }
                jack_default_audio_sample_t *inputBuffers[2]{inputLeftBuffer, inputRightBuffer};
                for (int activeOutputIndex = 0; activeOutputIndex < 3; ++activeOutputIndex) {
                    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                        juce::AudioBuffer<float> bufferWrapper(&inputBuffers[channelIndex], 1, int(nframes));
                        juce::dsp::AudioBlock<float> block(bufferWrapper);
                        juce::dsp::ProcessContextReplacing<float> context(block);
                        filterChain[channelIndex].process(context);
                    }
                }
            }
            if (compressorEnabled) {
                jack_default_audio_sample_t *sideChainInputBufferLeft = (jack_default_audio_sample_t *)jack_port_get_buffer(sideChainInputLeft, nframes);
                jack_default_audio_sample_t *sideChainInputBufferRight = (jack_default_audio_sample_t *)jack_port_get_buffer(sideChainInputRight, nframes);
                compressorLeft.getGainFromSidechainSignal(sideChainInputBufferLeft, sideChainGainLeft, int(nframes));
                compressorRight.getGainFromSidechainSignal(sideChainInputBufferRight, sideChainGainRight, int(nframes));
                juce::FloatVectorOperations::multiply(inputLeftBuffer, sideChainGainLeft, int(nframes));
                juce::FloatVectorOperations::multiply(inputRightBuffer, sideChainGainRight, int(nframes));
            }
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
            if (wetOutFx1PortsEnabled) {
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
            if (wetOutFx2PortsEnabled) {
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
                if (dryOutPortsEnabled) {
                    const float dryAmountLeft{dryAmount * std::min(1 - panAmount, 1.0f)};
                    const float dryAmountRight{dryAmount * std::min(1 + panAmount, 1.0f)};
                    juce::FloatVectorOperations::multiply(dryOutLeftBuffer, inputLeftBuffer, dryAmountLeft, int(nframes));
                    juce::FloatVectorOperations::multiply(dryOutRightBuffer, inputRightBuffer, dryAmountRight, int(nframes));
                }
                if (wetOutFx1PortsEnabled) {
                    const float wetFx1AmountLeft{wetFx1Amount * std::min(1 - panAmount, 1.0f)};
                    const float wetFx1AmountRight{wetFx1Amount * std::min(1 + panAmount, 1.0f)};
                    juce::FloatVectorOperations::multiply(wetOutFx1LeftBuffer, inputLeftBuffer, wetFx1AmountLeft, int(nframes));
                    juce::FloatVectorOperations::multiply(wetOutFx1RightBuffer, inputRightBuffer, wetFx1AmountRight, int(nframes));
                }
                if (wetOutFx2PortsEnabled) {
                    const float wetFx2AmountLeft{wetFx2Amount * std::min(1 - panAmount, 1.0f)};
                    const float wetFx2AmountRight{wetFx2Amount * std::min(1 + panAmount, 1.0f)};
                    juce::FloatVectorOperations::multiply(wetOutFx2LeftBuffer, inputLeftBuffer, wetFx2AmountLeft, int(nframes));
                    juce::FloatVectorOperations::multiply(wetOutFx2RightBuffer, inputRightBuffer, wetFx2AmountRight, int(nframes));
                }
            }
        }
        return 0;
    }

    void connectPorts(const QString &from, const QString &to) {
        int result = jack_connect(client, from.toUtf8(), to.toUtf8());
        if (result == 0 || result == EEXIST) {
            // successful connection or connection already exists
        } else {
            qWarning() << Q_FUNC_INFO << "Failed to connect" << from << "with" << to << "with error code" << result;
            // This should probably reschedule an attempt in the near future, with a limit to how long we're trying for?
        }
    }
};

static int jackPassthroughProcess(jack_nframes_t nframes, void* arg) {
    JackPassthroughAggregate *aggregate = static_cast<JackPassthroughAggregate*>(arg);
    for (JackPassthroughPrivate *passthrough : qAsConst(aggregate->passthroughs)) {
        passthrough->process(nframes);
    }
    return 0;
}

JackPassthroughPrivate::JackPassthroughPrivate(const QString &clientName, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled, JackPassthrough *q)
    : q(q)
{
    jack_status_t real_jack_status{};
    QString actualClientName{clientName};
    this->dryOutPortsEnabled = dryOutPortsEnabled;
    this->wetOutFx1PortsEnabled = wetOutFx1PortsEnabled;
    this->wetOutFx2PortsEnabled = wetOutFx2PortsEnabled;
    // Set respective output amount to 0 if ports are not enabled
    if (!dryOutPortsEnabled) {
        dryAmount = 0.0f;
    }
    if (!wetOutFx1PortsEnabled) {
        wetFx1Amount = 0.0f;
    }
    if (!wetOutFx2PortsEnabled) {
        wetFx2Amount = 0.0f;
    }
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
    if (aggregate) {
        bool dryOutPortsRegistrationFailed{false};
        bool wetOutFx1PortsRegistrationFailed{false};
        bool wetOutFx2PortsRegistrationFailed{false};
        inputLeft = jack_port_register(client, QString("%1inputLeft").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        inputRight = jack_port_register(client, QString("%1inputRight").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
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
            aggregate->passthroughs << this;
        } else {
            qWarning() << "JackPasstrough Client: Failed to register ports for" << clientName;
        }
        sideChainInputLeft = jack_port_register(client, QString("%1sidechainInputLeft").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        sideChainInputRight = jack_port_register(client, QString("%1sidechainInputRight").arg(portPrefix).toUtf8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        sideChainGainLeft = new jack_default_audio_sample_t[8192](); // TODO This is an awkward assumption, there has to be a sensible way to do this - jack should know this, right?
        sideChainGainRight = new jack_default_audio_sample_t[8192]();
        tempBuffer = new jack_default_audio_sample_t[8192]();
        const float sampleRate = jack_get_sample_rate(client);
        for (int equaliserBand = 0; equaliserBand < equaliserBandCount; ++equaliserBand) {
            JackPassthroughFilter *newBand = new JackPassthroughFilter(equaliserBand, q);
            newBand->setSampleRate(sampleRate);
            QObject::connect(newBand, &JackPassthroughFilter::activeChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::soloedChanged, q, [this](){ bypassUpdater(); });
            QObject::connect(newBand, &JackPassthroughFilter::graphUrlChanged, q, [this, q](){
                equaliserLastModifiedTime = QDateTime::currentMSecsSinceEpoch();
                Q_EMIT q->equaliserGraphUrlChanged();
            });
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
    }
}

JackPassthrough::JackPassthrough(const QString &clientName, QObject *parent, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled)
    : QObject(parent)
    , d(new JackPassthroughPrivate(clientName, dryOutPortsEnabled, wetOutFx1PortsEnabled, wetOutFx2PortsEnabled, this))
{
}

JackPassthrough::~JackPassthrough()
{
    delete d;
}

float JackPassthrough::dryAmount() const
{
    return d->dryAmount;
}

void JackPassthrough::setDryAmount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->dryAmount != newValue) {
        d->dryAmount = newValue;
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT dryAmountChanged();
    }
}

float JackPassthrough::wetFx1Amount() const
{
    return d->wetFx1Amount;
}

void JackPassthrough::setWetFx1Amount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->wetFx1Amount != newValue) {
        d->wetFx1Amount = newValue;
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT wetFx1AmountChanged();
    }
}

float JackPassthrough::wetFx2Amount() const
{
    return d->wetFx2Amount;
}

void JackPassthrough::setWetFx2Amount(const float &newValue, bool resetDryWetMixAmount)
{
    if (d->wetFx2Amount != newValue) {
        d->wetFx2Amount = newValue;
        if (resetDryWetMixAmount) {
            d->dryWetMixAmount = -1.0f;
        }
        Q_EMIT wetFx2AmountChanged();
    }
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
            setDryAmount(1.0f, false);
            setWetFx1Amount(newValue, false);
            setWetFx2Amount(newValue, false);
        } else if (newValue == 1.0f) {
            setDryAmount(1.0f, false);
            setWetFx1Amount(1.0f, false);
            setWetFx2Amount(1.0f, false);
        } else if (newValue > 1.0f && newValue <= 2.0f) {
            setDryAmount(2.0f - newValue, false);
            setWetFx1Amount(1.0f, false);
            setWetFx2Amount(1.0f, false);
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

bool JackPassthrough::muted() const
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

void JackPassthrough::setEqualiserUrlBase(const QString& equaliserUrlBase)
{
    d->equaliserGraphURL = equaliserUrlBase;
    Q_EMIT equaliserGraphUrlChanged();
    for (int bandIndex = 0; bandIndex < equaliserBandCount; ++bandIndex) {
        d->equaliserSettings[bandIndex]->setGraphUrlBase(QString("%1/%2").arg(equaliserUrlBase).arg(bandIndex));
    }
}

QUrl JackPassthrough::equaliserGraphUrl() const
{
    return QUrl(QString("%1?%2").arg(d->equaliserGraphURL).arg(d->equaliserLastModifiedTime));
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

bool JackPassthrough::compressorEnabled() const
{
    return d->compressorEnabled;
}

void JackPassthrough::setCompressorEnabled(const bool& compressorEnabled)
{
    if (d->compressorEnabled) {
        d->compressorEnabled = compressorEnabled;
        Q_EMIT compressorEnabledChanged();
    }
}

float JackPassthrough::compressorThreshold() const
{
    return d->compressorThreshold;
}

void JackPassthrough::setCompressorThreshold(const float& compressorThreshold)
{
    if (d->compressorThreshold != compressorThreshold) {
        d->compressorThreshold = compressorThreshold;
        Q_EMIT compressorThresholdChanged();
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
        // First disconnect anything currently connected to the left sidechannel input port
        jack_port_disconnect(d->client, d->sideChainInputLeft);
        // Then connect up the new sidechain input
        d->connectPorts(d->compressorSidechannelLeft, QString("%1sidechainInputLeft").arg(d->portPrefix));
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
        // First disconnect anything currently connected to the right sidechannel input port
        jack_port_disconnect(d->client, d->sideChainInputRight);
        d->connectPorts(d->compressorSidechannelLeft, QString("%1sidechainInputLeft").arg(d->portPrefix));
    }
}
