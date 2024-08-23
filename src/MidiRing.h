#pragma once

#include "JUCEHeaders.h"
#include <QDebug>

#define MidiRingSize 512
class MidiRing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        bool processed{true};
        juce::MidiBuffer buffer;
    };
    explicit MidiRing() {
        Entry* entryPrevious{&ringData[MidiRingSize - 1]};
        for (quint64 i = 0; i < MidiRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~MidiRing() {
    }

    void write(const juce::MidiBuffer &buffer) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location: midi buffer with" << entry->buffer.getNumEvents() << "events. This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->buffer = buffer;
        entry->processed = false;
    }
    // This ring does not have a read-and-clear function, as it is likely to be called from the jack process loop and we want to avoid that doing memory type things
    void markAsRead() {
        Entry *entry = readHead;
        readHead = readHead->next;
        entry->processed = true;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[MidiRingSize];
};
