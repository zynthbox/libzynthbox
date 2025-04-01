#include "SndLibraryModel.h"

#include <QFile>
#include <QDebug>
#include <QtGlobal>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>
#include <QDirIterator>
#include <QFileInfo>


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
            const QString fileIdentifier = baseSoundsDir.relativeFilePath(sndFileInfo.filePath());
            const QString sndFileName = sndFileInfo.baseName();
            const QString origin = fileIdentifier.split("/")[0];
            const QString category = fileInfo.dir().dirName();
            // And snd file to model only if symlink target snd file exists
            if (sndFileInfo.exists()) {
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
    if (DEBUG) qDebug() << "Adding snd file at index" << m_sounds.size();
    beginInsertRows(QModelIndex().parent(), m_sounds.size(), m_sounds.size());
    m_sounds.append(sound);
    endInsertRows();
    return true;
}

bool SndLibraryModel::removeSndFileInfo(SndFileInfo *sound)
{
    int index = m_sounds.indexOf(sound);
    if (index >= 0) {
        if (DEBUG) qDebug() << "Removing snd file from index" << index;
        beginRemoveRows(QModelIndex().parent(), index, index);
        m_sounds.removeAt(index);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

QObject * SndLibraryModel::getSound(const QString& absolutePath)
{
    QObject* foundFile{nullptr};
    // First, let's see if we've already indexed that file
    for (SndFileInfo *sndFile : qAsConst(m_sounds)) {
        if (sndFile->filePath() == absolutePath) {
            foundFile = sndFile;
            break;
        }
    }
    // If the file wasn't found, first see if the path is inside the sounds dir
    if (absolutePath.startsWith(baseSoundsDir.absolutePath())) {
        m_sndLibrary->processSndFiles({absolutePath});
        // Let's try and see if that helped
        for (SndFileInfo *sndFile : qAsConst(m_sounds)) {
            if (sndFile->filePath() == absolutePath) {
                foundFile = sndFile;
                break;
            }
        }
    } else {
        // If it for some reason is outside of the usual location... let's just create a container for it, which will be dangling, but...
        const QFileInfo sndFileInfo = QFileInfo(absolutePath);
        const QString fileIdentifier = sndFileInfo.filePath();
        const QString sndFileName = sndFileInfo.baseName();
        const QString origin = fileIdentifier.split("/")[0];
        foundFile = new SndFileInfo(
            fileIdentifier,
            sndFileName,
            origin,
            QString{},
            this);
    }
    return foundFile;
}
