#pragma once

#include <QDebug>
#include <QString>

#include <jack/midiport.h>

namespace DeviceMessageTranslations {
    const QLatin1String device_identifier_seaboard_rise{"Seaboard RISE MIDI"};
    const QLatin1String device_identifier_presonus_atom_sq{"ATM SQ ATM SQ"};
    jack_midi_event_t device_translations_cc_presonus_atom_sq[128];
    jack_midi_event_t device_translations_cc_none[128];
    int loadCount{0};
    void load() {
        for (int i = 0; i < 128; ++i) {
            switch(i) {
                case 85:
                    device_translations_cc_presonus_atom_sq[85].size = 1;
                    device_translations_cc_presonus_atom_sq[85].buffer = new jack_midi_data_t[1]();
                    device_translations_cc_presonus_atom_sq[85].buffer[0] = 0xFC;
                    break;
                case 86:
                    device_translations_cc_presonus_atom_sq[86].size = 1;
                    device_translations_cc_presonus_atom_sq[86].buffer = new jack_midi_data_t[1]();
                    device_translations_cc_presonus_atom_sq[86].buffer[0] = 0xFA;
                    break;
                default:
                    device_translations_cc_none[i].size = 0;
                    device_translations_cc_presonus_atom_sq[i].size = 0;
                    break;
            }
        }
        ++loadCount;
    }
    void unload() {
        --loadCount;
        if (loadCount == 0) {
            for (int i = 0; i < 128; ++i) {
                if (DeviceMessageTranslations::device_translations_cc_presonus_atom_sq[i].size > 0) {
                    delete[] DeviceMessageTranslations::device_translations_cc_presonus_atom_sq[i].buffer;
                }
            }
        }
    }
    void apply(const QString &identifier, jack_midi_event_t **translations_cc) {
        if (identifier.endsWith(device_identifier_presonus_atom_sq)) {
            qDebug() << "ZLRouter: Identified device as Presonus Atom SQ main device, applying CC translations";
            *translations_cc = device_translations_cc_presonus_atom_sq;
        } else {
            *translations_cc = device_translations_cc_none;
        }
    }
    int deviceMasterChannel(const QString& identifier) {
        if (identifier.startsWith(device_identifier_seaboard_rise)) {
            qDebug() << "ZLRouter: Identified device as a ROLI Seaboard Rise, returning master channel 0";
            // By default, the Touch Faders use MIDI CCs 107, 109, and 111 in MIDI mode (white dot)
            // By default, the XY Touchpad uses MIDI CCs 113 and 114.
            return 0;
        }
        return -1;
    }
}
