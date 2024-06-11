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

#ifndef JACKPASSTHROUGHFILTERIMAGEPROVIDER_H
#define JACKPASSTHROUGHFILTERIMAGEPROVIDER_H

#include <QQuickAsyncImageProvider>
#include <memory>

/**
 * \brief An image provider which sends back a visual representation of a JackPassthrough's filter setup (or alternatively an individual filter)
 *
 * Depending on the type of passthrough client in question, you might use any number of types of URL here:
 * Synth: These are stored using their midi channel identifier - image://passthroughfilter/synth/midiChannel
 * FX: These are stored per-track - image://passthroughfilter/fx/trackID/slotID
 * Optionally you can add a specific filter on the end to show only that one filter, otherwise the whole client's worth of filters will be rendered
 */
class JackPassthroughFilterImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit JackPassthroughFilterImageProvider();
    ~JackPassthroughFilterImageProvider() override = default;

    /**
     * \brief Get an image.
     *
     * @param id The source of the image.
     * @param requestedSize The required size of the final image, unused.
     *
     * @return an asynchronous image response
     */
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif//JACKPASSTHROUGHFILTERIMAGEPROVIDER_H
