/*
    ==============================================================================

    JackPassthroughVisualiserItem.h
    Created: 13/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include "QPainterContext.h"
#include <QQuickPaintedItem>

class JackPassthroughVisualiserItemPrivate;
class JackPassthroughVisualiserItem : public QQuickPaintedItem
{
Q_OBJECT
    /**
     * \brief The JackPassthrough or JackPassthroughFilter object you wish to visualise
     */
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit JackPassthroughVisualiserItem(QQuickItem *parent = nullptr);
    ~JackPassthroughVisualiserItem() override;
    void paint(QPainter *painter) override;

    QObject *source() const;
    void setSource(QObject *source);
    Q_SIGNAL void sourceChanged();
private:
    JackPassthroughVisualiserItemPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(JackPassthroughVisualiserItem*)
