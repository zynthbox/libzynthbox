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

#include <iostream>

#include "ClipAudioSource.h"
#include "Helper.h"
#include "JUCEHeaders.h"
#include "SyncTimer.h"
#include "WaveFormItem.h"

using namespace std;

ScopedJuceInitialiser_GUI *initializer = nullptr;
SyncTimer *syncTimer = new SyncTimer();

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

  void setClipLength(ClipAudioSource *c, float lengthInSeconds) {
    c->setLength(lengthInSeconds);
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

  void destroyClip(ClipAudioSource *c) { delete c; }
};

JuceEventLoopThread elThread;

//////////////
/// ClipAudioSource API Bridge
//////////////
ClipAudioSource *ClipAudioSource_new(const char *filepath) {
  ClipAudioSource *sClip;

  Helper::callFunctionOnMessageThread(
      [&]() { sClip = new ClipAudioSource(syncTimer, filepath); }, true);

  return sClip;
}

void ClipAudioSource_play(ClipAudioSource *c, bool loop) {
  elThread.playClip(c, loop);
}

void ClipAudioSource_stop(ClipAudioSource *c) {
  cerr << "libzl : Stop Clip " << c;

  elThread.stopClip(c);
}

float ClipAudioSource_getDuration(ClipAudioSource *c) {
  return c->getDuration();
}

float ClipAudioSource_getProgress(ClipAudioSource *c) {
  return c->getProgress();
}

const char *ClipAudioSource_getFileName(ClipAudioSource *c) {
  return c->getFileName();
}

void ClipAudioSource_setProgressCallback(ClipAudioSource *c, void *obj,
                                         void (*functionPtr)(void *)) {
  c->setProgressCallback(obj, functionPtr);
}

void ClipAudioSource_setStartPosition(ClipAudioSource *c,
                                      float startPositionInSeconds) {
  elThread.setClipStartPosition(c, startPositionInSeconds);
}

void ClipAudioSource_setLength(ClipAudioSource *c, float lengthInSeconds) {
  elThread.setClipLength(c, lengthInSeconds);
}

void ClipAudioSource_setSpeedRatio(ClipAudioSource *c, float speedRatio) {
  elThread.setClipSpeedRatio(c, speedRatio);
}

void ClipAudioSource_setPitch(ClipAudioSource *c, float pitchChange) {
  elThread.setClipPitch(c, pitchChange);
}

void ClipAudioSource_setGain(ClipAudioSource *c, float db) {
  elThread.setClipGain(c, db);
}

void ClipAudioSource_setVolume(ClipAudioSource *c, float vol) {
  elThread.setClipVolume(c, vol);
}

void ClipAudioSource_destroy(ClipAudioSource *c) { elThread.destroyClip(c); }
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

void SyncTimer_queueClipToStart(ClipAudioSource *clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStart(clip); }, true);
}

void SyncTimer_queueClipToStop(ClipAudioSource *clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStop(clip); }, true);
}
//////////////
/// END SyncTimer API Bridge
//////////////

void initJuce() {
  cerr << "### INIT JUCE\n";
  elThread.startThread();
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
>>>>>>> 400988a (progress reporting from tracktion to qml)
