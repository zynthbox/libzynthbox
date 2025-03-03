#include "SndLibraryModel.h"

SndLibraryModel::SndLibraryModel(QObject *parent) : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> SndLibraryModel::roleNames() const
{
    static const QHash<int, QByteArray> roleNames{
        {IdRole, "id"},
    };
    return roleNames;
}

int SndLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 0;
}

QVariant SndLibraryModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        switch (role) {
            default:
                break;
        }
    }
    return result;
}
