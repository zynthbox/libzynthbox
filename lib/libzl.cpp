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
#include <jack/jack.h>
#include <QDebug>
#include <QTimer>

#include "ClipAudioSource.h"
#include "Helper.h"
#include "JUCEHeaders.h"
#include "SyncTimer.h"
#include "WaveFormItem.h"

using namespace std;

ScopedJuceInitialiser_GUI *initializer = nullptr;
SyncTimer *syncTimer = new SyncTimer();
QList<ClipAudioSource*> createdClips;
jack_client_t* zlJackClient{nullptr};
jack_status_t zlJackStatus{};
jack_port_t* capturePortA{nullptr};
jack_port_t* capturePortB{nullptr};
float db;
void (*recordingAudioLevelCallback)(float leveldB) = nullptr;

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

  void setClipLength(ClipAudioSource *c, float beat, int bpm) {
    c->setLength(beat, bpm);
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
  ClipAudioSource *sClip;

  Helper::callFunctionOnMessageThread(
      [&]() { sClip = new ClipAudioSource(syncTimer, filepath, muted); }, true);

  sClip->setParent(qApp);

  static int clipID{1};
  sClip->setId(clipID);
  ++clipID;

  createdClips << sClip;
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

const char *ClipAudioSource_getFileName(ClipAudioSource *c) {
  return c->getFileName();
}

void ClipAudioSource_setProgressCallback(ClipAudioSource *c,
                                         void (*functionPtr)(float)) {
  c->setProgressCallback(functionPtr);
}

void ClipAudioSource_setStartPosition(ClipAudioSource *c,
                                      float startPositionInSeconds) {
  elThread.setClipStartPosition(c, startPositionInSeconds);
}

void ClipAudioSource_setLength(ClipAudioSource *c, float beat, int bpm) {
  elThread.setClipLength(c, beat, bpm);
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

void ClipAudioSource_setAudioLevelChangedCallback(ClipAudioSource *c,
                                                  void (*functionPtr)(float)) {
  c->setAudioLevelChangedCallback(functionPtr);
}

void ClipAudioSource_destroy(ClipAudioSource *c) {
  ClipAudioSource *clip = qobject_cast<ClipAudioSource*>(c);
  if (clip) {
    createdClips.removeAll(clip);
  }
  elThread.destroyClip(c);
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

void SyncTimer_queueClipToStop(ClipAudioSource *clip) {
  Helper::callFunctionOnMessageThread(
      [&]() { syncTimer->queueClipToStop(clip); }, true);
}
//////////////
/// END SyncTimer API Bridge
//////////////

inline float convertTodbFS(float raw) {
  if (raw <= 0) {
      return -200;
  }

  float fValue = 20 * log10f(raw);
  if (fValue < -200) {
      return -200;
  }

  return fValue;
}

inline float peakdBFSFromJackOutput(jack_port_t* port, jack_nframes_t nframes) {
  float peak{0.0f};
  jack_default_audio_sample_t *buf = (jack_default_audio_sample_t *)jack_port_get_buffer(port, nframes);

  for (jack_nframes_t i=0; i<nframes; i++) {
      const float sample = fabs(buf[i]) * 0.2;
      if (sample > peak) {
          peak = sample;
      }
  }

  return convertTodbFS(peak);
}

static int _zlJackProcessCb(jack_nframes_t nframes, void* arg) {
  Q_UNUSED(arg)
  static float dbLeft, dbRight;
  static QTimer* callbackCaller{nullptr};
  if (!callbackCaller) {
    // To avoid potentially causing Jack xruns, we put everything that is
    // not directly required to be done during the jack call into a timer,
    // which lives on the app's main thread. If it turns out to be heavy
    // on that thread, we can move it elsewhere, but given it /is/ a ui
    // update situation, it seems reasonable to put it there. A courteous
    // check says it has no measurable impact on the main thread's
    // processing cost.
    callbackCaller = new QTimer(qApp);
    callbackCaller->moveToThread(qApp->thread());
    callbackCaller->setInterval(10);
    QObject::connect(callbackCaller, &QTimer::timeout, callbackCaller, [&]() {
      float db{-200};
      if (dbLeft > -200 || dbRight > -200) {
        db = 10 * log10f(pow(10, dbLeft/10) + pow(10, dbRight/10));
      }
      if (recordingAudioLevelCallback != nullptr) {
        recordingAudioLevelCallback(db);
      }
    });
    QMetaObject::invokeMethod(callbackCaller, "start", Qt::QueuedConnection);
  }

  dbLeft = peakdBFSFromJackOutput(capturePortA, nframes);
  dbRight = peakdBFSFromJackOutput(capturePortB, nframes);

  return 0;
}

void initJuce() {
  cerr << "### INIT JUCE\n";
  elThread.startThread();

  zlJackClient = jack_client_open(
    "zynthiloops_client",
    JackNullOption,
    &zlJackStatus
  );

  if (zlJackClient) {
    cerr << "Initialized ZL Jack Client zynthiloops_client";

    capturePortA = jack_port_register(
      zlJackClient,
      "capture_port_a",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsInput,
      0
    );
    capturePortB = jack_port_register(
      zlJackClient,
      "capture_port_b",
      JACK_DEFAULT_AUDIO_TYPE,
      JackPortIsInput,
      0
    );

    if (
      jack_set_process_callback(
        zlJackClient,
        _zlJackProcessCb,
        nullptr
      ) != 0
    ) {
      cerr << "Failed to set the ZL Jack Client processing callback" << endl;
    } else {
      if (jack_activate(zlJackClient) == 0) {
        cerr << "Successfully created and set up the ZL Jack client" << endl;
      } else {
        cerr << "Failed to activate ZL Jack client" << endl;
      }
    }
  } else {
    cerr << "Error initializing ZL Jack Client zynthiloops_client" << endl;
  }
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
>>>>>>> 2246f24 (Add API to get dB from volume)
=======

void setRecordingAudioLevelCallback(void (*functionPtr)(float)) {
  recordingAudioLevelCallback = functionPtr;
}
>>>>>>> 6ae7881 (Implement jack client to listen to input audio level)
