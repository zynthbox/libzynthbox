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

#include "MidiRouter.h"
#include "Note.h"
#include "PlayGridManager.h"
#include "PatternModel.h"
#include "SyncTimer.h"
#include "TimerCommand.h"
#include "ZynthboxBasics.h"

#include <QDebug>
#include <QTimer>
#include <float.h>

// Hackety hack - we don't need all the thing, just need some storage things (MidiBuffer and MidiNote specifically)
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_audio_formats/juce_audio_formats.h>

#define MidiRecorderRingSize 65536
class MidiRecorderRing {
public:
    struct Entry {
        Entry *previous{nullptr};
        Entry *next{nullptr};
        double timestamp{0.0};
        int sketchpadTrack{-1};
        bool processed{true};
        unsigned char byte0{0};
        unsigned char byte1{0};
        unsigned char byte2{0};
        unsigned char size{0};
    };
    MidiRecorderRing() {
        Entry* entryPrevious{&ringData[MidiRecorderRingSize - 1]};
        for (quint64 i = 0; i < MidiRecorderRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~MidiRecorderRing() { }

    void write(const double &timestamp, const int &sketchpadTrack, const unsigned char &byte0, const unsigned char &byte1, const unsigned char &byte2, const unsigned char &size) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data stored at the write location for time" << writeHead->timestamp << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->sketchpadTrack = sketchpadTrack;
        entry->size = size;
        entry->byte0 = byte0;
        entry->byte1 = byte1;
        entry->byte2 = byte2;
        entry->timestamp = timestamp;
        entry->processed = false;
    }
    /**
     * \brief Attempt to read the data out of the ring, until there are no more unprocessed entries
     * @return Whether or not the read was valid
     */
    bool read(double *timestamp, int *sketchpadTrack, unsigned char *byte0, unsigned char *byte1, unsigned char *byte2, unsigned char *size) {
        if (readHead->processed == false) {
            Entry *entry = readHead;
            readHead = readHead->next;
            *sketchpadTrack = entry->sketchpadTrack;
            *byte0 = entry->byte0;
            *byte1 = entry->byte1;
            *byte2 = entry->byte2;
            *size = entry->size;
            *timestamp = entry->timestamp;
            entry->processed = true;
            return true;
        }
        return false;
    }

    Entry ringData[MidiRecorderRingSize];
    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
};

class MidiRecorderPrivate {
public:
    MidiRecorderPrivate() { }
    bool isRecording{false};
    bool isPlaying{false};
    bool trackEnabled[10];
    MidiRecorderRing recorderRing;
    // One for each of the sketchpad tracks
    juce::MidiMessageSequence globalMidiMessageSequence;
    juce::MidiMessageSequence midiMessageSequence[ZynthboxTrackCount];
    double recordingStartTime{DBL_MAX};
    double recordingStopTime{DBL_MAX};
    void handleMidiMessage(const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const unsigned char& size, const double& timestamp, const int &sketchpadTrack) {
        if (recordingStartTime <= timestamp && timestamp <= recordingStopTime) {
            // Using microseconds for timestamps (midi is commonly that anyway)
            // and juce expects ongoing timestamps, not intervals (it will create those when saving)
            double ourTimestamp = qMax(timestamp - recordingStartTime, 0.0);
            recorderRing.write(ourTimestamp, sketchpadTrack, byte1, byte2, byte3, size);
        // } else {
            // qDebug() << Q_FUNC_INFO << "Did not add message for" << sketchpadTrack << "containing bytes" << byte1 << byte2 << byte3 << "at time" << quint64(timestamp) << "which was not between" << quint64(recordingStartTime) << "and" << quint64(recordingStopTime);
        }
        if (recordingStopTime < timestamp && recordingStartTime < recordingStopTime) {
            recordingStartTime = DBL_MAX;
        }
    }

    QTimer processingTimer;
    void processRingData() {
        double timestamp{0.0};
        int sketchpadTrack{-1};
        unsigned char byte0{0};
        unsigned char byte1{0};
        unsigned char byte2{0};
        unsigned char size{0};
        while (recorderRing.read(&timestamp, &sketchpadTrack, &byte0, &byte1, &byte2, &size)) {
            juce::MidiMessage message;
            if (size == 3) {
                message = juce::MidiMessage(byte0, byte1, byte2, timestamp);
            } else if (size == 2) {
                message = juce::MidiMessage(byte0, byte1, timestamp);
            } else if (size == 1) {
                message = juce::MidiMessage(byte0, timestamp);
            }
            midiMessageSequence[sketchpadTrack].addEvent(message);
            globalMidiMessageSequence.addEvent(message);
            qDebug() << Q_FUNC_INFO << "Added message for track" << sketchpadTrack << "containing" << size << "bytes with values" << byte0 << byte1 << byte2 << "with local timestamp" << quint64(timestamp) << "µs, or" << (timestamp / 1000000) << "seconds";
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
                    if (d->isRecording && d->recordingStopTime == DBL_MAX) {
                        stopRecording();
                    }
                }
            }
    );
    d->processingTimer.setInterval(100);
    d->processingTimer.setSingleShot(false);
    connect(&d->processingTimer, &QTimer::timeout, this, [this](){
        d->processRingData();
    });
    connect(this, &MidiRecorder::isRecordingChanged, this, [this](){
        if (d->isRecording) {
            QMetaObject::invokeMethod(&d->processingTimer, "start");
        } else {
            QMetaObject::invokeMethod(&d->processingTimer, "stop");
        }
    });
}

