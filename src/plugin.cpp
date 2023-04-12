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

#include "plugin.h"

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

using namespace std;
using namespace std::chrono;

ScopedJuceInitialiser_GUI *initializer = nullptr;
SyncTimer *syncTimer{nullptr};
te::Engine *tracktionEngine{nullptr};
QList<ClipAudioSource*> createdClips;

class JuceEventLoopThread : public Thread {
public:
  JuceEventLoopThread() : Thread("Juce EventLoop Thread") {}

  void run() override {
    if (initializer == nullptr)
      initializer = new ScopedJuceInitialiser_GUI();

    MessageManager::getInstance()->runDispatchLoop();
  }

  void playClip(ClipAudioSource *c, bool loop) { c->play(loop); }

  void stopClip(ClipAudioSource *c) { c->stop(); }

  void playClipOnChannel(ClipAudioSource *c, bool loop, int midiChannel) { c->play(loop, midiChannel); }

  void stopClipOnChannel(ClipAudioSource *c, int midiChannel) { c->stop(midiChannel); }

  void setClipLength(ClipAudioSource *c, float beat, int bpm) {
    c->setLength(beat, bpm);
  }

  void setClipPan(ClipAudioSource *c, float pan) {
    c->setPan(pan);
  }

  void setClipStartPosition(ClipAudioSource *c, float startPositionInSeconds) {
    c->setStartPosition(startPositionInSeconds);
  }

  void setClipSpeedRatio(ClipAudioSource *c, float speedRatio) {
    c->setSpeedRatio(speedRatio);
  }

  void setClipPitch(ClipAudioSource *c, float pitchChange) {
    c->setPitch(pitchChange);
  }

  void setClipGain(ClipAudioSource *c, float db) { c->setGain(db); }

  void setClipVolume(ClipAudioSource *c, float vol) { c->setVolume(vol); }

  void stopClips(int size, ClipAudioSource **clips) {
    for (int i = 0; i < size; i++) {
      ClipAudioSource *clip = clips[i];

      cerr << "Stopping clip arr[" << i << "] : " << clips[i] << endl;
      clip->stop();
    }
  }

  void destroyClip(ClipAudioSource *c) {
    SamplerSynth::instance()->unregisterClip(c);
    c->deleteLater();
  }
};

JuceEventLoopThread elThread;

void registerTypes(const char *uri)
{
  qmlRegisterType<FilterProxy>(uri, 1, 0, "FilterProxy");
  qmlRegisterUncreatableType<Note>(uri, 1, 0, "Note", "Use the getNote function on the main PlayGrid global object to get one of these");
  qmlRegisterUncreatableType<NotesModel>(uri, 1, 0, "NotesModel", "Use the getModel function on the main PlayGrid global object to get one of these");
  qmlRegisterUncreatableType<PatternModel>(uri, 1, 0, "PatternModel", "Use the getPatternModel function on the main PlayGrid global object to get one of these");
  qmlRegisterUncreatableType<SettingsContainer>(uri, 1, 0, "SettingsContainer", "This is for internal use only");
  qmlRegisterType<PlayGrid>(uri, 1, 0, "PlayGrid");
  qmlRegisterSingletonType<PlayGridManager>(uri, 1, 0, "PlayGridManager", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
    Q_UNUSED(scriptEngine)
    // Register pattern ImageProvider here as we dont have access to QQmlEngine outside of this callback
    engine->addImageProvider("pattern", new PatternImageProvider());
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

class ZLEngineBehavior : public te::EngineBehaviour {
  bool autoInitialiseDeviceManager() override { return false; }
};

void initialize() {
  qDebug() << "### JUCE initialisation start";
  elThread.startThread();
  qDebug() << "Started juce event loop, initialising...";

  bool initialisationCompleted{false};
  auto juceInitialiser = [&](){
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
  qDebug() << "### JUCE initialisation took" << duration.count() << "ms";

  qDebug() << "Initialising SyncTimer";
  syncTimer = SyncTimer::instance();

  qDebug() << "Initialising MidiRouter";
  MidiRouter::instance();

  QObject::connect(MidiRouter::instance(), &MidiRouter::addedHardwareInputDevice, syncTimer, &SyncTimer::addedHardwareInputDevice);
  QObject::connect(MidiRouter::instance(), &MidiRouter::removedHardwareInputDevice, syncTimer, &SyncTimer::removedHardwareInputDevice);
  QObject::connect(MidiRouter::instance(), &MidiRouter::addedHardwareOutputDevice, syncTimer, &SyncTimer::addedHardwareOutputDevice);
  QObject::connect(MidiRouter::instance(), &MidiRouter::removedHardwareOutputDevice, syncTimer, &SyncTimer::removedHardwareOutputDevice);

  qDebug() << "Initialising SamplerSynth";
  SamplerSynth::instance()->initialize(tracktionEngine);

  // Make sure to have the AudioLevels instantiated by explicitly calling instance
  AudioLevels::instance();

  registerTypes("io.zynthbox.components");
}

void shutdown() {
  elThread.stopThread(500);
  initializer = nullptr;
}

void reloadZynthianConfiguration() {
  MidiRouter::instance()->reloadConfiguration();
}

void stopClips(int size, ClipAudioSource **clips) {
  elThread.stopClips(size, clips);
}

float dBFromVolume(float vol) { return te::volumeFaderPositionToDB(vol); }

//////////////
/// BEGIN ClipAudioSource API Bridge
//////////////
ClipAudioSource *ClipAudioSource_byID(int id) {
    ClipAudioSource *clip{nullptr};
    for (ClipAudioSource *needle : createdClips) {
        if (needle->id() == id) {
            clip = needle;
            break;
        }
    }
    return clip;
}

ClipAudioSource *ClipAudioSource_new(const char *filepath, bool muted) {
  ClipAudioSource *sClip = new ClipAudioSource(tracktionEngine, syncTimer, filepath, muted, qApp);
  sClip->moveToThread(qApp->thread());

  static int clipID{1};
  sClip->setId(clipID);
  ++clipID;

  createdClips << sClip;
  return sClip;
}

void ClipAudioSource_play(ClipAudioSource *c, bool loop) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.playClip(c, loop);
      }, true);
}

