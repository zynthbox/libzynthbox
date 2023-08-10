/*
 * Copyright (C) 2022 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "MidiRecorder.h"

#include "Note.h"
#include "PlayGridManager.h"
#include "PatternModel.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QTimer>

// Hackety hack - we don't need all the thing, just need some storage things (MidiBuffer and MidiNote specifically)
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_audio_formats/juce_audio_formats.h>

using frame_clock = std::chrono::steady_clock;

class MidiRecorderPrivate {
public:
    MidiRecorderPrivate() {}
    bool isRecording{false};
    bool isPlaying{false};
    bool trackEnabled[10];
    // One for each of the sketchpad tracks
    juce::MidiMessageSequence globalMidiMessageSequence;
    juce::MidiMessageSequence midiMessageSequence[10];
    double recordingStartTime;
    void handleMidiMessage(const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const double& timestamp, const int &sketchpadTrack) {
        if (isRecording) {
            if (0x7F < byte1 && byte1 < 0xA0) {
                // Using microseconds for timestamps (midi is commonly that anyway)
                // and juce expects ongoing timestamps, not intervals (it will create those when saving)
                double ourTimestamp = qMax(timestamp - recordingStartTime, 0.0);
                juce::MidiMessage message{byte1, byte2, byte3, ourTimestamp};
                midiMessageSequence[sketchpadTrack].addEvent(message);
                globalMidiMessageSequence.addEvent(message);
            }
        }
    }
};

MidiRecorder::MidiRecorder(QObject *parent)
    : QObject(parent)
    , d(new MidiRecorderPrivate)
{
    SyncTimer *syncTimer = SyncTimer::instance();
    connect(syncTimer, &SyncTimer::timerRunningChanged,
            this, [this, syncTimer]()
            {
                if (!syncTimer->timerRunning()) {
                    if (d->isPlaying) {
                        d->isPlaying = false;
                        Q_EMIT isPlayingChanged();
                    }
                    if (d->isRecording) {
                        stopRecording();
                    }
                }
            }
    );
    connect(PlayGridManager::instance(), &PlayGridManager::midiMessage,
            this, [this](const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const double& timeStamp, const int &sketchpadTrack)
            {
                d->handleMidiMessage(byte1, byte2, byte3, timeStamp, sketchpadTrack);
            }
    );
}

MidiRecorder::~MidiRecorder() = default;

void MidiRecorder::startRecording(int sketchpadTrack, bool clear)
{
    if (clear) {
        clearRecording();
    }
    if (sketchpadTrack > 0) {
        d->trackEnabled[sketchpadTrack] = true;
    }
    if (!d->isRecording) {
        d->recordingStartTime = SyncTimer::instance()->jackPlayheadUsecs();
        d->isRecording = true;
        Q_EMIT isRecordingChanged();
    }
}

void MidiRecorder::stopRecording(int sketchpadTrack)
{
    if (sketchpadTrack == -1) {
        for (int i = 0; i < 10; ++i) {
            d->trackEnabled[i] = false;
        }
    } else {
        d->trackEnabled[sketchpadTrack] = false;
    }
    bool isEmpty{true};
    for (int i = 0; i < 10; ++i) {
        if (d->trackEnabled[i]) {
            isEmpty = false;
            break;
        }
    }
    if (isEmpty) {
        d->isRecording = false;
        Q_EMIT isRecordingChanged();
    }
}

void MidiRecorder::clearRecording()
{
    d->globalMidiMessageSequence.clear();
    for (int i = 0; i < 10; ++i) {
        d->midiMessageSequence[i].clear();
    }
}

bool MidiRecorder::loadFromMidi(const QByteArray &midiData)
{
    return loadTrackFromMidi(midiData, -1);
}

bool MidiRecorder::loadTrackFromMidi(const QByteArray& midiData, const int& sketchpadTrack)
{
    bool success{false};

    juce::MemoryBlock block(midiData.data(), size_t(midiData.size()));
    juce::MemoryInputStream in(block, false);
    juce::MidiFile file;
    if (file.readFrom(in, true)) {
        if (file.getNumTracks() > 0) {
            if (sketchpadTrack == -1) {
                d->globalMidiMessageSequence = juce::MidiMessageSequence(*file.getTrack(0));
            } else {
                d->midiMessageSequence[sketchpadTrack] = juce::MidiMessageSequence(*file.getTrack(0));
            }
            success = true;
        }
    } else {
        qDebug() << Q_FUNC_INFO << "Failed to read midi from data" << midiData << "of size" << block.getSize();
    }

    return success;
}

QByteArray MidiRecorder::midi() const
{
    return trackMidi(-1);
}

QByteArray MidiRecorder::trackMidi(int sketchpadTrack) const
{
    QByteArray data;

    juce::MidiFile file;
    if (sketchpadTrack == -1) {
        file.addTrack(d->globalMidiMessageSequence);
    } else {
        file.addTrack(d->midiMessageSequence[sketchpadTrack]);
    }

    juce::MemoryOutputStream out;
    if (file.writeTo(out)) {
        out.flush();

        juce::MemoryBlock block = out.getMemoryBlock();
        data.append((char*)block.getData(), int(block.getSize()));
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to write midi to memory output stream";
    }
    return data;
}

bool MidiRecorder::loadFromBase64Midi(const QString &data)
{
    return loadFromMidi(QByteArray::fromBase64(data.toLatin1()));
}

bool MidiRecorder::loadTrackFromBase64Midi(const QString& data, const int& sketchpadTrack)
{
    return loadTrackFromMidi(QByteArray::fromBase64(data.toLatin1()), sketchpadTrack);
}

QString MidiRecorder::base64Midi() const
{
    return midi().toBase64();
}

QString MidiRecorder::base64TrackMidi(int sketchpadTrack) const
{
    return trackMidi(sketchpadTrack).toBase64();
}

bool MidiRecorder::loadFromAscii(const QString &/*asciiRepresentation*/)
{
    bool success{false};
    qWarning() << Q_FUNC_INFO << "NO ACTION TAKEN - UNIMPLEMENTED!";
    return success;
}

