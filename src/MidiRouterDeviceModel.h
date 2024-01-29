/*
    ==============================================================================

    MidiRouterDeviceModel
    Created: 29 Jan 2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <memory>

class MidiRouterDevice;
class MidiRouterDeviceModelPrivate;
/**
 * \brief A model which contains all the devices known by the MidiRouter
 * Get the global instance from the MidiRouter::model property
 */
class MidiRouterDeviceModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit MidiRouterDeviceModel(QObject *parent = nullptr);
    ~MidiRouterDeviceModel() override;

    enum Roles {
        HumanNameRole = Qt::UserRole + 1,
        ZynthianIdRole,
        HardwareIdRole,
        IsHardwareDeviceRole,
        HasInputRole,
    };
    Q_ENUM(Roles)
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void addDevice(MidiRouterDevice* device);
    void removeDevice(MidiRouterDevice* device);
private:
    std::unique_ptr<MidiRouterDeviceModelPrivate> d;
};
