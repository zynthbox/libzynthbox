#pragma once

#include "SyncTimer.h"

class MidiRouterDevice;
class TransportManagerPrivate;
class TransportManager : public QObject {
    Q_OBJECT
public:
    static TransportManager* instance(SyncTimer *q = nullptr) {
        static TransportManager* instance{nullptr};
        if (!instance) {
            instance = new TransportManager(q);
        }
        return instance;
    };
    explicit TransportManager(SyncTimer *parent = nullptr);
    virtual ~TransportManager();

    // This is called by MidiRouter, to ensure we are ready and able to connect to things
    void initialize();

    void restartTransport();

    /**
     * \brief Get the current externally defined BPM
     * @return The BPM as defined by whatever external device is plugged in, or -1 if that device is not available (or none is defined)
     */
    double bpm() const;
    /**
     * \brief The number of midi ticks from the external device since the most recent restart
     * This will simply remain at 0 if there is no external device
     * @return The number of midi ticks from the external device since the most recent timer restart
     */
    uint32_t midiTicksSinceLastRestart() const;
    /**
     * \brief The jack frame that the most recent midi tick arrived on (that is, the tick at the same count as midiTicksSinceLastRestart will return)
     * @return The jack frame the most recent MIDI Clock event arrived on, in absolute numbers
     */
    uint32_t mostRecentTickJackFrame() const;
    /**
     * \brief The jack frame that we most recently received a start or continue command on (or 0 if we never received one)
     * @return The jack frame on which we most recently received a MIDI Start event on, in absolute numbers
     */
    uint32_t jackFrameForLastRestart() const;
    /**
     * \brief The SyncTimer tick we most recently received a MIDI Clock event for
     * @return The SyncTimer tick we most recently received a MIDI Clock event for, counted since the most recent restart
     */
    quint64 mostRecentlyClockedSyncTimerTick() const;
    /**
     * \brief This is the jack frame for the most recent SyncTimer tick we've received a clock event for
     * @return The jack frame on which we received a MIDI Clock event, in absolute numbers
     */
    uint32_t jackFrameForLastSyncTimerTick() const;

    /**
     * \brief The pulses per quarter note used by the external device
     * This will be -1 if there is no external device
     */
    int externalPPQN() const;
    /**
     * \brief Called by MidiRouter when a new device is given the task of being the MIDI clock source
     * We will track the device's lifetime internally, so that should it be removed, we will stop using it
     * @param device The device which should be used as clock source (or null to reset to none)
     */
    void setClockSource(MidiRouterDevice* device);
private:
    TransportManagerPrivate *d{nullptr};
};