void ClipAudioSource_stop(ClipAudioSource *c) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.stopClip(c);
      }, true);
}

void ClipAudioSource_playOnChannel(ClipAudioSource *c, bool loop, int midiChannel) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.playClipOnChannel(c, loop, midiChannel);
      }, true);
}

void ClipAudioSource_stopOnChannel(ClipAudioSource *c, int midiChannel) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.stopClipOnChannel(c, midiChannel);
      }, true);
}

float ClipAudioSource_getDuration(ClipAudioSource *c) {
  return c->getDuration();
}

const char *ClipAudioSource_getFileName(ClipAudioSource *c) {
  return c->getFileName();
}

void ClipAudioSource_setProgressCallback(ClipAudioSource *c,
                                         void (*functionPtr)(float)) {
  c->setProgressCallback(functionPtr);
}

void ClipAudioSource_setStartPosition(ClipAudioSource *c,
                                      float startPositionInSeconds) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipStartPosition(c, startPositionInSeconds);
      }, true);
}

void ClipAudioSource_setLength(ClipAudioSource *c, float beat, int bpm) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipLength(c, beat, bpm);
      }, true);
}

void ClipAudioSource_setPan(ClipAudioSource *c, float pan) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipPan(c, pan);
      }, true);
}

void ClipAudioSource_setSpeedRatio(ClipAudioSource *c, float speedRatio) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipSpeedRatio(c, speedRatio);
      }, true);
}

void ClipAudioSource_setPitch(ClipAudioSource *c, float pitchChange) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipPitch(c, pitchChange);
      }, true);
}

void ClipAudioSource_setGain(ClipAudioSource *c, float db) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipGain(c, db);
      }, true);
}

void ClipAudioSource_setVolume(ClipAudioSource *c, float vol) {
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.setClipVolume(c, vol);
      }, true);
}

void ClipAudioSource_setAudioLevelChangedCallback(ClipAudioSource *c,
                                                  void (*functionPtr)(float)) {
  c->setAudioLevelChangedCallback(functionPtr);
}

void ClipAudioSource_setSlices(ClipAudioSource *c, int slices) {
    c->setSlices(slices);
}

int ClipAudioSource_keyZoneStart(ClipAudioSource *c) {
  return c->keyZoneStart();
}

void ClipAudioSource_setKeyZoneStart(ClipAudioSource *c, int keyZoneStart) {
  c->setKeyZoneStart(keyZoneStart);
}

int ClipAudioSource_keyZoneEnd(ClipAudioSource *c) {
  return c->keyZoneEnd();
}

void ClipAudioSource_setKeyZoneEnd(ClipAudioSource *c, int keyZoneEnd) {
  c->setKeyZoneEnd(keyZoneEnd);
}

int ClipAudioSource_rootNote(ClipAudioSource *c) {
  return c->rootNote();
}

void ClipAudioSource_setRootNote(ClipAudioSource *c, int rootNote) {
  c->setRootNote(rootNote);
}

void ClipAudioSource_destroy(ClipAudioSource *c) {
  ClipAudioSource *clip = qobject_cast<ClipAudioSource*>(c);
  if (clip) {
    createdClips.removeAll(clip);
  }
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.destroyClip(c);
      }, true);
}

int ClipAudioSource_id(ClipAudioSource *c) { return c->id(); }
//////////////
/// END ClipAudioSource API Bridge
//////////////

//////////////
/// BEGIN SyncTimer API Bridge
//////////////
void SyncTimer_startTimer(int interval) { syncTimer->start(interval); }

void SyncTimer_setBpm(uint bpm) { syncTimer->setBpm(bpm); }

int SyncTimer_getMultiplier() { return syncTimer->getMultiplier(); }

void SyncTimer_stopTimer() { syncTimer->stop(); }

void SyncTimer_registerTimerCallback(void (*functionPtr)(int)) {
  syncTimer->addCallback(functionPtr);
}

