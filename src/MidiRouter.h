#pragma once

#include "JackPassthrough.h"
#include "ZynthboxBasics.h"

#include <QObject>
#include <QCoreApplication>
#include <QThread>

class MidiRouterPrivate;
class MidiRouterDevice;
/**
 * \brief System for routing midi messages from one jack input port (ZLRouter:MidiIn) to a set of output ports (ZLRouter::Channel0 through 15) based on their input channel settings
 *
 * By default everything will be routed to ZynMidiRouter without changing the event's channel.
 * To route anywhere else, use  the function setChannelDestination() to set up your redirections.
 * Note that setting the external channel will only affect channels set to ExternalDestination.
 *
 * To ensure that Zynthian targets are correct, use setZynthianChannels() to change from the
 * default (that is, targeting the same channel in Zynthian as the channel's input channel)
 *
 * There is also the passthrough jack port (called PassthroughOut), which can be used to listen to all accepted midi messages for any destination other than NoDestination
 */
class MidiRouter : public QThread
{
    Q_OBJECT
    /**
     * \brief The sketchpadTrack (0 through 9) which is currently active
     * Used for routing the various inputs to their appropriate output
     * @default 0
     */
    Q_PROPERTY(int currentSketchpadTrack READ currentSketchpadTrack WRITE setCurrentSketchpadTrack NOTIFY currentSketchpadTrackChanged)
    /**
     * \brief The sketchpad track which will receive all midi events from the current sketchpad track
     */
    Q_PROPERTY(ZynthboxBasics::Track currentSketchpadTrackTargetTrack READ currentSketchpadTrackTargetTrack WRITE setCurrentSketchpadTrackTargetTrack NOTIFY currentSketchpadTrackTargetTracksChanged)
    /**
     * \brief The destination tracks for all sketchpad tracks, in order
     * @see currentSketchpadTrackTargetTrack
     */
    Q_PROPERTY(QVariantList sketchpadTrackTargetTracks READ sketchpadTrackTargetTracks NOTIFY sketchpadTrackTargetTracksChanged)
    /**
     * \brief The master channels as defined by the MPE split point (16 entries, one per channel)
     * @note if the split is all-Upper zone, this will be what is set in webconf, otherwise it will be 0 or 15, depending on the Zone
     * @see setExpressiveSplitPoint(int)
     */
    Q_PROPERTY(QList<int> masterChannels READ masterChannels NOTIFY masterChannelsChanged)
    /**
     * \brief The master channel set by in webconf (that is, our internal-use master channel)
     */
    Q_PROPERTY(int masterChannel READ masterChannel NOTIFY masterChannelsChanged)
    /**
     * \brief The amount of the allotted time taken up by the processing loop from 0 through 1
     *
     * This is as reported by Jack and updated regularly
     */
    Q_PROPERTY(float processingLoad READ processingLoad NOTIFY processingLoadChanged)
    /**
     * \brief A model which contains all the devices known to MidiRouter
     * Use a FilterProxyModel to filter out any devices you don't want
     */
    Q_PROPERTY(QObject* model READ model NOTIFY modelChanged)
public:
    static MidiRouter* instance() {
        static MidiRouter* instance{nullptr};
        if (!instance) {
            instance = new MidiRouter(qApp);
        }
        return instance;
    };
    explicit MidiRouter(QObject *parent = nullptr);
    virtual ~MidiRouter();

    void run() override;
    Q_SLOT void markAsDone();

    enum RoutingDestination {
        NoDestination = 0, // Don't route any events on this channel (including to the passthrough port)
        ZynthianDestination = 1, // Route all events to Zynthian
        ExternalDestination = 2, // Route all events to the enabled external ports
        SamplerDestination = 3, // Route all events only to passthrough (which is then handled elsewhere for distribution to the sampler)
    };
    Q_ENUM(RoutingDestination)
    /**
     * \brief Where notes on a specific sketchpad track should be routed
     * @param sketchpadTrack The sketchpad track (from 0 through 9)
     * @param destination Where the channel's notes should go (the default for all channels is ZynthianDestination)
     * @param externalChannel If set, messages on the given channel will be translated to end up on this channel instead
     */
    void setSkechpadTrackDestination(int sketchpadTrack, RoutingDestination destination, int externalChannel = -1);

