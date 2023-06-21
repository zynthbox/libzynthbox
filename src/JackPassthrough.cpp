/*
  ==============================================================================

    JackPassthrough.cpp
    Created: 26 Sep 2022
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#include "JackPassthrough.h"
#include "JackThreadAffinitySetter.h"

#include "JUCEHeaders.h"

#include <QDebug>
#include <QGlobalStatic>

#include <jack/jack.h>
#include <jack/midiport.h>

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
    JackPassthroughPrivate(const QString &clientName, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled);
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
    }
    float dryAmount{1.0f};
    float wetFx1Amount{1.0f};
    float wetFx2Amount{1.0f};
    float panAmount{0.0f};
    bool muted{false};
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
};

static int jackPassthroughProcess(jack_nframes_t nframes, void* arg) {
    JackPassthroughAggregate *aggregate = static_cast<JackPassthroughAggregate*>(arg);
    for (JackPassthroughPrivate *passthrough : qAsConst(aggregate->passthroughs)) {
        passthrough->process(nframes);
    }
    return 0;
}

JackPassthroughPrivate::JackPassthroughPrivate(const QString &clientName, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled)
{
    jack_status_t real_jack_status{};
    QString actualClientName{clientName};
    QString portPrefix;
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
    }
}

JackPassthrough::JackPassthrough(const QString &clientName, QObject *parent, bool dryOutPortsEnabled, bool wetOutFx1PortsEnabled, bool wetOutFx2PortsEnabled)
    : QObject(parent)
    , d(new JackPassthroughPrivate(clientName, dryOutPortsEnabled, wetOutFx1PortsEnabled, wetOutFx2PortsEnabled))
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

void JackPassthrough::setDryAmount(const float &newValue)
{
    if (d->dryAmount != newValue) {
        d->dryAmount = newValue;
        Q_EMIT dryAmountChanged();
    }
}

float JackPassthrough::wetFx1Amount() const
{
    return d->wetFx1Amount;
}

void JackPassthrough::setWetFx1Amount(const float &newValue)
{
    if (d->wetFx1Amount != newValue) {
        d->wetFx1Amount = newValue;
        Q_EMIT wetFx1AmountChanged();
    }
}

float JackPassthrough::wetFx2Amount() const
{
    return d->wetFx2Amount;
}

void JackPassthrough::setWetFx2Amount(const float &newValue)
{
    if (d->wetFx2Amount != newValue) {
        d->wetFx2Amount = newValue;
        Q_EMIT wetFx2AmountChanged();
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
