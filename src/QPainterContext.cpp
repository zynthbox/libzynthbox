/*
  ==============================================================================

    ClipAudioSource.h
    Created: 19/8/2021
    Author:  Marco Martin <mart@kde.org>

  ==============================================================================
*/

#include "QPainterContext.h"

#include <QPainter>
#include <QDebug>

using namespace juce;

QPainterContext::QPainterContext()
    : juce::LowLevelGraphicsContext()
{

}

QPainterContext::~QPainterContext()
{}

void QPainterContext::setPainter(QPainter *painter)
{
    m_painter = painter;
}

QPainter *QPainterContext::painter()
{
    return m_painter;
}

bool QPainterContext::isVectorDevice() const
{
    return false;
}

void QPainterContext::setOrigin(Point<int> jP)
{
    if (m_painter) {
        m_painter->setBrushOrigin(jP.getX(), jP.getY());
    }
}

void QPainterContext::addTransform(const AffineTransform&)
{
    return;
}

float QPainterContext::getPhysicalPixelScaleFactor() const
{
    return 1.0; //TODO
}

bool QPainterContext::clipToRectangle(const Rectangle<int>& jRect)
{
    if (m_painter) {
        m_painter->setClipRect(jRect.getX(), jRect.getY(), jRect.getWidth(), jRect.getHeight());
    } else {
        return false;
    }
    return true;
}

bool QPainterContext::clipToRectangleList(const RectangleList<int>&)
{
    return false;
}

void QPainterContext::excludeClipRectangle(const Rectangle<int>&)
{

}

void QPainterContext::clipToPath(const Path&, const AffineTransform&)
{

}

void QPainterContext::clipToImageAlpha(const Image&, const AffineTransform&)
{

}

bool QPainterContext::clipRegionIntersects(const Rectangle<int>&)
{
    return false;
}

Rectangle<int> QPainterContext::getClipBounds() const
{
    if (m_painter) {
        if (m_painter->clipBoundingRect().isEmpty()) {
            return Rectangle<int>(0, 0, m_painter->device()->width(), m_painter->device()->height());
        } else {
            const QRectF rect = m_painter->clipBoundingRect();
            return Rectangle<int>(rect.x(), rect.y(), rect.width(), rect.height());
        }
    } else {
        return Rectangle<int>();
    }
}

bool QPainterContext::isClipEmpty() const
{
    return false;
}

void QPainterContext::saveState()
{
    if (m_painter) {
        m_painter->save();
    }
}

void QPainterContext::restoreState()
{
    if (m_painter) {
        m_painter->restore();
    }
}

void QPainterContext::beginTransparencyLayer(float /*opacity*/)
{

}

void QPainterContext::endTransparencyLayer()
{

}


//==============================================================================
void QPainterContext::setQBrush(const QBrush &brush)
{
    m_brush = brush;
}

QBrush QPainterContext::qBrush() const
{
    return m_brush;
}

void QPainterContext::setQPen(const QPen& pen)
{
    m_pen = pen;
}

QPen QPainterContext::qPen() const
{
    return m_pen;
}

void QPainterContext::setFill(const FillType &fillType)
{
    if (m_painter) {
        m_brush = QBrush(QColor(fillType.colour.getRed(), fillType.colour.getGreen(), fillType.colour.getBlue(), fillType.colour.getAlpha()));
        m_painter->setBrush(m_brush);
    }
}

void QPainterContext::setOpacity(float opacity)
{
    m_painter->setOpacity(opacity);
}

void QPainterContext::setInterpolationQuality(Graphics::ResamplingQuality)
{

}

//==============================================================================
void QPainterContext::fillRect(const Rectangle<int> &jRect, bool /*replaceExistingContents*/)
{
    if (m_painter) {
        m_painter->fillRect(jRect.getX(), jRect.getY(), jRect.getWidth(), jRect.getHeight(), m_brush);
    }
}

void QPainterContext::fillRect(const Rectangle<float> &jRect)
{
    if (m_painter) {
        m_painter->fillRect(jRect.getX(), jRect.getY(), jRect.getWidth(), jRect.getHeight(), m_brush);
    }
}

void QPainterContext::fillRectList(const RectangleList<float> &jRegion)
{
    if (m_painter) {
        for (int i = 0; i < jRegion.getNumRectangles(); ++i) {
            const Rectangle<float> jRect = jRegion.getRectangle(i);
            m_painter->fillRect(jRect.getX(), jRect.getY(), jRect.getWidth(), jRect.getHeight(), m_brush);
        }
    }
}

void QPainterContext::fillPath(const Path& path, const AffineTransform& /*transform*/)
{
    if (m_painter) {
        if (path.isEmpty() == false) {
            m_painter->save();
            m_painter->setBrush(m_brush);
            m_pen.setCosmetic(true);
            m_painter->setPen(m_pen);
            int depth{0};
            Path::Iterator pathIterator(path);
            while (pathIterator.next()) {
                // qDebug() << Q_FUNC_INFO << "Painting next bit of the path...";
                switch(pathIterator.elementType) {
                    case Path::Iterator::startNewSubPath:
                        // qDebug() << Q_FUNC_INFO << "Starting a new path, so move one step deeper";
                        ++depth;
                        qPath[depth] << QPointF(pathIterator.x1, pathIterator.y1);
                        break;
                    case Path::Iterator::lineTo:
                        // qDebug() << Q_FUNC_INFO << "Adding a point to the path..." << pathIterator.x1 << pathIterator.y1;
                        qPath[depth] << QPointF(pathIterator.x1, pathIterator.y1);
                        break;
                    case Path::Iterator::quadraticTo:
                        // FIXME
                        // qDebug() << Q_FUNC_INFO << "This is not a quadratic curve... we're just slapping in the point and drawing a direct line :P" << pathIterator.x2 << pathIterator.y2;
                        qPath[depth] << QPointF(pathIterator.x2, pathIterator.y2);
                        break;
                    case Path::Iterator::cubicTo:
                        // FIXME
                        // qDebug() << Q_FUNC_INFO << "This is not a cubic curve... we're just slapping in the point and drawing a direct line :P" << pathIterator.x3 << pathIterator.y3;
                        qPath[depth] << QPointF(pathIterator.x3, pathIterator.y3);
                        break;
                    case Path::Iterator::closePath:
                        // qDebug() << Q_FUNC_INFO << "The path is now closed, and should be painted onto the canvas";
                        m_painter->drawPolygon(qPath[depth]);
                        // As the path has been painted, clear it
                        qPath[depth].clear();
                        // ...and move a step up in our hierarchy
                        --depth;
                        break;
                }
            }
            m_painter->restore();
        }
    }
}

void QPainterContext::drawImage(const Image&, const AffineTransform&)
{

}

void QPainterContext::drawLine(const Line<float>&)
{

}

void QPainterContext::setFont(const Font&)
{
}

const Font &QPainterContext::getFont()
{
    return m_font;
}

// void QPainterContext::drawGlyph(int /*glyphNumber*/, const AffineTransform&)
// {
//
// }
