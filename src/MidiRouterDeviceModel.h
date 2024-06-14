/*
    ==============================================================================

    MidiRouterDeviceModel
    Created: 29 Jan 2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include "MidiRouter.h"

#include <QAbstractListModel>
#include <QObject>
#include <QVariant>
#include <QSize>
#include <memory>

#include <jack/jack.h>

class MidiRouterDevice;
class MidiRouterDeviceModelPrivate;
/**
 * \brief A model which contains all the devices known by the MidiRouter
 * Get the global instance from the MidiRouter::model property
 */
class MidiRouterDeviceModel : public QAbstractListModel {
    Q_OBJECT
    /**
     * \brief A list of objects containing information about all available audio in ports
     */
    Q_PROPERTY(QVariantList audioInSources READ audioInSources NOTIFY audioInSourcesChanged)
    /**
     * \brief A list of objects containing information about all available midi in ports
     */
    Q_PROPERTY(QVariantList midiInSources READ midiInSources NOTIFY midiInSourcesChanged)
public:
    explicit MidiRouterDeviceModel(jack_client_t *jackClient, MidiRouter *parent = nullptr);
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
    MidiRouterDevice *getDevice(const QString &hardwareId) const;

    QVariantList audioInSources() const;
    Q_INVOKABLE int audioInSourceIndex(const QString &value) const;
    Q_INVOKABLE QStringList audioInSourceToJackPortNames(const QString &value, const QStringList &standardRouting) const;
    Q_SIGNAL void audioInSourcesChanged();
    QVariantList midiInSources() const;
    Q_INVOKABLE int midiInSourceIndex(const QString &value) const;
    Q_SIGNAL void midiInSourcesChanged();
private:
    std::unique_ptr<MidiRouterDeviceModelPrivate> d;
};