MidiRecorder::~MidiRecorder() = default;

void MidiRecorder::handleMidiMessage(const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const unsigned char& size, const double& timeStamp, const int& sketchpadTrack)
{
    d->handleMidiMessage(byte1, byte2, byte3, size, timeStamp, sketchpadTrack);
}

void MidiRecorder::startRecording(int sketchpadTrack, bool clear, quint64 startTimestamp)
{
    if (clear) {
        clearRecording();
    }
    if (sketchpadTrack > 0) {
        d->trackEnabled[sketchpadTrack] = true;
    }
    if (!d->isRecording) {
        if (startTimestamp > 0) {
            d->recordingStartTime = startTimestamp;
        } else {
            d->recordingStartTime = SyncTimer::instance()->jackPlayheadUsecs();
        }
        d->recordingStopTime = DBL_MAX;
        d->isRecording = true;
        Q_EMIT isRecordingChanged();
    }
}

void MidiRecorder::scheduleStartRecording(quint64 delay, int sketchpadTrack)
{
    TimerCommand *command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::MidiRecorderStartOperation;
    command->parameter = sketchpadTrack;
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
}

void MidiRecorder::stopRecording(int sketchpadTrack, quint64 stopTimestamp)
{
    qDebug() << Q_FUNC_INFO << sketchpadTrack << stopTimestamp;
    if (stopTimestamp > 0) {
        d->recordingStopTime = stopTimestamp;
    } else {
        d->recordingStopTime = SyncTimer::instance()->jackPlayheadUsecs();
    }
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

void MidiRecorder::scheduleStopRecording(quint64 delay, int sketchpadTrack)
{
    TimerCommand *command = SyncTimer::instance()->getTimerCommand();
    command->operation = TimerCommand::MidiRecorderStopOperation;
    command->parameter = sketchpadTrack;
    SyncTimer::instance()->scheduleTimerCommand(delay, command);
}

void MidiRecorder::clearRecording()
{
    d->globalMidiMessageSequence.clear();
    for (int i = 0; i < ZynthboxTrackCount; ++i) {
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
                qDebug() << Q_FUNC_INFO << "Loaded" << d->globalMidiMessageSequence.getNumEvents() << "events into the global midi sequence";
            } else {
                d->midiMessageSequence[sketchpadTrack] = juce::MidiMessageSequence(*file.getTrack(0));
                qDebug() << Q_FUNC_INFO << "Loaded" << d->midiMessageSequence[sketchpadTrack].getNumEvents() << "events into the sequence for track" << sketchpadTrack;
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

    // First, make sure we've processed everything we've recorded into our sequences
    d->processRingData();

    // Then load the data into a midi file
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
    qDebug() << Q_FUNC_INFO << data;
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
    qDebug() << Q_FUNC_INFO;
    SyncTimer *syncTimer = SyncTimer::instance();
    juce::MidiBuffer midiBuffer;
    double mostRecentTimestamp{-1};
    for (const juce::MidiMessageSequence::MidiEventHolder *holder : d->globalMidiMessageSequence) {
        qDebug() << "Investimagating" << QString::fromStdString(holder->message.getDescription().toStdString());
        if (holder->message.getTimeStamp() != mostRecentTimestamp) {
            if (midiBuffer.getNumEvents() > 0) {
                qDebug() << "Hey, things in the buffer, let's schedule those" << midiBuffer.getNumEvents() << "things, this far into the future:" << syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / 1000000.0);
                syncTimer->scheduleMidiBuffer(midiBuffer, syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / 1000000.0));
            }
            mostRecentTimestamp = holder->message.getTimeStamp();
            qDebug() << "New timestamp, clear the buffer, timestamp is now" << mostRecentTimestamp;
            midiBuffer.clear();
        }
        midiBuffer.addEvent(holder->message, midiBuffer.getNumEvents());
    }
    if (midiBuffer.getNumEvents() > 0) {
        qDebug() << "Hey, things in the buffer, let's schedule those" << midiBuffer.getNumEvents() << "things, this far into the future:" << syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / 1000000.0);
        syncTimer->scheduleMidiBuffer(midiBuffer, syncTimer->secondsToSubbeatCount(syncTimer->getBpm(), mostRecentTimestamp / 1000000.0));
    }
    qDebug() << "Unblocking, lets go! Calling stop after" << 100 + (mostRecentTimestamp / 1000.0) << "ms";
    d->isPlaying = true;
    Q_EMIT isPlayingChanged();
    QTimer::singleShot(100 + (mostRecentTimestamp / 1000.0), this, [this](){ stopPlayback(); });
}

