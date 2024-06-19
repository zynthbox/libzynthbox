/**
 *    JackPassthroughCompressor.h
 *    Created: 13 June, 2024
 *    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>
 */

#pragma once

#include "JackPassthrough.h"
#include "Compressor.h"

class JackPassthroughCompressorPrivate;
class JackPassthroughCompressor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(float threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(float thresholdDB READ thresholdDB WRITE setThresholdDB NOTIFY thresholdChanged)
    Q_PROPERTY(float kneeWidth READ kneeWidth WRITE setKneeWidth NOTIFY kneeWidthChanged)
    Q_PROPERTY(float kneeWidthDB READ kneeWidthDB WRITE setKneeWidthDB NOTIFY kneeWidthChanged)
    Q_PROPERTY(float attack READ attack WRITE setAttack NOTIFY attackChanged)
    Q_PROPERTY(float release READ release WRITE setRelease NOTIFY releaseChanged)
    Q_PROPERTY(float ratio READ ratio WRITE setRatio NOTIFY ratioChanged)
    Q_PROPERTY(float makeUpGain READ makeUpGain WRITE setMakeUpGain NOTIFY makeUpGainChanged)
    Q_PROPERTY(float makeUpGainDB READ makeUpGainDB WRITE setMakeUpGainDB NOTIFY makeUpGainChanged)

    Q_PROPERTY(float sidechainPeakLeft READ sidechainPeakLeft NOTIFY peakChanged)
    Q_PROPERTY(float sidechainPeakRight READ sidechainPeakRight NOTIFY peakChanged)
    Q_PROPERTY(float maxGainReductionLeft READ maxGainReductionLeft NOTIFY peakChanged)
    Q_PROPERTY(float maxGainReductionRight READ maxGainReductionRight NOTIFY peakChanged)
    Q_PROPERTY(float outputPeakLeft READ outputPeakLeft NOTIFY peakChanged)
    Q_PROPERTY(float outputPeakRight READ outputPeakRight NOTIFY peakChanged)
public:
    explicit JackPassthroughCompressor(JackPassthrough *parent = nullptr);
    ~JackPassthroughCompressor() override;

    /**
     * \brief This will reset all values to their defaults
     */
    Q_INVOKABLE void setDefaults();

    QString name() const;
    void setName(const QString &name);
    Q_SIGNAL void nameChanged();
    bool selected() const;
    void setSelected(const bool &selected);
    Q_SIGNAL void selectedChanged();

    float threshold() const;
    void setThreshold(const float &threshold);
    float thresholdDB() const;
    void setThresholdDB(const float &thresholdDB);
    Q_SIGNAL void thresholdChanged();
    float kneeWidth() const;
    void setKneeWidth(const float &kneeWidth);
    float kneeWidthDB() const;
    void setKneeWidthDB(const float &kneeWidthDB);
    Q_SIGNAL void kneeWidthChanged();
    float attack() const;
    void setAttack(const float &attack);
    Q_SIGNAL void attackChanged();
    float release() const;
    void setRelease(const float &release);
    Q_SIGNAL void releaseChanged();
    float ratio() const;
    void setRatio(const float &ratio);
    Q_SIGNAL void ratioChanged();
    float makeUpGain() const;
    void setMakeUpGain(const float &makeUpGain);
    float makeUpGainDB() const;
    void setMakeUpGainDB(const float makeUpGainDB);
    Q_SIGNAL void makeUpGainChanged();

    float sidechainPeakLeft() const;
    float sidechainPeakRight() const;
    float maxGainReductionLeft() const;
    float maxGainReductionRight() const;
    float outputPeakLeft() const;
    float outputPeakRight() const;
    void updatePeaks(const float &sidechainPeakLeft, const float &sidechainPeakRight, const float &maxGainReductionLeft, const float &maxGainReductionRight, const float &outputPeakLeft, const float &outputPeakRight);
    void setPeaks(const float &sidechainPeakLeft, const float &sidechainPeakRight, const float &maxGainReductionLeft, const float &maxGainReductionRight, const float &outputPeakLeft, const float &outputPeakRight);
    Q_SIGNAL void peakChanged();

    void setSampleRate(const float &sampleRate);

    // Called at the start of each process call to update the filters internal state, so needs to be very low impact
    void updateParameters();

    iem::Compressor compressors[2];
private:
    JackPassthroughCompressorPrivate *d{nullptr};
};
