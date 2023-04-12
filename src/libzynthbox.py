#!/usr/bin/python3
# -*- coding: utf-8 -*-
#********************************************************************
# ZYNTHIAN PROJECT: C++ Library Wrapper for libzynthbox
#
# A Python wrapper for libzynthbox
#
# Copyright (C) 2023 Anupam Basak <anupam.basak27@gmail.com>
#
#********************************************************************
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# For a full copy of the GNU General Public License see the LICENSE.txt file.
#
#********************************************************************

import ctypes
import logging
from os.path import dirname, realpath

from PySide2.QtCore import Property, QObject, QProcess, Signal
from PySide2.QtQml import QQmlEngine

libzynthbox_so = None
AudioLevelChangedCallback = ctypes.CFUNCTYPE(None, ctypes.c_float)
ProgressChangedCallback = ctypes.CFUNCTYPE(None, ctypes.c_float)

def init():
    global libzynthbox_so

    try:
        libzynthbox_so = ctypes.cdll.LoadLibrary("libzynthbox.so")
    except Exception as e:
        print(f"Could not load the libzynthbox.so shared library (at a guess, the libzynthbox package has not been installed): {str(e)}")

    if libzynthbox_so is not None:
        try:
            ### Type Definition
            libzynthbox_so.stopClips.argTypes = [ctypes.c_int]

            libzynthbox_so.dBFromVolume.argtypes = [ctypes.c_float]
            libzynthbox_so.dBFromVolume.restype = ctypes.c_float

            libzynthbox_so.SyncTimer_startTimer.argtypes = [ctypes.c_int]

            libzynthbox_so.SyncTimer_setBpm.argtypes = [ctypes.c_uint]

            libzynthbox_so.SyncTimer_getMultiplier.restype = ctypes.c_int

            libzynthbox_so.SyncTimer_queueClipToStart.argtypes = [ctypes.c_void_p]

            libzynthbox_so.SyncTimer_queueClipToStartOnChannel.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.SyncTimer_queueClipToStop.argtypes = [ctypes.c_void_p]

            libzynthbox_so.SyncTimer_queueClipToStopOnChannel.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_new.argtypes = [ctypes.c_char_p, ctypes.c_bool]
            libzynthbox_so.ClipAudioSource_new.restype = ctypes.c_void_p

            libzynthbox_so.ClipAudioSource_play.argtypes = [ctypes.c_void_p, ctypes.c_bool]

            libzynthbox_so.ClipAudioSource_stop.argtypes = [ctypes.c_void_p]

            libzynthbox_so.ClipAudioSource_playOnChannel.argtypes = [ctypes.c_void_p, ctypes.c_bool, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_stopOnChannel.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_getDuration.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_getDuration.restype = ctypes.c_float

            libzynthbox_so.ClipAudioSource_setProgressCallback.argtypes = [ctypes.c_void_p, AudioLevelChangedCallback]

            libzynthbox_so.ClipAudioSource_getFileName.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_getFileName.restype = ctypes.c_char_p

            libzynthbox_so.ClipAudioSource_setStartPosition.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setLength.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_setPan.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setSpeedRatio.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setPitch.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setGain.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setVolume.argtypes = [ctypes.c_void_p, ctypes.c_float]

            libzynthbox_so.ClipAudioSource_setAudioLevelChangedCallback.argtypes = [ctypes.c_void_p, AudioLevelChangedCallback]

            libzynthbox_so.ClipAudioSource_id.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_id.restype = ctypes.c_int

            libzynthbox_so.ClipAudioSource_setSlices.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_keyZoneStart.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_keyZoneStart.restype = ctypes.c_int
            libzynthbox_so.ClipAudioSource_setKeyZoneStart.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_keyZoneEnd.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_keyZoneEnd.restype = ctypes.c_int
            libzynthbox_so.ClipAudioSource_setKeyZoneEnd.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_rootNote.argtypes = [ctypes.c_void_p]
            libzynthbox_so.ClipAudioSource_rootNote.restype = ctypes.c_int
            libzynthbox_so.ClipAudioSource_setRootNote.argtypes = [ctypes.c_void_p, ctypes.c_int]

            libzynthbox_so.ClipAudioSource_destroy.argtypes = [ctypes.c_void_p]

            libzynthbox_so.AudioLevels_setRecordGlobalPlayback.argtypes = [ctypes.c_bool]
            libzynthbox_so.AudioLevels_setGlobalPlaybackFilenamePrefix.argtypes = [ctypes.c_char_p]

            libzynthbox_so.AudioLevels_setRecordPortsFilenamePrefix.argtypes = [ctypes.c_char_p]
            libzynthbox_so.AudioLevels_addRecordPort.argtypes = [ctypes.c_char_p, ctypes.c_int]
            libzynthbox_so.AudioLevels_removeRecordPort.argtypes = [ctypes.c_char_p, ctypes.c_int]
            libzynthbox_so.AudioLevels_setShouldRecordPorts.argtypes = [ctypes.c_bool]

            libzynthbox_so.AudioLevels_isRecording.restype = ctypes.c_bool

            libzynthbox_so.JackPassthrough_setPanAmount.argtypes = [ctypes.c_int, ctypes.c_float]
            libzynthbox_so.JackPassthrough_getPanAmount.argtypes = [ctypes.c_int]
            libzynthbox_so.JackPassthrough_getPanAmount.restype = ctypes.c_float

            libzynthbox_so.JackPassthrough_setWetFx1Amount.argtypes = [ctypes.c_int, ctypes.c_float]
            libzynthbox_so.JackPassthrough_getWetFx1Amount.argtypes = [ctypes.c_int]
            libzynthbox_so.JackPassthrough_getWetFx1Amount.restype = ctypes.c_float

            libzynthbox_so.JackPassthrough_setWetFx2Amount.argtypes = [ctypes.c_int, ctypes.c_float]
            libzynthbox_so.JackPassthrough_getWetFx2Amount.argtypes = [ctypes.c_int]
            libzynthbox_so.JackPassthrough_getWetFx2Amount.restype = ctypes.c_float

            libzynthbox_so.JackPassthrough_setDryAmount.argtypes = [ctypes.c_int, ctypes.c_float]
            libzynthbox_so.JackPassthrough_getDryAmount.argtypes = [ctypes.c_int]
            libzynthbox_so.JackPassthrough_getDryAmount.restype = ctypes.c_float
            ### END Type Definition

            # Initialize libzynthbox
            libzynthbox_so.initialize()
        except Exception as e:
            libzynthbox_so = None
            print(f"Failed to initialise libzynthbox_so library: {str(e)}")


def reloadZynthianConfiguration():
    if libzynthbox_so:
        libzynthbox_so.reloadZynthianConfiguration()

def registerTimerCallback(callback):
    if libzynthbox_so:
        libzynthbox_so.SyncTimer_registerTimerCallback(callback)


def registerGraphicTypes():
    if libzynthbox_so:
        libzynthbox_so.registerGraphicTypes()


def startTimer(interval: int):
    if libzynthbox_so:
        libzynthbox_so.SyncTimer_startTimer(interval)


def setBpm(bpm: int):
    if libzynthbox_so:
        libzynthbox_so.SyncTimer_setBpm(bpm)

def getMultiplier():
    if libzynthbox_so:
        return libzynthbox_so.SyncTimer_getMultiplier()
    else:
        return 0

def stopTimer():
    if libzynthbox_so:
        libzynthbox_so.SyncTimer_stopTimer()


def stopClips(clips: list):
    if len(clips) > 0:
        logging.debug(f"{clips[0]}, {clips[0].audioSource}")

        arr = (ctypes.c_void_p * len(clips))()
        arr[:] = [c.audioSource.obj for c in clips]

        if libzynthbox_so:
            libzynthbox_so.stopClips(len(clips), arr)


def dbFromVolume(vol: float):
    if libzynthbox_so:
        return libzynthbox_so.dBFromVolume(vol)


def setRecordingAudioLevelCallback(cb):
    if libzynthbox_so:
        libzynthbox_so.setRecordingAudioLevelCallback(cb)


def AudioLevels_setRecordGlobalPlayback(shouldRecord):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_setRecordGlobalPlayback(shouldRecord)


def AudioLevels_setGlobalPlaybackFilenamePrefix(fileNamePrefix: str):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_setGlobalPlaybackFilenamePrefix(fileNamePrefix.encode())


def AudioLevels_startRecording():
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_startRecording()


def AudioLevels_stopRecording():
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_stopRecording()


def AudioLevels_isRecording() -> bool:
    if libzynthbox_so:
        return libzynthbox_so.AudioLevels_isRecording()


def AudioLevels_setRecordPortsFilenamePrefix(fileNamePrefix: str):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_setRecordPortsFilenamePrefix(fileNamePrefix.encode())


def AudioLevels_addRecordPort(portName: str, channel: int):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_addRecordPort(portName.encode(), channel)


def AudioLevels_removeRecordPort(portName: str, channel: int):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_removeRecordPort(portName.encode(), channel)


def AudioLevels_setShouldRecordPorts(shouldRecord):
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_setShouldRecordPorts(shouldRecord)


def AudioLevels_clearRecordPorts():
    if libzynthbox_so:
        libzynthbox_so.AudioLevels_clearRecordPorts()


def setPanAmount(channel, amount):
    if libzynthbox_so:
        libzynthbox_so.JackPassthrough_setPanAmount(channel, amount)

def getPanAmount(channel):
    if libzynthbox_so:
        return libzynthbox_so.JackPassthrough_getPanAmount(channel)
    else:
        return 0


def setWetFx1Amount(channel, amount):
    if libzynthbox_so:
        libzynthbox_so.JackPassthrough_setWetFx1Amount(channel, amount)

def getWetFx1Amount(channel):
    if libzynthbox_so:
        return libzynthbox_so.JackPassthrough_getWetFx1Amount(channel)
    else:
        return 0


def setWetFx2Amount(channel, amount):
    if libzynthbox_so:
        libzynthbox_so.JackPassthrough_setWetFx2Amount(channel, amount)

def getWetFx2Amount(channel):
    if libzynthbox_so:
        return libzynthbox_so.JackPassthrough_getWetFx2Amount(channel)
    else:
        return 0


def setDryAmount(channel, amount):
    if libzynthbox_so:
        libzynthbox_so.JackPassthrough_setDryAmount(channel, amount)

def getDryAmount(channel):
    if libzynthbox_so:
        return libzynthbox_so.JackPassthrough_getDryAmount(channel)
    else:
        return 0


def setMuted(channel, value):
    if libzynthbox_so:
        libzynthbox_so.JackPassthrough_setMuted(channel, value)

def getMuted(channel):
    if libzynthbox_so:
        return libzynthbox_so.JackPassthrough_getMuted(channel)
    else:
        return 0


class ClipAudioSource(QObject):
    audioLevelChanged = Signal(float)
    progressChanged = Signal(float)

    def __init__(self, zl_clip, filepath: bytes, muted=False):
        super(ClipAudioSource, self).__init__()

        if libzynthbox_so:
            self.obj = libzynthbox_so.ClipAudioSource_new(filepath, muted)

            if zl_clip is not None:
                self.audio_level_changed_callback = AudioLevelChangedCallback(self.audio_level_changed_callback)
                self.progress_changed_callback = ProgressChangedCallback(self.progress_changed_callback)

                libzynthbox_so.ClipAudioSource_setProgressCallback(self.obj, self.progress_changed_callback)
                libzynthbox_so.ClipAudioSource_setAudioLevelChangedCallback(self.obj, self.audio_level_changed_callback)

    def audio_level_changed_callback(self, level_db):
        self.audioLevelChanged.emit(level_db)

    def progress_changed_callback(self, progress):
        self.progressChanged.emit(progress)

    def play(self, loop=True):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_play(self.obj, loop)

    def stop(self):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_stop(self.obj)

    def playOnChannel(self, loop=True, midiChannel=-2):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_playOnChannel(self.obj, loop, midiChannel)

    def stopOnChannel(self, midiChannel=-2):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_stopOnChannel(self.obj, midiChannel)

    def get_id(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_id(self.obj)

    def get_duration(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_getDuration(self.obj)

    def get_filename(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_getFileName(self.obj)

    def set_start_position(self, start_position_in_seconds: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setStartPosition(self.obj, start_position_in_seconds)

    def set_length(self, length: int, bpm: int):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setLength(self.obj, length, bpm)

    def set_pan(self, pan: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setPan(self.obj, pan)

    def set_pitch(self, pitch: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setPitch(self.obj, pitch)

    def set_gain(self, gain: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setGain(self.obj, gain)

    def set_volume(self, volume: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setVolume(self.obj, volume)

    def set_speed_ratio(self, speed_ratio: float):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setSpeedRatio(self.obj, speed_ratio)

    def queueClipToStart(self):
        if libzynthbox_so:
            libzynthbox_so.SyncTimer_queueClipToStart(self.obj)

    def queueClipToStartOnChannel(self, midiChannel):
        if libzynthbox_so:
            libzynthbox_so.SyncTimer_queueClipToStartOnChannel(self.obj, midiChannel)

    def queueClipToStop(self):
        if libzynthbox_so:
            libzynthbox_so.SyncTimer_queueClipToStop(self.obj)

    def queueClipToStopOnChannel(self, midiChannel):
        if libzynthbox_so:
            libzynthbox_so.SyncTimer_queueClipToStopOnChannel(self.obj, midiChannel)

    def setSlices(self, slices : int):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setSlices(self.obj, slices)

    def keyZoneStart(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_keyZoneStart(self.obj)

    def setKeyZoneStart(self, keyZoneStart):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setKeyZoneStart(self.obj, keyZoneStart)

    def keyZoneEnd(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_keyZoneEnd(self.obj)

    def setKeyZoneEnd(self, keyZoneEnd):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setKeyZoneEnd(self.obj, keyZoneEnd)

    def rootNote(self):
        if libzynthbox_so:
            return libzynthbox_so.ClipAudioSource_rootNote(self.obj)

    def setRootNote(self, rootNote):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_setRootNote(self.obj, rootNote)

    def destroy(self):
        if libzynthbox_so:
            libzynthbox_so.ClipAudioSource_destroy(self.obj)

    def get_cpp_obj(self):
        return self.obj