void SyncTimer_deregisterTimerCallback(void (*functionPtr)(int)) {
  syncTimer->removeCallback(functionPtr);
}

void SyncTimer_queueClipToStart(ClipAudioSource *clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStart(clip); }, true);
}

void SyncTimer_queueClipToStartOnChannel(ClipAudioSource *clip, int midiChannel) {
  cerr << "libzl : Queue Clip " << clip << " to start on channel " << midiChannel << std::endl;
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStartOnChannel(clip, midiChannel); }, true);
}

void SyncTimer_queueClipToStop(ClipAudioSource *clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStop(clip); }, true);
}

void SyncTimer_queueClipToStopOnChannel(ClipAudioSource *clip, int midiChannel) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStopOnChannel(clip, midiChannel); }, true);
}
//////////////
/// END SyncTimer API Bridge
//////////////

//////////////
/// BEGIN AudioLevels API Bridge
//////////////
bool AudioLevels_isRecording() {
  return AudioLevels::instance()->isRecording();
}

void AudioLevels_setRecordGlobalPlayback(bool shouldRecord) {
  AudioLevels::instance()->setRecordGlobalPlayback(shouldRecord);
}

void AudioLevels_setGlobalPlaybackFilenamePrefix(const char *fileNamePrefix) {
  AudioLevels::instance()->setGlobalPlaybackFilenamePrefix(QString::fromUtf8(fileNamePrefix));
}

void AudioLevels_startRecording() {
  AudioLevels::instance()->startRecording();
}

void AudioLevels_stopRecording() {
  AudioLevels::instance()->stopRecording();
}

void AudioLevels_setRecordPortsFilenamePrefix(const char *fileNamePrefix)
{
  AudioLevels::instance()->setRecordPortsFilenamePrefix(QString::fromUtf8(fileNamePrefix));
}

void AudioLevels_addRecordPort(const char *portName, int channel)
{
  AudioLevels::instance()->addRecordPort(QString::fromUtf8(portName), channel);
}

void AudioLevels_removeRecordPort(const char *portName, int channel)
{
  AudioLevels::instance()->removeRecordPort(QString::fromUtf8(portName), channel);
}

void AudioLevels_clearRecordPorts()
{
  AudioLevels::instance()->clearRecordPorts();
}

void AudioLevels_setShouldRecordPorts(bool shouldRecord)
{
  AudioLevels::instance()->setShouldRecordPorts(shouldRecord);
}
/// //////////
/// END AudioLevels API Bridge
//////////////

//////////////
/// BEGIN JackPassthrough API Bridge
//////////////
void JackPassthrough_setPanAmount(int channel, float amount)
{
  if (channel == -1) {
    qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->setPanAmount(amount);
  } else if (channel > -1 && channel < 10) {
    qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->setPanAmount(amount);
  }
}

float JackPassthrough_getPanAmount(int channel)
{
  float amount{0.0f};
  if (channel == -1) {
    amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->panAmount();
  } else if (channel > -1 && channel < 10) {
    amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->panAmount();
  }
  return amount;
}

float JackPassthrough_getWetFx1Amount(int channel)
{
    float amount{0.0f};
    if (channel == -1) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->wetFx1Amount();
    } else if (channel > -1 && channel < 10) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->wetFx1Amount();
    }
    return amount;
}

void JackPassthrough_setWetFx1Amount(int channel, float amount)
{
    if (channel == -1) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->setWetFx1Amount(amount);
    } else if (channel > -1 && channel < 10) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->setWetFx1Amount(amount);
    }
}

float JackPassthrough_getWetFx2Amount(int channel)
{
    float amount{0.0f};
    if (channel == -1) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->wetFx2Amount();
    } else if (channel > -1 && channel < 10) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->wetFx2Amount();
    }
    return amount;
}

void JackPassthrough_setWetFx2Amount(int channel, float amount)
{

    if (channel == -1) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->setWetFx2Amount(amount);
    } else if (channel > -1 && channel < 10) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->setWetFx2Amount(amount);
    }
}

float JackPassthrough_getDryAmount(int channel)
{
    float amount{0.0f};
    if (channel == -1) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->dryAmount();
    } else if (channel > -1 && channel < 10) {
      amount = qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->dryAmount();
    }
    return amount;
}

void JackPassthrough_setDryAmount(int channel, float amount)
{
    if (channel == -1) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->setDryAmount(amount);
    } else if (channel > -1 && channel < 10) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->setDryAmount(amount);
    }
}

float JackPassthrough_getMuted(int channel)
{
    bool muted{false};
    if (channel == -1) {
      muted = qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->muted();
    } else if (channel > -1 && channel < 10) {
      muted = qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->muted();
    }
    return muted;
}

void JackPassthrough_setMuted(int channel, bool muted)
{
    if (channel == -1) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->globalPlaybackClient())->setMuted(muted);
    } else if (channel > -1 && channel < 10) {
      qobject_cast<JackPassthrough*>(MidiRouter::instance()->channelPassthroughClients().at(channel))->setMuted(muted);
    }
}
//////////////
/// END JackPassthrough API Bridge
//////////////
