/**
 *    JackPassthroughCompressor.h
 *    Created: 13 June, 2024
 *    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>
 */

#include "JackPassthroughCompressor.h"

class JackPassthroughCompressorPrivate {
public:
    JackPassthroughCompressorPrivate() {}

    QString name;
    bool selected{false};
    float sampleRate{48000};
    bool parametersChanged{true};
    float threshold{-10.0f}; // Threshold (dB)
    juce::NormalisableRange<float> thresholdRange{-50.0f, 10.0f, 0.1f};
    float kneeWidth{0.0f}; // Knee Width (dB)
    juce::NormalisableRange<float> kneeWidthRange{0.0f, 30.0f, 0.1f};
    float attack{30.f}; // Attack Time (ms)
    juce::NormalisableRange<float> attackRange{0.0f, 100.0f, 0.1f};
    float release{150.0f}; // Release Time (ms)
    juce::NormalisableRange<float> releaseRange{0.0f, 500.0f, 0.1f};
    float ratio{4.0f}; // Ratio ( : 1)
    juce::NormalisableRange<float> ratioRange{1.0f, 16.0f, 0.1f};
    float makeUpGain{0.0f}; // Make Up Gain (dB)
    juce::NormalisableRange<float> makeUpGainRange{-10.0f, 20.0f, 0.1f};

    int observerCount{0};
    float sidechainPeakLeft{0.0f};
    float sidechainPeakRight{0.0f};
    float maxGainReductionLeft{0.0f};
    float maxGainReductionRight{0.0f};
    float outputPeakLeft{0.0f};
    float outputPeakRight{0.0f};
};

JackPassthroughCompressor::JackPassthroughCompressor(QObject* parent)
    : QObject(parent)
    , d(new JackPassthroughCompressorPrivate)
{
}

JackPassthroughCompressor::~JackPassthroughCompressor()
{
    delete d;
}

void JackPassthroughCompressor::setDefaults()
{
    setName("");
    setSelected(false);
    setThresholdDB(-10.0f);
    setKneeWidthDB(0.0f);
    setAttack(30.0f);
    setRelease(150.0f);
    setRatio(4.0f);
    setMakeUpGainDB(0.0f);
}

QString JackPassthroughCompressor::name() const
{
    return d->name;
}

void JackPassthroughCompressor::setName(const QString& name)
{
    if (d->name != name) {
        d->name = name;
        Q_EMIT nameChanged();
    }
}

bool JackPassthroughCompressor::selected() const
{
    return d->selected;
}

void JackPassthroughCompressor::setSelected(const bool& selected)
{
    if (d->selected != selected) {
        d->selected = selected;
        Q_EMIT selectedChanged();
    }
}

float JackPassthroughCompressor::threshold() const
{
    return d->thresholdRange.convertTo0to1(d->threshold);
}

void JackPassthroughCompressor::setThreshold(const float& threshold)
{
    setThresholdDB(d->thresholdRange.convertFrom0to1(threshold));
}

float JackPassthroughCompressor::thresholdDB() const
{
    return d->threshold;
}

void JackPassthroughCompressor::setThresholdDB(const float& thresholdDB)
{
    if (d->threshold != thresholdDB) {
        d->threshold = d->thresholdRange.getRange().clipValue(thresholdDB);
        d->parametersChanged = true;
        Q_EMIT thresholdChanged();
    }
}

float JackPassthroughCompressor::kneeWidth() const
{
    return d->kneeWidthRange.convertTo0to1(d->kneeWidth);
}

void JackPassthroughCompressor::setKneeWidth(const float& kneeWidth)
{
    setKneeWidthDB(d->kneeWidthRange.convertFrom0to1(kneeWidth));
}

float JackPassthroughCompressor::kneeWidthDB() const
{
    return d->kneeWidth;
}

void JackPassthroughCompressor::setKneeWidthDB(const float& kneeWidthDB)
{
    if (d->kneeWidth != kneeWidthDB) {
        d->kneeWidth = d->kneeWidthRange.getRange().clipValue(kneeWidthDB);
        d->parametersChanged = true;
        Q_EMIT kneeWidthChanged();
    }
}

float JackPassthroughCompressor::attack() const
{
    return d->attack;
}

void JackPassthroughCompressor::setAttack(const float& attack)
{
    if (d->attack != attack) {
        d->attack = d->attackRange.getRange().clipValue(attack);
        d->parametersChanged = true;
        Q_EMIT attackChanged();
    }
}

float JackPassthroughCompressor::release() const
{
    return d->release;
}

void JackPassthroughCompressor::setRelease(const float& release)
{
    if (d->release != release) {
        d->release = d->releaseRange.getRange().clipValue(release);
        d->parametersChanged = true;
        Q_EMIT releaseChanged();
    }
}

float JackPassthroughCompressor::ratio() const
{
    return d->ratio;
}

