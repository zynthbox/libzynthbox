#pragma once

#include "JackPassthrough.h"
#include <QObject>
#include <QCoreApplication>
#include <QThread>

class MidiRouterPrivate;
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

    Q_SIGNAL void addedHardwareDevice(const QString &deviceId, const QString &humanReadableName);
    Q_SIGNAL void removedHardwareDevice(const QString &deviceId, const QString &humanReadableName);

    enum ListenerPort {
        UnknownPort = -1,
        PassthroughPort = 0,
        InternalPassthroughPort = 1,
        HardwareInPassthroughPort = 2,
        ExternalOutPort = 3,
    };
    Q_ENUM( ListenerPort )
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
     */
    Q_SIGNAL void midiMessage(int port, int size, const unsigned char &byte1, const unsigned char &byte2, const unsigned char& byte3, const int &sketchpadTrack, bool fromInternal);

private:
    MidiRouterPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(MidiRouter::ListenerPort)
