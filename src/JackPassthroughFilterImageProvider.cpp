/*
 * Copyright (C) 2024 Dan Leinir Turthra Jensen <admin@leinir.dk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "JackPassthroughFilterImageProvider.h"
#include "JackPassthrough.h"
#include "JackPassthroughFilter.h"
#include "Plugin.h"
#include "QPainterContext.h"

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QRunnable>
#include <QThreadPool>

/**
 * \brief A worker class which does the bulk of the work for PreviewImageProvider
 */
class JackPassthroughFilterRunnable : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit JackPassthroughFilterRunnable(const QString &id, const QSize &requestedSize);
    virtual ~JackPassthroughFilterRunnable();

    void run() override;

    /**
     * Request that the preview worker abort what it's doing
     */
    Q_SLOT void abort();

    /**
     * \brief Emitted once the preview has been retrieved (successfully or not)
     * @param image The preview image in the requested size (possibly a placeholder)
     */
    Q_SIGNAL void done(QImage image);
private:
    class Private;
    std::unique_ptr<Private> d;
};

class JackPassthroughFilterImageProvider::Private {
public:
    Private() {}
    ~Private() {}
};

JackPassthroughFilterImageProvider::JackPassthroughFilterImageProvider()
    : QQuickAsyncImageProvider()
    , d(new Private)
{
}

class JackPassthroughFilterResponse : public QQuickImageResponse
{
    public:
        JackPassthroughFilterResponse(const QString &id, const QSize &requestedSize)
        {
            m_runnable = new JackPassthroughFilterRunnable(id, requestedSize);
            m_runnable->setAutoDelete(false);
            connect(m_runnable, &JackPassthroughFilterRunnable::done, this, &JackPassthroughFilterResponse::handleDone, Qt::QueuedConnection);
            connect(this, &QQuickImageResponse::finished, m_runnable, &QObject::deleteLater,  Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(m_runnable);
        }

        void handleDone(QImage image) {
            m_image = image;
            Q_EMIT finished();
        }

        QQuickTextureFactory *textureFactory() const override
        {
            return QQuickTextureFactory::textureFactoryForImage(m_image);
        }

        void cancel() override
        {
            m_runnable->abort();
        }

        JackPassthroughFilterRunnable* m_runnable{nullptr};
        QImage m_image;
};

QQuickImageResponse * JackPassthroughFilterImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    JackPassthroughFilterResponse* response = new JackPassthroughFilterResponse(id, requestedSize);
    return response;
}


class JackPassthroughFilterRunnable::Private {
public:
    Private() {}
    QString id;
    QSize requestedSize;

    bool abort{false};
    QMutex abortMutex;
    bool isAborted() {
        QMutexLocker locker(&abortMutex);
        return abort;
    }
};

JackPassthroughFilterRunnable::JackPassthroughFilterRunnable(const QString& id, const QSize& requestedSize)
    : d(new Private)
{
    d->id = id;
    if (requestedSize.isValid()) {
        d->requestedSize = requestedSize;
    } else {
        d->requestedSize = QSize(800, 300);
    }
}

JackPassthroughFilterRunnable::~JackPassthroughFilterRunnable()
{
    abort();
}

void JackPassthroughFilterRunnable::abort()
{
    QMutexLocker locker(&d->abortMutex);
    d->abort = true;
}

static float getPositionForFrequency(float freq) {
    return (std::log (freq / 20.0f) / std::log (2.0f)) / 10.0f;
}

static float getPositionForGain(float gain, float top, float bottom) {
    static float maxDB{24.0f};
    return juce::jmap (juce::Decibels::gainToDecibels (gain, -maxDB), -maxDB, maxDB, bottom, top);
}

void JackPassthroughFilterRunnable::run()
{
    QImage img;
    // In case there's a ? to signify e.g. a timestamp or other trickery to get our thumbnail updated, ignore that section
    QStringList splitId = d->id.split('?').first().split('/');

    JackPassthrough *passthrough{nullptr};
    JackPassthroughFilter *filter{nullptr};
    if (splitId.count() > 1) {
        static const QString synthType{"synth"};
        static const QString fxType{"fx"};
        if (splitId[0] == synthType) {
            const int slotID{splitId[1].toInt()};
            passthrough = Plugin::instance()->synthPassthroughClients()[slotID];
            if (splitId.count() > 2) {
                const int filterID{splitId[2].toInt()};
                filter = qobject_cast<JackPassthroughFilter*>(passthrough->equaliserSettings()[filterID].value<QObject*>());
            }
        } else if (splitId[0] == fxType) {
            const int trackID{splitId[1].toInt()};
            const int slotID{splitId[2].toInt()};
            passthrough = Plugin::instance()->fxPassthroughClients()[trackID][slotID];
            if (splitId.count() > 2) {
                const int filterID{splitId[3].toInt()};
                filter = qobject_cast<JackPassthroughFilter*>(passthrough->equaliserSettings()[filterID].value<QObject*>());
            }
        }
    }

    if (passthrough) {
        JackPassthroughFilter *soloFilter{nullptr};
        for (const QVariant &variant : passthrough->equaliserSettings()) {
            JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
            if (filter->soloed()) {
                soloFilter = filter;
                break;
            }
        }
        img = QImage(d->requestedSize, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColorConstants::Transparent);
        QPainter painter(&img);
        QRect frame(0, 0, d->requestedSize.width(), d->requestedSize.height());
        static float maxDB{24.0f};
        auto pixelsPerDouble = 2.0f * d->requestedSize.height() / juce::Decibels::decibelsToGain(maxDB);
        auto drawAndClearPath = [&painter, &frame, soloFilter](JackPassthroughFilter* filter,  QPolygonF &path) {
            QPen pen(filter->color());
            pen.setCosmetic(true);
            pen.setWidth(1);
            if (soloFilter) {
                pen.setStyle(soloFilter == filter ? Qt::SolidLine : Qt::DotLine);
            } else {
                pen.setStyle(filter->active() ? Qt::SolidLine : Qt::DotLine);
            }
            painter.setPen(pen);
            painter.drawPolyline(path);
            painter.setBrush(filter->selected() ? filter->color() : QColorConstants::Transparent);
            auto x = juce::roundToInt(frame.width() * getPositionForFrequency(filter->frequency()));
            auto y = juce::roundToInt(getPositionForGain(filter->gain(), 0, float(frame.height())));
            painter.drawLine(x, 0, x, y - 5);
            painter.drawLine(x, y + 4, x, frame.height());
            painter.drawEllipse(x - 4, y - 4, 7, 7);
            path.clear();
        };
        juce::Path path;
        QPolygonF polygon;
        if (filter) {
            // If there's a filter, then we are only drawing that one filter rather than the entire passthrough
            filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
            drawAndClearPath(filter, polygon);
        } else {
            // Otherwise we're drawing all of them
            passthrough->equaliserCreateFrequencyPlot(polygon, frame, pixelsPerDouble);
            QPen pen{QColorConstants::White};
            pen.setCosmetic(true);
            pen.setWidth(3);
            painter.setPen(pen);
            painter.drawPolyline(polygon);
            polygon.clear();
            for (const QVariant &variant : passthrough->equaliserSettings()) {
                JackPassthroughFilter *filter = qobject_cast<JackPassthroughFilter*>(variant.value<QObject*>());
                filter->createFrequencyPlot(polygon, frame, pixelsPerDouble);
                drawAndClearPath(filter, polygon);
            }
        }
    }

    Q_EMIT done(img);
}

#include "JackPassthroughFilterImageProvider.moc" // We have us some Q_OBJECT bits in here, so we need this one in here as well
