/*
  ==============================================================================

    JackPassthroughFilter.h
    Created: 3 June, 2024
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#pragma once

#include <QObject>
#include <QColor>

#include "JUCEHeaders.h"

class JackPassthroughFilterPrivate;
class JackPassthroughFilter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QObject* previous READ previous NOTIFY previousChanged)
    Q_PROPERTY(QObject* next READ next NOTIFY nextChanged)
    Q_PROPERTY(FilterType filterType READ filterType WRITE setFilterType NOTIFY filterTypeChanged)
    Q_PROPERTY(float frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(float quality READ quality WRITE setQuality NOTIFY qualityChanged)
    Q_PROPERTY(float gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(float gainDb READ gainDb NOTIFY gainChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool soloed READ soloed WRITE setSoloed NOTIFY soloedChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit JackPassthroughFilter(int index, QObject *parent = nullptr);
    ~JackPassthroughFilter() override;

    enum FilterType {
        NoFilterType = 0,
        HighPassType,
        HighPass1stType,
        LowShelfType,
        BandPassType,
        AllPassType,
        AllPass1stType,
        NotchType,
        PeakType,
        HighShelfType,
        LowPass1stType,
        LowPassType,
    };
    Q_ENUM(FilterType)
    Q_INVOKABLE QString filterTypeName(FilterType filterType) const;
    Q_INVOKABLE QStringList filterTypeNames() const;

    QObject *previous() const;
    void setPrevious(JackPassthroughFilter *previous);
    Q_SIGNAL void previousChanged();
    QObject *next() const;
    void setNext(JackPassthroughFilter *next);
    Q_SIGNAL void nextChanged();

    QString name() const;
    void setName(const QString &name);
    Q_SIGNAL void nameChanged();
    bool selected() const;
    void setSelected(const bool &selected);
    Q_SIGNAL void selectedChanged();
    FilterType filterType() const;
    void setFilterType(const FilterType &filterType);
    Q_SIGNAL void filterTypeChanged();
    float frequency() const;
    void setFrequency(const float &frequency);
    Q_SIGNAL void frequencyChanged();
    float quality() const;
    void setQuality(const float &quality);
    Q_SIGNAL void qualityChanged();
    float gain() const;
    float gainDb() const;
    void setGain(const float &gain);
    Q_SIGNAL void gainChanged();
    bool active() const;
    void setActive(const bool &active);
    Q_SIGNAL void activeChanged();
    bool soloed() const;
    void setSoloed(const bool &soloed);
    Q_SIGNAL void soloedChanged();
    QColor color() const;
    void setColor(const QColor &color);
    Q_SIGNAL void colorChanged();

    void createFrequencyPlot(juce::Path &p, const std::vector<double> &mags, const juce::Rectangle<int> bounds, float pixelsPerDouble);
    void setDspObjects(dsp::IIR::Filter<float> *filterLeft, dsp::IIR::Filter<float> *filterRight);
    void setSampleRate(const float &sampleRate);

    // Called at the start of each process call to update the filters internal state, so needs to be very low impact
    void updateCoefficients() const;
private:
    JackPassthroughFilterPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(JackPassthroughFilter::FilterType)