    /**
     * \brief The track which should receive midi events on the given track
     * @param sketchpadTrack The track you wish to retrieve the target track for
     * @return The target track for the given sketchpad track
     */
    ZynthboxBasics::Track sketchpadTrackTargetTrack(const ZynthboxBasics::Track &sketchpadTrack) const;
    /**
     * \brief Set the track which should receive the midi events on the given track
     * @param sketchpadTrack The track that you wish to set the target track for (AnyTrack and NoTrack are not valid and will result in the function being ignored)
     * @param targetTrack The track that should receive all midi events from the given track (AnyTrack is not valid and will result in the function being ignored)
     */
    Q_INVOKABLE void setSketchpadTrackTargetTrack(const ZynthboxBasics::Track &sketchpadTrack, const ZynthboxBasics::Track &targetTrack);
    QVariantList sketchpadTrackTargetTracks() const;
    Q_SIGNAL void sketchpadTrackTargetTracksChanged();
    ZynthboxBasics::Track currentSketchpadTrackTargetTrack() const;
    // Same exclusion rules as setSketchpadTrackTargetTrack (AnyTrack is not valid and will be ignored)
    Q_INVOKABLE void setCurrentSketchpadTrackTargetTrack(const ZynthboxBasics::Track &targetTrack);
    Q_SIGNAL void currentSketchpadTrackTargetTracksChanged();

    void setCurrentSketchpadTrack(int currentSketchpadTrack);
    int currentSketchpadTrack() const;
    Q_SIGNAL void currentSketchpadTrackChanged();

    /**
     * \brief Set the channels which will be used to map events for the given sketchpad track into zynthian
     * @param sketchpadTrack The sketchpad track (0 through 9)
     * @param zynthianChannels The channels that zynthian should play notes on for the channel with the given input channel
     */
    void setZynthianChannels(int sketchpadTrack, const QList<int> &zynthianChannels);
    /**
     * \brief Set the midi channels accepted by the Zynthian synth at the given index
     * @param zynthianChannel The index of the Zynthian synth to set accepted midi channels for
     * @param acceptedMidiChannels A list of all channels accepted by the given synth
     */
    void setZynthianSynthAcceptedChannels(int zynthianChannel, const QList<int> &acceptedMidiChannels);

    /**
     * \brief Set the keyzone information for the Zynthian synth at the given index
     * @param zynthianChannel The index of the Zynthian synth to set up the keyzone for
     * @param keyZoneStart The first accepted note for the given synth
     * @param keyZoneEnd The last accepted note for the given synth
     * @param rootNote The note that should be considered C4 by this synth (that is, how much notes will be transposed by, compared to the default value of 60)
     */
    Q_INVOKABLE void setZynthianSynthKeyzones(int zynthianChannel, int keyZoneStart = 0, int keyZoneEnd = 127, int rootNote = 60);

    /**
     * \brief Call this function to reload the midi routing configuration and set ports back up
     */
    Q_SLOT void reloadConfiguration();

    /**
     * \brief Set the point at which the MPE zones are split
     * Conceptually, this is the upper limit for the Lower zone. Consequently, you can perform any normal setup using this:
     * To use an all-Lower zone setup, set the split point to 15 (that is, give all channels to the Lower zone)
     * To use an all-Upper zone setup, set the split point to -1 (that is, give all channels to the Upper zone)
     * To use a "standard" split, set the split point to 7 (giving channels 0 through 7 to the Lower zone, and channels 8 through 15 to the Upper zone)
     * To use a mono-channel setup for the Lower zone, set the split point to 1 (giving the Lower zone channels 0 and 1, and 2 through 15 to the Upper zone)
     * @note If you set the split point to 14, you will have only given the Upper zone a master channel (which isn't very useful). So, probably don't do that. Notes will still be sent out for the Upper zone, but on the master channel, which is not very MPE
     */
    void setExpressiveSplitPoint(const int &splitPoint);
    const int &expressiveSplitPoint() const;
    Q_SIGNAL void expressiveSplitPointChanged();
    const QList<int> &masterChannels() const;
    const int &masterChannel() const;
    Q_SIGNAL void masterChannelsChanged();

    float processingLoad() const;
    Q_SIGNAL void processingLoadChanged();

    QObject *model() const;
    Q_SIGNAL void modelChanged();

    Q_INVOKABLE MidiRouterDevice *getSketchpadTrackControllerDevice(const ZynthboxBasics::Track &track) const;

    Q_SIGNAL void addedHardwareDevice(const QString &deviceId, const QString &humanReadableName);
    Q_SIGNAL void removedHardwareDevice(const QString &deviceId, const QString &humanReadableName);

