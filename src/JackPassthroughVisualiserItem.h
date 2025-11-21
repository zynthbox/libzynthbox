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
     * \brief The AudioLevelChannel, ClipAudioSource, JackPassthrough, or JackPassthroughFilter object you wish to visualise
     */
    Q_PROPERTY(QObject* source READ source WRITE setSource NOTIFY sourceChanged)
    /**
     * \brief Whether or not to perform live input and output analysis on the audio passing through
     * @default true
     * Set this to false to simply paint the filter curve
     */
    Q_PROPERTY(bool analyseAudio READ analyseAudio WRITE setAnalyseAudio NOTIFY analyseAudioChanged)
    /**
     * \brief Whether or not to draw the curves for disabled bands
     * @default true
     */
    Q_PROPERTY(bool drawDisabledBands READ drawDisabledBands WRITE setDrawDisabledBands NOTIFY drawDisabledBandsChanged)

public:
    explicit JackPassthroughVisualiserItem(QQuickItem *parent = nullptr);
    ~JackPassthroughVisualiserItem() override;
    void paint(QPainter *painter) override;

    QObject *source() const;
    void setSource(QObject *source);
    Q_SIGNAL void sourceChanged();

    bool analyseAudio() const;
    void setAnalyseAudio(const bool &analyseAudio);
    Q_SIGNAL void analyseAudioChanged();

    bool drawDisabledBands() const;
    void setDrawDisabledBands(const bool &drawDisabledBands);
    Q_SIGNAL void drawDisabledBandsChanged();
private:
    JackPassthroughVisualiserItemPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(JackPassthroughVisualiserItem*)
