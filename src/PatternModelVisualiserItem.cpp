/*
    ==============================================================================

    PatternModelVisualiserItem.cpp
    Created: 06/01/2025
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "PatternModelVisualiserItem.h"

#include "PatternModel.h"
#include "Note.h"

#include <QPainter>

class PatternModelVisualiserItem::Private {
public:
    Private() {}
    ~Private() {}
    PatternModel *patternModel{nullptr};

    QColor backgroundColor = {"#333"};
    QColor foregroundColor = {"white"};
    QColor fillColor = {"black"};
};

PatternModelVisualiserItem::PatternModelVisualiserItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , d(new Private)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

PatternModelVisualiserItem::~PatternModelVisualiserItem()
{
    delete d;
}

QObject * PatternModelVisualiserItem::patternModel() const
{
    return d->patternModel;
}

QColor PatternModelVisualiserItem::backgroundColor() const
{
    return d->backgroundColor;
}

void PatternModelVisualiserItem::setBackgroundColor(const QColor &color) 
{
    if(d->backgroundColor == color)
        return;
    d->backgroundColor =  color;
    Q_EMIT backgroundColorChanged();
    update();
}

QColor PatternModelVisualiserItem::foregroundColor() const
{
    return d->foregroundColor;
}

void PatternModelVisualiserItem::setForegroundColor(const QColor &color) 
{
    if(d->foregroundColor == color)
        return;
    d->foregroundColor =  color;
    Q_EMIT foregroundColorChanged();
    update();
}

QColor PatternModelVisualiserItem::fillColor() const
{
    return d->fillColor;
}

void PatternModelVisualiserItem::setFillColor(const QColor &color) 
{
    if(d->fillColor == color)
        return;
    d->fillColor =  color;
    Q_EMIT fillColorChanged();
    update();
}

void PatternModelVisualiserItem::setPatternModel(QObject *patternModel)
{
    if (d->patternModel != patternModel) {
        if (d->patternModel) {
            d->patternModel->disconnect(this);
        }
        d->patternModel = qobject_cast<PatternModel*>(patternModel);
        Q_EMIT patternModelChanged();
        if (d->patternModel) {
            connect(d->patternModel, &PatternModel::lastModifiedChanged, this, &QQuickItem::update);
        }
        update();
    }
}

void PatternModelVisualiserItem::paint(QPainter* outerPainter)
{
    outerPainter->save();
    PatternModel *pattern{d->patternModel};
    // White dot for "got notes to play"
    // static const QColor white{"white"};
    // Dark gray dot for "no note, but pattern is enabled"
    // static const QColor gray{"#333"};
    // Black dot for "bar is not within availableBars
    // static const QColor black{"transparent"};
    if (pattern) {
        if (pattern->performanceActive()) {
            pattern = qobject_cast<PatternModel*>(pattern->performanceClone());
        }
        int height = 128;
        int width = pattern->width() * pattern->bankLength();
        QImage img = QImage(width, height, QImage::Format_RGBA8888);
        img.fill(fillColor());
        QPainter painter(&img);
        painter.fillRect(0, 0, pattern->patternLength(), height, backgroundColor());
        for (int row = 0; row < pattern->bankLength(); ++row) {
            for (int column = 0; column < pattern->width(); ++column) {
                if (row < pattern->availableBars()) {
                    const Note *note = qobject_cast<const Note*>(pattern->getNote(row + pattern->bankOffset(), column));
                    if (note) {
                        const QVariantList &subnotes = note->subnotes();
                        for (const QVariant &subnoteVar : subnotes) {
                            Note *subnote = subnoteVar.value<Note*>();
                            const int midiNote{subnote->midiNote()};
                            const int yPos{height - midiNote - 1};
                            const int xPos{(row * pattern->width() + column)};
                            painter.setOpacity(0.5);
                            painter.setPen(foregroundColor());
                            painter.drawLine(xPos, qMax(0, yPos - 3), xPos, qMin(127, yPos + 3));
                            painter.drawLine(xPos, qMax(0, yPos - 2), xPos, qMin(127, yPos + 2));
                            painter.drawLine(xPos, qMax(0, yPos - 1), xPos, qMin(127, yPos + 1));
                        }
                        for (const QVariant &subnoteVar : subnotes) {
                            Note *subnote = subnoteVar.value<Note*>();
                            const QColor &solid{foregroundColor()};
                            img.setPixelColor((row * pattern->width() + column), height - subnote->midiNote() - 1, solid);
                        }
                    }
                }
            }
        }
        outerPainter->drawImage(0, 0, img.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        outerPainter->setCompositionMode(QPainter::CompositionMode_Source);
        outerPainter->fillRect(boundingRect(), fillColor());
    }
    outerPainter->restore();
}
