/*
  ==============================================================================

    libzl.h
    Created: 10 Aug 2021 10:12:17am
    Author:  root

  ==============================================================================
*/

#pragma once

#include "ClipAudioSource.h"

extern "C" {

//////////////
/// ClipAudioSource API Bridge
//////////////
ClipAudioSource* ClipAudioSource_new(const char* filepath);
void ClipAudioSource_play(ClipAudioSource* c, bool shouldLoop);
void ClipAudioSource_stop(ClipAudioSource* c);
float ClipAudioSource_getDuration(ClipAudioSource* c);
const char* ClipAudioSource_getFileName(ClipAudioSource* c);
void ClipAudioSource_setStartPosition(ClipAudioSource* c,
                                      float startPositionInSeconds);
void ClipAudioSource_setLength(ClipAudioSource* c, float lengthInSeconds);
void ClipAudioSource_setSpeedRatio(ClipAudioSource* c, float speedRatio);
void ClipAudioSource_setPitch(ClipAudioSource* c, float pitchChange);
//////////////
/// END ClipAudioSource API Bridge
//////////////

//////////////
/// SyncTimer API Bridge
//////////////
void SyncTimer_startTimer(int interval);
void SyncTimer_stopTimer();
void SyncTimer_registerTimerCallback(void (*functionPtr)());
void SyncTimer_addClip(ClipAudioSource* clip);
void SyncTimer_removeClip(ClipAudioSource* clip);
//////////////
/// END SyncTimer API Bridge
//////////////

void initJuce();
void shutdownJuce();

<<<<<<< HEAD
void startTimer(int interval);
void stopTimer();
void registerTimerCallback(void (*functionPtr)());
<<<<<<< HEAD

=======
>>>>>>> 7d78d37 (Implement sync playing from c++)
void startLoop(const char* filepath);
=======
void registerGraphicTypes();
>>>>>>> 5d4ff66 (can be instantiated from QML)
}
