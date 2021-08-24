#include "SyncTimer.h"

using namespace std;

SyncTimer::SyncTimer(int bpm) : bpm(bpm) {}

void SyncTimer::hiResTimerCallback() {
  beat = (beat + 1) % 4;

  if (beat == 0) {
    while (!clipsQueue.isEmpty()) {
      clipsQueue.dequeue()->play();
    }
  }

  if (callback != nullptr) {
    callback();
  }
}

void SyncTimer::setCallback(void (*functionPtr)()) { callback = functionPtr; }

void SyncTimer::addClip(ClipAudioSource *clip) {
  this->clipsQueue.enqueue(clip);
}

void SyncTimer::start(int interval) {
  beat = 0;
  startTimer(interval);
}

void SyncTimer::stop() { stopTimer(); }