QString MidiRecorder::ascii() const
{
    QString data;
    qWarning() << Q_FUNC_INFO << "NO ACTION TAKEN - UNIMPLEMENTED!";
    return data;
}

void MidiRecorder::forceToChannel(int channel)
{
    for (juce::MidiMessageSequence::MidiEventHolder *holder : d->globalMidiMessageSequence) {
        holder->message.setChannel(channel + 1);
    }
}

void MidiRecorder::playRecording()
{
//     qDebug() << Q_FUNC_INFO;
    SyncTimer *syncTimer = SyncTimer::instance();
    juce::MidiBuffer midiBuffer;
    double mostRecentTimestamp{-1};
    for (const juce::MidiMessageSequence::MidiEventHolder *holder : d->globalMidiMessageSequence) {
//         qDebug() << "Investimagating" << QString::fromStdString(holder->message.getDescription().toStdString());
        if (holder->message.getTimeStamp() != mostRecentTimestamp) {
            if (midiBuffer.getNumEvents() > 0) {
//                 qDebug() << "Hey, things in the buffer, let's schedule those" << midiBuffer.getNumEvents() << "things, this far into the future:" << syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / (double)1000000);
                syncTimer->scheduleMidiBuffer(midiBuffer, syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / (double)1000000));
            }
            mostRecentTimestamp = holder->message.getTimeStamp();
//             qDebug() << "New timestamp, clear the buffer, timestamp is now" << mostRecentTimestamp;
            midiBuffer.clear();
        }
        midiBuffer.addEvent(holder->message, midiBuffer.getNumEvents());
    }
//     qDebug() << "Unblocking, lets go!";
    syncTimer->start();
    d->isPlaying = true;
    Q_EMIT isPlayingChanged();
    QTimer::singleShot(100 + mostRecentTimestamp / 1000, syncTimer, &SyncTimer::stop);
}

void MidiRecorder::stopPlayback()
{
    SyncTimer *syncTimer = SyncTimer::instance();
    syncTimer->stop();
}

