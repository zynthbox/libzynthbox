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
                            case 0x01:
                                result = QString{"Time/Warner Interactive"};
                                break;
                            case 0x02:
                                result = QString{"Advanced Gravis Comp. Tech Ltd."};
                                break;
                            case 0x03:
                                result = QString{"Media Vision"};
                                break;
                            case 0x04:
                                result = QString{"Dornes Research Group"};
                                break;
                            case 0x05:
                                result = QString{"K-Muse"};
                                break;
                            case 0x06:
                                result = QString{"Stypher"};
                                break;
                            case 0x07:
                                result = QString{"Digital Music Corp."};
                                break;
                            case 0x08:
                                result = QString{"IOTA Systems"};
                                break;
                            case 0x09:
                                result = QString{"New England Digital"};
                                break;
                            case 0x0A:
                                result = QString{"Artisyn"};
                                break;
                            case 0x0B:
                                result = QString{"IVL Technologies Ltd."};
                                break;
                            case 0x0C:
                                result = QString{"Southern Music Systems"};
                                break;
                            case 0x0D:
                                result = QString{"Lake Butler Sound Company"};
                                break;
                            case 0x0E:
                                result = QString{"Alesis Studio Electronics"};
                                break;
                            case 0x0F:
                                result = QString{"Sound Creation"};
                                break;
                            case 0x10:
                                result = QString{"DOD Electronics Corp."};
                                break;
                            case 0x11:
                                result = QString{"Studer-Editech"};
                                break;
                            case 0x12:
                                result = QString{"Sonus"};
                                break;
                            case 0x13:
                                result = QString{"Temporal Acuity Products"};
                                break;
                            case 0x14:
                                result = QString{"Perfect Fretworks"};
                                break;
                            case 0x15:
                                result = QString{"KAT Inc."};
                                break;
                            case 0x16:
                                result = QString{"Opcode Systems"};
                                break;
                            case 0x17:
                                result = QString{"Rane Corporation"};
                                break;
                            case 0x18:
                                result = QString{"Anadi Electronique"};
                                break;
                            case 0x19:
                                result = QString{"KMX"};
                                break;
                            case 0x1A:
                                result = QString{"Allen & Heath Brenell"};
                                break;
                            case 0x1B:
                                result = QString{"Peavey Electronics"};
                                break;
                            case 0x1C:
                                result = QString{"360 Systems"};
                                break;
                            case 0x1D:
                                result = QString{"Spectrum Design and Development"};
                                break;
                            case 0x1E:
                                result = QString{"Marquis Music"};
                                break;
                            case 0x1F:
                                result = QString{"Zeta Systems"};
                                break;
                            case 0x20:
                                result = QString{"Axxes (Brian Parsonett)"};
                                break;
                            case 0x21:
                                result = QString{"Orban"};
                                break;
                            case 0x22:
                                result = QString{"Indian Valley Mfg."};
                                break;
                            case 0x23:
                                result = QString{"Triton"};
                                break;
                            case 0x24:
                                result = QString{"KTI"};
                                break;
                            case 0x25:
                                result = QString{"Breakaway Technologies"};
                                break;
                            case 0x26:
                                result = QString{"Leprecon / CAE Inc."};
                                break;
                            case 0x27:
                                result = QString{"Harrison Systems Inc."};
                                break;
                            case 0x28:
                                result = QString{"Future Lab/Mark Kuo"};
                                break;
                            case 0x29:
                                result = QString{"Rocktron Corporation"};
                                break;
                            case 0x2A:
                                result = QString{"PianoDisc"};
                                break;
                            case 0x2B:
                                result = QString{"Cannon Research Group"};
                                break;
                            case 0x2C:
                                result = QString{"Reserved"};
                                break;
                            case 0x2D:
                                result = QString{"Rodgers Instrument LLC"};
                                break;
                            case 0x2E:
                                result = QString{"Blue Sky Logic"};
                                break;
                            case 0x2F:
                                result = QString{"Encore Electronics"};
                                break;
                            case 0x30:
                                result = QString{"Uptown"};
                                break;
                            case 0x31:
                                result = QString{"Voce"};
                                break;
                            case 0x32:
                                result = QString{"CTI Audio, Inc. (Musically Intel. Devs.)"};
                                break;
                            case 0x33:
                                result = QString{"S3 Incorporated"};
                                break;
                            case 0x34:
                                result = QString{"Broderbund / Red Orb"};
                                break;
                            case 0x35:
                                result = QString{"Allen Organ Co."};
                                break;
                            case 0x36:
                                result = QString{"Reserved"};
                                break;
                            case 0x37:
                                result = QString{"Music Quest"};
                                break;
                            case 0x38:
                                result = QString{"Aphex"};
                                break;
                            case 0x39:
                                result = QString{"Gallien Krueger"};
                                break;
                            case 0x3A:
                                result = QString{"IBM"};
                                break;
                            case 0x3B:
                                result = QString{"Mark Of The Unicorn"};
                                break;
                            case 0x3C:
                                result = QString{"Hotz Corporation"};
                                break;
                            case 0x3D:
                                result = QString{"ETA Lighting"};
                                break;
                            case 0x3E:
                                result = QString{"NSI Corporation"};
                                break;
                            case 0x3F:
                                result = QString{"Ad Lib, Inc."};
                                break;
                            case 0x40:
                                result = QString{"Richmond Sound Design"};
                                break;
                            case 0x41:
                                result = QString{"Microsoft"};
                                break;
                            case 0x42:
                                result = QString{"Mindscape (Software Toolworks)"};
                                break;
                            case 0x43:
                                result = QString{"Russ Jones Marketing / Niche"};
                                break;
                            case 0x44:
                                result = QString{"Intone"};
                                break;
                            case 0x45:
                                result = QString{"Advanced Remote Technologies"};
                                break;
                            case 0x46:
                                result = QString{"White Instruments"};
                                break;
                            case 0x47:
                                result = QString{"GT Electronics/Groove Tubes"};
                                break;
                            case 0x48:
                                result = QString{"Pacific Research & Engineering"};
                                break;
                            case 0x49:
                                result = QString{"Timeline Vista, Inc."};
                                break;
                            case 0x4A:
                                result = QString{"Mesa Boogie Ltd."};
                                break;
                            case 0x4B:
                                result = QString{"FSLI"};
                                break;
                            case 0x4C:
                                result = QString{"Sequoia Development Group"};
                                break;
                            case 0x4D:
                                result = QString{"Studio Electronics"};
                                break;
                            case 0x4E:
                                result = QString{"Euphonix, Inc"};
                                break;
                            case 0x4F:
                                result = QString{"InterMIDI, Inc."};
                                break;
                            case 0x50:
                                result = QString{"MIDI Solutions Inc."};
                                break;
                            case 0x51:
                                result = QString{"3DO Company"};
                                break;
                            case 0x52:
                                result = QString{"Lightwave Research / High End Systems"};
                                break;
                            case 0x53:
                                result = QString{"Micro-W Corporation"};
                                break;
                            case 0x54:
                                result = QString{"Spectral Synthesis, Inc."};
                                break;
                            case 0x55:
                                result = QString{"Lone Wolf"};
                                break;
                            case 0x56:
                                result = QString{"Studio Technologies Inc."};
                                break;
                            case 0x57:
                                result = QString{"Peterson Electro-Musical Product, Inc."};
                                break;
                            case 0x58:
                                result = QString{"Atari Corporation"};
                                break;
                            case 0x59:
                                result = QString{"Marion Systems Corporation"};
                                break;
                            case 0x5A:
                                result = QString{"Design Event"};
                                break;
                            case 0x5B:
                                result = QString{"Winjammer Software Ltd."};
                                break;
                            case 0x5C:
                                result = QString{"AT&T Bell Laboratories"};
                                break;
                            case 0x5D:
                                result = QString{"Reserved"};
                                break;
                            case 0x5E:
                                result = QString{"Symetrix"};
                                break;
                            case 0x5F:
                                result = QString{"MIDI the World"};
                                break;
                            case 0x60:
                                result = QString{"Spatializer"};
                                break;
                            case 0x61:
                                result = QString{"Micros ‘N MIDI"};
                                break;
                            case 0x62:
                                result = QString{"Accordians International"};
                                break;
                            case 0x63:
                                result = QString{"EuPhonics (now 3Com)"};
                                break;
                            case 0x64:
                                result = QString{"Musonix"};
                                break;
                            case 0x65:
                                result = QString{"Turtle Beach Systems (Voyetra)"};
                                break;
                            case 0x66:
                                result = QString{"Loud Technologies / Mackie"};
                                break;
                            case 0x67:
                                result = QString{"Compuserve"};
                                break;
                            case 0x68:
                                result = QString{"BEC Technologies"};
                                break;
                            case 0x69:
                                result = QString{"QRS Music Inc"};
                                break;
                            case 0x6A:
                                result = QString{"P.G. Music"};
                                break;
                            case 0x6B:
                                result = QString{"Sierra Semiconductor"};
                                break;
                            case 0x6C:
                                result = QString{"EpiGraf"};
                                break;
                            case 0x6D:
                                result = QString{"Electronics Diversified Inc"};
                                break;
                            case 0x6E:
                                result = QString{"Tune 1000"};
                                break;
                            case 0x6F:
                                result = QString{"Advanced Micro Devices"};
                                break;
                            case 0x70:
                                result = QString{"Mediamation"};
                                break;
                            case 0x71:
                                result = QString{"Sabine Musical Mfg. Co. Inc."};
                                break;
                            case 0x72:
                                result = QString{"Woog Labs"};
                                break;
                            case 0x73:
                                result = QString{"Micropolis Corp"};
                                break;
                            case 0x74:
                                result = QString{"Ta Horng Musical Instrument"};
                                break;
                            case 0x75:
                                result = QString{"e-Tek Labs (Forte Tech)"};
                                break;
                            case 0x76:
                                result = QString{"Electro-Voice"};
                                break;
                            case 0x77:
                                result = QString{"Midisoft Corporation"};
                                break;
                            case 0x78:
                                result = QString{"QSound Labs"};
                                break;
                            case 0x79:
                                result = QString{"Westrex"};
                                break;
                            case 0x7A:
                                result = QString{"Nvidia"};
                                break;
                            case 0x7B:
                                result = QString{"ESS Technology"};
                                break;
                            case 0x7C:
                                result = QString{"Media Trix Peripherals"};
                                break;
                            case 0x7D:
                                result = QString{"Brooktree Corp"};
                                break;
                            case 0x7E:
                                result = QString{"Otari Corp"};
                                break;
                            case 0x7F:
                                result = QString{"Key Electronics, Inc."};
                                break;
                            default:
                                break;
                        }
                        break;
                    case 0x01:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Shure Incorporated"};
                                break;
                            case 0x01:
                                result = QString{"AuraSound"};
                                break;
                            case 0x02:
                                result = QString{"Crystal Semiconductor"};
                                break;
                            case 0x03:
                                result = QString{"Conexant (Rockwell)"};
                                break;
                            case 0x04:
                                result = QString{"Silicon Graphics"};
                                break;
                            case 0x05:
                                result = QString{"M-Audio (Midiman)"};
                                break;
                            case 0x06:
                                result = QString{"PreSonus"};
                                break;
                            // case 0x07:
                            //     result = QString{""};
                            //     break;
                            case 0x08:
                                result = QString{"Topaz Enterprises"};
                                break;
                            case 0x09:
                                result = QString{"Cast Lighting"};
                                break;
                            case 0x0A:
                                result = QString{"Microsoft Consumer Division"};
                                break;
                            case 0x0B:
                                result = QString{"Sonic Foundry"};
                                break;
                            case 0x0C:
                                result = QString{"Line 6 (Fast Forward) (Yamaha)"};
                                break;
                            case 0x0D:
                                result = QString{"Beatnik Inc"};
                                break;
                            case 0x0E:
                                result = QString{"Van Koevering Company"};
                                break;
                            case 0x0F:
                                result = QString{"Altech Systems"};
                                break;
                            case 0x10:
                                result = QString{"S & S Research"};
                                break;
                            case 0x11:
                                result = QString{"VLSI Technology"};
                                break;
                            case 0x12:
                                result = QString{"Chromatic Research"};
                                break;
                            case 0x13:
                                result = QString{"Sapphire"};
                                break;
                            case 0x14:
                                result = QString{"IDRC"};
                                break;
                            case 0x15:
                                result = QString{"Justonic Tuning"};
                                break;
                            case 0x16:
                                result = QString{"TorComp Research Inc."};
                                break;
                            case 0x17:
                                result = QString{"Newtek Inc."};
                                break;
                            case 0x18:
                                result = QString{"Sound Sculpture"};
                                break;
                            case 0x19:
                                result = QString{"Walker Technical"};
                                break;
                            case 0x1A:
                                result = QString{"Digital Harmony (PAVO)"};
                                break;
                            case 0x1B:
                                result = QString{"InVision Interactive"};
                                break;
                            case 0x1C:
                                result = QString{"T-Square Design"};
                                break;
                            case 0x1D:
                                result = QString{"Nemesys Music Technology"};
                                break;
                            case 0x1E:
                                result = QString{"DBX Professional (Harman Intl)"};
                                break;
                            case 0x1F:
                                result = QString{"Syndyne Corporation"};
                                break;
                            case 0x20:
                                result = QString{"Bitheadz"};
                                break;
                            case 0x21:
                                result = QString{"BandLab Technologies"};
                                break;
                            case 0x22:
                                result = QString{"Analog Devices"};
                                break;
                            case 0x23:
                                result = QString{"National Semiconductor"};
                                break;
                            case 0x24:
                                result = QString{"Boom Theory / Adinolfi Alternative Percussion"};
                                break;
                            case 0x25:
                                result = QString{"Virtual DSP Corporation"};
                                break;
                            case 0x26:
                                result = QString{"Antares Systems"};
                                break;
                            case 0x27:
                                result = QString{"Angel Software"};
                                break;
                            case 0x28:
                                result = QString{"St Louis Music"};
                                break;
                            case 0x29:
                                result = QString{"Passport Music Software LLC (Gvox)"};
                                break;
                            case 0x2A:
                                result = QString{"Ashley Audio Inc."};
                                break;
                            case 0x2B:
                                result = QString{"Vari-Lite Inc."};
                                break;
                            case 0x2C:
                                result = QString{"Summit Audio Inc."};
                                break;
                            case 0x2D:
                                result = QString{"Aureal Semiconductor Inc."};
                                break;
                            case 0x2E:
                                result = QString{"SeaSound LLC"};
                                break;
                            case 0x2F:
                                result = QString{"U.S. Robotics"};
                                break;
                            case 0x30:
                                result = QString{"Aurisis Research"};
                                break;
                            case 0x31:
                                result = QString{"Nearfield Research"};
                                break;
                            case 0x32:
                                result = QString{"FM7 Inc"};
                                break;
                            case 0x33:
                                result = QString{"Swivel Systems"};
                                break;
                            case 0x34:
                                result = QString{"Hyperactive Audio Systems"};
                                break;
                            case 0x35:
                                result = QString{"MidiLite (Castle Studios Productions)"};
                                break;
                            case 0x36:
                                result = QString{"Radikal Technologies"};
                                break;
                            case 0x37:
                                result = QString{"Roger Linn Design"};
                                break;
                            case 0x38:
                                result = QString{"TC-Helicon Vocal Technologies"};
                                break;
                            case 0x39:
                                result = QString{"Event Electronics"};
                                break;
                            case 0x3A:
                                result = QString{"Sonic Network Inc"};
                                break;
                            case 0x3B:
                                result = QString{"Realtime Music Solutions"};
                                break;
                            case 0x3C:
                                result = QString{"Apogee Digital"};
                                break;
                            case 0x3D:
                                result = QString{"Classical Organs, Inc."};
                                break;
                            case 0x3E:
                                result = QString{"Microtools Inc."};
                                break;
                            case 0x3F:
                                result = QString{"Numark Industries"};
                                break;
                            case 0x40:
                                result = QString{"Frontier Design Group, LLC"};
                                break;
                            case 0x41:
                                result = QString{"Recordare LLC"};
                                break;
                            case 0x42:
                                result = QString{"Starr Labs"};
                                break;
                            case 0x43:
                                result = QString{"Voyager Sound Inc."};
                                break;
                            case 0x44:
                                result = QString{"Manifold Labs"};
                                break;
                            case 0x45:
                                result = QString{"Aviom Inc."};
                                break;
                            case 0x46:
                                result = QString{"Mixmeister Technology"};
                                break;
                            case 0x47:
                                result = QString{"Notation Software"};
                                break;
                            case 0x48:
                                result = QString{"Mercurial Communications"};
                                break;
                            case 0x49:
                                result = QString{"Wave Arts"};
                                break;
                            case 0x4A:
                                result = QString{"Logic Sequencing Devices"};
                                break;
                            case 0x4B:
                                result = QString{"Axess Electronics"};
                                break;
                            case 0x4C:
                                result = QString{"Muse Research"};
                                break;
                            case 0x4D:
                                result = QString{"Open Labs"};
                                break;
                            case 0x4E:
                                result = QString{"Guillemot Corp"};
                                break;
                            case 0x4F:
                                result = QString{"Samson Technologies"};
                                break;
                            case 0x50:
                                result = QString{"Electronic Theatre Controls"};
                                break;
                            case 0x51:
                                result = QString{"Blackberry (RIM)"};
                                break;
                            case 0x52:
                                result = QString{"Mobileer"};
                                break;
                            case 0x53:
                                result = QString{"Synthogy"};
                                break;
                            case 0x54:
                                result = QString{"Lynx Studio Technology Inc."};
                                break;
                            case 0x55:
                                result = QString{"Damage Control Engineering LLC"};
                                break;
                            case 0x56:
                                result = QString{"Yost Engineering, Inc."};
                                break;
                            case 0x57:
                                result = QString{"Brooks & Forsman Designs LLC / DrumLite"};
                                break;
                            case 0x58:
                                result = QString{"Infinite Response"};
                                break;
                            case 0x59:
                                result = QString{"Garritan Corp"};
                                break;
                            case 0x5A:
                                result = QString{"Plogue Art et Technologie, Inc"};
                                break;
                            case 0x5B:
                                result = QString{"RJM Music Technology"};
                                break;
                            case 0x5C:
                                result = QString{"Custom Solutions Software"};
                                break;
                            case 0x5D:
                                result = QString{"Sonarcana LLC / Highly Liquid"};
                                break;
                            case 0x5E:
                                result = QString{"Centrance"};
                                break;
                            case 0x5F:
                                result = QString{"Kesumo LLC"};
                                break;
                            case 0x60:
                                result = QString{"Stanton (Gibson Brands)"};
                                break;
                            case 0x61:
                                result = QString{"Livid Instruments"};
                                break;
                            case 0x62:
                                result = QString{"First Act / 745 Media"};
                                break;
                            case 0x63:
                                result = QString{"Pygraphics, Inc."};
                                break;
                            case 0x64:
                                result = QString{"Panadigm Innovations Ltd"};
                                break;
                            case 0x65:
                                result = QString{"Avedis Zildjian Co"};
                                break;
                            case 0x66:
                                result = QString{"Auvital Music Corp"};
                                break;
                            case 0x67:
                                result = QString{"You Rock Guitar (was: Inspired Instruments)"};
                                break;
                            case 0x68:
                                result = QString{"Chris Grigg Designs"};
                                break;
                            case 0x69:
                                result = QString{"Slate Digital LLC"};
                                break;
                            case 0x6A:
                                result = QString{"Mixware"};
                                break;
                            case 0x6B:
                                result = QString{"Social Entropy"};
                                break;
                            case 0x6C:
                                result = QString{"Source Audio LLC"};
                                break;
                            case 0x6D:
                                result = QString{"Ernie Ball / Music Man"};
                                break;
                            case 0x6E:
                                result = QString{"Fishman"};
                                break;
                            case 0x6F:
                                result = QString{"Custom Audio Electronics"};
                                break;
                            case 0x70:
                                result = QString{"American Audio/DJ"};
                                break;
                            case 0x71:
                                result = QString{"Mega Control Systems"};
                                break;
                            case 0x72:
                                result = QString{"Kilpatrick Audio"};
                                break;
                            case 0x73:
                                result = QString{"iConnectivity"};
                                break;
                            case 0x74:
                                result = QString{"Fractal Audio"};
                                break;
                            case 0x75:
                                result = QString{"NetLogic Microsystems"};
                                break;
                            case 0x76:
                                result = QString{"Music Computing"};
                                break;
                            case 0x77:
                                result = QString{"Nektar Technology Inc"};
                                break;
                            case 0x78:
                                result = QString{"Zenph Sound Innovations"};
                                break;
                            case 0x79:
                                result = QString{"DJTechTools.com"};
                                break;
                            case 0x7A:
                                result = QString{"Rezonance Labs"};
                                break;
                            case 0x7B:
                                result = QString{"Decibel Eleven"};
                                break;
                            case 0x7C:
                                result = QString{"CNMAT"};
                                break;
                            case 0x7D:
                                result = QString{"Media Overkill"};
                                break;
                            case 0x7E:
                                result = QString{"Confusion Studios"};
                                break;
                            case 0x7F:
                                result = QString{"moForte Inc"};
                                break;
                            default:
                                break;
                        }
                        break;
                    case 0x02:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Miselu Inc"};
                                break;
                            case 0x01:
                                result = QString{"Amelia’s Compass LLC"};
                                break;
                            case 0x02:
                                result = QString{"Zivix LLC"};
                                break;
                            case 0x03:
                                result = QString{"Artiphon"};
                                break;
                            case 0x04:
                                result = QString{"Synclavier Digital"};
                                break;
                            case 0x05:
                                result = QString{"Light & Sound Control Devices LLC"};
                                break;
                            case 0x06:
                                result = QString{"Retronyms Inc"};
                                break;
                            case 0x07:
                                result = QString{"JS Technologies"};
                                break;
                            case 0x08:
                                result = QString{"Quicco Sound"};
                                break;
                            case 0x09:
                                result = QString{"A-Designs Audio"};
                                break;
                            case 0x0A:
                                result = QString{"McCarthy Music Corp"};
                                break;
                            case 0x0B:
                                result = QString{"Denon DJ"};
                                break;
                            case 0x0C:
                                result = QString{"Keith Robert Murray"};
                                break;
                            case 0x0D:
                                result = QString{"Google"};
                                break;
                            case 0x0E:
                                result = QString{"ISP Technologies"};
                                break;
                            case 0x0F:
                                result = QString{"Abstrakt Instruments LLC"};
                                break;
                            case 0x10:
                                result = QString{"Meris LLC"};
                                break;
                            case 0x11:
                                result = QString{"Sensorpoint LLC"};
                                break;
                            case 0x12:
                                result = QString{"Hi-Z Labs"};
                                break;
                            case 0x13:
                                result = QString{"Imitone"};
                                break;
                            case 0x14:
                                result = QString{"Intellijel Designs Inc."};
                                break;
                            case 0x15:
                                result = QString{"Dasz Instruments Inc."};
                                break;
                            case 0x16:
                                result = QString{"Remidi"};
                                break;
                            case 0x17:
                                result = QString{"Disaster Area Designs LLC"};
                                break;
                            case 0x18:
                                result = QString{"Universal Audio"};
                                break;
                            case 0x19:
                                result = QString{"Carter Duncan Corp"};
                                break;
                            case 0x1A:
                                result = QString{"Essential Technology"};
                                break;
                            case 0x1B:
                                result = QString{"Cantux Research LLC"};
                                break;
                            case 0x1C:
                                result = QString{"Hummel Technologies"};
                                break;
                            case 0x1D:
                                result = QString{"Sensel Inc"};
                                break;
                            case 0x1E:
                                result = QString{"DBML Group"};
                                break;
                            case 0x1F:
                                result = QString{"Madrona Labs"};
                                break;
                            case 0x20:
                                result = QString{"Mesa Boogie"};
                                break;
                            case 0x21:
                                result = QString{"Effigy Labs"};
                                break;
                            case 0x22:
                                result = QString{"Amenote"};
                                break;
                            case 0x23:
                                result = QString{"Red Panda LLC"};
                                break;
                            case 0x24:
                                result = QString{"OnSong LLC"};
                                break;
                            case 0x25:
                                result = QString{"Jamboxx Inc."};
                                break;
                            case 0x26:
                                result = QString{"Electro-Harmonix"};
                                break;
                            case 0x27:
                                result = QString{"RnD64 Inc"};
                                break;
                            case 0x28:
                                result = QString{"Neunaber Technology LLC"};
                                break;
                            case 0x29:
                                result = QString{"Kaom Inc."};
                                break;
                            case 0x2A:
                                result = QString{"Hallowell EMC"};
                                break;
                            case 0x2B:
                                result = QString{"Sound Devices, LLC"};
                                break;
                            case 0x2C:
                                result = QString{"Spectrasonics, Inc"};
                                break;
                            case 0x2D:
                                result = QString{"Second Sound, LLC"};
                                break;
                            case 0x2E:
                                result = QString{"8eo (Horn)"};
                                break;
                            case 0x2F:
                                result = QString{"VIDVOX LLC"};
                                break;
                            case 0x30:
                                result = QString{"Matthews Effects"};
                                break;
                            case 0x31:
                                result = QString{"Bright Blue Beetle"};
                                break;
                            case 0x32:
                                result = QString{"Audio Impressions"};
                                break;
                            case 0x33:
                                result = QString{"Looperlative"};
                                break;
                            case 0x34:
                                result = QString{"Steinway"};
                                break;
                            case 0x35:
                                result = QString{"Ingenious Arts and Technologies LLC"};
                                break;
                            case 0x36:
                                result = QString{"DCA Audio"};
                                break;
                            case 0x37:
                                result = QString{"Buchla USA"};
                                break;
                            case 0x38:
                                result = QString{"Sinicon"};
                                break;
                            case 0x39:
                                result = QString{"Isla Instruments"};
                                break;
                            case 0x3A:
                                result = QString{"Soundiron LLC"};
                                break;
                            case 0x3B:
                                result = QString{"Sonoclast, LLC"};
                                break;
                            case 0x3C:
                                result = QString{"Copper and Cedar"};
                                break;
                            case 0x3D:
                                result = QString{"Whirled Notes"};
                                break;
                            case 0x3E:
                                result = QString{"Cejetvole, LLC"};
                                break;
                            case 0x3F:
                                result = QString{"DAWn Audio LLC"};
                                break;
                            case 0x40:
                                result = QString{"Space Brain Circuits"};
                                break;
                            case 0x41:
                                result = QString{"Caedence"};
                                break;
                            case 0x42:
                                result = QString{"HCN Designs, LLC (The MIDI Maker)"};
                                break;
                            case 0x43:
                                result = QString{"PTZOptics"};
                                break;
                            case 0x44:
                                result = QString{"Noise Engineering"};
                                break;
                            case 0x45:
                                result = QString{"Synthesia LLC"};
                                break;
                            case 0x46:
                                result = QString{"Jeff Whitehead Lutherie LLC"};
                                break;
                            case 0x47:
                                result = QString{"Wampler Pedals Inc."};
                                break;
                            case 0x48:
                                result = QString{"Tapis Magique"};
                                break;
                            case 0x49:
                                result = QString{"Leaf Secrets"};
                                break;
                            case 0x4A:
                                result = QString{"Groove Synthesis"};
                                break;
                            case 0x4B:
                                result = QString{"Audiocipher Technologies LLC"};
                                break;
                            case 0x4C:
                                result = QString{"Mellotron Inc."};
                                break;
                            case 0x4D:
                                result = QString{"Hologram Electronics LLC"};
                                break;
                            case 0x4E:
                                result = QString{"iCON Americas, LLC"};
                                break;
                            case 0x4F:
                                result = QString{"Singular Sound"};
                                break;
                            case 0x50:
                                result = QString{"Genovation Inc"};
                                break;
                            case 0x51:
                                result = QString{"Method Red"};
                                break;
                            case 0x52:
                                result = QString{"Brain Inventions"};
                                break;
                            case 0x53:
                                result = QString{"Synervoz Communications Inc."};
                                break;
                            case 0x54:
                                result = QString{"Hypertriangle Inc"};
                                break;
                            case 0x55:
                                result = QString{"DigiBrass LLC"};
                                break;
                            case 0x56:
                                result = QString{"MIDI2 Marketing LLC"};
                                break;
                            case 0x57:
                                result = QString{"TAQS.IM"};
                                break;
                            case 0x58:
                                result = QString{"CSS Designs"};
                                break;
                            case 0x59:
                                result = QString{"Mozaic Beats"};
                                break;
                            case 0x5A:
                                result = QString{"PianoDisc"};
                                break;
                            case 0x5B:
                                result = QString{"Uwyn"};
                                break;
                            case 0x5C:
                                result = QString{"FSK Audio"};
                                break;
                            case 0x5D:
                                result = QString{"Eternal Research LLC"};
                                break;
                            case 0x5E:
                                result = QString{"Azoteq Inc"};
                                break;
                            case 0x5F:
                                result = QString{"Hy Music"};
                                break;
                            case 0x60:
                                result = QString{"Sound Magic Co.,Ltd"};
                                break;
                            case 0x61:
                                result = QString{"Dirtywave"};
                                break;
                            case 0x62:
                                result = QString{"Kinotone"};
                                break;
                            case 0x63:
                                result = QString{"Yoder Precision Instruments"};
                                break;
                            case 0x64:
                                result = QString{"McLaren Labs"};
                                break;
                            case 0x65:
                                result = QString{"Gigperfomer"};
                                break;
                            case 0x66:
                                result = QString{"Groove Synthesis"};
                                break;
                            // case 0x67:
                            //     result = QString{""};
                            //     break;
                            case 0x68:
                                result = QString{"Stellr Audio"};
                                break;
                            default:
                                break;
                        }
                        break;
                    case 0x20:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Dream SAS"};
                                break;
                            case 0x01:
                                result = QString{"Strand Lighting"};
                                break;
                            case 0x02:
                                result = QString{"Amek Div of Harman Industries"};
                                break;
                            case 0x03:
                                result = QString{"Casa Di Risparmio Di Loreto"};
                                break;
                            case 0x04:
                                result = QString{"Böhm electronic GmbH"};
                                break;
                            case 0x05:
                                result = QString{"Syntec Digital Audio"};
                                break;
                            case 0x06:
                                result = QString{"Trident Audio Developments"};
                                break;
                            case 0x07:
                                result = QString{"Real World Studio"};
                                break;
                            case 0x08:
                                result = QString{"Evolution Synthesis, Ltd"};
                                break;
                            case 0x09:
                                result = QString{"Yes Technology"};
                                break;
                            case 0x0A:
                                result = QString{"Audiomatica"};
                                break;
                            case 0x0B:
                                result = QString{"Bontempi SpA (Sigma)"};
                                break;
                            case 0x0C:
                                result = QString{"F.B.T. Elettronica SpA"};
                                break;
                            case 0x0D:
                                result = QString{"MidiTemp GmbH"};
                                break;
                            case 0x0E:
                                result = QString{"LA Audio (Larking Audio)"};
                                break;
                            case 0x0F:
                                result = QString{"Zero 88 Lighting Limited"};
                                break;
                            case 0x10:
                                result = QString{"Micon Audio Electronics GmbH"};
                                break;
                            case 0x11:
                                result = QString{"Forefront Technology"};
                                break;
                            case 0x12:
                                result = QString{"Studio Audio and Video Ltd."};
                                break;
                            case 0x13:
                                result = QString{"Kenton Electronics"};
                                break;
                            case 0x14:
                                result = QString{"Celco/ Electrosonic"};
                                break;
                            case 0x15:
                                result = QString{"ADB"};
                                break;
                            case 0x16:
                                result = QString{"Marshall Products Limited"};
                                break;
                            case 0x17:
                                result = QString{"DDA"};
                                break;
                            case 0x18:
                                result = QString{"BSS Audio Ltd."};
                                break;
                            case 0x19:
                                result = QString{"MA Lighting Technology"};
                                break;
                            case 0x1A:
                                result = QString{"Fatar SRL c/o Music Industries"};
                                break;
                            case 0x1B:
                                result = QString{"QSC Audio Products Inc."};
                                break;
                            case 0x1C:
                                result = QString{"Artisan Clasic Organ Inc."};
                                break;
                            case 0x1D:
                                result = QString{"Orla Spa"};
                                break;
                            case 0x1E:
                                result = QString{"Pinnacle Audio (Klark Teknik PLC)"};
                                break;
                            case 0x1F:
                                result = QString{"TC Electronics"};
                                break;
                            case 0x20:
                                result = QString{"Doepfer Musikelektronik GmbH"};
                                break;
                            case 0x21:
                                result = QString{"Creative ATC / E-mu"};
                                break;
                            case 0x22:
                                result = QString{"Seyddo/Minami"};
                                break;
                            case 0x23:
                                result = QString{"LG Electronics (Goldstar)"};
                                break;
                            case 0x24:
                                result = QString{"Midisoft sas di M.Cima & C"};
                                break;
                            case 0x25:
                                result = QString{"Samick Musical Inst. Co. Ltd."};
                                break;
                            case 0x26:
                                result = QString{"Penny and Giles (Bowthorpe PLC)"};
                                break;
                            case 0x27:
                                result = QString{"Acorn Computer"};
                                break;
                            case 0x28:
                                result = QString{"LSC Electronics Pty. Ltd."};
                                break;
                            case 0x29:
                                result = QString{"Focusrite/Novation"};
                                break;
                            case 0x2A:
                                result = QString{"Samkyung Mechatronics"};
                                break;
                            case 0x2B:
                                result = QString{"Medeli Electronics Co."};
                                break;
                            case 0x2C:
                                result = QString{"Charlie Lab SRL"};
                                break;
                            case 0x2D:
                                result = QString{"Blue Chip Music Technology"};
                                break;
                            case 0x2E:
                                result = QString{"BEE OH Corp"};
                                break;
                            case 0x2F:
                                result = QString{"LG Semicon America"};
                                break;
                            case 0x30:
                                result = QString{"TESI"};
                                break;
                            case 0x31:
                                result = QString{"EMAGIC"};
                                break;
                            case 0x32:
                                result = QString{"Behringer GmbH"};
                                break;
                            case 0x33:
                                result = QString{"Access Music Electronics"};
                                break;
                            case 0x34:
                                result = QString{"Synoptic"};
                                break;
                            case 0x35:
                                result = QString{"Hanmesoft"};
                                break;
                            case 0x36:
                                result = QString{"Terratec Electronic GmbH"};
                                break;
                            case 0x37:
                                result = QString{"Proel SpA"};
                                break;
                            case 0x38:
                                result = QString{"IBK MIDI"};
                                break;
                            case 0x39:
                                result = QString{"IRCAM"};
                                break;
                            case 0x3A:
                                result = QString{"Propellerhead Software"};
                                break;
                            case 0x3B:
                                result = QString{"Red Sound Systems Ltd"};
                                break;
                            case 0x3C:
                                result = QString{"Elektron ESI AB"};
                                break;
                            case 0x3D:
                                result = QString{"Sintefex Audio"};
                                break;
                            case 0x3E:
                                result = QString{"MAM (Music and More)"};
                                break;
                            case 0x3F:
                                result = QString{"Amsaro GmbH"};
                                break;
                            case 0x40:
                                result = QString{"CDS Advanced Technology BV (Lanbox)"};
                                break;
                            case 0x41:
                                result = QString{"Mode Machines (Touched By Sound GmbH)"};
                                break;
                            case 0x42:
                                result = QString{"DSP Arts"};
                                break;
                            case 0x43:
                                result = QString{"Phil Rees Music Tech"};
                                break;
                            case 0x44:
                                result = QString{"Stamer Musikanlagen GmbH"};
                                break;
                            case 0x45:
                                result = QString{"Musical Muntaner S.A. dba Soundart"};
                                break;
                            case 0x46:
                                result = QString{"C-Mexx Software"};
                                break;
                            case 0x47:
                                result = QString{"Klavis Technologies"};
                                break;
                            case 0x48:
                                result = QString{"Noteheads AB"};
                                break;
                            case 0x49:
                                result = QString{"Algorithmix"};
                                break;
                            case 0x4A:
                                result = QString{"Skrydstrup R&D"};
                                break;
                            case 0x4B:
                                result = QString{"Professional Audio Company"};
                                break;
                            case 0x4C:
                                result = QString{"NewWave Labs (MadWaves)"};
                                break;
                            case 0x4D:
                                result = QString{"Vermona"};
                                break;
                            case 0x4E:
                                result = QString{"Nokia"};
                                break;
                            case 0x4F:
                                result = QString{"Wave Idea"};
                                break;
                            case 0x50:
                                result = QString{"Hartmann GmbH"};
                                break;
                            case 0x51:
                                result = QString{"Lion’s Tracs"};
                                break;
                            case 0x52:
                                result = QString{"Analogue Systems"};
                                break;
                            case 0x53:
                                result = QString{"Focal-JMlab"};
                                break;
                            case 0x54:
                                result = QString{"Ringway Electronics (Chang-Zhou) Co Ltd"};
                                break;
                            case 0x55:
                                result = QString{"Faith Technologies (Digiplug)"};
                                break;
                            case 0x56:
                                result = QString{"Showworks"};
                                break;
                            case 0x57:
                                result = QString{"Manikin Electronic"};
                                break;
                            case 0x58:
                                result = QString{"1 Come Tech"};
                                break;
                            case 0x59:
                                result = QString{"Phonic Corp"};
                                break;
                            case 0x5A:
                                result = QString{"Dolby Australia (Lake)"};
                                break;
                            case 0x5B:
                                result = QString{"Silansys Technologies"};
                                break;
                            case 0x5C:
                                result = QString{"Winbond Electronics"};
                                break;
                            case 0x5D:
                                result = QString{"Cinetix Medien und Interface GmbH"};
                                break;
                            case 0x5E:
                                result = QString{"A&G Soluzioni Digitali"};
                                break;
                            case 0x5F:
                                result = QString{"Sequentix GmbH"};
                                break;
                            case 0x60:
                                result = QString{"Oram Pro Audio"};
                                break;
                            case 0x61:
                                result = QString{"Be4 Ltd"};
                                break;
                            case 0x62:
                                result = QString{"Infection Music"};
                                break;
                            case 0x63:
                                result = QString{"Central Music Co. (CME)"};
                                break;
                            case 0x64:
                                result = QString{"genoQs Machines GmbH"};
                                break;
                            case 0x65:
                                result = QString{"Medialon"};
                                break;
                            case 0x66:
                                result = QString{"Waves Audio Ltd"};
                                break;
                            case 0x67:
                                result = QString{"Jerash Labs"};
                                break;
                            case 0x68:
                                result = QString{"Da Fact"};
                                break;
                            case 0x69:
                                result = QString{"Elby Designs"};
                                break;
                            case 0x6A:
                                result = QString{"Spectral Audio"};
                                break;
                            case 0x6B:
                                result = QString{"Arturia"};
                                break;
                            case 0x6C:
                                result = QString{"Vixid"};
                                break;
                            case 0x6D:
                                result = QString{"C-Thru Music"};
                                break;
                            case 0x6E:
                                result = QString{"Ya Horng Electronic Co LTD"};
                                break;
                            case 0x6F:
                                result = QString{"SM Pro Audio"};
                                break;
                            case 0x70:
                                result = QString{"OTO Machines"};
                                break;
                            case 0x71:
                                result = QString{"ELZAB S.A. (G LAB)"};
                                break;
                            case 0x72:
                                result = QString{"Blackstar Amplification Ltd"};
                                break;
                            case 0x73:
                                result = QString{"M3i Technologies GmbH"};
                                break;
                            case 0x74:
                                result = QString{"Gemalto (from Xiring)"};
                                break;
                            case 0x75:
                                result = QString{"Prostage SL"};
                                break;
                            case 0x76:
                                result = QString{"Teenage Engineering"};
                                break;
                            case 0x77:
                                result = QString{"Tobias Erichsen Consulting"};
                                break;
                            case 0x78:
                                result = QString{"Nixer Ltd"};
                                break;
                            case 0x79:
                                result = QString{"Hanpin Electron Co Ltd"};
                                break;
                            case 0x7A:
                                result = QString{"\"MIDI-hardware\" R.Sowa"};
                                break;
                            case 0x7B:
                                result = QString{"Beyond Music Industrial Ltd"};
                                break;
                            case 0x7C:
                                result = QString{"Kiss Box B.V."};
                                break;
                            case 0x7D:
                                result = QString{"Misa Digital Technologies Ltd"};
                                break;
                            case 0x7E:
                                result = QString{"AI Musics Technology Inc"};
                                break;
                            case 0x7F:
                                result = QString{"Serato Inc LP"};
                                break;
                            default:
                                break;
                        }
                        break;
                    case 0x21:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Limex"};
                                break;
                            case 0x01:
                                result = QString{"Kyodday (Tokai)"};
                                break;
                            case 0x02:
                                result = QString{"Mutable Instruments"};
                                break;
                            case 0x03:
                                result = QString{"PreSonus Software Ltd"};
                                break;
                            case 0x04:
                                result = QString{"Ingenico (was Xiring)"};
                                break;
                            case 0x05:
                                result = QString{"Fairlight Instruments Pty Ltd"};
                                break;
                            case 0x06:
                                result = QString{"Musicom Lab"};
                                break;
                            case 0x07:
                                result = QString{"Modal Electronics (Modulus/VacoLoco)"};
                                break;
                            case 0x08:
                                result = QString{"RWA (Hong Kong) Limited"};
                                break;
                            case 0x09:
                                result = QString{"Native Instruments"};
                                break;
                            case 0x0A:
                                result = QString{"Naonext"};
                                break;
                            case 0x0B:
                                result = QString{"MFB"};
                                break;
                            case 0x0C:
                                result = QString{"Teknel Research"};
                                break;
                            case 0x0D:
                                result = QString{"Ploytec GmbH"};
                                break;
                            case 0x0E:
                                result = QString{"Surfin Kangaroo Studio"};
                                break;
                            case 0x0F:
                                result = QString{"Philips Electronics HK Ltd"};
                                break;
                            case 0x10:
                                result = QString{"ROLI Ltd"};
                                break;
                            case 0x11:
                                result = QString{"Panda-Audio Ltd"};
                                break;
                            case 0x12:
                                result = QString{"BauM Software"};
                                break;
                            case 0x13:
                                result = QString{"Machinewerks Ltd."};
                                break;
                            case 0x14:
                                result = QString{"Xiamen Elane Electronics"};
                                break;
                            case 0x15:
                                result = QString{"Marshall Amplification PLC"};
                                break;
                            case 0x16:
                                result = QString{"Kiwitechnics Ltd"};
                                break;
                            case 0x17:
                                result = QString{"Rob Papen"};
                                break;
                            case 0x18:
                                result = QString{"Spicetone OU"};
                                break;
                            case 0x19:
                                result = QString{"V3Sound"};
                                break;
                            case 0x1A:
                                result = QString{"IK Multimedia"};
                                break;
                            case 0x1B:
                                result = QString{"Novalia Ltd"};
                                break;
                            case 0x1C:
                                result = QString{"Modor Music"};
                                break;
                            case 0x1D:
                                result = QString{"Ableton"};
                                break;
                            case 0x1E:
                                result = QString{"Dtronics"};
                                break;
                            case 0x1F:
                                result = QString{"ZAQ Audio"};
                                break;
                            case 0x20:
                                result = QString{"Muabaobao Education Technology Co Ltd"};
                                break;
                            case 0x21:
                                result = QString{"Flux Effects"};
                                break;
                            case 0x22:
                                result = QString{"Audiothingies (MCDA)"};
                                break;
                            case 0x23:
                                result = QString{"Retrokits"};
                                break;
                            case 0x24:
                                result = QString{"Morningstar FX Pte Ltd"};
                                break;
                            case 0x25:
                                result = QString{"Changsha Hotone Audio Co Ltd"};
                                break;
                            case 0x26:
                                result = QString{"Expressive E"};
                                break;
                            case 0x27:
                                result = QString{"Expert Sleepers Ltd"};
                                break;
                            case 0x28:
                                result = QString{"Timecode-Vision Technology"};
                                break;
                            case 0x29:
                                result = QString{"Hornberg Research GbR"};
                                break;
                            case 0x2A:
                                result = QString{"Sonic Potions"};
                                break;
                            case 0x2B:
                                result = QString{"Audiofront"};
                                break;
                            case 0x2C:
                                result = QString{"Fred’s Lab"};
                                break;
                            case 0x2D:
                                result = QString{"Audio Modeling"};
                                break;
                            case 0x2E:
                                result = QString{"C. Bechstein Digital GmbH"};
                                break;
                            case 0x2F:
                                result = QString{"Motas Electronics Ltd"};
                                break;
                            case 0x30:
                                result = QString{"Elk Audio"};
                                break;
                            case 0x31:
                                result = QString{"Sonic Academy Ltd"};
                                break;
                            case 0x32:
                                result = QString{"Bome Software"};
                                break;
                            case 0x33:
                                result = QString{"AODYO SAS"};
                                break;
                            case 0x34:
                                result = QString{"Pianoforce S.R.O"};
                                break;
                            case 0x35:
                                result = QString{"Dreadbox P.C."};
                                break;
                            case 0x36:
                                result = QString{"TouchKeys Instruments Ltd"};
                                break;
                            case 0x37:
                                result = QString{"The Gigrig Ltd"};
                                break;
                            case 0x38:
                                result = QString{"ALM Co"};
                                break;
                            case 0x39:
                                result = QString{"CH Sound Design"};
                                break;
                            case 0x3A:
                                result = QString{"Beat Bars"};
                                break;
                            case 0x3B:
                                result = QString{"Blokas"};
                                break;
                            case 0x3C:
                                result = QString{"GEWA Music GmbH"};
                                break;
                            case 0x3D:
                                result = QString{"dadamachines"};
                                break;
                            case 0x3E:
                                result = QString{"Augmented Instruments Ltd (Bela)"};
                                break;
                            case 0x3F:
                                result = QString{"Supercritical Ltd"};
                                break;
                            case 0x40:
                                result = QString{"Genki Instruments"};
                                break;
                            case 0x41:
                                result = QString{"Marienberg Devices Germany"};
                                break;
                            case 0x42:
                                result = QString{"Supperware Ltd"};
                                break;
                            case 0x43:
                                result = QString{"Imoxplus BVBA"};
                                break;
                            case 0x44:
                                result = QString{"Swapp Technologies SRL"};
                                break;
                            case 0x45:
                                result = QString{"Electra One S.R.O."};
                                break;
                            case 0x46:
                                result = QString{"Digital Clef Limited"};
                                break;
                            case 0x47:
                                result = QString{"Paul Whittington Group Ltd"};
                                break;
                            case 0x48:
                                result = QString{"Music Hackspace"};
                                break;
                            case 0x49:
                                result = QString{"Bitwig GMBH"};
                                break;
                            case 0x4A:
                                result = QString{"Enhancia"};
                                break;
                            case 0x4B:
                                result = QString{"KV 331"};
                                break;
                            case 0x4C:
                                result = QString{"Tehnicadelarte"};
                                break;
                            case 0x4D:
                                result = QString{"Endlesss Studio"};
                                break;
                            case 0x4E:
                                result = QString{"Dongguan MIDIPLUS Co., LTD"};
                                break;
                            case 0x4F:
                                result = QString{"Gracely Pty Ltd."};
                                break;
                            case 0x50:
                                result = QString{"Embodme"};
                                break;
                            case 0x51:
                                result = QString{"MuseScore"};
                                break;
                            case 0x52:
                                result = QString{"EPFL (E-Lab)"};
                                break;
                            case 0x53:
                                result = QString{"Orb3 Ltd."};
                                break;
                            case 0x54:
                                result = QString{"Pitch Innovations"};
                                break;
                            case 0x55:
                                result = QString{"Playces"};
                                break;
                            case 0x56:
                                result = QString{"UDO Audio LTD"};
                                break;
                            case 0x57:
                                result = QString{"RSS Sound Design"};
                                break;
                            case 0x58:
                                result = QString{"Nonlinear Labs GmbH"};
                                break;
                            case 0x59:
                                result = QString{"Robkoo Information & Technologies Co., Ltd."};
                                break;
                            case 0x5A:
                                result = QString{"Cari Electronic"};
                                break;
                            case 0x5B:
                                result = QString{"Oxi Electronic Instruments SL"};
                                break;
                            case 0x5C:
                                result = QString{"XMPT"};
                                break;
                            case 0x5D:
                                result = QString{"SHANGHAI HUAXIN MUSICAL INSTRUMENT"};
                                break;
                            case 0x5E:
                                result = QString{"Shenzhen Huashi Technology Co., Ltd"};
                                break;
                            // case 0x5F:
                            //     result = QString{""};
                            //     break;
                            case 0x60:
                                result = QString{"Guangzhou Rantion Technology Co., Ltd."};
                                break;
                            case 0x61:
                                result = QString{"Ryme Music"};
                                break;
                            case 0x62:
                                result = QString{"GS Music"};
                                break;
                            case 0x63:
                                result = QString{"Shenzhen Flamma Innovation Co., Ltd"};
                                break;
                            case 0x64:
                                result = QString{"Shenzhen Mooer Audio Co.,LTD."};
                                break;
                            case 0x65:
                                result = QString{"Raw Material Software Limited (JUCE)"};
                                break;
                            case 0x66:
                                result = QString{"Birdkids"};
                                break;
                            case 0x67:
                                result = QString{"Beijing QianYinHuLian Tech. Co"};
                                break;
                            case 0x68:
                                result = QString{"Nimikry Music OG"};
                                break;
                            case 0x69:
                                result = QString{"Newzik"};
                                break;
                            case 0x6A:
                                result = QString{"Hamburg Wave"};
                                break;
                            case 0x6B:
                                result = QString{"Grimm Audio"};
                                break;
                            case 0x6C:
                                result = QString{"Arcana Instruments LTD."};
                                break;
                            case 0x6D:
                                result = QString{"GameChanger Audio"};
                                break;
                            case 0x6E:
                                result = QString{"OakTone"};
                                break;
                            case 0x6F:
                                result = QString{"The Digi-Gurdy: A MIDI Hurdy Gurdy"};
                                break;
                            case 0x70:
                                result = QString{"MusiKraken"};
                                break;
                            case 0x71:
                                result = QString{"PhotoSynth > InterFACE"};
                                break;
                            case 0x72:
                                result = QString{"Instruments of Things"};
                                break;
                            case 0x73:
                                result = QString{"oodi"};
                                break;
                            case 0x74:
                                result = QString{"Komires Sp. z o.o."};
                                break;
                            case 0x75:
                                result = QString{"Lehle GmbH"};
                                break;
                            case 0x76:
                                result = QString{"Joué Music Instruments"};
                                break;
                            case 0x77:
                                result = QString{"Guangzhou Pearl River Amason Digital Musical Instrument Co. Ltd"};
                                break;
                            case 0x78:
                                result = QString{"Rhesus Engineering GmbH "};
                                break;
                            case 0x79:
                                result = QString{"Bremmers Audio Design"};
                                break;
                            case 0x7A:
                                result = QString{"Cherub Technology Co., Ltd"};
                                break;
                            case 0x7B:
                                result = QString{"Synthstrom Audible"};
                                break;
                            case 0x7C:
                                result = QString{"Neural DSP Technologies Oy"};
                                break;
                            case 0x7D:
                                result = QString{"2box AB"};
                                break;
                            case 0x7E:
                                result = QString{"Intuitive Instruments"};
                                break;
                            case 0x7F:
                                result = QString{"Twisted-electrons"};
                                break;
                            default:
                                break;
                        }
                        break;
                    case 0x22:
                        switch(bytes[2]) {
                            case 0x00:
                                result = QString{"Wildcard Engineering BV"};
                                break;
                            case 0x01:
                                result = QString{"Dato Musical Instruments"};
                                break;
                            case 0x02:
                                result = QString{"JTJ AUDIO"};
                                break;
                            case 0x03:
                                result = QString{"Melbourne Instruments"};
                                break;
                            case 0x04:
                                result = QString{"AZMINO"};
                                break;
                            case 0x05:
                                result = QString{"TELEMIDI (TRIGITAL PTY LTD)"};
                                break;
                            case 0x06:
                                result = QString{"Tylium"};
                                break;
                            case 0x07:
                                result = QString{"Engineering Lab"};
                                break;
                            case 0x08:
                                result = QString{"Archaea Modular Synthesis Ltd."};
                                break;
                            case 0x09:
                                result = QString{"Tentacle Sync GmbH"};
                                break;
                            case 0x0A:
                                result = QString{"Misa Digital Pty Ltd"};
                                break;
                            case 0x0B:
                                result = QString{"Heavy Procrastination Industries ApS"};
                                break;
                            case 0x0C:
                                result = QString{"Telepathic Pty Ltd"};
                                break;
                            case 0x0D:
                                result = QString{"Neoharp LLC-FZ"};
                                break;
                            case 0x0E:
                                result = QString{"Rhodes Music Group"};
                                break;
                            case 0x0F:
                                result = QString{"Halbestunde GmbH"};
                                break;
                            case 0x10:
                                result = QString{"FluQe"};
                                break;
                            case 0x11:
                                result = QString{""};
                                break;
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
