/*
 * Copyright (C) 2025 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#pragma once

#include <QObject>

class SysexMessage;
class SysexIdentityPrivate;
class SysexIdentity : public QObject {
    Q_OBJECT
    /**
     * \brief A human-readable general description of this device, for when you just need a quick label for something to identify it
     */
    Q_PROPERTY(QString description READ description CONSTANT)
    /**
     * \brief The one or three bytes which make up the manufacturer code for the device
     * This will be either a single byte (for old manufacturers like Roland with 0x41), or three bytes (for newer manufacturers)
     * For the newer manufacturers, the three bytes will always have a 0 byte as the first (for example Embodme, with 0x00 0x21 0x50)
     * This will be a list of integers, ranged from 0x00 through 0x7F)
     */
    Q_PROPERTY(QVariantList manufacturerId READ manufacturerId CONSTANT)
    /**
     * \brief This holds a human-readable version of the manufacturer
     * For example, Roland, Embodme, and LG Electronics (Goldstar)...
     */
    Q_PROPERTY(QString manufacturerName READ manufacturerName CONSTANT)
    /**
     * \brief The two bytes which make up the device's family (product group) code
     * Each manufacturer defines their own family code structure
     * This will be a list of integers, ranged from 0x00 through 0x7F)
     */
    Q_PROPERTY(QVariantList familyId READ familyId CONSTANT)
    /**
     * \brief If we know the device, this will contain a human-readable name (otherwise it will be empty)
     */
    Q_PROPERTY(QString familyName READ familyName CONSTANT)
    /**
     * \brief The two bytes which make up the model code for the device
     * This will be a list of integers, ranged from 0x00 through 0x7F)
     */
    Q_PROPERTY(QVariantList modelId READ modelId CONSTANT)
    /**
     * \brief A human-readable version of the name
     */
    Q_PROPERTY(QString modelName READ modelName CONSTANT)
    /**
     * \brief The four bytes which make up the device's version code
     * This will be a list of integers, ranged from 0x00 through 0x7F)
     */
    Q_PROPERTY(QVariantList versionId READ versionId CONSTANT)
    /**
     * \brief A human-readable interpretation of the version code
     */
    Q_PROPERTY(QString versionName READ versionName CONSTANT)
public:
    explicit SysexIdentity(const SysexMessage *identityResponse, QObject *parent = nullptr);
    ~SysexIdentity() override;

    QString description() const;
    QVariantList manufacturerId() const;
    QList<int> manufacturerIdRaw() const;
    QString manufacturerName() const;
    QVariantList familyId() const;
    QList<int> familyIdRaw() const;
    QString familyName() const;
    QVariantList modelId() const;
    QList<int> modelIdRaw() const;
    QString modelName() const;
    QVariantList versionId() const;
    QList<int> versionIdRaw() const;
    QString versionName() const;
private:
    SysexIdentityPrivate *d{nullptr};
};
