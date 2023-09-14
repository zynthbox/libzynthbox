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

/*
 * To use, uncomment the define below, and then from gdb, you can call:

 (gdb) call KDAB::printQmlStackTraces()
Stack trace for QQuickView(0x7fffffffd2e0 active exposed, visibility=QWindow::Windowed, flags=QFlags<Qt::WindowType>(Window), geometry=0,1290 500x500)
    onSomeIndirection2Changed [qrc:/main.qml:28]
    onSomeIndirectionChanged [qrc:/main.qml:22]
    onClicked [qrc:/main.qml:17]

    * Based on: https://github.com/iamsergio/gdb_qml_backtraces
 */
// #define KDAB_QML_STACKTRACE
#ifdef KDAB_QML_STACKTRACE
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlContext>
#include <QQuickItem>
#include <5.15.8/QtQml/private/qqmlengine_p.h>

namespace KDAB {

QString qmlStackTrace(QV4::ExecutionEngine *engine)
{
    QString result;
    QVector<QV4::StackFrame> frames = engine->stackTrace(15);
    for (auto &f : frames)
        result += QStringLiteral("    %1 [%2:%3]\n").arg(f.function, f.source).arg(f.line);

    return result;
}

void printQmlStackTraces()
{
    auto windows = qApp->topLevelWindows();
    for (QWindow *w : windows) {
        if (auto qw = qobject_cast<QQuickWindow*>(w)) {
            QQuickItem *item = qw->contentItem();
            QQmlContext* context = QQmlEngine::contextForObject(item);
            if (context) {
                QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(context->engine());
                QV4::ExecutionEngine *v4engine = enginePriv->v4engine();
                qDebug() << "Stack trace for" << qw << "engine" << v4engine << "with the current top frame being" << v4engine->currentStackFrame;
                qDebug().noquote() << qmlStackTrace(v4engine);
                qDebug() << "\n";
            }
        }
    }
    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(Plugin::instance()->qmlEngine());
    QV4::ExecutionEngine *v4engine = enginePriv->v4engine();
    qDebug() << "Stack trace for the Plugin's engine" << v4engine << "with the current top frame being" << v4engine->currentStackFrame;
    qDebug().noquote() << qmlStackTrace(v4engine);
    qDebug() << "\n";
}

}

#endif

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

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    if (msg == QString::fromLocal8Bit("QObject::connect(QObject, QQmlDMObjectData): invalid nullptr parameter")) {
        raise(SIGSEGV);
    }
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }
}

void Plugin::initialize()
{
    // qInstallMessageHandler(myMessageOutput);
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

    qDebug() << "Creating GlobalPlayback Passthrough Client";
    m_globalPlaybackClient = new JackPassthrough("GlobalPlayback", QCoreApplication::instance(), true, false, false);
    qDebug() << "Creating SynthPassthroughClient";
    for (int i = 0; i < 16; i++) {
        m_synthPassthroughClients << new JackPassthrough(QString("SynthPassthrough:Synth%1").arg(i+1), QCoreApplication::instance(), true, false, false);
    }
    qDebug() << "Creating Channel Passthrough Client";
    // Create a ChannelPassthrough client for each of five lanes on each of the ten channels
    for (int channelNumber = 0; channelNumber < 10; ++channelNumber) {
        for (int laneNumber = 0; laneNumber < 5; ++laneNumber) {
            JackPassthrough* client = new JackPassthrough(QString("ChannelPassthrough:Channel%1-lane%2").arg(channelNumber+1).arg(laneNumber+1), QCoreApplication::instance());
            client->setWetFx1Amount(0.0f);
            client->setWetFx2Amount(0.0f);
            m_channelPassthroughClients << client;
        }
    }
    // Create 10 FX Passthrough client for 10 channels having 5 lanes each for each fx slot in a channel
    qDebug() << "Creating FX Passthrough Client";
    for (int channelNumber = 0; channelNumber < 10; ++channelNumber) {
        QList<JackPassthrough*> lanes;
        for (int laneNumber = 0; laneNumber < 5; ++laneNumber) {
            JackPassthrough* fxPassthrough = new JackPassthrough(QString("FXPassthrough:Channel%1-lane%2").arg(channelNumber+1).arg(laneNumber+1), QCoreApplication::instance(), true, true, false);
            fxPassthrough->setDryWetMixAmount(1.0f);
            lanes << fxPassthrough;
        }
        m_fxPassthroughClients << lanes;
    }

    qDebug() << "Registering qml meta types";
    qRegisterMetaType<AudioLevels*>("AudioLevels");
    qRegisterMetaType<JackPassthrough*>("JackPassthrough");
    qRegisterMetaType<PatternModel*>("PatternModel");
    qRegisterMetaType<PlayGridManager*>("PlayGridManager");
    qRegisterMetaType<SegmentHandler*>("SegmentHandler");
    qRegisterMetaType<SyncTimer*>("SyncTimer");
    qRegisterMetaType<WaveFormItem*>("WaveFormItem");
    qRegisterMetaType<Plugin*>("Plugin");

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
    m_qmlEngine = engine;
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
    qmlRegisterSingletonType<Plugin>(uri, 1, 0, "Plugin", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(scriptEngine)
        Plugin *plugin = instance();
        QQmlEngine::setObjectOwnership(plugin, QQmlEngine::CppOwnership);
        return plugin;
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

JackPassthrough *Plugin::globalPlaybackClient() const
{
    return m_globalPlaybackClient;
}

QList<JackPassthrough *> Plugin::synthPassthroughClients() const
{
    return m_synthPassthroughClients;
}

QList<JackPassthrough *> Plugin::channelPassthroughClients() const
{
    return m_channelPassthroughClients;
}

QList<QList<JackPassthrough *>> Plugin::fxPassthroughClients() const
{
    return m_fxPassthroughClients;
}

QQmlEngine * Plugin::qmlEngine() const
{
    if (m_qmlEngine == nullptr) {
        qWarning() << Q_FUNC_INFO << "QML Engine was null when attempting to retrieve it - this function should never be called before the Plugin types have been registered";
    }
    return m_qmlEngine;
}

std::atomic<Plugin*> Plugin::singletonInstance;
std::mutex Plugin::singletonMutex;
