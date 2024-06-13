/*
    ==============================================================================

    JackPassthroughVisualiserItem.cpp
    Created: 13/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "JackPassthroughVisualiserItem.h"

#include "JackPassthrough.h"
#include "JackPassthroughFilter.h"

#include <QTimer>
#include <QPainter>

class JackPassthroughVisualiserItemPrivate {
public:
    JackPassthroughVisualiserItemPrivate() {}
    QObject *source{nullptr};
    JackPassthrough *passthrough{nullptr};
    JackPassthroughFilter *filter{nullptr};
};

JackPassthroughVisualiserItem::JackPassthroughVisualiserItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , d(new JackPassthroughVisualiserItemPrivate)
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
    }
}

static float getPositionForFrequency(float freq) {
    return (std::log (freq / 20.0f) / std::log (2.0f)) / 10.0f;
}

static float getPositionForGain(float gain, float top, float bottom) {
    static float maxDB{24.0f};
    return juce::jmap (juce::Decibels::gainToDecibels (gain, -maxDB), -maxDB, maxDB, bottom, top);
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
        auto drawAndClearPath = [&painter, &frame, soloFilter](JackPassthroughFilter* filter,  QPolygonF &path) {
            QPen pen(filter->color());
            pen.setCosmetic(true);
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
        juce::Path path;
        QPolygonF polygon;
        if (d->filter) {
            // If there's a filter, then we are only drawing that one filter rather than the entire passthrough
            d->filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
            drawAndClearPath(d->filter, polygon);
        } else {
            // Otherwise we're drawing all of them
            d->passthrough->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            QPen pen{QColorConstants::White};
            pen.setCosmetic(true);
            pen.setWidth(3);
            painter->setPen(pen);
            painter->drawPolyline(polygon);
            polygon.clear();
            for (const QVariant &variant : d->passthrough->equaliserSettings()) {
                JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
                filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
                drawAndClearPath(filter, polygon);
            }
        }
    }
}
