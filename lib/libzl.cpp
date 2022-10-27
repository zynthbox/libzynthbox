<<<<<<< HEAD
/*
  ==============================================================================

    libzl.cpp
    Created: 9 Aug 2021 6:28:51pm
    Author:  root

  ==============================================================================
*/

#include "libzl.h"

#include <iostream>

#include "ClipAudioSource.h"
#include "Helper.h"
#include "JUCEHeaders.h"
#include "SyncTimer.h"
#include "WaveFormItem.h"

using namespace std;

ScopedJuceInitialiser_GUI* initializer = nullptr;
SyncTimer* syncTimer = new SyncTimer();

class JuceEventLoopThread : public Thread {
 public:
  JuceEventLoopThread() : Thread("Juce EventLoop Thread") {}

  void run() override {
    if (initializer == nullptr) initializer = new ScopedJuceInitialiser_GUI();

    MessageManager::getInstance()->runDispatchLoop();
  }

  void playClip(ClipAudioSource* c) { c->play(); }

  void stopClip(ClipAudioSource* c) { c->stop(); }

  void setClipLength(ClipAudioSource* c, float lengthInSeconds) {
    c->setLength(lengthInSeconds);
  }

  void setClipStartPosition(ClipAudioSource* c, float startPositionInSeconds) {
    c->setStartPosition(startPositionInSeconds);
  }

  void setClipSpeedRatio(ClipAudioSource* c, float speedRatio) {
    c->setSpeedRatio(speedRatio);
  }

  void setClipPitch(ClipAudioSource* c, float pitchChange) {
    c->setPitch(pitchChange);
  }

  void stopClips(int size, ClipAudioSource** clips) {
    for (int i = 0; i < size; i++) {
      ClipAudioSource* clip = clips[i];

      cerr << "Stopping clip arr[" << i << "] : " << clips[i] << endl;
      clip->stop();
    }
  }

  void destroyClip(ClipAudioSource* c) { delete c; }
};

JuceEventLoopThread elThread;

//////////////
/// ClipAudioSource API Bridge
//////////////
ClipAudioSource* ClipAudioSource_new(const char* filepath) {
  ClipAudioSource* sClip;

  Helper::callFunctionOnMessageThread(
      [&]() { sClip = new ClipAudioSource(syncTimer, filepath); }, true);

  return sClip;
}

void ClipAudioSource_play(ClipAudioSource* c) {
  //  Helper::callFunctionOnMessageThread([&]() { c->play(); }, true);
  elThread.playClip(c);
}

void ClipAudioSource_stop(ClipAudioSource* c) {
  cerr << "libzl : Stop Clip " << c;

  //  Helper::callFunctionOnMessageThread([&]() { c->stop(); });  //, true);
  //  c->stop();

  elThread.stopClip(c);
}

float ClipAudioSource_getDuration(ClipAudioSource* c) {
  return c->getDuration();
}

const char* ClipAudioSource_getFileName(ClipAudioSource* c) {
  return c->getFileName();
}

void ClipAudioSource_setStartPosition(ClipAudioSource* c,
                                      float startPositionInSeconds) {
  //  Helper::callFunctionOnMessageThread(
  //      [&]() { c->setStartPosition(startPositionInSeconds); }, true);
  elThread.setClipStartPosition(c, startPositionInSeconds);
}

void ClipAudioSource_setLength(ClipAudioSource* c, float lengthInSeconds) {
  //  Helper::callFunctionOnMessageThread([&]() { c->setLength(lengthInSeconds);
  //  },
  //                                      true);
  elThread.setClipLength(c, lengthInSeconds);
}

void ClipAudioSource_setSpeedRatio(ClipAudioSource* c, float speedRatio) {
  //  Helper::callFunctionOnMessageThread([&]() { c->setSpeedRatio(speedRatio);
  //  },
  //                                      true);
  elThread.setClipSpeedRatio(c, speedRatio);
}

void ClipAudioSource_setPitch(ClipAudioSource* c, float pitchChange) {
  //  Helper::callFunctionOnMessageThread([&]() { c->setPitch(pitchChange); },
  //                                      true);

  elThread.setClipPitch(c, pitchChange);
}

void ClipAudioSource_destroy(ClipAudioSource* c) { elThread.destroyClip(c); }
//////////////
/// END ClipAudioSource API Bridge
//////////////

//////////////
/// SynTimer API Bridge
//////////////
void SyncTimer_startTimer(int interval) { syncTimer->start(interval); }

void SyncTimer_stopTimer() { syncTimer->stop(); }

void SyncTimer_registerTimerCallback(void (*functionPtr)()) {
  syncTimer->setCallback(functionPtr);
}

void SyncTimer_queueClipToStart(ClipAudioSource* clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStart(clip); }, true);
}

void SyncTimer_queueClipToStop(ClipAudioSource* clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStop(clip); }, true);
}
//////////////
/// END SyncTimer API Bridge
//////////////

