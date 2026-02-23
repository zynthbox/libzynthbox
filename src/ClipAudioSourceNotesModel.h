/*
 * Copyright (C) 2026 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#ifndef CLIPAUDIOSOURCENOTESMODEL_H
#define CLIPAUDIOSOURCENOTESMODEL_H

#include <QAbstractListModel>

class ClipAudioSourceNotesModelPrivate;
/**
 * \brief A model containing an ordered list of all possible midi notes, each entry with a list of any associated ClipAudioSourceSliceSettings instance associated in the attached ClipAudioSource instances
 */
class ClipAudioSourceNotesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantList clipIds READ clipIds WRITE setClipIds NOTIFY clipIdsChanged)
public:
    explicit ClipAudioSourceNotesModel(QObject *parent = nullptr);
    ~ClipAudioSourceNotesModel() override;

    enum Roles {
        NoteRole = Qt::UserRole + 1,
        HasClipsRole,
        ClipsRole,
        SlicesRole,
    };
    QVariantMap roles() const;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const override;

    QVariantList clipIds() const;
    void setClipIds(const QVariantList &newValue);
    Q_SIGNAL void clipIdsChanged();
private:
    ClipAudioSourceNotesModelPrivate *d{nullptr};
};

#endif
