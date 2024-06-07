/*
  ==============================================================================

    JackPassthroughFilter.cpp
    Created: 3 June, 2024
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/
#include "JackPassthroughFilter.h"

#include "JUCEHeaders.h"

#include <QDebug>
#include <QTimer>

class JackPassthroughFilterPrivate {
public:
    JackPassthroughFilterPrivate(JackPassthroughFilter *q)
        : q(q)
    {
        frequencies.resize (300);
        for (size_t i=0; i < frequencies.size(); ++i) {
            frequencies [i] = 20.0 * std::pow (2.0, i / 30.0);
        }
        magnitudes.resize (frequencies.size());

        coefficientUpdater.setSingleShot(true);
        coefficientUpdater.setInterval(0);
        QObject::connect(&coefficientUpdater, &QTimer::timeout, q, [this](){ updateCoefficientsActual(); });
    }
    JackPassthroughFilter *q{nullptr};
    QString name;
    bool selected{false};
    float sampleRate{48000.0f};
    JackPassthroughFilter::FilterType filterType;
    float frequency;
    float quality{0.707f};
    float gain{1.0f};
    bool active{true}; // The global setting is off, but when enabling the qualiser, we want all of the filters to be active by default
    bool soloed{false};
    QColor color;

    JackPassthroughFilter *previous{nullptr};
    JackPassthroughFilter *next{nullptr};

    dsp::IIR::Filter<float> *filters[2]{nullptr, nullptr};
    QTimer coefficientUpdater;
    void updateCoefficients() {
        coefficientUpdater.start();
    }
    void updateCoefficientsActual();
    juce::dsp::IIR::Coefficients<float>::Ptr updatedCoefficients;

    std::vector<double> frequencies;
    std::vector<double> magnitudes;
};

JackPassthroughFilter::JackPassthroughFilter(int index, QObject* parent)
    : QObject(parent)
    , d(new JackPassthroughFilterPrivate(this))
{
    switch(index) {
        case 0:
            d->name = QLatin1String{"Lowest"};
            d->filterType = HighPassType;
            d->frequency = 20.0f;
            d->color = QColorConstants::Svg::blue;
            break;
        case 1:
            d->name = QLatin1String{"Low"};
            d->filterType = LowShelfType;
            d->frequency = 250.0f;
            d->color = QColorConstants::Svg::brown;
            break;
        case 2:
            d->name = QLatin1String{"Low Mids"};
            d->filterType = PeakType;
            d->frequency = 500.0f;
            d->color = QColorConstants::Svg::green;
            break;
        case 3:
            d->name = QLatin1String{"High Mids"};
            d->filterType = PeakType;
            d->frequency = 1000.0f;
            d->color = QColorConstants::Svg::coral;
            break;
        case 4:
            d->name = QLatin1String{"High"};
            d->filterType = HighShelfType;
            d->frequency = 5000.0f;
            d->color = QColorConstants::Svg::orange;
            break;
        case 5:
            d->name = QLatin1String{"Highest"};
            d->filterType = LowPassType;
            d->frequency = 12000.0f;
            d->color = QColorConstants::Svg::red;
            break;
        default:
            qCritical() << Q_FUNC_INFO << "Attempted to create a JackPassthroughFilter with an index outside the expected range of 0 through 5 - probably look at that. Given index was" << index;
            break;
    }
}

JackPassthroughFilter::~JackPassthroughFilter()
{
    delete d;
}

QString JackPassthroughFilter::filterTypeName(FilterType filterType) const
{
    static const QHash<FilterType, QString> names{
        {NoFilterType, "No Filter"},
        {HighPassType, "High Pass"},
        {HighPass1stType, "1st High Pass"},
        {LowShelfType, "Low Shelf"},
        {BandPassType, "Band Pass"},
        {AllPassType, "All Pass"},
        {AllPass1stType, "1st All Pass"},
        {NotchType, "Notch"},
        {PeakType, "Peak"},
        {HighShelfType, "High Shelf"},
        {LowPass1stType, "1st Low Pass"},
        {LowPassType, "Low Pass"}
    };
    return names[filterType];
}

QStringList JackPassthroughFilter::filterTypeNames() const
{
    static const QStringList names{"No Filter", "High Pass", "1st High Pass", "Low Shelf", "Band Pass", "All Pass", "1st All Pass", "Notch", "Peak", "High Shelf", "1st Low Pass", "Low Pass"};
    return names;
}

QObject * JackPassthroughFilter::previous() const
{
    return d->previous;
}

void JackPassthroughFilter::setPrevious(JackPassthroughFilter* previous)
{
    d->previous = previous;
}

QObject * JackPassthroughFilter::next() const
{
    return d->next;
}

void JackPassthroughFilter::setNext(JackPassthroughFilter* next)
{
    d->next = next;
}

QString JackPassthroughFilter::name() const
{
    return d->name;
}

void JackPassthroughFilter::setName(const QString& name)
{
    if (d->name != name) {
        d->name = name;
        Q_EMIT nameChanged();
    }
}

bool JackPassthroughFilter::selected() const
{
    return d->selected;
}

void JackPassthroughFilter::setSelected(const bool& selected)
{
    if (d->selected != selected) {
        // Only allow one selected filter in a chain, so de-select all the others
        if (selected) {
            JackPassthroughFilter *current{this};
            while (current->d->previous) {
                current = current->d->previous;
            }
            while (current) {
                if (current != this) {
                    // Don't reset self, just makes for unnecessarily noisy signals
                    current->setSelected(false);
                }
                current = current->d->next;
            }
        }
        d->selected = selected;
        Q_EMIT selectedChanged();
    }
}

JackPassthroughFilter::FilterType JackPassthroughFilter::filterType() const
{
    return d->filterType;
}

void JackPassthroughFilter::setFilterType(const FilterType& filterType)
{
    if (d->filterType != filterType) {
        d->filterType = filterType;
        Q_EMIT filterTypeChanged();
        d->updateCoefficients();
    }
}

float JackPassthroughFilter::frequency() const
{
    return d->frequency;
}

void JackPassthroughFilter::setFrequency(const float& frequency)
{
    if (d->frequency != frequency && 20.0f <= frequency && frequency <= 20000.0f) {
        d->frequency = frequency;
        Q_EMIT frequencyChanged();
        d->updateCoefficients();
    }
}

float JackPassthroughFilter::quality() const
{
    return d->quality;
}

void JackPassthroughFilter::setQuality(const float& quality)
{
    if (d->quality != quality && 0.0f <= quality && quality <= 10.0f) {
        d->quality = quality;
        Q_EMIT qualityChanged();
        d->updateCoefficients();
    }
}

float JackPassthroughFilter::gain() const
{
    return d->gain;
}

float JackPassthroughFilter::gainDb() const
{
    return juce::Decibels::gainToDecibels(d->gain);
}

void JackPassthroughFilter::setGain(const float& gain)
{
    if (d->gain != gain && 0.0f <= gain && gain <= 2.0f) {
        d->gain = gain;
        Q_EMIT gainChanged();
        d->updateCoefficients();
    }
}

bool JackPassthroughFilter::active() const
{
    return d->active;
}

void JackPassthroughFilter::setActive(const bool& active)
{
    if (d->active != active) {
        d->active = active;
        Q_EMIT activeChanged();
    }
}

bool JackPassthroughFilter::soloed() const
{
    return d->soloed;
}

void JackPassthroughFilter::setSoloed(const bool& soloed)
{
    if (d->soloed != soloed) {
        // Only allow one soloed filter in a chain, so de-select all the others
        if (soloed) {
            JackPassthroughFilter *current{this};
            while (current->d->previous) {
                current = current->d->previous;
            }
            while (current) {
                if (current != this) {
                    // Don't reset self, just makes for unnecessarily noisy signals
                    current->setSoloed(false);
                }
                current = current->d->next;
            }
        }
        d->soloed = soloed;
        Q_EMIT soloedChanged();
    }
}

QColor JackPassthroughFilter::color() const
{
    return d->color;
}

void JackPassthroughFilter::setColor(const QColor& color)
{
    if (d->color != color) {
        d->color = color;
        Q_EMIT colorChanged();
    }
}

void JackPassthroughFilter::createFrequencyPlot(juce::Path &p, const std::vector<double> &mags, const juce::Rectangle<int> bounds, float pixelsPerDouble)
{
    // Some TODO -ness here, as we don't really do this... we likely will want this, but for now it's more of a "for later" type situation
    p.startNewSubPath (float (bounds.getX()), mags[0] > 0 ? float (bounds.getCentreY() - pixelsPerDouble * std::log (mags[0]) / std::log (2.0)) : bounds.getBottom());
    const auto xFactor = static_cast<double> (bounds.getWidth()) / d->frequencies.size();
    for (size_t i=1; i < d->frequencies.size(); ++i) {
        p.lineTo (float (bounds.getX() + i * xFactor), float (mags[i] > 0 ? bounds.getCentreY() - pixelsPerDouble * std::log (mags[i]) / std::log (2.0) : bounds.getBottom()));
    }
}

void JackPassthroughFilter::setDspObjects(dsp::IIR::Filter<float> *filterLeft, dsp::IIR::Filter<float> *filterRight)
{
    d->filters[0] = filterLeft;
    d->filters[1] = filterRight;
    d->updateCoefficients();
}

void JackPassthroughFilter::setSampleRate(const float& sampleRate)
{
    d->sampleRate = sampleRate;
}

void JackPassthroughFilter::updateCoefficients() const
{
    if (d->updatedCoefficients) {
        d->filters[0]->coefficients = *d->updatedCoefficients;
        d->filters[1]->coefficients = *d->updatedCoefficients;
        d->updatedCoefficients = nullptr;
    }
}

void JackPassthroughFilterPrivate::updateCoefficientsActual()
{
    juce::dsp::IIR::Coefficients<float>::Ptr newCoefficients;
    switch (filterType) {
        case JackPassthroughFilter::NoFilterType:
            newCoefficients = new juce::dsp::IIR::Coefficients<float>(1, 0, 1, 0);
            break;
        case JackPassthroughFilter::LowPassType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, frequency, quality);
            break;
        case JackPassthroughFilter::LowPass1stType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, frequency);
            break;
        case JackPassthroughFilter::LowShelfType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, frequency, quality, gain);
            break;
        case JackPassthroughFilter::BandPassType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, frequency, quality);
            break;
        case JackPassthroughFilter::AllPassType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, frequency, quality);
            break;
        case JackPassthroughFilter::AllPass1stType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderAllPass(sampleRate, frequency);
            break;
        case JackPassthroughFilter::NotchType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeNotch(sampleRate, frequency, quality);
            break;
        case JackPassthroughFilter::PeakType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, frequency, quality, gain);
            break;
        case JackPassthroughFilter::HighShelfType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, frequency, quality, gain);
            break;
        case JackPassthroughFilter::HighPass1stType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, frequency);
            break;
        case JackPassthroughFilter::HighPassType:
            newCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, frequency, quality);
            break;
        default:
            break;
    }

    if (newCoefficients)
    {
        // set the coefficients somewhere else, and before performing operations in process(), set the new coefficients if they are != nullptr
        updatedCoefficients = newCoefficients;
        newCoefficients->getMagnitudeForFrequencyArray(frequencies.data(), magnitudes.data(), frequencies.size(), sampleRate);
    }
}