bool MidiRecorder::applyToPattern(PatternModel *patternModel, QFlags<MidiRecorder::ApplicatorSetting> settings) const
{
    bool success{false};
    QList<int> acceptChannel;
    if (settings.testFlag(ClearPatternBeforeApplying)) {
        patternModel->clear();
    }
    if (settings.testFlag(LimitToPatternChannel)) {
        acceptChannel << patternModel->midiChannel();
    } else {
        if (settings.testFlag(ApplyChannel0)) {
            acceptChannel << 0;
        }
        if (settings.testFlag(ApplyChannel1)) {
            acceptChannel << 1;
        }
        if (settings.testFlag(ApplyChannel2)) {
            acceptChannel << 2;
        }
        if (settings.testFlag(ApplyChannel3)) {
            acceptChannel << 3;
        }
        if (settings.testFlag(ApplyChannel4)) {
            acceptChannel << 4;
        }
        if (settings.testFlag(ApplyChannel5)) {
            acceptChannel << 5;
        }
        if (settings.testFlag(ApplyChannel6)) {
            acceptChannel << 6;
        }
        if (settings.testFlag(ApplyChannel7)) {
            acceptChannel << 7;
        }
        if (settings.testFlag(ApplyChannel8)) {
            acceptChannel << 8;
        }
        if (settings.testFlag(ApplyChannel9)) {
            acceptChannel << 9;
        }
        if (settings.testFlag(ApplyChannel10)) {
            acceptChannel << 10;
        }
        if (settings.testFlag(ApplyChannel11)) {
            acceptChannel << 11;
        }
        if (settings.testFlag(ApplyChannel12)) {
            acceptChannel << 12;
        }
        if (settings.testFlag(ApplyChannel13)) {
            acceptChannel << 13;
        }
        if (settings.testFlag(ApplyChannel14)) {
            acceptChannel << 14;
        }
        if (settings.testFlag(ApplyChannel15)) {
            acceptChannel << 15;
        }
    }

    // work out how many microseconds we've got per step in the given pattern
    SyncTimer *syncTimer = SyncTimer::instance();
    quint64 subbeatsPerStep{0};
    switch (patternModel->noteLength()) {
    case 1:
        subbeatsPerStep = 32;
        break;
    case 2:
        subbeatsPerStep = 16;
        break;
    case 3:
        subbeatsPerStep = 8;
        break;
    case 4:
        subbeatsPerStep = 4;
        break;
    case 5:
        subbeatsPerStep = 2;
        break;
    case 6:
        subbeatsPerStep = 1;
        break;
    default:
        break;
    }
    int microsecondsPerStep = syncTimer->subbeatCountToSeconds(syncTimer->getBpm(), subbeatsPerStep * quint64(syncTimer->getMultiplier() / 64)) * 1000000;
    int microsecondsPerSubbeat = syncTimer->subbeatCountToSeconds(syncTimer->getBpm(), 1) * 1000000;

    // Update the matching on/off pairs in the sequence (just to make sure they're there, and logically
    // matched, as we depend on that below, but also kind of just want things ready before we use the data
    d->globalMidiMessageSequence.updateMatchedPairs();
    for(int i = 0; i < 10; ++i) {
        d->midiMessageSequence[i].updateMatchedPairs();
    }

    // find out what the last "on" message is, and use that to determine what the last step would be in the current sequence
    int lastStep{-1};
    if (d->globalMidiMessageSequence.getNumEvents() > 0) {
        qDebug() << Q_FUNC_INFO << "Operating on" << d->globalMidiMessageSequence.getNumEvents() << "events, for a pattern with note length" << patternModel->noteLength() << "meaning" << subbeatsPerStep << "subbeats per step" << "µs per step" << microsecondsPerStep << "µs per subbeat" << microsecondsPerSubbeat;
        qDebug() << Q_FUNC_INFO << "We've got more than one event recorded, let's find the last on note...";
        for (int messageIndex = d->globalMidiMessageSequence.getNumEvents() - 1; messageIndex > -1; --messageIndex) {
            juce::MidiMessageSequence::MidiEventHolder *message = d->globalMidiMessageSequence.getEventPointer(messageIndex);
            if (message->message.isNoteOn()) {
                // use the position of that message to work out how many steps we need
                lastStep = message->message.getTimeStamp() / microsecondsPerStep;
                qDebug() << Q_FUNC_INFO << "Found an on note while traversing backwards, position is" << lastStep;
                break;
            }
        }
    }
    if (lastStep > -1) {
        // if it's more than pattern width*bankLength, we've got a problem (and should probably do the first part, and then spit out a message somewhere to the effect that some bits are missing)
        if (lastStep > patternModel->width() * patternModel->bankLength()) {
            qWarning() << Q_FUNC_INFO << "We've got more notes in this recording than what will fit in the given pattern with its current note length. Adding what there's room for and ignoring the rest. Last step was supposed to be" << lastStep << "and we have room for" << patternModel->width() * patternModel->bankLength();
            lastStep = patternModel->width() * patternModel->bankLength();
        }
        // resize the pattern to the right number of bars (number of steps divided by the pattern's width)
        patternModel->setAvailableBars((lastStep / patternModel->width()) + 1);
        // fetch the messages in order until the step position is "next step" and then forward the step, find the matching off note (if none is found, set duration 0) and insert them on the current step (if the message's channel is in the accepted list, remembering juce's 1-indexing)
        int step{0}, midiChannel{0}, midiNote{0}, timestamp{0}, duration{0}, velocity{0}, delay{0}, row{0}, column{0}, subnoteIndex{0};
        Note* note{nullptr};
        for (int messageIndex = 0; messageIndex < d->globalMidiMessageSequence.getNumEvents(); ++messageIndex) {
            juce::MidiMessageSequence::MidiEventHolder *message = d->globalMidiMessageSequence.getEventPointer(messageIndex);
            if (!message) {
                qDebug() << "Apparently got an incorrect message, this is bad";
            }
            // Only operate on noteOn messages, because they're the ones being inserted
            if (message->message.isNoteOn() && acceptChannel.contains(message->message.getChannel() - 1)) {
                midiChannel = message->message.getChannel() - 1;
                midiNote = message->message.getNoteNumber();
                velocity = message->message.getVelocity();
                timestamp = message->message.getTimeStamp();
                qDebug() << Q_FUNC_INFO << "Found an on message, for channel, note, velocity, and timestamp" << midiChannel << midiNote << velocity << timestamp;
                while (timestamp > (step * microsecondsPerStep)) {
                    ++step;
                }
                qDebug() << Q_FUNC_INFO << "Increased step position to match" << double(timestamp) / 1000000.0 << "seconds, now operating on step" << step;
                delay = ((step * microsecondsPerStep) - timestamp) / microsecondsPerSubbeat;
                if (message->noteOffObject) {
                    duration = (message->noteOffObject->message.getTimeStamp() - timestamp - delay) / microsecondsPerStep;
                    qDebug() << Q_FUNC_INFO << "Found a note off partner, duration is now" << duration;
                } else {
                    duration = 0;
                }
            } else {
                if (!acceptChannel.contains(message->message.getChannel() - 1)) {
                    qDebug() << Q_FUNC_INFO << "Message channel is not accepted, skipping";
                } else {
                    qDebug() << Q_FUNC_INFO << "Not an on message, skipping";
                }
                continue;
            }
            // Actually insert the message's note data into the step
            note = qobject_cast<Note*>(patternModel->playGridManager()->getNote(midiNote, midiChannel));
            row = patternModel->bankOffset() + (step / patternModel->width());
            column = step % patternModel->width();
            subnoteIndex = patternModel->addSubnote(row, column, note);
            qDebug() << Q_FUNC_INFO << "Inserted subnote at" << row << column << "New subnote is" << note << "with duration" << duration << "delay" << delay;
            patternModel->setSubnoteMetadata(row, column, subnoteIndex, "velocity", velocity);
            if (duration > 0) {
                patternModel->setSubnoteMetadata(row, column, subnoteIndex, "duration", duration);
            }
            if (delay > 0) {
                patternModel->setSubnoteMetadata(row, column, subnoteIndex, "delay", delay);
            }
            // If we're now past the last step, break out
            if (step > lastStep) {
                qDebug() << Q_FUNC_INFO << "We're past the final step, break out";
                break;
            }
        }
        success = true;
    } else {
        qWarning() << Q_FUNC_INFO << "Failed to find a last step";
    }

    return success;
}

bool MidiRecorder::isPlaying() const
{
    return d->isPlaying;
}

bool MidiRecorder::isRecording() const
{
    return d->isRecording;
}
