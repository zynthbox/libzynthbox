/*
    ==============================================================================

    ClipAudioSource.h
    Created: 19/8/2021
    Author:  Marco Martin <mart@kde.org>
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include "JUCEHeaders.h"
#include "QPainterContext.h"
#include <QQuickPaintedItem>

class AudioLevelsChannel;
class WaveFormItem : public QQuickPaintedItem,
                     private juce::ChangeListener
{
Q_OBJECT
    /**
     * \brief The source (either a file, or an audioLevelsChannel uri) for what you want to see a thumbnail of
     * If set to an audioLevelsChannel uri, you will be shown the thumbnail for the result of any ongoing recording.
     * This uri is in the following form:
     * * audioLevelsChannel:/(a number from 0 through 9) - for the sketchpad track at that index
     * * audioLevelsChannel:/capture - for the system capture channel (nominally the "microphone" input)
     * * audioLevelsChannel:/global - for the master output channel
     * * audioLevelsChannel:/ports - the manual-set capture channel on AudioLevels (see AudioLevels::addRecordPort)
     */
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal length READ length NOTIFY lengthChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal start READ start WRITE setStart NOTIFY startChanged)
    Q_PROPERTY(qreal end READ end WRITE setEnd NOTIFY endChanged)

public:
    explicit WaveFormItem(QQuickItem *parent = nullptr);
    ~WaveFormItem() override;
    void paint(QPainter *painter) override;

    QString source() const;
    void setSource(QString &source);

    qreal length() const;

    QColor color() const;
    void setColor(const QColor &color);

    qreal start() const;
    void setStart(qreal start);

    qreal end() const;
    void setEnd(qreal end);

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    Q_SLOT void thumbnailChanged();

Q_SIGNALS:
    void sourceChanged();
    void lengthChanged();
    void colorChanged();

    void startChanged();
    void endChanged();

private:
    QString m_source;

    QTimer *m_repaintTimer{nullptr};
    QTimer *m_rapidRepaintTimer{nullptr};
    QPainterContext m_painterContext;
    juce::Graphics m_juceGraphics;
    QColor m_color;
    std::unique_ptr<juce::AudioFormatReaderSource> m_readerSource;
    juce::AudioThumbnail m_thumbnail;
    juce::AudioThumbnail *m_externalThumbnail{nullptr};
    AudioLevelsChannel *m_externalThumbnailChannel{nullptr};
    qreal m_start = 0;
    qreal m_end = 0;
};
Q_DECLARE_METATYPE(WaveFormItem*)

