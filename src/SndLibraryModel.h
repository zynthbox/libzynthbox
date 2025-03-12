#pragma once

#include "SndFileInfo.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QList>


class SndLibraryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SndLibraryModel(QObject *parent);

    enum Roles {
        NameRole = Qt::UserRole + 1,
        OriginRole,
        CategoryRole,
        SoundRole
    };

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    /**
     * @brief Re-read the statistics file and re-populate the sounds model
     */
    Q_INVOKABLE void refresh();
    /**
     * @brief addSndFileInfo Add a snd file info to model
     * @param sound SndFileInfo instance that will be inserted into the model
     * @return Returns if the action was successful
     */
    Q_INVOKABLE bool addSndFileInfo(SndFileInfo *sound);
    /**
     * @brief removeSndFileInfo Remove a snd file info from model
     * @param sound SndFileInfo instance that will be removed from the model
     * @return Returns if the action was successful
     */
    Q_INVOKABLE bool removeSndFileInfo(SndFileInfo *sound);
private:
    QList<SndFileInfo*> m_sounds;
};
