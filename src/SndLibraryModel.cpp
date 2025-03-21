#include "SndLibraryModel.h"

#include <QFile>
#include <QDebug>
#include <QtGlobal>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

/**
 * When DEBUG is set to true it will print a set of logs
 * which is not meant for production builds
 */
#ifndef DEBUG
#define DEBUG true
#endif

SndLibraryModel::SndLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> SndLibraryModel::roleNames() const
{
    static const QHash<int, QByteArray> roleNames{
        {NameRole, "name"},
        {OriginRole, "origin"},
        {CategoryRole, "category"},
        {SoundRole, "sound"},
    };
    return roleNames;
}

int SndLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_sounds.size();
}

QVariant SndLibraryModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        SndFileInfo *sndFileInfo = m_sounds.at(index.row());
        switch (role) {
            case NameRole:
                result.setValue(sndFileInfo->m_name);
                break;
            case OriginRole:
                result.setValue(sndFileInfo->m_origin);
                break;
            case CategoryRole:
                result.setValue(sndFileInfo->m_category);
                break;
            case SoundRole:
                result.setValue(sndFileInfo);
                break;
            default:
                break;
        }
    }
    return result;
}

void SndLibraryModel::refresh()
{
    beginRemoveRows(index(0).parent(), 0, m_sounds.size());
    m_sounds.clear();
    endRemoveRows();

    const QStringList origins = {"my-sounds", "community-sounds"};
    for (auto origin : origins) {
        QFile file("/zynthian/zynthian-my-data/sounds/" + origin + "/.stat.json");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            auto obj = QJsonDocument::fromJson(file.readAll()).object();
            for (auto categoryEntry = obj.constBegin(); categoryEntry != obj.constEnd(); ++categoryEntry) {
                const QString category = categoryEntry.key();
                const int categoryFilesCount = categoryEntry.value().toObject()["count"].toInt();
                const QJsonObject categoryFilesObj = categoryEntry.value().toObject()["files"].toObject();
                beginInsertRows(index(m_sounds.size()).parent(), m_sounds.size(), m_sounds.size() + categoryFilesCount - 1);
                for (auto categorySoundEntry = categoryFilesObj.constBegin(); categorySoundEntry != categoryFilesObj.constEnd(); ++categorySoundEntry) {
                    if(DEBUG) qDebug() << "Reading sound details for" << categorySoundEntry.key();
                    m_sounds.append(new SndFileInfo(
                        categorySoundEntry.key(),
                        origin,
                        category,
                        categorySoundEntry.value().toObject()["synthSlotsData"].toVariant().toStringList(),
                        categorySoundEntry.value().toObject()["sampleSlotsData"].toVariant().toStringList(),
                        categorySoundEntry.value().toObject()["fxSlotsData"].toVariant().toStringList(),
                        this
                    ));
                }
                endInsertRows();
                Q_EMIT categoryFilesCountChanged(category, origin, categoryFilesCount);
            }
        } else {
            qCritical() << "Cannot open statistics file" << file.fileName();
        }
    }
}

bool SndLibraryModel::addSndFileInfo(SndFileInfo *sound)
{
    beginInsertRows(index(m_sounds.size()).parent(), m_sounds.size(), m_sounds.size());
    m_sounds.append(sound);
    endInsertRows();
    return true;
}

bool SndLibraryModel::removeSndFileInfo(SndFileInfo *sound)
{
    Q_UNUSED(sound);
    return false;
}
