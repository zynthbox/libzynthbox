/*
 * Copyright (C) 2025 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#pragma once

#include "MidiRouter.h"
#include "KeyScales.h"
#include "PatternModel.h"
#include "ClipAudioSource.h"

#include <jack/midiport.h>

/**
 * \brief A class containing information about a single track
 * In particular, this holds the MidiRouterDevice instances associated with this track
 */
class SketchpadTrackInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* sequencerDevice READ getSequencerDevice CONSTANT)
    Q_PROPERTY(QObject* controllerDevice READ getControllerDevice CONSTANT)
    Q_PROPERTY(QObject* externalDevice READ getExternalDevice NOTIFY externalDeviceChanged)
    Q_PROPERTY(QObject* currentPattern READ getCurrentPattern NOTIFY currentPatternChanged)
public:
    // This is our translation from midi input channels to destinations. It contains
    // information on what external output channel should be used if it's not a straight
    // passthrough to the same channel the other side, and what channels should be
    // targeted on the zynthian outputs.
    explicit SketchpadTrackInfo(int trackIndex, QObject *parent = nullptr);
    ~SketchpadTrackInfo() = default;
    int zynthianChannels[16];
    MidiRouterDevice *routerDevice{nullptr};
    MidiRouterDevice *syncTimerSequencer{nullptr};
    MidiRouterDevice *syncTimerController{nullptr};
    MidiRouterDevice *externalDevice{nullptr}; // If set, send to this device instead of whatever enabled devices we've got (updated based on externalDeviceID whenever the hardware setup changes)
    QString portName;
    int trackIndex{-1};
    int externalChannel{-1};
    QString externalDeviceID; // Used to determine whether an external device should be assigned
    MidiRouter::RoutingDestination destination{MidiRouter::ZynthianDestination};
    int currentlySelectedPatternIndex{-1};
    PatternModel *currentlySelectedPattern{nullptr};
    ClipAudioSource::SamplePickingStyle slotSelectionStyle{ClipAudioSource::AllPickingStyle};
    bool trustExternalInputChannel{false};
    KeyScales::Octave octave{KeyScales::Octave4};
    KeyScales::Pitch pitch{KeyScales::PitchC};
    KeyScales::Scale scale{KeyScales::ScaleChromatic};
    PatternModel::KeyScaleLockStyle lockStyle{PatternModel::KeyScaleLockOff};
    KeyScales *keyScales{KeyScales::instance()};
    inline bool applyKeyScale(jack_midi_event_t& event) {
        // We only care about events...
        // - if we're supposed to be *some* kind of handling
        // - the scale is not chromatic (if it is, any given note will be on scale)
        // - it is a note related message (so note on or off, or poly aftertouch)
        if (lockStyle != PatternModel::KeyScaleLockOff && scale != KeyScales::ScaleChromatic && 0x79 < event.buffer[0] && event.buffer[0] < 0xB0) {
            // If we've got a pattern model defined, then we know what to do (otherwise we'll not have much idea)
            if (lockStyle == PatternModel::KeyScaleLockRewrite) {
                // Set the note value of the event to what it is supposed to be
                event.buffer[1] = keyScales->onScaleNote(event.buffer[1], scale, pitch, octave);
            } else if (lockStyle == PatternModel::KeyScaleLockBlock && keyScales->midiNoteOnScale(event.buffer[1], scale, pitch, octave) == false) {
                // We do not accept this event, as it is for a note which is not on scale
                return false;
            }
        }
        return true;
    }
    QObject* getSequencerDevice() const;
    QObject* getControllerDevice() const;
    QObject* getExternalDevice() const;
    Q_SIGNAL void externalDeviceChanged();
    QObject* getCurrentPattern() const;
    Q_SIGNAL void currentPatternChanged();
};
