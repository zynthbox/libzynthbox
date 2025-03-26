#include "SndLibraryModel.h"

#include <QFile>
#include <QDebug>
#include <QtGlobal>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

/**
 * When DEBUG is set to true it will print a set of logs
 * which is not meant for production builds
 */
#ifndef DEBUG
#define DEBUG true
#endif

SndLibraryModel::SndLibraryModel(SndLibrary *sndLibrary)
    : QAbstractListModel(sndLibrary)
    , m_sndLibrary(sndLibrary)
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
                result.setValue(sndFileInfo->name());
                break;
            case OriginRole:
                result.setValue(sndFileInfo->origin());
                break;
            case CategoryRole:
                result.setValue(sndFileInfo->category());
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
    auto t_start = std::chrono::high_resolution_clock::now();
    QMap<QString, int> categoriesFileCount;
    beginRemoveRows(index(0).parent(), 0, m_sounds.size());
    m_sounds.clear();
    endRemoveRows();


    QDirIterator it(m_sndLibrary->sndIndexPath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fileInfo = QFileInfo(it.next());
        if (fileInfo.isSymbolicLink() && !fileInfo.symLinkTarget().isEmpty()) {
            const QFileInfo sndFileInfo = QFileInfo(fileInfo.symLinkTarget());
            const QString fileIdentifier = QByteArray::fromBase64(fileInfo.baseName().toUtf8(), QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
            const QString sndFileName = sndFileInfo.baseName();
            const QString origin = fileIdentifier.split("/")[0];
            const QString category = fileInfo.dir().dirName();
            beginInsertRows(QModelIndex().parent(), m_sounds.size(), m_sounds.size());
            if(DEBUG) qDebug() << "Reading sound index :" << fileIdentifier;
            m_sounds.append(new SndFileInfo(
                fileIdentifier,
                sndFileName,
                origin,
                category,
                this
            ));
            if (!categoriesFileCount.contains(origin + "/" + category)) {
                categoriesFileCount[origin + "/" + category] = 0;
            }
            ++categoriesFileCount[origin + "/" + category];
            endInsertRows();
        }
        for (auto it = categoriesFileCount.constBegin(); it != categoriesFileCount.constEnd(); ++it) {
            Q_EMIT categoryFilesCountChanged(it.key().split("/")[1], it.key().split("/")[0], it.value());
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    if (DEBUG) qDebug() << "SndLibraryModel Refresh Time Taken :" << std::chrono::duration<double, std::chrono::seconds::period>(t_end-t_start).count();
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
