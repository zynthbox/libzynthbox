#pragma once

#include "SndFileInfo.h"
#include "SndLibrary.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QList>


class SndLibrary;


class SndLibraryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SndLibraryModel(SndLibrary *sndLibrary);

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

signals:
    /**
     * @brief categoryFilesCountChanged Emitted when files get added/removed for a specific category
     * @param category Category whose file count changed
     * @param origin Origin of the category whose file count changed
     * @param count Number of files in category
     */
    void categoryFilesCountChanged(QString category, QString origin, int count);

private:
    QList<SndFileInfo*> m_sounds;
    SndLibrary *m_sndLibrary{nullptr};
};
