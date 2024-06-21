/*
    ==============================================================================

    ClipAudioSource.h
    Created: 19/8/2021
    Author:  Marco Martin <mart@kde.org>
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "WaveFormItem.h"
#include "AudioLevels.h"

#include <QPainter>
#include <QDebug>
#include <QTimer>

WaveFormItem::WaveFormItem(QQuickItem *parent)
    : QQuickPaintedItem(parent),
      m_juceGraphics(m_painterContext),
      m_thumbnail(512, AudioLevels::instance()->m_formatManager, AudioLevels::instance()->m_thumbnailsCache)
{
    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    m_repaintTimer->setInterval(200);
    connect(m_repaintTimer, &QTimer::timeout, this, [this]() {update();});
    m_rapidRepaintTimer = new QTimer(this);
    m_rapidRepaintTimer->setSingleShot(true);
    m_rapidRepaintTimer->setInterval(0);
    connect(m_rapidRepaintTimer, &QTimer::timeout, this, &WaveFormItem::thumbnailChanged);
    m_thumbnail.addChangeListener(this);
    // We're not in the habit of resizing these things, so more speed is more better
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

WaveFormItem::~WaveFormItem()
{
    m_thumbnail.removeChangeListener(this);
    if (m_externalThumbnailChannel) {
        m_externalThumbnailChannel->removeChangeListener(this);
    } else if (m_externalThumbnail) {
        m_externalThumbnail->removeChangeListener(this);
    }
}

QString WaveFormItem::source() const
{
    return m_source;
}

void WaveFormItem::setSource(QString &source)
{
    static const QLatin1String audioLevelsChannelUri{"audioLevelsChannel:/"};
    static const QLatin1String captureUri{"audioLevelsChannel:/capture"};
    static const QLatin1String globalUri{"audioLevelsChannel:/global"};
    static const QLatin1String portsUri{"audioLevelsChannel:/ports"};
    if (source != m_source) {
        m_source = source;
        Q_EMIT sourceChanged();

        if (m_externalThumbnailChannel) {
            m_externalThumbnailChannel->removeChangeListener(this);
            m_externalThumbnailChannel = nullptr;
        } else if (m_externalThumbnail) {
            m_externalThumbnail->removeChangeListener(this);
        }
        m_externalThumbnail = nullptr;

        if (m_source.startsWith(audioLevelsChannelUri)) {
            if (m_source == captureUri) {
                m_externalThumbnailChannel = AudioLevels::instance()->systemCaptureAudioLevelsChannel();
            } else if(m_source == globalUri) {
                m_externalThumbnailChannel = AudioLevels::instance()->globalAudioLevelsChannel();
            } else if(m_source == portsUri) {
                m_externalThumbnailChannel = AudioLevels::instance()->portsRecorderAudioLevelsChannel();
            } else {
                const int channelIndex = m_source.midRef(20).toInt();
                m_externalThumbnailChannel = AudioLevels::instance()->audioLevelsChannel(channelIndex);
            }
        } else {
            m_thumbnail.clear();

            juce::File file(source.toUtf8().constData());
            auto *reader = AudioLevels::instance()->m_formatManager.createReaderFor(file);

            if (reader != nullptr) {
                std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
                m_thumbnail.setSource(new juce::FileInputSource(file));
                m_readerSource.reset(newSource.release());
            }
        }

        if (m_externalThumbnailChannel) {
            m_externalThumbnailChannel->addChangeListener(this);
            m_externalThumbnail = m_externalThumbnailChannel->thumbnail();
        } else if (m_externalThumbnail) {
            m_externalThumbnail->addChangeListener(this);
        }
    }
}

qreal WaveFormItem::length() const
{
    if (m_externalThumbnail) {
        return m_externalThumbnail->getTotalLength();
    }
    return m_thumbnail.getTotalLength();
}

QColor WaveFormItem::color() const
{
    return m_color;
}

void WaveFormItem::setColor(const QColor &color)
{
    if (color == m_color) {
        return;
    }

    m_color = color;
    m_painterContext.setQBrush(m_color);
    Q_EMIT colorChanged();
}

qreal WaveFormItem::start() const
{
    return m_start;
}

void WaveFormItem::setStart(qreal start)
{
    if (start == m_start) {
        return;
    }

    m_start = start;
    Q_EMIT startChanged();
    QMetaObject::invokeMethod(m_repaintTimer, "start", Qt::QueuedConnection, Q_ARG(int, 1));
}

qreal WaveFormItem::end() const
{
    return m_end;
}

void WaveFormItem::setEnd(qreal end)
{
    if (end == m_end) {
        return;
    }

    m_end = end;
    Q_EMIT endChanged();
    QMetaObject::invokeMethod(m_repaintTimer, "start", Qt::QueuedConnection, Q_ARG(int, 1));
}

void WaveFormItem::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (source == &m_thumbnail || (m_externalThumbnail && source == m_externalThumbnail)) {
        // qWarning() << "Thumbnail Source Changed. Repainting.";
        QMetaObject::invokeMethod(m_rapidRepaintTimer, "start", Qt::QueuedConnection);
    }
}

void WaveFormItem::thumbnailChanged()
{
    m_start = 0;
    if (m_externalThumbnail) {
        m_end = m_externalThumbnail->getTotalLength();
    } else {
        m_end = m_thumbnail.getTotalLength();
    }

    Q_EMIT startChanged();
    Q_EMIT endChanged();
    Q_EMIT sourceChanged();
    Q_EMIT lengthChanged();
    update();
}

void WaveFormItem::paint(QPainter *painter)
{
    m_painterContext.setPainter(painter);
    juce::Rectangle<int> thumbnailBounds (0, 0, width(), height());
    if (m_externalThumbnail) {
        const int numChannels{m_externalThumbnail->getNumChannels()};
        if (numChannels == 1) {
            m_externalThumbnail->drawChannel(m_juceGraphics, thumbnailBounds, m_start, qMin(m_end, m_externalThumbnail->getTotalLength()), 0, 1.0f);
        } else {
            const double spacing{height() / (numChannels + 1)};
            for (int channel = 0; channel < numChannels; ++channel) {
                thumbnailBounds.setTop(channel * spacing);
                thumbnailBounds.setHeight(height() - spacing);
                m_externalThumbnail->drawChannel(m_juceGraphics, thumbnailBounds, m_start, qMin(m_end, m_externalThumbnail->getTotalLength()), channel, 1.0f);
            }
        }
    } else {
        const int numChannels{m_thumbnail.getNumChannels()};
        if (numChannels == 1) {
            m_thumbnail.drawChannel(m_juceGraphics, thumbnailBounds, m_start, qMin(m_end, m_thumbnail.getTotalLength()), 0, 1.0f);
        } else {
            const double spacing{height() / (numChannels + 1)};
            for (int channel = 0; channel < numChannels; ++channel) {
                thumbnailBounds.setTop(channel * spacing);
                thumbnailBounds.setHeight(height() - spacing);
                m_thumbnail.drawChannel(m_juceGraphics, thumbnailBounds, m_start, qMin(m_end, m_thumbnail.getTotalLength()), channel, 1.0f);
            }
        }
        if (!m_thumbnail.isFullyLoaded()) {
            // qDebug() << Q_FUNC_INFO << m_source << "is not fully loaded yet, schedule a repaint...";
            QMetaObject::invokeMethod(m_repaintTimer, "start", Qt::QueuedConnection);
        }
    }
}

#include "moc_WaveFormItem.cpp"
