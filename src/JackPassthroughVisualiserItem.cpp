/*
    ==============================================================================

    JackPassthroughVisualiserItem.cpp
    Created: 13/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "JackPassthroughVisualiserItem.h"

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
    JackPassthrough *passthrough{nullptr};
    JackPassthroughFilter *filter{nullptr};
    float sampleRate{48000.0f};
    JackPassthroughAnalyser equaliserInputAnalyser[2];
    JackPassthroughAnalyser equaliserOutputAnalyser[2];
    QList<JackPassthroughAnalyser*> equaliserInputAnalyserList, equaliserOutputAnalyserList;
    QTimer repaintTimer;
};

JackPassthroughVisualiserItem::JackPassthroughVisualiserItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , d(new JackPassthroughVisualiserItemPrivate(this))
{
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
        d->filter = qobject_cast<JackPassthroughFilter*>(source);
        if (d->filter) {
            d->passthrough = qobject_cast<JackPassthrough*>(d->filter->parent());
            connect(d->filter, &JackPassthroughFilter::dataChanged, this, &QQuickItem::update);
        } else {
            d->passthrough = qobject_cast<JackPassthrough*>(source);
            if (d->passthrough) {
                connect(d->passthrough, &JackPassthrough::equaliserDataChanged, this, &QQuickItem::update);
            }
        }
        if (d->passthrough) {
            // Set the analysers on the new passthrough
            d->passthrough->setEqualiserInputAnalysers(d->equaliserInputAnalyserList);
            d->passthrough->setEqualiserOutputAnalysers(d->equaliserOutputAnalyserList);
            d->repaintTimer.start();
        } else {
            d->repaintTimer.stop();
        }
    }
}

static float getPositionForFrequency(float freq) {
    return (std::log(freq / 20.0f) / std::log(2.0f)) / 10.0f;
}

static float getPositionForGain(float gain, float top, float bottom) {
    static float maxDB{24.0f};
    return juce::jmap(juce::Decibels::gainToDecibels(gain, -maxDB), -maxDB, maxDB, bottom, top);
}

void JackPassthroughVisualiserItem::paint(QPainter* painter)
{
    if (d->passthrough) {
        JackPassthroughFilter *soloFilter{nullptr};
        for (const QVariant &variant : d->passthrough->equaliserSettings()) {
            JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
            if (filter->soloed()) {
                soloFilter = filter;
                break;
            }
        }
        QRect frame(0, 0, width(), height());
        static const float maxDB{24.0f};
        auto pixelsPerDouble = 2.0f * height() / juce::Decibels::decibelsToGain(maxDB);
        auto drawAndClearPath = [&painter, &frame, soloFilter](JackPassthroughFilter* filter, QPolygonF &path, QPen &pen) {
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
        };
        QPolygonF polygon;
        QPen pen;
        pen.setCosmetic(true);
        pen.setWidth(1);
        for (int channelIndex = 0; channelIndex < 2; ++channelIndex) {
            d->equaliserInputAnalyser[channelIndex].createPath(polygon, frame, 20.0f);
            pen.setColor(QColorConstants::LightGray);
            painter->setPen(pen);
            painter->drawPolyline(polygon);
            d->equaliserOutputAnalyser[channelIndex].createPath(polygon, frame, 20.0f);
            pen.setColor(QColorConstants::White);
            painter->setPen(pen);
            painter->drawPolyline(polygon);
        }
        polygon.clear();
        if (d->filter) {
            // If there's a filter, then we are only drawing that one filter rather than the entire passthrough
            d->filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
            drawAndClearPath(d->filter, polygon, pen);
        } else {
            // Otherwise we're drawing all of them
            d->passthrough->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            pen.setColor(QColorConstants::White);
            pen.setWidth(3);
            painter->setPen(pen);
            painter->drawPolyline(polygon);
            polygon.clear();
            for (const QVariant &variant : d->passthrough->equaliserSettings()) {
                JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
                filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
                drawAndClearPath(filter, polygon, pen);
            }
        }
    }
}
