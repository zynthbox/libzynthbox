/*
    ==============================================================================

    JackPassthroughVisualiserItem.cpp
    Created: 13/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "JackPassthroughVisualiserItem.h"

#include "AudioLevelsChannel.h"
#include "ClipAudioSource.h"
#include "JackPassthrough.h"
#include "JackPassthroughAnalyser.h"
#include "JackPassthroughFilter.h"

#include <QTimer>
#include <QPainter>

class JackPassthroughVisualiserItemPrivate {
public:
    JackPassthroughVisualiserItemPrivate(JackPassthroughVisualiserItem *q)
        :q(q)
    {
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            equaliserInputAnalyser[channelIndex].setupAnalyser(int(sampleRate), sampleRate);
            equaliserInputAnalyserList << &equaliserInputAnalyser[channelIndex];
            equaliserOutputAnalyser[channelIndex].setupAnalyser(int(sampleRate), sampleRate);
            equaliserOutputAnalyserList << &equaliserOutputAnalyser[channelIndex];
        }
        repaintTimer.setInterval(50);
        repaintTimer.callOnTimeout([this, q](){
            if (equaliserInputAnalyser[0].checkForNewData() || equaliserInputAnalyser[1].checkForNewData() || equaliserOutputAnalyser[0].checkForNewData() || equaliserOutputAnalyser[1].checkForNewData()) {
                q->update();
            }
        });
    }
    ~JackPassthroughVisualiserItemPrivate() {
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            equaliserInputAnalyser[channelIndex].stopThread(1000);
            equaliserOutputAnalyser[channelIndex].stopThread(1000);
        }
    }
    JackPassthroughVisualiserItem *q{nullptr};
    QObject *source{nullptr};
    bool analyseAudio{true};
    bool drawDisabledBands{true};
    int eqCurveThickness{3};
    JackPassthrough *passthrough{nullptr};
    ClipAudioSource *clip{nullptr};
    AudioLevelsChannel *audioLevelsChannel{nullptr};
    JackPassthroughFilter *filter{nullptr};
    float sampleRate{48000.0f};
    JackPassthroughAnalyser equaliserInputAnalyser[2];
    QColor inputColours[2]{QColorConstants::Svg::lightskyblue,QColorConstants::Svg::lightsteelblue};
    JackPassthroughAnalyser equaliserOutputAnalyser[2];
    QColor outputColours[2]{QColorConstants::Svg::salmon,QColorConstants::Svg::sandybrown};
    QList<JackPassthroughAnalyser*> equaliserInputAnalyserList, equaliserOutputAnalyserList;
    QTimer repaintTimer;
};

JackPassthroughVisualiserItem::JackPassthroughVisualiserItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , d(new JackPassthroughVisualiserItemPrivate(this))
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

JackPassthroughVisualiserItem::~JackPassthroughVisualiserItem()
{
    delete d;
}

QObject * JackPassthroughVisualiserItem::source() const
{
    return d->source;
}

void JackPassthroughVisualiserItem::setSource(QObject* source)
{
    if (d->source != source) {
        d->source = source;
        if (d->filter) {
            d->filter->disconnect(this);
        }
        if (d->passthrough) {
            d->passthrough->disconnect(this);
            // Clear the analysers on the previous passthrough
            QList<JackPassthroughAnalyser*> emptyList{nullptr, nullptr};
            d->passthrough->setEqualiserInputAnalysers(emptyList);
            d->passthrough->setEqualiserOutputAnalysers(emptyList);
            d->passthrough = nullptr;
        }
        if (d->clip) {
            d->clip->disconnect(this);
            // Clear the analysers on the previous clip
            QList<JackPassthroughAnalyser*> emptyList{nullptr, nullptr};
            d->clip->setEqualiserInputAnalysers(emptyList);
            d->clip->setEqualiserOutputAnalysers(emptyList);
            d->clip = nullptr;
        }
        if (d->audioLevelsChannel) {
            d->audioLevelsChannel->disconnect(this);
            // Clear the analysers on the previous audiolevels channel
            QList<JackPassthroughAnalyser*> emptyList{nullptr, nullptr};
            d->audioLevelsChannel->setEqualiserInputAnalysers(emptyList);
            d->audioLevelsChannel->setEqualiserOutputAnalysers(emptyList);
            d->audioLevelsChannel = nullptr;
        }
        d->filter = qobject_cast<JackPassthroughFilter*>(source);
        if (d->filter) {
            d->passthrough = qobject_cast<JackPassthrough*>(d->filter->parent());
            connect(d->filter, &JackPassthroughFilter::dataChanged, this, &QQuickItem::update);
        } else {
            d->passthrough = qobject_cast<JackPassthrough*>(source);
            if (d->passthrough) {
                connect(d->passthrough, &JackPassthrough::equaliserDataChanged, this, &QQuickItem::update);
            } else {
                d->clip = qobject_cast<ClipAudioSource*>(source);
                if (d->clip) {
                    connect(d->clip, &ClipAudioSource::equaliserDataChanged, this, &QQuickItem::update);
                } else {
                    d->audioLevelsChannel = qobject_cast<AudioLevelsChannel*>(source);
                    if (d->audioLevelsChannel) {
                        connect(d->audioLevelsChannel, &AudioLevelsChannel::equaliserDataChanged, this, &QQuickItem::update);
                    }
                }
            }
        }
        if (d->analyseAudio) {
            if (d->passthrough) {
                // Set the analysers on the new passthrough
                d->passthrough->setEqualiserInputAnalysers(d->equaliserInputAnalyserList);
                d->passthrough->setEqualiserOutputAnalysers(d->equaliserOutputAnalyserList);
                d->repaintTimer.start();
            } else if (d->clip) {
                // Set the analysers on the new clip
                d->clip->setEqualiserInputAnalysers(d->equaliserInputAnalyserList);
                d->clip->setEqualiserOutputAnalysers(d->equaliserOutputAnalyserList);
                d->repaintTimer.start();
            } else if (d->audioLevelsChannel) {
                // Set the analysers on the new audiolevels channel
                d->audioLevelsChannel->setEqualiserInputAnalysers(d->equaliserInputAnalyserList);
                d->audioLevelsChannel->setEqualiserOutputAnalysers(d->equaliserOutputAnalyserList);
                d->repaintTimer.start();
            } else {
                d->repaintTimer.stop();
            }
        } else {
            d->repaintTimer.stop();
            update();
        }
    }
}

bool JackPassthroughVisualiserItem::analyseAudio() const
{
    return d->analyseAudio;
}

void JackPassthroughVisualiserItem::setAnalyseAudio(const bool& analyseAudio)
{
    if (d->analyseAudio != analyseAudio) {
        d->analyseAudio = analyseAudio;
        Q_EMIT analyseAudioChanged();
        if (d->source) {
            QObject *oldSource{d->source};
            setSource(nullptr);
            setSource(oldSource);
        }
    }
}

bool JackPassthroughVisualiserItem::drawDisabledBands() const
{
    return d->drawDisabledBands;
}

void JackPassthroughVisualiserItem::setDrawDisabledBands(const bool& drawDisabledBands)
{
    if (d->drawDisabledBands != drawDisabledBands) {
        d->drawDisabledBands = drawDisabledBands;
        Q_EMIT drawDisabledBandsChanged();
        update();
    }
}

int JackPassthroughVisualiserItem::eqCurveThickness() const
{
    return d->eqCurveThickness;
}

void JackPassthroughVisualiserItem::setEqCurveThickness(const int& eqCurveThickness)
{
    if (d->eqCurveThickness != eqCurveThickness) {
        d->eqCurveThickness = eqCurveThickness;
        Q_EMIT eqCurveThicknessChanged();
        update();
    }
}

static float getPositionForFrequency(float freq) {
    static const juce::NormalisableRange<float> frequencyRangeNormalised{20.0f, 20000.0f, 1.0f, 0.2f};
    return frequencyRangeNormalised.convertTo0to1(freq);
}

static float getPositionForGain(float gain, float top, float bottom) {
    static float maxDB{24.0f};
    return juce::jmap(juce::Decibels::gainToDecibels(gain, -maxDB), -maxDB, maxDB, bottom, top);
}

void JackPassthroughVisualiserItem::paint(QPainter* painter)
{
    if (d->passthrough || d->clip || d->audioLevelsChannel) {
        JackPassthroughFilter *soloFilter{nullptr};
        const QVariantList &equaliserSettings{d->audioLevelsChannel
            ? d->audioLevelsChannel->equaliserSettings()
            : d->passthrough
                ? d->passthrough->equaliserSettings()
                : d->clip->equaliserSettings()};
        for (const QVariant &variant : equaliserSettings) {
            JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
            if (filter->soloed()) {
                soloFilter = filter;
                break;
            }
        }
        QRect frame(0, 0, width(), height());
        static const float maxDB{24.0f};
        auto pixelsPerDouble = 2.0f * height() / juce::Decibels::decibelsToGain(maxDB);
        auto drawAndClearPath = [this, &painter, &frame, soloFilter](JackPassthroughFilter* filter, QPolygonF &path, QPen &pen) {
            if (filter->active() || (filter->active() == false && d->drawDisabledBands)) {
                pen.setColor(filter->color());
                pen.setWidth(1);
                if (soloFilter) {
                    pen.setStyle(soloFilter == filter ? Qt::SolidLine : Qt::DotLine);
                } else {
                    pen.setStyle(filter->active() ? Qt::SolidLine : Qt::DotLine);
                }
                painter->setPen(pen);
                painter->drawPolyline(path);
                painter->setBrush(filter->selected() ? filter->color() : QColorConstants::Transparent);
                auto x = juce::roundToInt(frame.width() * getPositionForFrequency(filter->frequency()));
                auto y = juce::roundToInt(getPositionForGain(filter->gain(), 0, float(frame.height())));
                painter->drawLine(x, 0, x, y - 5);
                painter->drawLine(x, y + 4, x, frame.height());
                painter->drawEllipse(x - 4, y - 4, 7, 7);
                path.clear();
            }
        };
        QPolygonF polygon;
        QPen pen;
        pen.setCosmetic(true);
        if (d->analyseAudio) {
            pen.setWidth(1);
            QFont font = painter->font();
            font.setPixelSize(12);
            painter->setFont(font);
            const QRect insetFrame = frame.adjusted(3, 3, -3, -3);
            const QRect insetFrameDown = insetFrame.translated(0, 13);
            pen.setColor(d->inputColours[0]);
            painter->setPen(pen);
            painter->drawText(insetFrame, Qt::AlignLeft | Qt::AlignTop, "Input (left)");
            pen.setColor(d->inputColours[1]);
            painter->setPen(pen);
            painter->drawText(insetFrameDown, Qt::AlignLeft | Qt::AlignTop, "Input (right)");
            pen.setColor(d->outputColours[0]);
            painter->setPen(pen);
            painter->drawText(insetFrame, Qt::AlignRight | Qt::AlignTop, "Output (left)");
            pen.setColor(d->outputColours[1]);
            painter->setPen(pen);
            painter->drawText(insetFrameDown, Qt::AlignRight | Qt::AlignTop, "Output (right)");
            for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
                d->equaliserInputAnalyser[channelIndex].createPath(polygon, frame, 20.0f);
                pen.setColor(d->inputColours[channelIndex]);
                painter->setPen(pen);
                painter->drawPolyline(polygon);
                d->equaliserOutputAnalyser[channelIndex].createPath(polygon, frame, 20.0f);
                pen.setColor(d->outputColours[channelIndex]);
                painter->setPen(pen);
                painter->drawPolyline(polygon);
            }
            polygon.clear();
        }
        if (d->filter) {
            // If there's a filter, then we are only drawing that one filter rather than the entire passthrough
            d->filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
            drawAndClearPath(d->filter, polygon, pen);
        } else {
            // Otherwise we're drawing all of them
            if (d->passthrough) {
                d->passthrough->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            } else if (d->clip) {
                d->clip->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            } else if (d->audioLevelsChannel) {
                d->audioLevelsChannel->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            }
            pen.setColor(QColorConstants::White);
            pen.setWidth(d->eqCurveThickness);
            painter->setPen(pen);
            painter->drawPolyline(polygon);
            polygon.clear();
            for (const QVariant &variant : equaliserSettings) {
                JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
                filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
                drawAndClearPath(filter, polygon, pen);
            }
        }
    }
}
