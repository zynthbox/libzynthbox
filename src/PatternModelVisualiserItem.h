/*
    ==============================================================================

    PatternModelVisualiserItem.h
    Created: 06/01/2025
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include <QQuickPaintedItem>
#include <QColor>

class PatternModelVisualiserItem : public QQuickPaintedItem
{
    Q_OBJECT
    /**
     * \brief The pattern model object you want to display
     */
    Q_PROPERTY(QObject* patternModel READ patternModel WRITE setPatternModel NOTIFY patternModelChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY foregroundColorChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)

public:
    explicit PatternModelVisualiserItem(QQuickItem *parent = nullptr);
    ~PatternModelVisualiserItem() override;
    void paint(QPainter *painter) override;

    QObject* patternModel() const;
    void setPatternModel(QObject* patternModel);

    void setBackgroundColor(const QColor &color);
    void setForegroundColor(const QColor &color);
    void setFillColor(const QColor &color);

    QColor backgroundColor() const;
    QColor foregroundColor() const;
    QColor fillColor() const;

Q_SIGNALS:
    void patternModelChanged();
    void backgroundColorChanged();
    void foregroundColorChanged();
    void fillColorChanged();

private:
    class Private;
    Private *d{nullptr};
};
Q_DECLARE_METATYPE(PatternModelVisualiserItem*)
