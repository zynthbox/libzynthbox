/*
 * Copyright (C) 2021 Dan Leinir Turthra Jensen <admin@leinir.dk>
 * Copyright (C) 2023 Anupam Basak <anupam.basak27@gmail.com>
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

#pragma once
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1

#include <QCoreApplication>
#include <QObject>
#include <QList>
#include <QQmlEngine>
#include <QHash>
#include <atomic>
#include <mutex>

#include "ClipAudioSource.h"
#include "JUCEHeaders.h"
#include "JuceEventLoop.h"

class Plugin : public QObject {
    Q_OBJECT

public:
    static Plugin* instance();

    // Delete the methods we dont want to avoid having copies of the singleton class
    Plugin(Plugin const&) = delete;
    void operator=(Plugin const&) = delete;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void shutdown();
    // Called by zynthbox when the configuration in webconf has been changed (for example the midi setup, so our MidiRouter can pick up any changes)
    Q_INVOKABLE void reloadZynthianConfiguration();
    Q_INVOKABLE float dBFromVolume(float vol);
    Q_INVOKABLE te::Engine* getTracktionEngine();
    Q_INVOKABLE void registerTypes(QQmlEngine *engine, const char *uri);
    Q_INVOKABLE void addCreatedClipToMap(ClipAudioSource *clip);
    Q_INVOKABLE void removeCreatedClipFromMap(ClipAudioSource *clip);
    Q_INVOKABLE ClipAudioSource* getClipById(int id);
    Q_INVOKABLE int nextClipId();

    /**
     * \brief Set the panning amount for the given channel
     * @param channel The channel you wish to set the pan amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @param amount The amount (-1 through 1, 0 being neutral) that you wish to set as the new pan amount
     */
    Q_INVOKABLE void setPanAmount(int channel, float amount);
    /**
    * \brief Retrieve the panning amount for the given channel
     * @param channel The channel you wish to get the pan amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @return The panning amount for the given channel (-1 through 1, 0 being neutral or if channel is out of bounds)
     */
    Q_INVOKABLE float getPanAmount(int channel);

    /**
    * \brief Retrieve the wet amount for Fx1
     * @param channel The channel you wish to get the wet amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @return The wet amount for the given channel (0 through 1, 0 being no audio and 1 being full)
     */
    Q_INVOKABLE float getWetFx1Amount(int channel);
    /**
     * \brief Set the wet amount for the Fx1
     * @param channel The channel you wish to get the wet amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @param amount The amount (0 through 1, 0 being no audio and 1 being full) that you wish to set as the new wet amount for Fx1
     */
    Q_INVOKABLE void setWetFx1Amount(int channel, float amount);

    /**
    * \brief Retrieve the wet amount for Fx2
     * @param channel The channel you wish to get the wet amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @return The wet amount for the given channel (0 through 1, 0 being no audio and 1 being full)
     */
    Q_INVOKABLE float getWetFx2Amount(int channel);
    /**
     * \brief Set the wet amount for the Fx2
     * @param channel The channel you wish to get the wet amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @param amount The amount (0 through 1, 0 being no audio and 1 being full) that you wish to set as the new wet amount for Fx2
     */
    Q_INVOKABLE void setWetFx2Amount(int channel, float amount);

    /**
    * \brief Retrieve the dry amount
     * @param channel The channel you wish to get the dry amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @return The dry amount for the given channel (0 through 1, 0 being no audio and 1 being full)
     */
    Q_INVOKABLE float getDryAmount(int channel);
    /**
     * \brief Set the dry amount
     * @param channel The channel you wish to set the dry amount for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @param amount The amount (0 through 1, 0 being no audio and 1 being full) that you wish to set as the new dry amount
     */
    Q_INVOKABLE void setDryAmount(int channel, float amount);

    /**
    * \brief Get muted property value
     * @param channel The channel you wish to get the muted value for (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @return The value for the given channel
     */
    Q_INVOKABLE float getMuted(int channel);
    /**
     * \brief Set muted property value
     * @param channel The channel you wish to set muted (-1 is GlobalPlayback, 0-9 is the channel with that index)
     * @param muted The value for the given channel that you wish to set
     */
    Q_INVOKABLE void setMuted(int channel, bool muted);

private:
    explicit Plugin(QObject *parent = nullptr);

    te::Engine *tracktionEngine{nullptr};
    JuceEventLoop juceEventLoop;
    QHash<int, ClipAudioSource *> createdClipsMap;
    int lastCreatedClipId{-1};

    static std::atomic<Plugin*> singletonInstance;
    static std::mutex singletonMutex;
};
