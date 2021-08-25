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
  Helper::callFunctionOnMessageThread([&]() { c->play(); }, true);
}

void ClipAudioSource_stop(ClipAudioSource* c) {
  cerr << "libzl : Stop Clip " << c;

  Helper::callFunctionOnMessageThread([&]() { c->stop(); }, true);
}

float ClipAudioSource_getDuration(ClipAudioSource* c) {
  return c->getDuration();
}

const char* ClipAudioSource_getFileName(ClipAudioSource* c) {
  return c->getFileName();
}

void ClipAudioSource_setStartPosition(ClipAudioSource* c,
                                      float startPositionInSeconds) {
  Helper::callFunctionOnMessageThread(
      [&]() { c->setStartPosition(startPositionInSeconds); }, true);
}

void ClipAudioSource_setLength(ClipAudioSource* c, float lengthInSeconds) {
  Helper::callFunctionOnMessageThread([&]() { c->setLength(lengthInSeconds); },
                                      true);
}

void ClipAudioSource_setSpeedRatio(ClipAudioSource* c, float speedRatio) {
  Helper::callFunctionOnMessageThread([&]() { c->setSpeedRatio(speedRatio); },
                                      true);
}

void ClipAudioSource_setPitch(ClipAudioSource* c, float pitchChange) {
  Helper::callFunctionOnMessageThread([&]() { c->setPitch(pitchChange); },
                                      true);
}
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

void SyncTimer_addClip(ClipAudioSource* clip) {
  Helper::callFunctionOnMessageThread([&]() { syncTimer->addClip(clip); },
                                      true);
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