void JackPassthroughCompressor::setRatio(const float& ratio)
{
    if (d->ratio != ratio) {
        d->ratio = d->ratioRange.getRange().clipValue(ratio);
        d->parametersChanged = true;
        Q_EMIT ratioChanged();
    }
}

float JackPassthroughCompressor::makeUpGain() const
{
    return d->makeUpGainRange.convertTo0to1(d->makeUpGain);
}

void JackPassthroughCompressor::setMakeUpGain(const float& makeUpGain)
{
    setMakeUpGainDB(d->makeUpGainRange.convertFrom0to1(makeUpGain));
}

float JackPassthroughCompressor::makeUpGainDB() const
{
    return d->makeUpGain;
}

void JackPassthroughCompressor::setMakeUpGainDB(const float makeUpGainDB)
{
    if (d->makeUpGain != makeUpGainDB) {
        d->makeUpGain = d->makeUpGainRange.getRange().clipValue(makeUpGainDB);
        d->parametersChanged = true;
        Q_EMIT makeUpGainChanged();
    }
}

void JackPassthroughCompressor::registerObserver() const
{
    d->observerCount++;
}

void JackPassthroughCompressor::unregisterObserver() const
{
    d->observerCount = qMax(0, d->observerCount - 1);
}

bool JackPassthroughCompressor::hasObservers() const
{
    return d->observerCount > 0;
}

float JackPassthroughCompressor::sidechainPeakLeft() const
{
    return d->sidechainPeakLeft;
}

float JackPassthroughCompressor::sidechainPeakRight() const
{
    return d->sidechainPeakRight;
}

float JackPassthroughCompressor::maxGainReductionLeft() const
{
    return d->maxGainReductionLeft;
}

float JackPassthroughCompressor::maxGainReductionRight() const
{
    return d->maxGainReductionRight;
}

float JackPassthroughCompressor::outputPeakLeft() const
{
    return d->outputPeakLeft;
}

float JackPassthroughCompressor::outputPeakRight() const
{
    return d->outputPeakRight;
}

void JackPassthroughCompressor::updatePeaks(const float& sidechainPeakLeft, const float& sidechainPeakRight, const float &maxGainReductionLeft, const float &maxGainReductionRight, const float& outputPeakLeft, const float& outputPeakRight)
{
    d->sidechainPeakLeft = qMin(qMax(d->sidechainPeakLeft - 0.005f, sidechainPeakLeft), 1.0f);
    d->sidechainPeakRight = qMin(qMax(d->sidechainPeakRight - 0.005f, sidechainPeakRight), 1.0f);
    d->maxGainReductionLeft = qMin(qMax(0.0f, maxGainReductionLeft), 1.0f);
    d->maxGainReductionRight = qMin(qMax(0.0f, maxGainReductionRight), 1.0f);
    d->outputPeakLeft = qMin(qMax(d->outputPeakLeft - 0.005f, outputPeakLeft), 1.0f);
    d->outputPeakRight = qMin(qMax(d->outputPeakRight - 0.005f, outputPeakRight), 1.0f);
    Q_EMIT peakChanged();
}

void JackPassthroughCompressor::setPeaks(const float& sidechainPeakLeft, const float& sidechainPeakRight, const float &maxGainReductionLeft, const float &maxGainReductionRight, const float& outputPeakLeft, const float& outputPeakRight)
{
    if (d->sidechainPeakLeft != sidechainPeakLeft || d->sidechainPeakRight != sidechainPeakRight || d->maxGainReductionLeft != maxGainReductionLeft || d->maxGainReductionRight != maxGainReductionRight || d->outputPeakLeft != outputPeakLeft || d->outputPeakRight != outputPeakRight) {
        d->sidechainPeakLeft = sidechainPeakLeft;
        d->sidechainPeakRight = sidechainPeakRight;
        d->maxGainReductionLeft = maxGainReductionLeft;
        d->maxGainReductionRight = maxGainReductionRight;
        d->outputPeakLeft = outputPeakLeft;
        d->outputPeakRight = outputPeakRight;
        Q_EMIT peakChanged();
    }
}

void JackPassthroughCompressor::setSampleRate(const float& sampleRate)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = 1;
    spec.maximumBlockSize = 8192;
    for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
        compressors[channelIndex].prepare(spec);
    }
}

void JackPassthroughCompressor::updateParameters()
{
    if (d->parametersChanged) {
        d->parametersChanged = false;
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            compressors[channelIndex].setThreshold(d->threshold);
            compressors[channelIndex].setKnee(d->kneeWidth);
            compressors[channelIndex].setAttackTime(d->attack * 0.001f);
            compressors[channelIndex].setReleaseTime(d->release * 0.001f);
            compressors[channelIndex].setRatio(d->ratio > 15.9f ? INFINITY : d->ratio);
            compressors[channelIndex].setMakeUpGain(d->makeUpGain);
        }
    }
}
