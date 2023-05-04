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

#include <unistd.h>
#include <iostream>
#include <chrono>
#include <jack/jack.h>
#include <QDebug>
#include <QTimer>
#include <QtQml/qqml.h>
#include <QQmlEngine>
#include <QQmlContext>
#include <QString>

#include "ClipAudioSource.h"
#include "Helper.h"
#include "JUCEHeaders.h"
#include "SamplerSynth.h"
#include "SyncTimer.h"
#include "WaveFormItem.h"
#include "AudioLevels.h"
#include "MidiRouter.h"
#include "JackPassthrough.h"
#include "FilterProxy.h"
#include "MidiRecorder.h"
#include "Note.h"
#include "NotesModel.h"
#include "PatternImageProvider.h"
#include "PatternModel.h"
#include "PlayGrid.h"
#include "SettingsContainer.h"
#include "SegmentHandler.h"
#include "MidiRouter.h"
#include "SyncTimer.h"
#include "ZLEngineBehaviour.h"

#include "Plugin.h"

using namespace std;
using namespace std::chrono;

Plugin::Plugin(QObject *parent)
    : QObject(parent) {}

Plugin *Plugin::instance()
{
    Plugin* sin = singletonInstance.load(std::memory_order_acquire);
    if (!sin) {
        std::lock_guard<std::mutex> myLock(singletonMutex);
        sin = singletonInstance.load(std::memory_order_relaxed);
        if (!sin) {
            sin = new Plugin(QCoreApplication::instance());
            singletonInstance.store(sin, std::memory_order_release);
        }
    }
    return sin;
}

void Plugin::initialize()
{
    qDebug() << "libzynthbox Initialisation Started";
    juceEventLoop.start();
    qDebug() << "Started juce event loop";

    bool initialisationCompleted{false};
    auto juceInitialiser = [&]() {
        qDebug() << "Instantiating tracktion engine";
        tracktionEngine = new te::Engine("libzynthbox", nullptr, std::make_unique<ZLEngineBehavior>());
        qDebug() << "Setting device type to JACK";
        tracktionEngine->getDeviceManager().deviceManager.setCurrentAudioDeviceType("JACK", true);
        qDebug() << "Initialising device manager";
        tracktionEngine->getDeviceManager().initialise(0, 2);
        qDebug() << "Initialisation completed";
        initialisationCompleted = true;
    };
    auto start = high_resolution_clock::now();
    while (!initialisationCompleted) {
        Helper::callFunctionOnMessageThread(juceInitialiser, true, 10000);
        if (!initialisationCompleted) {
            qWarning() << "Failed to initialise juce in 10 seconds, retrying...";
            if (tracktionEngine) {
                delete tracktionEngine;
                tracktionEngine = nullptr;
            }
        }
    }
    auto duration = duration_cast<milliseconds>(high_resolution_clock::now() - start);
    qDebug() << "JUCE initialisation took" << duration.count() << "ms";

    qDebug() << "Initialising SyncTimer";
    SyncTimer::instance();

    qDebug() << "Initialising MidiRouter";
    MidiRouter::instance();

    QObject::connect(MidiRouter::instance(), &MidiRouter::addedHardwareInputDevice, SyncTimer::instance(), &SyncTimer::addedHardwareInputDevice);
    QObject::connect(MidiRouter::instance(), &MidiRouter::removedHardwareInputDevice, SyncTimer::instance(), &SyncTimer::removedHardwareInputDevice);
    QObject::connect(MidiRouter::instance(), &MidiRouter::addedHardwareOutputDevice, SyncTimer::instance(), &SyncTimer::addedHardwareOutputDevice);
    QObject::connect(MidiRouter::instance(), &MidiRouter::removedHardwareOutputDevice, SyncTimer::instance(), &SyncTimer::removedHardwareOutputDevice);

    qDebug() << "Initialising SamplerSynth";
    SamplerSynth::instance()->initialize(tracktionEngine);

    // Make sure to have the AudioLevels instantiated by explicitly calling instance
    qDebug() << "Initialising AudioLevels";
    AudioLevels::instance();
}

