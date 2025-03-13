#pragma once

#include <QString>
#include <QList>

// A header-only class with a few useful functions for identifying things by their ID
class SysexIdTable {
public:
    static QString manufacturerNameFromId(const QList<int> &bytes){
        QString result{"Unknown Manufacturer"};
        switch(bytes[0]) {
            case 0x00:
                // This is where we put the multi-byte options
                switch(bytes[1]) {
                    case 0x00:
                        switch(bytes[2]) {
                            default:
                                break;
                        }
                        break;
                    case 0x40:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Crimson Technology Inc."};
                                break;
                            case 0x01:
                                result = QString{"Softbank Mobile Corp"};
                                break;
                            case 0x03:
                                result = QString{"D&M Holdings Inc."};
                                break;
                            case 0x04:
                                result = QString{"Xing Inc."};
                                break;
                            case 0x05:
                                result = QString{"AlphaTheta Corporation"};
                                break;
                            case 0x06:
                                result = QString{"Pioneer Corporation"};
                                break;
                            case 0x07:
                                result = QString{"Slik Corporation"};
                                break;
                            default:
                                break;
                        }
                        break;
                    default:
                        break;
                }
                break;
            case 0x01:
                result = QString{"Sequential Circuits"};
                break;
            case 0x02:
                result = QString{"IDP"};
                break;
            case 0x03:
                result = QString{"Voyetra Turtle Beach, Inc."};
                break;
            case 0x04:
                result = QString{"Moog Music"};
                break;
            case 0x05:
                result = QString{"Passport Designs"};
                break;
            case 0x06:
                result = QString{"Lexicon Inc."};
                break;
            case 0x07:
                result = QString{"Kurzweil / Young Chang"};
                break;
            case 0x08:
                result = QString{"Fender"};
                break;
            case 0x09:
                result = QString{"MIDI9"};
                break;
            case 0x0A:
                result = QString{"AKG Acoustics"};
                break;
            case 0x0B:
                result = QString{"Voyce Music"};
                break;
            case 0x0C:
                result = QString{"WaveFrame (Timeline)"};
                break;
            case 0x0D:
                result = QString{"ADA Signal Processors, Inc."};
                break;
            case 0x0E:
                result = QString{"Garfield Electronics"};
                break;
            case 0x0F:
                result = QString{"Ensoniq"};
                break;
            case 0x10:
                result = QString{"Oberheim"};
                break;
            case 0x11:
                result = QString{"Apple"};
                break;
            case 0x12:
                result = QString{"Grey Matter Response"};
                break;
            case 0x13:
                result = QString{"Digidesign Inc."};
                break;
            case 0x14:
                result = QString{"Palmtree Instruments"};
                break;
            case 0x15:
                result = QString{"JLCooper Electronics"};
                break;
            case 0x16:
                result = QString{"Lowrey Organ Company"};
                break;
            case 0x17:
                result = QString{"Adams-Smith"};
                break;
            case 0x18:
                result = QString{"E-mu"};
                break;
            case 0x19:
                result = QString{"Harmony Systems"};
                break;
            case 0x1A:
                result = QString{"ART"};
                break;
            case 0x1B:
                result = QString{"Baldwin"};
                break;
            case 0x1C:
                result = QString{"Eventide"};
                break;
            case 0x1D:
                result = QString{"Inventronics"};
                break;
            case 0x1E:
                result = QString{"Key Concepts"};
                break;
            case 0x1F:
                result = QString{"Clarity"};
                break;
            case 0x20:
                result = QString{"Passac"};
                break;
            case 0x21:
                result = QString{"Proel Labs (SIEL)"};
                break;
            case 0x22:
                result = QString{"Synthaxe (UK)"};
                break;
            case 0x23:
                result = QString{"Stepp"};
                break;
            case 0x24:
                result = QString{"Hohner"};
                break;
            case 0x25:
                result = QString{"Twister"};
                break;
            case 0x26:
                result = QString{"Ketron s.r.l."};
                break;
            case 0x27:
                result = QString{"Jellinghaus MS"};
                break;
            case 0x28:
                result = QString{"Southworth Music Systems"};
                break;
            case 0x29:
                result = QString{"PPG (Germany)"};
                break;
            case 0x2A:
                result = QString{"CESYG Ltd."};
                break;
            case 0x2B:
                result = QString{"Solid State Logic Organ Systems"};
                break;
            case 0x2C:
                result = QString{"Audio Veritrieb-P. Struven"};
                break;
            case 0x2D:
                result = QString{"Neve"};
                break;
            case 0x2E:
                result = QString{"Soundtracs Ltd."};
                break;
            case 0x2F:
                result = QString{"Elka"};
                break;
            case 0x30:
                result = QString{"Dynacord"};
                break;
            case 0x31:
                result = QString{"Viscount International Spa (Intercontinental Electronics)"};
                break;
            case 0x32:
                result = QString{"Drawmer"};
                break;
            case 0x33:
                result = QString{"Clavia Digital Instruments"};
                break;
            case 0x34:
                result = QString{"Audio Architecture"};
                break;
            case 0x35:
                result = QString{"Generalmusic Corp SpA"};
                break;
            case 0x36:
                result = QString{"Cheetah Marketing"};
                break;
            case 0x37:
                result = QString{"C.T.M."};
                break;
            case 0x38:
                result = QString{"Simmons UK"};
                break;
            case 0x39:
                result = QString{"Soundcraft Electronics"};
                break;
            case 0x3A:
                result = QString{"Steinberg Media Technologies GmbH"};
                break;
            case 0x3B:
                result = QString{"Wersi Gmbh"};
                break;
            case 0x3C:
                result = QString{"AVAB Niethammer AB"};
                break;
            case 0x3D:
                result = QString{"Digigram"};
                break;
            case 0x3E:
                result = QString{"Waldorf Electronics GmbH"};
                break;
            case 0x3F:
                result = QString{"Quasimidi"};
                break;
            case 0x40:
                result = QString{"Kawai Musical Instruments MFG. CO. Ltd"};
                break;
            case 0x41:
                result = QString{"Roland Corporation"};
                break;
            case 0x42:
                result = QString{"Korg Inc."};
                break;
            case 0x43:
                result = QString{"Yamaha Corporation"};
                break;
            case 0x44:
                result = QString{"Casio Computer Co. Ltd"};
                break;
            // case 0x45:
            //     result = QString{""};
            //     break;
            case 0x46:
                result = QString{"Kamiya Studio Co. Ltd"};
                break;
            case 0x47:
                result = QString{"Akai Electric Co. Ltd."};
                break;
            case 0x48:
                result = QString{"Victor Company of Japan, Ltd."};
                break;
            // case 0x49:
            //     result = QString{""};
            //     break;
            // case 0x4A:
            //     result = QString{""};
            //     break;
            case 0x4B:
                result = QString{"Fujitsu Limited"};
                break;
            case 0x4C:
                result = QString{"Sony Corporation"};
                break;
            // case 0x4D:
            //     result = QString{""};
            //     break;
            case 0x4E:
                result = QString{"Teac Corporation"};
                break;
            // case 0x4F:
            //     result = QString{""};
            //     break;
            case 0x50:
                result = QString{"Matsushita Electric Industrial Co. , Ltd"};
                break;
            case 0x51:
                result = QString{"Fostex Corporation"};
                break;
            case 0x52:
                result = QString{"Zoom Corporation"};
                break;
            // case 0x53:
            //     result = QString{""};
            //     break;
            case 0x54:
                result = QString{"Matsushita Communication Industrial Co., Ltd."};
                break;
            case 0x55:
                result = QString{"Suzuki Musical Instruments MFG. Co., Ltd."};
                break;
            case 0x56:
                result = QString{"Fuji Sound Corporation Ltd."};
                break;
            case 0x57:
                result = QString{"Acoustic Technical Laboratory, Inc."};
                break;
            // case 0x58:
            //     result = QString{""};
            //     break;
            case 0x59:
                result = QString{"Faith, Inc."};
                break;
            case 0x5A:
                result = QString{"Internet Corporation"};
                break;
            // case 0x5B:
            //     result = QString{""};
            //     break;
            case 0x5C:
                result = QString{"Seekers Co. Ltd."};
                break;
            // case 0x5D:
            //     result = QString{""};
            //     break;
            // case 0x5E:
            //     result = QString{""};
            //     break;
            case 0x5F:
                result = QString{"SD Card Association"};
                break;
            default:
                break;
        }
        return result;
    };
};
