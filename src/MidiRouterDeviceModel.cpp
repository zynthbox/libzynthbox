/*
    ==============================================================================

    MidiRouterDeviceModel
    Created: 29 Jan 2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "MidiRouterDeviceModel.h"
#include "MidiRouterDevice.h"

class MidiRouterDeviceModelPrivate {
public:
    MidiRouterDeviceModelPrivate(MidiRouterDeviceModel *q)
        : q(q)
    {}
    ~MidiRouterDeviceModelPrivate() {}
    MidiRouterDeviceModel *q{nullptr};
    QList<MidiRouterDevice*> devices;

    void deviceDataChanged(MidiRouterDevice *device, MidiRouterDeviceModel::Roles role) {
        const int deviceIndex = devices.indexOf(device);
        const QModelIndex modelIndex = q->index(deviceIndex);
        q->dataChanged(modelIndex, modelIndex, QVector<int>{role});
    }
};

MidiRouterDeviceModel::MidiRouterDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
    , d(new MidiRouterDeviceModelPrivate(this))
{
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
                result.setValue<bool>(device->hardwareId().length() > 0);
                break;
            case HasInputRole:
                result.setValue<bool>(device->inputPortName().length() > 0);
                break;
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
    connect(device, &MidiRouterDevice::hardwareIdChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::IsHardwareDeviceRole); });
    connect(device, &MidiRouterDevice::inputPortNameChanged, this, [this, device](){ d->deviceDataChanged(device, MidiRouterDeviceModel::HasInputRole); });
    endInsertRows();
}

void MidiRouterDeviceModel::removeDevice(MidiRouterDevice* device)
{
    for (int deviceIndex = 0; deviceIndex < d->devices.count(); ++deviceIndex) {
        MidiRouterDevice *existingDevice = d->devices.at(deviceIndex);
        if (device == existingDevice) {
            beginRemoveRows(QModelIndex(), deviceIndex, deviceIndex);
            d->devices.removeAt(deviceIndex);
            endRemoveRows();
            break;
        }
    }
}
