/*
  ==============================================================================

    libzl.h
    Created: 10 Aug 2021 10:12:17am
    Author:  root

  ==============================================================================
*/

#pragma once

#include "ClipAudioSource.h"
#include "SyncTimer.h"

extern "C" {

//////////////
/// ClipAudioSource API Bridge
//////////////
ClipAudioSource* ClipAudioSource_new(const char* filepath);
void ClipAudioSource_play(ClipAudioSource* c);
void ClipAudioSource_stop(ClipAudioSource* c);
float ClipAudioSource_getDuration(ClipAudioSource* c);
const char* ClipAudioSource_getFileName(ClipAudioSource* c);
void ClipAudioSource_setStartPosition(ClipAudioSource* c,
                                      float startPositionInSeconds);
void ClipAudioSource_setLength(ClipAudioSource* c, float lengthInSeconds);
void ClipAudioSource_setSpeedRatio(ClipAudioSource* c, float speedRatio);
void ClipAudioSource_setPitch(ClipAudioSource* c, float pitchChange);
void ClipAudioSource_destroy(ClipAudioSource* c);
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
//////////////
/// END SyncTimer API Bridge
//////////////

void initJuce();
void shutdownJuce();
<<<<<<< HEAD

<<<<<<< HEAD
void startTimer(int interval);
void stopTimer();
void registerTimerCallback(void (*functionPtr)());
<<<<<<< HEAD

=======
>>>>>>> 7d78d37 (Implement sync playing from c++)
void startLoop(const char* filepath);
=======
=======
>>>>>>> 9236df2 (Keep looping clip once started at beat 1)
void registerGraphicTypes();
<<<<<<< HEAD
>>>>>>> 5d4ff66 (can be instantiated from QML)
=======
void stopClips(int size, ClipAudioSource** clips);
>>>>>>> a3fce31 (- Implement stopping an array of clips)
}
