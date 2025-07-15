/*
    ==============================================================================

    MidiRouterDeviceModel
    Created: 29 Jan 2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "MidiRouterDeviceModel.h"
#include "MidiRouterDevice.h"

#include "ZynthboxBasics.h"

class MidiRouterDeviceModelPrivate {
public:
    MidiRouterDeviceModelPrivate(MidiRouterDeviceModel *q)
        : q(q)
    {
        audioInSources << QVariantMap{ {"text", "Standard Routing - Left Channel"}, {"value", "standard-routing:left"} };
        audioInSources << QVariantMap{ {"text", "Standard Routing - Right Channel"}, {"value", "standard-routing:right"} };
        audioInSources << QVariantMap{ {"text", "Standard Routing - Both Channels"}, {"value", "standard-routing:both"} };
        audioInSources << QVariantMap{ {"text", "No Audio Input"}, {"value", "no-input"} };
        audioInSources << QVariantMap{ {"text", "Audio In - Left Channel"}, {"value", "external:left"} };
        audioInSources << QVariantMap{ {"text", "Audio In - Right Channel"}, {"value", "external:right"} };
        audioInSources << QVariantMap{ {"text", "Audio In - Both Channels"}, {"value", "external:both"} };
        audioInSources << QVariantMap{ {"text", "USB Audio In - Left Channel"}, {"value", "usb-gadget:left"} };
        audioInSources << QVariantMap{ {"text", "USB Audio In - Right Channel"}, {"value", "usb-gadget:right"} };
        audioInSources << QVariantMap{ {"text", "USB Audio In - Both Channels"}, {"value", "usb-gadget:both"} };
        audioInSources << QVariantMap{ {"text", "Master Output - Left Channel"}, {"value", "internal-master:left"} };
        audioInSources << QVariantMap{ {"text", "Master Output - Right Channel"}, {"value", "internal-master:right"} };
        audioInSources << QVariantMap{ {"text", "Master Output - Both Channels"}, {"value", "internal-master:both"} };
        static const QStringList clients{"synthSlot", "sampleSlot", "fxSlot"};
        static const QStringList clientNames{"Synth", "Sample", "FX"};
        static const QList<QStringList> entries = {
            QStringList{"dry0", "dry1", "dry2", "dry3", "dry4"},
            QStringList{"dry0", "dry1", "dry2", "dry3", "dry4"},
            QStringList{"dry0", "wet0", "dry1", "wet1", "dry2", "wet2", "dry3", "wet3", "dry4", "wet4"}
        };
        static const QList<QStringList> entryNames = {
            QStringList{"1", "2", "3", "4", "5"},
            QStringList{"1", "2", "3", "4", "5"},
            QStringList{"1 (Dry)", "1 (Wet)", "2 (Dry)", "2 (Wet)", "3 (Dry)", "3 (Wet)", "4 (Dry)", "4 (Wet)", "5 (Dry)", "5 (Wet)"}
        };
        static const QStringList channels{"left", "right", "both"};
        static const QStringList channelNames{"Left Channel", "Right Channel", "Both Channels"};
        for (int clientIndex = 0; clientIndex < clients.length(); ++clientIndex) {
            for (int trackIndex = -1; trackIndex < ZynthboxTrackCount; ++trackIndex) {
                for (int entryIndex = 0; entryIndex < entries[clientIndex].length(); ++entryIndex) {
                    for (int channelIndex = 0; channelIndex < channels.length(); ++channelIndex) {
                        audioInSources << QVariantMap{
                            {"text", QString("Track %1 %2 %3 - %4").arg(trackIndex == -1 ? "Same" : QString::number(trackIndex + 1)).arg(clientNames[clientIndex]).arg(entryNames[clientIndex][entryIndex]).arg(channelNames[channelIndex])},
                            {"value", QString("%1:%2:%3:%4").arg(clients[clientIndex]).arg(trackIndex == -1 ? "same" : QString::number(trackIndex)).arg(entries[clientIndex][entryIndex]).arg(channels[channelIndex])}
                        };
                    }
                }
            }
        }

        midiInSources << QVariantMap{ {"text", "Current Track"}, {"value", "sketchpadTrack:-1"}, {"device", QVariant::fromValue<QObject*>(nullptr)} }; // -1 is the internal shorthand used for the current track basically everywhere
        midiInSources << QVariantMap{ {"text", "Track 1"}, {"value", "sketchpadTrack:0"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 2"}, {"value", "sketchpadTrack:1"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 3"}, {"value", "sketchpadTrack:2"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 4"}, {"value", "sketchpadTrack:3"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 5"}, {"value", "sketchpadTrack:4"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 6"}, {"value", "sketchpadTrack:5"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 7"}, {"value", "sketchpadTrack:6"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 8"}, {"value", "sketchpadTrack:7"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 9"}, {"value", "sketchpadTrack:8"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "Track 10"}, {"value", "sketchpadTrack:9"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
        midiInSources << QVariantMap{ {"text", "No Midi Input"}, {"value", "no-input"}, {"device", QVariant::fromValue<QObject*>(nullptr)} };
    }
    ~MidiRouterDeviceModelPrivate() {}
    MidiRouterDeviceModel *q{nullptr};
    MidiRouter *qq{nullptr};
    jack_client_t *jackClient{nullptr};
    QList<MidiRouterDevice*> devices;

    QVariantList audioInSources;
    QVariantList midiInSources;
    QVariantList midiOutSources;

    void deviceDataChanged(MidiRouterDevice *device, MidiRouterDeviceModel::Roles role) {
        const int deviceIndex = devices.indexOf(device);
        const QModelIndex modelIndex = q->index(deviceIndex);
        q->dataChanged(modelIndex, modelIndex, QVector<int>{role});
    }
};

MidiRouterDeviceModel::MidiRouterDeviceModel(jack_client_t *jackClient, MidiRouter *parent)
    : QAbstractListModel(parent)
    , d(new MidiRouterDeviceModelPrivate(this))
{
    d->qq = parent;
    d->jackClient = jackClient;
}

MidiRouterDeviceModel::~MidiRouterDeviceModel() = default;

QHash<int, QByteArray> MidiRouterDeviceModel::roleNames() const
{
    static const QHash<int, QByteArray>roles{
        {HumanNameRole, "humanName"},
        {ZynthianIdRole, "zynthianId"},
        {HardwareIdRole, "hardwareIdRoles"},
        {IsHardwareDeviceRole, "isHardwareDevice"},
        {HasInputRole, "hasInput"},
        {HasOutputRole, "hasOutput"},
        {DeviceObjectRole, "deviceObject"},
        {VisibleRole, "visible"},
    };
    return roles;
}

int MidiRouterDeviceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return d->devices.count();
}

QVariant MidiRouterDeviceModel::data(const QModelIndex& index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        MidiRouterDevice *device = d->devices.at(index.row());
        switch(role) {
            case HumanNameRole:
                result.setValue<QString>(device->humanReadableName());
                break;
            case ZynthianIdRole:
                result.setValue<QString>(device->zynthianId());
                break;
            case HardwareIdRole:
                result.setValue<QString>(device->hardwareId());
                break;
            case IsHardwareDeviceRole:
                result.setValue<bool>(device->deviceType(MidiRouterDevice::HardwareDeviceType));
                break;
            case HasInputRole:
                result.setValue<bool>(device->inputPortName().length() > 0);
                break;
            case HasOutputRole:
                result.setValue<bool>(device->outputPortName().length() > 0);
                break;
            case DeviceObjectRole:
                result.setValue<QObject*>(device);
                break;
            case VisibleRole:
                result.setValue<bool>(device->visible());
            default:
                break;
        }
    }
    return result;
}

void MidiRouterDeviceModel::addDevice(MidiRouterDevice* device)
{
    const int newRow = d->devices.count();
    beginInsertRows(QModelIndex(), newRow, newRow);
    d->devices.append(device);
    connect(device, &MidiRouterDevice::humanReadableNameChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::HumanNameRole); });
    connect(device, &MidiRouterDevice::zynthianIdChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::ZynthianIdRole); });
    connect(device, &MidiRouterDevice::hardwareIdChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::HardwareIdRole); });
    connect(device, &MidiRouterDevice::inputPortNameChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::HasInputRole); });
    connect(device, &MidiRouterDevice::visibleChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::VisibleRole); });
    endInsertRows();
    if (device->deviceType(MidiRouterDevice::HardwareDeviceType)) {
        d->midiInSources << QVariantMap{
            {"text", device->humanReadableName()},
            {"value", "external:" + device->hardwareId()},
            {"device", QVariant::fromValue<QObject*>(device)}
        };
        QMetaObject::invokeMethod(this, "midiInSourcesChanged", Qt::QueuedConnection);
        d->midiOutSources << QVariantMap{
            {"text", device->humanReadableName()},
            {"value", "external:" + device->hardwareId()},
            {"device", QVariant::fromValue<QObject*>(device)}
        };
        QMetaObject::invokeMethod(this, "midiOutSourcesChanged", Qt::QueuedConnection);
        connect(device, &MidiRouterDevice::humanReadableNameChanged, this, [this, device](){
            for (int index = 0; index < d->midiInSources.length(); ++index) {
                if (d->midiInSources[index].toMap()["device"].value<QObject*>() == device) {
                    auto theSource = d->midiInSources.takeAt(index).toMap();
                    theSource["text"] = device->humanReadableName();
                    d->midiInSources << theSource;
                    Q_EMIT midiInSourcesChanged();
                }
            }
            for (int index = 0; index < d->midiOutSources.length(); ++index) {
                if (d->midiOutSources[index].toMap()["device"].value<QObject*>() == device) {
                    auto theSource = d->midiOutSources.takeAt(index).toMap();
                    theSource["text"] = device->humanReadableName();
                    d->midiOutSources << theSource;
                    Q_EMIT midiOutSourcesChanged();
                }
            }
        }, Qt::QueuedConnection);
    }
}

void MidiRouterDeviceModel::removeDevice(MidiRouterDevice* device)
{
    for (int deviceIndex = 0; deviceIndex < d->devices.count(); ++deviceIndex) {
        MidiRouterDevice *existingDevice = d->devices.at(deviceIndex);
        if (device == existingDevice) {
            beginRemoveRows(QModelIndex(), deviceIndex, deviceIndex);
            d->devices.removeAt(deviceIndex);
            device->disconnect(this);
            endRemoveRows();
            for (int index = 0; index < d->midiInSources.length(); ++index) {
                if (d->midiInSources[index].toMap()["device"].value<QObject*>() == device) {
                    d->midiInSources.removeAt(index);
                    QMetaObject::invokeMethod(this, "midiInSourcesChanged", Qt::QueuedConnection);
                }
            }
            for (int index = 0; index < d->midiOutSources.length(); ++index) {
                if (d->midiOutSources[index].toMap()["device"].value<QObject*>() == device) {
                    d->midiOutSources.removeAt(index);
                    QMetaObject::invokeMethod(this, "midiOutSourcesChanged", Qt::QueuedConnection);
                }
            }
            break;
        }
    }
}

MidiRouterDevice * MidiRouterDeviceModel::getDevice(const QString& hardwareId) const
{
    MidiRouterDevice* device{nullptr};
    for (MidiRouterDevice *needle : qAsConst(d->devices)) {
        if (needle->hardwareId() == hardwareId) {
            device = needle;
            break;
        }
    }
    return device;
}

QVariantList MidiRouterDeviceModel::audioInSources() const
{
    return d->audioInSources;
}

int MidiRouterDeviceModel::audioInSourceIndex(const QString& value) const
{
    for (int index = 0; index < d->audioInSources.length(); ++index) {
        QVariantMap element = d->audioInSources[index].toMap();
        if (element["value"] == value) {
            return index;
        }
    }
    return -1;
}

QStringList MidiRouterDeviceModel::audioInSourceToJackPortNames(const QString& value, const QStringList &standardRouting, const ZynthboxBasics::Track &selfTrack) const
{
    QStringList jackPortNames;
    if (value.startsWith("standard-routing:")) {
        // Standard routing is whatever we're told it is
        jackPortNames = standardRouting;
    } else if (value == "no-input") {
        // No input means just don't have anything connected
    } else if (value.startsWith("external:")) {
        // Use the system/mic input
        const char **ports = jack_get_ports(d->jackClient, "system", JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical);
        QStringList physicalPorts;
        if (ports == nullptr) {
            // No ports found, this is going to be something of a problem...
        } else {
            for (const char **p = ports; *p; p++) {
                physicalPorts << QString::fromLocal8Bit(*p);
            }
            if (value.endsWith(":left")) {
                jackPortNames << physicalPorts[0];
            } else if (value.endsWith(":right")) {
                jackPortNames << physicalPorts[1];
            } else {
                jackPortNames = physicalPorts;
            }
        }
        free(ports);
    } else if (value.startsWith("internal-master:")) {
        if (value.endsWith(":left") || value.endsWith(":both")) {
            jackPortNames << "GlobalPlayback:dryOurLeft";
        }
        if (value.endsWith(":right") || value.endsWith(":both")) {
            jackPortNames << "GlobalPlayback:dryOurRight";
        }
    } else if (value.startsWith("synthSlot:") || value.startsWith("sampleSlot:") || value.startsWith("fxSlot:")) {
        const QStringList splitData = value.split(":");
        QString portRootName, dryOrWet;
        int theLane = QString(splitData[2].right(1)).toInt() + 1;
        int theTrack{0};
        if (splitData[1] == "same") {
            theTrack = selfTrack;
        } else {
            theTrack = QString(splitData[1]).toInt();
        }
        if (theTrack == ZynthboxBasics::NoTrack) {
            // If we've defined no track as the source, we've no ports to read from... simply pass back the empty list
        } else {
            ++theTrack;
            if (splitData[0] == "sampleSlot") {
                portRootName = QString("SamplerSynth:channel_%2-lane%1").arg(theLane).arg(theTrack);
            } else if (splitData[0] == "synthSlot") {
                portRootName = QString("TrackPassthrough:Channel%2-lane%1").arg(theLane).arg(theTrack);
            } else if (splitData[0] == "fxSlot") {
                portRootName = QString("FXPassthrough-lane%1:Channel%2").arg(theLane).arg(theTrack);
            }
            if (splitData[0] == "sampleSlot") {
                // sample slot outputs are named differently to the passthrough outputs, and don't have dry/wet prefixes
                if (splitData[3] == "left" || splitData[3] == "both") {
                    jackPortNames << QString("%1-left").arg(portRootName);
                }
                if (splitData[3] == "right" || splitData[3] == "both") {
                    jackPortNames << QString("%1-right").arg(portRootName);
                }
            } else {
                    if (splitData[2].startsWith("dry")) {
                    dryOrWet = "dryOut";
                } else if (splitData[2].startsWith("wet")) {
                    dryOrWet = "wetOutFx1";
                }
                if (splitData[3] == "left" || splitData[3] == "both") {
                    jackPortNames << QString("%1-%2Left").arg(portRootName).arg(dryOrWet);
                }
                if (splitData[3] == "right" || splitData[3] == "both") {
                    jackPortNames << QString("%1-%2Right").arg(portRootName).arg(dryOrWet);
                }
            }
        }
    }
    return jackPortNames;
}

QVariantList MidiRouterDeviceModel::midiInSources() const
{
    return d->midiInSources;
}

int MidiRouterDeviceModel::midiInSourceIndex(const QString& value) const
{
    for (int index = 0; index < d->midiInSources.length(); ++index) {
        QVariantMap element = d->midiInSources[index].toMap();
        if (element["value"] == value) {
            return index;
        }
    }
    return -1;
}

QVariantList MidiRouterDeviceModel::midiOutSources() const
{
    return d->midiOutSources;
}

int MidiRouterDeviceModel::midiOutSourceIndex(const QString& value) const
{
    for (int index = 0; index < d->midiOutSources.length(); ++index) {
        QVariantMap element = d->midiOutSources[index].toMap();
        if (element["value"] == value) {
            return index;
        }
    }
    return -1;
}