void MidiRecorder::stopPlayback()
{
    d->isPlaying = false;
    Q_EMIT isPlayingChanged();
    // (ab)use the stop call to force this call to reschedule all the off notes to "just do it now please"
    SyncTimer::instance()->stop();
}

bool MidiRecorder::applyToPattern(PatternModel *patternModel, QFlags<MidiRecorder::ApplicatorSetting> settings) const
{
    bool success{false};
    QList<int> acceptChannel;
    if (settings.testFlag(ClearPatternBeforeApplying)) {
        patternModel->clear();
    }
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

    // work out how many microseconds we've got per step in the given pattern
    SyncTimer *syncTimer = SyncTimer::instance();
    static const double timerTicksInOnePatternSubbeat{syncTimer->getMultiplier() / 32.0};
    const double microsecondsPerSubbeat = syncTimer->subbeatCountToSeconds(syncTimer->getBpm(), timerTicksInOnePatternSubbeat) * 1000000.0;
    static const double timerTicksInOneStepLengthUnit{(double(syncTimer->getMultiplier()) / 96.0)};
    const double microsecondsPerStep = syncTimer->subbeatCountToSeconds(syncTimer->getBpm(), patternModel->stepLength() * timerTicksInOneStepLengthUnit) * 1000000.0;

    // Update the matching on/off pairs in the sequence (just to make sure they're there, and logically
    // matched, as we depend on that below, but also kind of just want things ready before we use the data
    d->globalMidiMessageSequence.updateMatchedPairs();
    for(int i = 0; i < ZynthboxTrackCount; ++i) {
        d->midiMessageSequence[i].updateMatchedPairs();
    }

    // find out what the last "on" message is, and use that to determine what the last step would be in the current sequence
    int lastStep{-1};
    if (d->globalMidiMessageSequence.getNumEvents() > 0) {
        qDebug() << Q_FUNC_INFO << "Operating on" << d->globalMidiMessageSequence.getNumEvents() << "events, for a pattern with step length" << patternModel->stepLength() << "meaning" << microsecondsPerStep << "µs per step and" << microsecondsPerSubbeat << "µs per subbeat";
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
    int totalStepEntries{0};
    if (lastStep > -1) {
        // if it's more than pattern width*bankLength, we've got a problem (and should probably do the first part, and then spit out a message somewhere to the effect that some bits are missing)
        if (lastStep > patternModel->width() * patternModel->bankLength()) {
            qWarning() << Q_FUNC_INFO << "We've got more notes in this recording than what will fit in the given pattern with its current note length. Adding what there's room for and ignoring the rest. Last step was supposed to be" << lastStep << "and we have room for" << patternModel->width() * patternModel->bankLength();
            lastStep = patternModel->width() * patternModel->bankLength();
        }
        // resize the pattern to the right number of bars (number of steps divided by the pattern's width)
        patternModel->setPatternLength(((lastStep / patternModel->width()) + 1) * patternModel->width());
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
                while (timestamp > ((step + 1) * microsecondsPerStep)) {
                    ++step;
                }
                qDebug() << Q_FUNC_INFO << "Increased step position to match" << double(timestamp) / 1000000.0 << "seconds, now operating on step" << step;
                delay = (timestamp - (step * microsecondsPerStep)) / microsecondsPerSubbeat;
                if (message->noteOffObject) {
                    duration = double(message->noteOffObject->message.getTimeStamp() - timestamp) / microsecondsPerSubbeat;
                    qDebug() << Q_FUNC_INFO << "Found a note off partner, duration is now" << duration << "based on an off note timestamp of" << int(message->noteOffObject->message.getTimeStamp());
                } else {
                    duration = 0;
                }
            } else {
                if (acceptChannel.contains(message->message.getChannel() - 1)) {
                    qDebug() << Q_FUNC_INFO << "Not an on message, skipping";
                } else {
                    qDebug() << Q_FUNC_INFO << "Message channel is not accepted, skipping";
                }
                continue;
            }
            // Actually insert the message's note data into the step
            note = qobject_cast<Note*>(patternModel->playGridManager()->getNote(midiNote, midiChannel));
            row = patternModel->bankOffset() + (step / patternModel->width());
            column = step % patternModel->width();
            subnoteIndex = patternModel->addSubnote(row, column, note);
            ++totalStepEntries;
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
        qDebug() << Q_FUNC_INFO << "Added a total of" << totalStepEntries << "entries to" << step << "steps";
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