<<<<<<< HEAD
<<<<<<< HEAD
void startLoop(const char* filepath) {
  //  ScopedJuceInitialiser_GUI libraryInitialiser;

  //  te::Engine engine{"libzl"};
  //  te::Edit edit{engine, te::createEmptyEdit(engine), te::Edit::forEditing,
  //                nullptr, 0};
  //  te::TransportControl& transport{edit.getTransport()};

  //  auto wavFile = File(filepath);
  //  const File editFile("/tmp/editfile");
  //  auto clip = EngineHelpers::loadAudioFileAsClip(edit, wavFile);

  //  te::TimeStretcher ts;

  //  for (auto mode : ts.getPossibleModes(engine, true)) {
  //    cerr << "Mode : " << mode << endl;
  //  }

  //  clip->setAutoTempo(false);
  //  clip->setAutoPitch(false);
  //  clip->setTimeStretchMode(te::TimeStretcher::defaultMode);

  //  EngineHelpers::loopAroundClip(*clip);
  //  clip->setSpeedRatio(2.0);
  //  clip->setPitchChange(12);
  //  EngineHelpers::loopAroundClip(*clip);

  //  MessageManager::getInstance()->runDispatchLoop();
}

=======
>>>>>>> 9236df2 (Keep looping clip once started at beat 1)
void initJuce() {
  cerr << "### INIT JUCE\n";
  elThread.startThread();
}

void shutdownJuce() {
  elThread.stopThread(500);
  initializer = nullptr;
<<<<<<< HEAD
=======
void registerGraphicTypes()
{
    qmlRegisterType<WaveFormItem>("JuceGraphics", 1, 0, "WaveFormItem");
>>>>>>> 5d4ff66 (can be instantiated from QML)
=======
}

void registerGraphicTypes() {
  qmlRegisterType<WaveFormItem>("JuceGraphics", 1, 0, "WaveFormItem");
>>>>>>> 80dd50d (Refactor and add logs)
}

void stopClips(int size, ClipAudioSource** clips) {
  elThread.stopClips(size, clips);
}
=======
/*
  ==============================================================================

    libzl.cpp
    Created: 9 Aug 2021 6:28:51pm
    Author:  root

  ==============================================================================
*/

#include "libzl.h"

#include <unistd.h>
#include <iostream>
#include <chrono>
using namespace std::chrono;

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

using namespace std;

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

//////////////
/// ClipAudioSource API Bridge
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
  cerr << "libzl : Start Clip " << c << std::endl;
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.playClip(c, loop);
      }, true);
}

void ClipAudioSource_stop(ClipAudioSource *c) {
  cerr << "libzl : Stop Clip " << c << std::endl;
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.stopClip(c);
      }, true);
}

void ClipAudioSource_playOnChannel(ClipAudioSource *c, bool loop, int midiChannel) {
  cerr << "libzl : Play Clip " << c << " on channel " << midiChannel << std::endl;
  Helper::callFunctionOnMessageThread(
      [&]() {
        elThread.playClipOnChannel(c, loop, midiChannel);
      }, true);
}

void ClipAudioSource_stopOnChannel(ClipAudioSource *c, int midiChannel) {
  cerr << "libzl : Stop Clip " << c << " on channel " << midiChannel << std::endl;
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
/// SynTimer API Bridge
//////////////
QObject *SyncTimer_instance() { return syncTimer; }

void SyncTimer_startTimer(int interval) { syncTimer->start(interval); }

void SyncTimer_setBpm(uint bpm) { syncTimer->setBpm(bpm); }

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
  cerr << "libzl : Queue Clip " << clip << " to stop on channel " << midiChannel << std::endl;
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStopOnChannel(clip, midiChannel); }, true);
}
//////////////
/// END SyncTimer API Bridge
//////////////

class ZLEngineBehavior : public te::EngineBehaviour {
  bool autoInitialiseDeviceManager() override { return false; }
};

void initJuce() {
  qDebug() << "### JUCE initialisation start";
  elThread.startThread();
  qDebug() << "Started juce event loop, initialising...";

  bool initialisationCompleted{false};
  auto juceInitialiser = [&](){
    qDebug() << "Getting us an engine";
    tracktionEngine = new te::Engine("libzl", nullptr, std::make_unique<ZLEngineBehavior>());
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

  qDebug() << "Initialising SamplerSynth";
  SamplerSynth::instance()->initialize(tracktionEngine);

  // Make sure to have the AudioLevels instantiated by explicitly calling instance
  AudioLevels::instance();

  qmlRegisterSingletonType<AudioLevels>("libzl", 1, 0, "AudioLevels", [](QQmlEngine */*engine*/, QJSEngine *scriptEngine) -> QObject * {
    Q_UNUSED(scriptEngine)

    return AudioLevels::instance();
  });
}

void shutdownJuce() {
  elThread.stopThread(500);
  initializer = nullptr;
}

void registerGraphicTypes() {
  qmlRegisterType<WaveFormItem>("JuceGraphics", 1, 0, "WaveFormItem");
}

void stopClips(int size, ClipAudioSource **clips) {
  elThread.stopClips(size, clips);
}
<<<<<<< HEAD
>>>>>>> 400988a (progress reporting from tracktion to qml)
=======

float dBFromVolume(float vol) { return te::volumeFaderPositionToDB(vol); }
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 2246f24 (Add API to get dB from volume)
=======

void setRecordingAudioLevelCallback(void (*functionPtr)(float)) {
  recordingAudioLevelCallback = functionPtr;
}
>>>>>>> 6ae7881 (Implement jack client to listen to input audio level)
=======
>>>>>>> be89107 (Implement AudioLevels QML Singleton Type to expose audio levels of jack ports as properties)
=======

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
<<<<<<< HEAD
>>>>>>> ffd17cc (AudioLevels :)
=======

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
>>>>>>> 00c1200 (libzl : Add port recorder methods to C API for consumption)
