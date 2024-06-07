/*
  ==============================================================================

    JackPassthrough.h
    Created: 26 Sep 2022
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#pragma once

#include <QObject>

class JackPassthroughPrivate;
/**
 * \brief A splitting passthrough client which has a pair of inputs, and two pairs of outputs (dry and wet) with individual volume for each output
 *
 * The input ports are inputLeft and inpitRight
 * The output ports are dryOutLeft and dryOutRight for the dry pair, and wetOutLeft and wetOutRight for the wet pair
 *
 * Due to the manner in which the client operates, setting the dry and wet amounts to 0 or 1 makes the
 * client operate much faster than any value between the two (the two extremes are direct copies, and
 * the others have to modify the sample values).
 *
 * There is further a pair of inputs named sidechainLeft and sidechainRight, which are used to perform sidechained compression if the compressor is enabled
 */
class JackPassthrough : public QObject {
    Q_OBJECT
    Q_PROPERTY(float dryAmount READ dryAmount WRITE setDryAmount NOTIFY dryAmountChanged)
    Q_PROPERTY(float wetFx1Amount READ wetFx1Amount WRITE setWetFx1Amount NOTIFY wetFx1AmountChanged)
    Q_PROPERTY(float wetFx2Amount READ wetFx2Amount WRITE setWetFx2Amount NOTIFY wetFx2AmountChanged)
    /**
     * \brief Control dry/wet output mixture
     *
     * dryWetMixAmount allows you to redirect what percentage of dry and what percent of wet data is put out.
     *
     * Initially -1 is stored as dryWetMixAmount as it is unused for all passthrough clients by default. Setting dryWetMixAmount will actually set dry and wet amounts individually.
     * Setting dry amount or wet amount individually for any client will uninitialize dryWetMixAmount by setting it to -1
     *
     * Value from 0.0f to 1.0f will control wet output from 0% to 100% while dry remains 100%
     * Value from 1.0f to 2.0f will control dry output from 100% to 0% while wet remains 100%
     * Value 1.0f will mean both wet and dry set to 100%
     *
     * @default -1.0f Unused by default
     * @minimum 0.0f 0% wet and 100% dry. 0.0f to 1.0f will control wet output from 0% to 100% while dry remains 100%
     * @maximum 2.0f 100% wet and 0% dry. 1.0f to 2.0f will control dry output from 100% to 0% while wet remains 100%
     */
    Q_PROPERTY(float dryWetMixAmount READ dryWetMixAmount WRITE setDryWetMixAmount NOTIFY dryWetMixAmountChanged)
    Q_PROPERTY(float panAmount READ panAmount WRITE setPanAmount NOTIFY panAmountChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

    /**
     * \brief Whether or not the equaliser will be applied to incoming audio
     * @default false
     */
    Q_PROPERTY(bool equaliserEnabled READ equaliserEnabled WRITE setEqualiserEnabled NOTIFY equaliserEnabledChanged)
    /**
     * \brief A list of the settings container objects for each of the equaliser bands
     */
    Q_PROPERTY(QVariantList equaliserSettings READ equaliserSettings NOTIFY equaliserSettingsChanged)

    /**
     * \brief Whether or not the compressor will be applied to incoming audio (post-equaliser)
     * @default false
     */
    Q_PROPERTY(bool compressorEnabled READ compressorEnabled WRITE setCompressorEnabled NOTIFY compressorEnabledChanged)
    /**
     * \brief The sources used for the left channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelLeft READ compressorSidechannelLeft WRITE setCompressorSidechannelLeft NOTIFY compressorSidechannelLeftChanged)
    /**
     * \brief The sources used for the right channel of the compressor side channel
     */
    Q_PROPERTY(QString compressorSidechannelRight READ compressorSidechannelRight WRITE setCompressorSidechannelRight NOTIFY compressorSidechannelRightChanged)
    /**
     * \brief The threshold for detecting in the side channel whether to apply the compressor
     */
    Q_PROPERTY(float compressorThreshold READ compressorThreshold WRITE setCompressorThreshold NOTIFY compressorThresholdChanged)
public:
    explicit JackPassthrough(const QString &clientName, QObject *parent = nullptr, bool dryOutPortsEnabled = true, bool wetOutFx1PortsEnabled = true, bool wetOutFx2PortsEnabled = true);
    ~JackPassthrough() override;

    float dryAmount() const;
    void setDryAmount(const float& newValue, bool resetDryWetMixAmount=true);
    Q_SIGNAL void dryAmountChanged();

    float wetFx1Amount() const;
    void setWetFx1Amount(const float& newValue, bool resetDryWetMixAmount=true);
    Q_SIGNAL void wetFx1AmountChanged();

    float wetFx2Amount() const;
    void setWetFx2Amount(const float& newValue, bool resetDryWetMixAmount=true);
    Q_SIGNAL void wetFx2AmountChanged();

    float dryWetMixAmount() const;
    void setDryWetMixAmount(const float& newValue);
    Q_SIGNAL void dryWetMixAmountChanged();

    float panAmount() const;
    void setPanAmount(const float& newValue);
    Q_SIGNAL void panAmountChanged();

    bool muted() const;
    void setMuted(const bool& newValue);
    Q_SIGNAL void mutedChanged();

    bool equaliserEnabled() const;
    void setEqualiserEnabled(const bool &equaliserEnabled);
    Q_SIGNAL void equaliserEnabledChanged();
    QVariantList equaliserSettings() const;
    Q_SIGNAL void equaliserSettingsChanged();

    bool compressorEnabled() const;
    void setCompressorEnabled(const bool &compressorEnabled);
    Q_SIGNAL void compressorEnabledChanged();
    QString compressorSidechannelLeft() const;
    void setCompressorSidechannelLeft(const QString &compressorSidechannelLeft);
    Q_SIGNAL void compressorSidechannelLeftChanged();
    QString compressorSidechannelRight() const;
    void setCompressorSidechannelRight(const QString &compressorSidechannelRight);
    Q_SIGNAL void compressorSidechannelRightChanged();
    float compressorThreshold() const;
    void setCompressorThreshold(const float &compressorThreshold);
    Q_SIGNAL void compressorThresholdChanged();
private:
    JackPassthroughPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(JackPassthrough*)
