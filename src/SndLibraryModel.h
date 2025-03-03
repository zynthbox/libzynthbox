#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QObject>

class SndLibraryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SndLibraryModel(QObject *parent);

    enum Roles {
        IdRole = Qt::UserRole + 1,
    };

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};
