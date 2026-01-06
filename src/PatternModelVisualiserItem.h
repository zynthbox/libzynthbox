/*
    ==============================================================================

    PatternModelVisualiserItem.h
    Created: 06/01/2025
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include <QQuickPaintedItem>

class PatternModelVisualiserItem : public QQuickPaintedItem
{
    Q_OBJECT
    /**
     * \brief The pattern model object you want to display
     */
    Q_PROPERTY(QObject* patternModel READ patternModel WRITE setPatternModel NOTIFY patternModelChanged)
public:
    explicit PatternModelVisualiserItem(QQuickItem *parent = nullptr);
    ~PatternModelVisualiserItem() override;
    void paint(QPainter *painter) override;

    QObject* patternModel() const;
    void setPatternModel(QObject* patternModel);
    Q_SIGNAL void patternModelChanged();

private:
    class Private;
    Private *d{nullptr};
};
Q_DECLARE_METATYPE(PatternModelVisualiserItem*)
