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

#include <QCoreApplication>
#include <QObject>
#include <QList>
#include <atomic>
#include <mutex>

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

private:
    explicit Plugin(QObject *parent = nullptr);
    void registerTypes(QQmlEngine *engine, const char *uri);

    te::Engine *tracktionEngine{nullptr};
    QList<ClipAudioSource*> createdClips;
    JuceEventLoop juceEventLoop;

    static std::atomic<Plugin*> singletonInstance;
    static std::mutex singletonMutex;
};
