/*
 * Copyright (C) 2025 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "SketchpadTrackInfo.h"
#include "MidiRouterDevice.h"

SketchpadTrackInfo::SketchpadTrackInfo(int trackIndex, QObject* parent)
    : QObject(parent)
    , trackIndex(trackIndex)
{
    for (int i = 0; i < ZynthboxSlotCount; ++i) {
        zynthianChannels[i] = -1;
    }
}

QObject * SketchpadTrackInfo::getSequencerDevice() const
{
    return syncTimerSequencer;
}

QObject * SketchpadTrackInfo::getControllerDevice() const
{
    return syncTimerController;
}

QObject * SketchpadTrackInfo::getExternalDevice() const
{
    return externalDevice;
}

QObject * SketchpadTrackInfo::getCurrentPattern() const
{
    return currentlySelectedPattern;
}