void Plugin::shutdown()
{
    juceEventLoop.stop();
}

void Plugin::reloadZynthianConfiguration()
{
    MidiRouter::instance()->reloadConfiguration();
}

float Plugin::dBFromVolume(float vol)
{
    return te::volumeFaderPositionToDB(vol);
}

tracktion_engine::Engine *Plugin::getTracktionEngine()
{
    return tracktionEngine;
}

void Plugin::registerTypes(QQmlEngine *engine, const char *uri)
{
    engine->addImageProvider("pattern", new PatternImageProvider());

    qmlRegisterType<FilterProxy>(uri, 1, 0, "FilterProxy");
    qmlRegisterUncreatableType<Note>(uri, 1, 0, "Note", "Use the getNote function on the main PlayGrid global object to get one of these");
    qmlRegisterUncreatableType<NotesModel>(uri, 1, 0, "NotesModel", "Use the getModel function on the main PlayGrid global object to get one of these");
    qmlRegisterUncreatableType<PatternModel>(uri, 1, 0, "PatternModel", "Use the getPatternModel function on the main PlayGrid global object to get one of these");
    qmlRegisterUncreatableType<SettingsContainer>(uri, 1, 0, "SettingsContainer", "This is for internal use only");
    qmlRegisterType<PlayGrid>(uri, 1, 0, "PlayGrid");
    qmlRegisterSingletonType<PlayGridManager>(uri, 1, 0, "PlayGridManager", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(scriptEngine)
        PlayGridManager *playGridManager = PlayGridManager::instance();
        playGridManager->setEngine(engine);
        QQmlEngine::setObjectOwnership(playGridManager, QQmlEngine::CppOwnership);
        return playGridManager;
    });
    qmlRegisterSingletonType<SegmentHandler>(uri, 1, 0, "SegmentHandler", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        SegmentHandler *segmentHandler = SegmentHandler::instance();
        QQmlEngine::setObjectOwnership(segmentHandler, QQmlEngine::CppOwnership);
        return segmentHandler;
    });
    qmlRegisterSingletonType<MidiRecorder>(uri, 1, 0, "MidiRecorder", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        MidiRecorder *midiRecorder = MidiRecorder::instance();
        QQmlEngine::setObjectOwnership(midiRecorder, QQmlEngine::CppOwnership);
        return midiRecorder;
    });
    qmlRegisterSingletonType<MidiRouter>(uri, 1, 0, "MidiRouter", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        MidiRouter *midiRouter = MidiRouter::instance();
        QQmlEngine::setObjectOwnership(midiRouter, QQmlEngine::CppOwnership);
        return midiRouter;
    });
    qmlRegisterSingletonType<SyncTimer>(uri, 1, 0, "SyncTimer", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        SyncTimer *syncTimer = SyncTimer::instance();
        QQmlEngine::setObjectOwnership(syncTimer, QQmlEngine::CppOwnership);
        return syncTimer;
    });
    qmlRegisterSingletonType<AudioLevels>(uri, 1, 0, "AudioLevels", [](QQmlEngine */*engine*/, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(scriptEngine)
        return AudioLevels::instance();
    });
    qmlRegisterType<WaveFormItem>(uri, 1, 0, "WaveFormItem");
}

void Plugin::addCreatedClipToMap(ClipAudioSource *clip)
{
    createdClipsMap.insert(clip->id(), clip);
}

void Plugin::removeCreatedClipFromMap(ClipAudioSource *clip)
{
    createdClipsMap.remove(clip->id());
}

ClipAudioSource* Plugin::getClipById(int id)
{
    return createdClipsMap.value(id, nullptr);
}

int Plugin::nextClipId()
{
    return ++lastCreatedClipId;
}

std::atomic<Plugin*> Plugin::singletonInstance;
std::mutex Plugin::singletonMutex;