    enum ListenerPort {
        UnknownPort = -1,
        PassthroughPort = 0,
        InternalPassthroughPort = 1,
        InternalControllerPassthroughPort = 2,
        HardwareInPassthroughPort = 3,
        ExternalOutPort = 4,
    };
    Q_ENUM( ListenerPort )
// Ouch not cool hack: https://forum.qt.io/topic/130255/shiboken-signals-don-t-work
// Core message (by vberlier): Turns out Shiboken shouldn't do anything for signals and let PySide setup the signals using the MOC data. Shiboken generates bindings for signals as if they were plain methods and shadows the actual signals.
#ifndef PYSIDE_BINDINGS_H
    /**
     * \brief Fired whenever a note has changed
     * @param port The listener port the message arrived on (you will likely want to filter on just managing PassthroughPort, unless you have a specific reason)
     * @param midiNote The note value for the note which has changed
     * @param midiChannel The channel of the note which changed
     * @param velocity The velocity value for the changed note
     * @param setOn Whether the message was an on message (false if it was an off message)
     * @param timestamp The timestamp at which the message arrived, counted in jack frames (absolute value since startup)
     * @param byte1 The first byte of the message
     * @param byte2 The second byte of the message
     * @param byte3 The third byte of the message
     * @param sketchpadTrack The sketchpad track the message arrived on
     * @param hardwareDeviceId The device ID of the hardware device the event arrived on (this will only be valid if the event in fact arrived from a hardware device)
     */
    Q_SIGNAL void noteChanged( MidiRouter::ListenerPort port, int midiNote, int midiChannel, int velocity, bool setOn, quint64 timestamp, const unsigned char &byte1, const unsigned char &byte2, const unsigned char &byte3, const int &sketchpadTrack, const QString &hardwareDeviceId);
    /**
     * \brief Fired whenever any midi message arrives
     * @param port The listener port that the message arrived on (you will likely want to filter on just managing PassthroughPort, unless you have a specific reason)
     * @param size How many bytes the message contains (either 1, 2, or 3)
     * @param byte1 The first byte (always valid)
     * @param byte2 The second byte (usually valid, but always test size before using)
     * @param byte3 The third byte (commonly valid, but always test size before using)
     * @param sketchpadTrack The sketchpad track the message arrived on
     * @param fromInternal Whether the message arrived from an internal source
     * @param hardwareDeviceId The device ID of the hardware device the event arrived on (this will only be valid if the event in fact arrived from a hardware device)
     */
    Q_SIGNAL void midiMessage(int port, int size, const unsigned char &byte1, const unsigned char &byte2, const unsigned char& byte3, const int &sketchpadTrack, bool fromInternal, const QString &hardwareDeviceId);
    /**
     * \brief Fired whenever a cuia command is requested by the MidiRouter (nominally done by way of setting up MidiRouterFilter rules)
     * @param cuiaCommand The command that should be called
     * @param originId The ID of the MidiRouter device which requested that this event be fired
     * @param track The sketchpad track this applies to (where relevant)
     * @param slot The sketchpad slot this applies to (where relevant)
     * @param value The value associated with this command (where relevant) - will be an integer between 0 and 127 inclusive (a midi byte value)
     */
    Q_SIGNAL void cuiaEvent(const QString &cuiaCommand, const int &originId, const ZynthboxBasics::Track &track, const ZynthboxBasics::Slot &slot, const int &value);
#endif
    /**
     * \brief Convenience function to enqueue a cuia command for processing
     * This function is useful in cases where attempting to perform cuia
     * commands from an unsafe thread.
     * The given command will be returned by the cuiaEvent signal above,
     * sent out by an arbitrary device
     * @note If the given command is not known by CUIAHelper, it will be ignored
     * @param cuiaCommand The cuia command which should be added to the queue
     */
    Q_SLOT void enqueueCuiaCommand(const QString &cuiaCommand);
    /**
     * \brief Called from the Ui whenever a cuia command has been consumed
     * @param cuiaCommand The command which was fired
     * @param originId The ID of the MidiRouter device which requested the event be fired (this will be -1 for things which were not done by an external device)
     * @param track The sketchpad track this applies to (where relevant)
     * @param slot The sketchpad slot this applies to (where relevant)
     * @param value The value associated with this command (where relevant) - this will be an integer between 0 and 127 inclusive (a midi byte value)
     */
    Q_SLOT void cuiaEventFeedback(const QString &cuiaCommand, const int &originId, const ZynthboxBasics::Track &track, const ZynthboxBasics::Slot &slot, const int &value);
private:
    MidiRouterPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(MidiRouter::ListenerPort)
