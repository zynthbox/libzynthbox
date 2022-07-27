<<<<<<< HEAD
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
void SyncTimer_queueClipToStart(ClipAudioSource* clip);
void SyncTimer_queueClipToStop(ClipAudioSource* clip);
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
=======
/*
  ==============================================================================

    libzl.h
    Created: 10 Aug 2021 10:12:17am
    Author:  root

  ==============================================================================
*/

#pragma once

#include <QObject>

class ClipAudioSource;
class SyncTimer;

extern "C" {

//////////////
/// ClipAudioSource API Bridge
//////////////
ClipAudioSource *ClipAudioSource_byID(int id);
ClipAudioSource *ClipAudioSource_new(const char *filepath, bool muted = false);
void ClipAudioSource_setProgressCallback(ClipAudioSource *c,
                                         void (*functionPtr)(float));
void ClipAudioSource_connectProgress(ClipAudioSource *c, void *obj);
void ClipAudioSource_play(ClipAudioSource *c, bool loop);
void ClipAudioSource_stop(ClipAudioSource *c);
void ClipAudioSource_playOnChannel(ClipAudioSource *c, bool loop, int midiChannel);
void ClipAudioSource_stopOnChannel(ClipAudioSource *c, int midiChannel);
float ClipAudioSource_getDuration(ClipAudioSource *c);
const char *ClipAudioSource_getFileName(ClipAudioSource *c);
void ClipAudioSource_setStartPosition(ClipAudioSource *c,
                                      float startPositionInSeconds);
void ClipAudioSource_setLength(ClipAudioSource *c, float beat, int bpm);
void ClipAudioSource_setSpeedRatio(ClipAudioSource *c, float speedRatio);
void ClipAudioSource_setPitch(ClipAudioSource *c, float pitchChange);
void ClipAudioSource_setGain(ClipAudioSource *c, float db);
void ClipAudioSource_setVolume(ClipAudioSource *c, float vol);
void ClipAudioSource_setAudioLevelChangedCallback(ClipAudioSource *c,
                                                  void (*functionPtr)(float));
void ClipAudioSource_setSlices(ClipAudioSource *c, int slices);
int ClipAudioSource_keyZoneStart(ClipAudioSource *c);
void ClipAudioSource_setKeyZoneStart(ClipAudioSource *c, int keyZoneStart);
int ClipAudioSource_keyZoneEnd(ClipAudioSource *c);
void ClipAudioSource_setKeyZoneEnd(ClipAudioSource *c, int keyZoneEnd);
int ClipAudioSource_rootNote(ClipAudioSource *c);
void ClipAudioSource_setRootNote(ClipAudioSource *c, int rootNote);
void ClipAudioSource_destroy(ClipAudioSource *c);
int ClipAudioSource_id(ClipAudioSource *c);
//////////////
/// END ClipAudioSource API Bridge
//////////////

//////////////
/// SyncTimer API Bridge
//////////////
QObject *SyncTimer_instance();
void SyncTimer_startTimer(int interval);
void SyncTimer_setBpm(uint bpm);
void SyncTimer_stopTimer();
void SyncTimer_registerTimerCallback(void (*functionPtr)(int));
void SyncTimer_deregisterTimerCallback(void (*functionPtr)(int));
void SyncTimer_queueClipToStart(ClipAudioSource *clip);
void SyncTimer_queueClipToStartOnChannel(ClipAudioSource *clip, int midiChannel);
void SyncTimer_queueClipToStop(ClipAudioSource *clip);
void SyncTimer_queueClipToStopOnChannel(ClipAudioSource *clip, int midiChannel);
//////////////
/// END SyncTimer API Bridge
//////////////

void initJuce();
void shutdownJuce();
void registerGraphicTypes();
void stopClips(int size, ClipAudioSource **clips);
float dBFromVolume(float vol);

//////////////
/// AudioLevels API Bridge
//////////////
bool AudioLevels_isRecording();
void AudioLevels_setRecordGlobalPlayback(bool shouldRecord = true);
void AudioLevels_setGlobalPlaybackFilenamePrefix(const char *fileNamePrefix);
void AudioLevels_startRecording();
void AudioLevels_stopRecording();
/// //////////////
/// END AudioLevels API Bridge
//////////////
}
>>>>>>> 400988a (progress reporting from tracktion to qml)
