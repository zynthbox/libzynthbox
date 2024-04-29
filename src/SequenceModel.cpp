/*
 * Copyright (C) 2021 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#include "SequenceModel.h"
#include "Note.h"
#include "PatternModel.h"
#include "SegmentHandler.h"
#include "SyncTimer.h"
#include "ZynthboxBasics.h"

#include <QCollator>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>

#define ZynthboxPartCount 5
#define PATTERN_COUNT (ZynthboxTrackCount * ZynthboxPartCount)
static const QStringList globalSequenceNames{"global", "global2", "global3", "global4", "global5", "global6", "global7", "global8", "global9", "global10"};
static const QStringList partNames{"a", "b", "c", "d", "e"};

class ZLSequenceSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLSequenceSynchronisationManager(SequenceModel *parent = nullptr)
        : QObject(parent)
        , q(parent)
    {
        connect(q, &SequenceModel::sceneIndexChanged, this, &ZLSequenceSynchronisationManager::selectedSketchpadSongIndexChanged, Qt::QueuedConnection);
        // This actually means current /channel/ changed, the channel index and our current midi channel are the same number
        connect(q->playGridManager(), &PlayGridManager::currentMidiChannelChanged, this, &ZLSequenceSynchronisationManager::currentMidiChannelChanged, Qt::QueuedConnection);
    };
    SequenceModel *q{nullptr};
    QObject *zlSong{nullptr};
    QObject *zlScenesModel{nullptr};
    QObject *zlMetronomeManager{nullptr};

    int soloChannel{-1};
    void setZlSong(QObject *newZlSong) {
        if (zlSong != newZlSong) {
            if (zlSong) {
                setZlMetronomeManager(nullptr);
                zlSong->disconnect(this);
            }
            zlSong = newZlSong;
            if (zlSong) {
                setZlMetronomeManager(qvariant_cast<QObject *>(zlSong->property("metronomeManager")));
                connect(zlSong, SIGNAL(__scenes_model_changed__()), this, SLOT(scenesModelChanged()), Qt::QueuedConnection);
                connect(zlSong, SIGNAL(playChannelSoloChanged()), this, SLOT(playChannelSoloChanged()), Qt::QueuedConnection);
                connect(zlSong, SIGNAL(isLoadingChanged()), this, SLOT(isLoadingChanged()), Qt::QueuedConnection);
            }
            scenesModelChanged();
            currentMidiChannelChanged();
            playChannelSoloChanged();
            isLoadingChanged();
        }
    }

    void setZlMetronomeManager(QObject *newZlMetronomeManager) {
        if (zlMetronomeManager != newZlMetronomeManager) {
            if (zlMetronomeManager) {
                zlMetronomeManager->disconnect(this);
            }
            zlMetronomeManager = newZlMetronomeManager;
            if (zlMetronomeManager) {
                connect(zlMetronomeManager, SIGNAL(recordSoloChanged()), this, SLOT(recordSoloChanged()), Qt::QueuedConnection);
                connect(zlMetronomeManager, SIGNAL(isRecordingChanged()), this, SLOT(isRecordingChanged()), Qt::QueuedConnection);
                connect(zlMetronomeManager, SIGNAL(sketchpadLoadingInProgressChanged()), this, SLOT(isLoadingChanged()), Qt::QueuedConnection);
            }
            recordSoloChanged();
            isRecordingChanged();
            isLoadingChanged();
        }
    }

    void setZlScenesModel(QObject *newZlScenesModel) {
        if (zlScenesModel != newZlScenesModel) {
            if (zlScenesModel) {
                zlScenesModel->disconnect(this);
            }
            zlScenesModel = newZlScenesModel;
            if (zlScenesModel) {
                connect(zlScenesModel, SIGNAL(selected_sketchpad_song_index_changed()), this, SLOT(selectedSketchpadSongIndexChanged()), Qt::QueuedConnection);
                selectedSketchpadSongIndexChanged();
            }
        }
    }

    void updateShouldMakeSounds() {
        if (zlMetronomeManager && zlScenesModel) {
            const int selectedSketchpadSongIndex = zlScenesModel->property("selectedSketchpadSongIndex").toInt();
            const bool isRecording = zlMetronomeManager->property("isRecording").toBool();
            const bool recordSolo = zlMetronomeManager->property("recordSolo").toBool();
            q->setShouldMakeSounds(selectedSketchpadSongIndex == q->sceneIndex() && (!isRecording or (isRecording && !recordSolo)));
        }
    }
public Q_SLOTS:
    void scenesModelChanged() {
        setZlScenesModel(zlSong->property("scenesModel").value<QObject*>());
    }
    void selectedSketchpadSongIndexChanged() {
        updateShouldMakeSounds();
    }
    void isRecordingChanged() {
        updateShouldMakeSounds();
    }
    void recordSoloChanged() {
        updateShouldMakeSounds();
    }
    void playChannelSoloChanged() {
        if (zlSong) {
            soloChannel = zlSong->property("playChannelSolo").toInt();
        } else {
            soloChannel = -1;
        }
    }
    void isLoadingChanged() {
        q->setIsDirty(false); // As we are either loading, or just got done loading the song, we're a member of, we can assume that the data was recently loaded and actually fresh, so... mark self as not dirty
    }
    void currentMidiChannelChanged() {
        if (zlSong) {
            QObject *channelsModel = zlSong->property("channelsModel").value<QObject*>();
            QObject *channel{nullptr};
            QMetaObject::invokeMethod(channelsModel, "getChannel", Qt::DirectConnection, Q_RETURN_ARG(QObject*, channel), Q_ARG(int, PlayGridManager::instance()->currentMidiChannel()));
            if (channel) {
                const int channelId{channel->property("id").toInt()};
                const int selectedPart{channel->property("selectedPart").toInt()};
                q->setActiveChannel(channelId, selectedPart);
            }
        }
    }
};

class SequenceModel::Private {
public:
    Private(SequenceModel *q)
        : q(q)
    {}
    SequenceModel *q;
    ZLSequenceSynchronisationManager *zlSyncManager{nullptr};
    PlayGridManager *playGridManager{nullptr};
    SyncTimer *syncTimer{nullptr};
    SegmentHandler *segmentHandler{nullptr};
    QObject *song{nullptr};
    int soloPattern{-1};
    PatternModel *soloPatternObject{nullptr};
    QList<PatternModel*> patternModels;
    PatternModel* patternModelIterator[PATTERN_COUNT];
    int bpm{0};
    int activePattern{0};
    QString filePath;
    bool isDirty{false};
    int version{0};
    QObjectList onifiedNotes;
    QObjectList queuedForOffNotes;
    bool isPlaying{false};
    int sceneIndex{-1};
    bool shouldMakeSounds{true};
    bool isLoading{false};

    void ensureFilePath(const QString &explicitFile) {
        if (!explicitFile.isEmpty()) {
            q->setFilePath(explicitFile);
        }
        if (filePath.isEmpty()) {
            if (song) {
                QString sketchpadFolder = song->property("sketchpadFolder").toString();
                const QString sequenceNameForFiles = QString(q->objectName().toLower()).replace(" ", "-");
                q->setFilePath(QString("%1/sequences/%2/metadata.sequence.json").arg(sketchpadFolder).arg(sequenceNameForFiles));
            }
        }
    }

    QString getDataLocation()
    {
        QStringList keepcharacters{" ",".","_"};
        QString safe;
        for (const QChar &letter : q->objectName()) {
            if (letter.isLetterOrNumber() || keepcharacters.contains(letter)) {
                safe.append(letter);
            }
        }
        // test and make sure that this env var contains something, or spit out .local/zynthian or something
        return QString("%1/session/sequences/%2").arg(QString(qgetenv("ZYNTHIAN_MY_DATA_DIR"))).arg(safe);
    }

    void updatePatternIterator() {
        int actualCount = patternModels.count();
        for (int i = 0; i < PATTERN_COUNT; ++i) {
            patternModelIterator[i] = (i < actualCount) ? patternModels[i] : nullptr;
        }
    }
};

SequenceModel::SequenceModel(PlayGridManager* parent)
    : QAbstractListModel(parent)
    , d(new Private(this))
{
    d->playGridManager = parent;
    d->zlSyncManager = new ZLSequenceSynchronisationManager(this);
    d->syncTimer = SyncTimer::instance();
    d->segmentHandler = SegmentHandler::instance();
    connect(d->syncTimer, &SyncTimer::timerRunningChanged, this, [this](){
        if (!d->syncTimer->timerRunning()) {
            stopSequencePlayback();
        }
    }, Qt::DirectConnection);
    // Save yourself anytime changes, but not too often, and only after a second... Let's be a bit gentle here
    QTimer *saveThrottle = new QTimer(this);
    saveThrottle->setSingleShot(true);
    saveThrottle->setInterval(1000);
    connect(saveThrottle, &QTimer::timeout, this, [this](){
        if (isDirty()) {
            save();
        }
    });
    connect(this, &SequenceModel::isDirtyChanged, this, [this, saveThrottle](){
        if (isDirty()) {
            saveThrottle->start();
        }
    });
    connect(this, &SequenceModel::countChanged, this, [this](){
        d->updatePatternIterator();
    });
}

SequenceModel::~SequenceModel()
{
    delete d;
}

QHash<int, QByteArray> SequenceModel::roleNames() const
{
    static const QHash<int, QByteArray> roles{
        {PatternRole, "pattern"},
        {TextRole, "text"},
        {NameRole, "name"},
        {LayerRole, "layer"},
        {BankRole, "bank"},
        {PlaybackPositionRole, "playbackPosition"},
        {BankPlaybackPositionRole, "bankPlaybackPosition"},
    };
    return roles;
}

QVariant SequenceModel::data(const QModelIndex& index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        PatternModel *model = d->patternModelIterator[index.row()];
        if (model) {
            switch (role) {
            case PatternRole:
                result.setValue<QObject*>(model);
                break;
            // We might well want to do something more clever with the text later on, so...
            case TextRole:
            case NameRole:
                result.setValue(model->name());
                break;
            case LayerRole:
                result.setValue(model->sketchpadTrack());
                break;
            case BankRole:
                result.setValue(model->bank());
                break;
            case PlaybackPositionRole:
                result.setValue(model->playbackPosition());
                break;
            case BankPlaybackPositionRole:
                result.setValue(model->bankPlaybackPosition());
                break;
            default:
                break;
            }
        }
    }
    return result;
}

int SequenceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return PATTERN_COUNT;
}

QModelIndex SequenceModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return createIndex(row, column);
}

QObject* SequenceModel::get(int patternIndex) const
{
    QObject *pattern{nullptr};
    if (patternIndex > -1 && patternIndex < PATTERN_COUNT) {
        pattern = d->patternModelIterator[patternIndex];
    }
    return pattern;
}

QObject *SequenceModel::getByPart(int channelIndex, int partIndex) const
{
    QObject *pattern{nullptr};
    for (PatternModel *needle : d->patternModelIterator) {
        if (needle && needle->sketchpadTrack() == channelIndex && needle->partIndex() == partIndex) {
            pattern = needle;
            break;
        }
    }
    return pattern;
}

void SequenceModel::insertPattern(PatternModel* pattern, int row)
{
    auto updatePattern = [this,pattern](){
        if (!d->isLoading) {
            int row = d->patternModels.indexOf(pattern);
            QModelIndex index(createIndex(row, 0));
            dataChanged(index, index);
        }
    };
    connect(pattern, &PatternModel::objectNameChanged, this, updatePattern);
    connect(pattern, &PatternModel::bankOffsetChanged, this, updatePattern);
    connect(pattern, &PatternModel::playingColumnChanged, this, updatePattern);
    connect(pattern, &PatternModel::layerDataChanged, this, updatePattern);
    connect(pattern, &NotesModel::lastModifiedChanged, this, &SequenceModel::setDirty);
    int insertionRow = d->patternModels.count();
    if (row > -1) {
        // If we've been requested to add in a specific location, do so
        insertionRow = qMin(qMax(0, row), d->patternModels.count());
    }
    if (!d->isLoading) { beginInsertRows(QModelIndex(), insertionRow, insertionRow); }
    d->patternModels.insert(insertionRow, pattern);
    if (!d->isLoading) {
        endInsertRows();
        setActivePattern(d->activePattern);
    }
    if (!d->isLoading) { Q_EMIT countChanged(); }
}

void SequenceModel::removePattern(PatternModel* pattern)
{
    int removalPosition = d->patternModels.indexOf(pattern);
    if (removalPosition > -1) {
        if (!d->isLoading) { beginRemoveRows(QModelIndex(), removalPosition, removalPosition); }
        d->patternModels.removeAt(removalPosition);
        pattern->disconnect(this);
        setActivePattern(d->activePattern);
        if (!d->isLoading) { endRemoveRows(); }
    }
    if (!d->isLoading) { Q_EMIT countChanged(); }
}

bool SequenceModel::contains(QObject* pattern) const
{
    return d->patternModels.contains(qobject_cast<PatternModel*>(pattern));
}

int SequenceModel::indexOf(QObject *pattern) const
{
    return d->patternModels.indexOf(qobject_cast<PatternModel*>(pattern));
}

PlayGridManager* SequenceModel::playGridManager() const
{
    return d->playGridManager;
}

void SequenceModel::setBpm(int bpm)
{
    if(d->bpm != bpm) {
        d->bpm = bpm;
        Q_EMIT bpmChanged();
    }
}

int SequenceModel::bpm() const
{
    return d->bpm;
}

void SequenceModel::setActivePattern(int activePattern)
{
    int adjusted = qMin(qMax(0, activePattern), PATTERN_COUNT);
    if (d->activePattern != adjusted) {
        d->activePattern = adjusted;
        Q_EMIT activePatternChanged();
        setDirty();
    }
}

void SequenceModel::setActiveChannel(int channelId, int partId)
{
    setActivePattern((channelId * ZynthboxPartCount) + partId);
}

int SequenceModel::activePattern() const
{
    return d->activePattern;
}

QObject* SequenceModel::activePatternObject() const
{
    if (d->activePattern > -1 && d->activePattern < PATTERN_COUNT) {
        PatternModel *pattern = d->patternModelIterator[d->activePattern];
        if (pattern) {
            return pattern;
        }
    }
    return nullptr;
}

QString SequenceModel::filePath() const
{
    return d->filePath;
}

void SequenceModel::setFilePath(const QString &filePath)
{
    if (d->filePath != filePath) {
        d->filePath = filePath;
        Q_EMIT filePathChanged();
    }
}

bool SequenceModel::isDirty() const
{
    return d->isDirty;
}

void SequenceModel::setIsDirty(bool isDirty)
{
    if (d->isDirty != isDirty) {
        d->isDirty = isDirty;
        Q_EMIT isDirtyChanged();
    }
}

bool SequenceModel::isLoading() const
{
    return d->isLoading;
}

int SequenceModel::sceneIndex() const
{
    return d->sceneIndex;
}

void SequenceModel::setSceneIndex(int sceneIndex)
{
    if (d->sceneIndex != sceneIndex) {
        d->sceneIndex = sceneIndex;
        Q_EMIT sceneIndexChanged();
    }
}

bool SequenceModel::shouldMakeSounds() const
{
    return d->shouldMakeSounds;
}

void SequenceModel::setShouldMakeSounds(bool shouldMakeSounds)
{
    if (d->shouldMakeSounds != shouldMakeSounds) {
        d->shouldMakeSounds = shouldMakeSounds;
        Q_EMIT shouldMakeSoundsChanged();
    }
}

void SequenceModel::load(const QString &fileName)
{
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    int loadedPatternCount{0};
    d->isLoading = true;
    Q_EMIT isLoadingChanged();
    beginResetModel();
    QString data;
    d->ensureFilePath(fileName);
    QFile file(d->filePath);

    // Clear our the existing model...
    QList<PatternModel*> oldModels = d->patternModels;
    for (PatternModel *model : d->patternModels) {
        model->disconnect(this);
        model->startLongOperation();
    }
    d->patternModels.clear();

    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            data = QString::fromUtf8(file.readAll());
            file.close();
        }
    }
    const QString sequenceName{globalSequenceNames.contains(objectName()) ? objectName() : ""};
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (jsonDoc.isObject()) {
        // First, load the patterns from disk
        QDir dir(QString("%1/patterns").arg(d->filePath.left(d->filePath.lastIndexOf("/"))));
        QFileInfoList entries = dir.entryInfoList({"*.pattern.json"}, QDir::Files, QDir::NoSort);
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(entries.begin(), entries.end(), [&](const QFileInfo &file1, const QFileInfo &file2){ return collator.compare(file1.absoluteFilePath(), file2.absoluteFilePath()) < 0; });
        // Now we have a list of all the entries in the patterns directory that has the pattern
        // file suffix, sorted naturally (so 10 is at the end, not after 1, which is just silly)
        int actualIndex{0};
        // The filename for patterns is "part(trackIndex)(partLetter).pattern.json"
        // where trackIndex is a number from 1 through 10, and partName is a single lower-case letter
        QRegularExpression patternFilenameRegexp("part(\\d\\d?)([a-z])");
        for (const QFileInfo &entry : entries) {
            // The filename for patterns is "sequencename-(channelIndex)(partName).pattern.json"
            const QString absolutePath{entry.absoluteFilePath()};
            QRegularExpressionMatch match = patternFilenameRegexp.match(entry.fileName());
            if (!match.hasMatch()) {
                qWarning() << Q_FUNC_INFO << "This file is not recognised as a pattern file, skipping (is this an old-style filename? In that case, you can restore it by renaming it to part#n.pattern.json to match the name of the clip it is in):" << entry.fileName();
                continue;
            }
            const int trackIndex{match.captured(1).toInt() - 1};
            const QString partName{match.captured(2)};
            const int partIndex = partNames.indexOf(partName);
//             qDebug() << "Loading pattern track" << trackIndex + 1 << "part" << partName << "for sequence" << this << "from file" << absolutePath;
            while (actualIndex < (trackIndex * ZynthboxPartCount) + partIndex) {
                // then we're missing some patterns, which is not great and we should deal with that so we don't end up with holes in the model...
                const int intermediaryTrackIndex = actualIndex / ZynthboxPartCount;
                const QString &intermediaryPartName = partNames[actualIndex - (intermediaryTrackIndex * ZynthboxPartCount)];
                PatternModel *model = qobject_cast<PatternModel*>(playGridManager()->getPatternModel(QString("%1-%2%3").arg(sequenceName).arg(QString::number(intermediaryTrackIndex + 1)).arg(intermediaryPartName), this));
                model->startLongOperation();
                model->resetPattern(true);
                model->setSketchpadTrack(intermediaryTrackIndex);
                model->setPartIndex(actualIndex % ZynthboxPartCount);
                insertPattern(model);
                model->endLongOperation();
//                 qWarning() << "Sequence missing patterns prior to that, added:" << model;
                ++actualIndex;
            }
            PatternModel *model = qobject_cast<PatternModel*>(playGridManager()->getPatternModel(QString("%1-%2%3").arg(sequenceName).arg(QString::number(trackIndex + 1)).arg(partName), this));
            model->startLongOperation();
            model->resetPattern(true);
            model->setSketchpadTrack(trackIndex);
            model->setPartIndex(partIndex);
            insertPattern(model);
            if (entry.exists()) {
                QFile patternFile{absolutePath};
                if (patternFile.open(QIODevice::ReadOnly)) {
                    QString patternData = QString::fromUtf8(patternFile.readAll());
                    patternFile.close();
                    playGridManager()->setModelFromJson(model, patternData);
                }
            }
            model->endLongOperation();
            ++loadedPatternCount;
//             qWarning() << "Loaded and added:" << model;
            ++actualIndex;
        }
        // Then set the values on the sequence
        QJsonObject obj = jsonDoc.object();
        setActivePattern(obj.value("activePattern").toInt());
        setBpm(obj.value("bpm").toInt());
    }
    // This ensures that when we're first creating ourselves a sequence, we end up with some models in it
    if (d->patternModels.count() < PATTERN_COUNT) {
        for (int i = d->patternModels.count(); i < PATTERN_COUNT; ++i) {
            const int intermediaryChannelIndex = i / ZynthboxPartCount;
            const QString &intermediaryPartName = partNames[i % ZynthboxPartCount];
            PatternModel *model = qobject_cast<PatternModel*>(playGridManager()->getPatternModel(QString("%1-%2%3").arg(sequenceName).arg(QString::number(intermediaryChannelIndex + 1)).arg(intermediaryPartName), this));
            model->startLongOperation();
            model->resetPattern(true);
            model->setSketchpadTrack(intermediaryChannelIndex);
            model->setPartIndex(i % ZynthboxPartCount);
            insertPattern(model);
            model->endLongOperation();
//             qDebug() << "Added missing model" << intermediaryChannelIndex << intermediaryPartName << "to" << objectName() << model->channelIndex() << model->partIndex();
        }
    }
    if (activePattern() == -1) {
        setActivePattern(0);
    }
    setIsDirty(false);
    endResetModel();
    d->isLoading = false;
    // Unlock the patterns, in case...
    for (PatternModel *model : oldModels) {
        model->endLongOperation();
    }
    Q_EMIT isLoadingChanged();
    Q_EMIT countChanged();
    if (loadedPatternCount > 0 || objectName() == QLatin1String("global")) {
        qDebug() << this << "Loaded" << loadedPatternCount << "patterns and filled in" << PATTERN_COUNT - loadedPatternCount << "in" << elapsedTimer.elapsed() << "milliseconds";
    }
}

bool SequenceModel::save(const QString &fileName, bool exportOnly)
{
    bool success = false;
    if (d->isLoading) {
        success = true;
    } else {
        QJsonObject sequenceObject;
        sequenceObject["activePattern"] = activePattern();
        sequenceObject["bpm"] = bpm();

        QJsonDocument jsonDoc;
        jsonDoc.setObject(sequenceObject);
        QString data = jsonDoc.toJson();

        QString saveToPath;
        if (exportOnly) {
            saveToPath = fileName;
        } else {
            d->ensureFilePath(fileName);
            saveToPath = d->filePath;
        }
        QDir sequenceLocation(saveToPath.left(saveToPath.lastIndexOf("/")));
        QDir patternLocation(saveToPath.left(saveToPath.lastIndexOf("/")) + "/patterns");
        bool hasAnyPattern{false};
        for (int i = 0; i < PATTERN_COUNT; ++i) {
            PatternModel *pattern = d->patternModelIterator[i];
            if (pattern) {
                if (pattern->hasNotes()) {
                    hasAnyPattern = true;
                }
            }
        }
        if (sequenceLocation.exists() || (hasAnyPattern && sequenceLocation.mkpath(sequenceLocation.path()))) {
            QFile dataFile(saveToPath);
            if (dataFile.open(QIODevice::WriteOnly) && dataFile.write(data.toUtf8())) {
                dataFile.close();
                if (patternLocation.exists() || patternLocation.mkpath(patternLocation.path())) {
                    // The filename for patterns is "part(trackIndex)(partLetter).pattern.json"
                    for (int i = 0; i < PATTERN_COUNT; ++i) {
                        PatternModel *pattern = d->patternModelIterator[i];
                        if (pattern) {
                            QString patternIdentifier = QString::number(i + 1);
                            if (pattern->sketchpadTrack() > -1 && pattern->partIndex() > -1) {
                                patternIdentifier = QString("%1%2").arg(QString::number(pattern->sketchpadTrack() + 1)).arg(partNames[pattern->partIndex()]);
                            }
                            QString fileName = QString("%1/part%2.pattern.json").arg(patternLocation.path()).arg(patternIdentifier);
                            QFile patternFile(fileName);
                            if (pattern->hasNotes()) {
                                pattern->exportToFile(fileName);
                            } else if (patternFile.exists()) {
                                qDebug() << Q_FUNC_INFO << "Pattern" << patternIdentifier << "in sequence" << objectName() << "has no notes, but the file exists, so delete it";
                                patternFile.remove();
                            }
                        }
                    }
                    if (hasAnyPattern == false) {
                        // If we've not got any patterns, get rid of the container folder again, keep things nice and lean and clean
                        qDebug() << Q_FUNC_INFO << "No patterns in sequence" << objectName() << "have notes, get rid of the sequences folder" << sequenceLocation.path();
                        sequenceLocation.removeRecursively();
                    }
                }
                success = true;
            }
        }
        setIsDirty(false);
    }
    return success;
}

void SequenceModel::clear()
{
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        PatternModel *pattern = d->patternModelIterator[i];
        if (pattern) {
            pattern->clear();
            pattern->setLayerData("");
            pattern->setNoteLength(3);
            pattern->setAvailableBars(1);
            pattern->setActiveBar(0);
            pattern->setBankOffset(0);
            pattern->setBankLength(8);
            pattern->setEnabled(true);
        }
    }
    setActivePattern(0);
}

QObject* SequenceModel::song() const
{
    return d->song;
}

void SequenceModel::setSong(QObject* song)
{
    if (d->song != song) {
        setIsDirty(false); // just in case, let's just sort of... not cause any saving after loading
        if (d->song) {
            d->song->disconnect(this);
        }
        d->song = song;
        if (d->song) {
            QString sketchpadFolder = d->song->property("sketchpadFolder").toString();
            const QString sequenceNameForFiles = QString(objectName().toLower()).replace(" ", "-");
            setFilePath(QString("%1/sequences/%2/metadata.sequence.json").arg(sketchpadFolder).arg(sequenceNameForFiles));
        }
        load();
        Q_EMIT songChanged();
        d->zlSyncManager->setZlSong(song);
        setIsDirty(false); // just in case, let's just sort of... not cause any saving after loading
    }
}

int SequenceModel::soloPattern() const
{
    return d->soloPattern;
}

PatternModel* SequenceModel::soloPatternObject() const
{
    return d->soloPatternObject;
}

void SequenceModel::setSoloPattern(int soloPattern)
{
    if (d->soloPattern != soloPattern) {
        d->soloPattern = soloPattern;
        if (d->soloPattern > -1 && d->soloPattern < PATTERN_COUNT) {
            d->soloPatternObject = d->patternModelIterator[d->soloPattern];
        } else {
            d->soloPatternObject = nullptr;
        }
        Q_EMIT soloPatternChanged();
        setDirty();
    }
}

void SequenceModel::setPatternProperty(int patternIndex, const QString& property, const QVariant& value)
{
    if (patternIndex > -1 && patternIndex < PATTERN_COUNT) {
        PatternModel *model = d->patternModelIterator[patternIndex];
        if (model) {
            model->setProperty(property.toUtf8(), value);
        }
    }
}

bool SequenceModel::isPlaying() const
{
    return d->isPlaying;
}

void SequenceModel::prepareSequencePlayback()
{
    if (!d->isPlaying) {
        d->isPlaying = true;
        Q_EMIT isPlayingChanged();
        // These two must be direct connections, or things will not be done in the correct
        // order, and all the notes will end up scheduled at the wrong time, and the
        // pattern position will be set sporadically, which leads to everything
        // all kinds of looking laggy and weird. So, direct connection.
        connect(playGridManager(), &PlayGridManager::metronomeTick, this, &SequenceModel::advanceSequence, Qt::DirectConnection);
        connect(playGridManager(), &PlayGridManager::metronomeTick, this, &SequenceModel::updatePatternPositions, Qt::DirectConnection);
    }
    playGridManager()->hookUpTimer();
}

void SequenceModel::startSequencePlayback()
{
    prepareSequencePlayback();
    playGridManager()->startMetronome();
}

void SequenceModel::disconnectSequencePlayback()
{
    if (d->isPlaying) {
        disconnect(playGridManager(), &PlayGridManager::metronomeTick, this, &SequenceModel::advanceSequence);
        disconnect(playGridManager(), &PlayGridManager::metronomeTick, this, &SequenceModel::updatePatternPositions);
        d->isPlaying = false;
        Q_EMIT isPlayingChanged();
    }
    for (QObject *noteObject : d->queuedForOffNotes) {
        Note *note = qobject_cast<Note*>(noteObject);
        note->setOff();
    }
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        PatternModel *pattern = d->patternModelIterator[i];
        if (pattern) {
            pattern->handleSequenceStop();
        }
    }
    d->queuedForOffNotes.clear();
}

void SequenceModel::stopSequencePlayback()
{
    if (d->isPlaying) {
        disconnectSequencePlayback();
        playGridManager()->stopMetronome();
    }
}

void SequenceModel::resetSequence()
{
    // This function is mostly cosmetic... the playback will, in fact, follow the global beat.
    // TODO Maybe we need some way of feeding some reset information back to the sync timer from here?
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        PatternModel *pattern = d->patternModelIterator[i];
        if (pattern) {
            pattern->updateSequencePosition(0);
        }
    }
}

void SequenceModel::advanceSequence()
{
    if (d->shouldMakeSounds || d->segmentHandler->songMode()) {
        // The timer schedules ahead internally for sequence advancement type things,
        // so the sequenceProgressionLength thing is only for prefilling at this point.
        const int sequenceProgressionLength{0};
        const qint64 cumulativeBeat{qint64(d->syncTimer->cumulativeBeat())};
        if (d->soloPattern > -1 && d->soloPattern < PATTERN_COUNT) {
            const PatternModel *pattern = d->patternModelIterator[d->soloPattern];
            if (pattern) {
                pattern->handleSequenceAdvancement(cumulativeBeat, sequenceProgressionLength);
            }
        } else {
            for (int i = 0; i < PATTERN_COUNT; ++i) {
                const PatternModel *pattern = d->patternModelIterator[i];
                if (pattern && (d->zlSyncManager->soloChannel == -1 || d->zlSyncManager->soloChannel == pattern->sketchpadTrack())) {
                    pattern->handleSequenceAdvancement(cumulativeBeat, sequenceProgressionLength);
                }
            }
        }
    }
}

void SequenceModel::updatePatternPositions()
{
    if (d->shouldMakeSounds) {
        const qint64 sequencePosition{qint64(d->syncTimer->cumulativeBeat() - d->syncTimer->scheduleAheadAmount())};
        if (d->soloPattern > -1 && d->soloPattern < PATTERN_COUNT) {
            PatternModel *pattern = d->patternModelIterator[d->soloPattern];
            if (pattern) {
                pattern->updateSequencePosition(sequencePosition);
            }
        } else {
            for (int i = 0; i < PATTERN_COUNT; ++i) {
                const PatternModel *pattern = d->patternModelIterator[i];
                if (pattern) {
                    d->patternModelIterator[i]->updateSequencePosition(sequencePosition);
                }
            }
        }
    }
}

// Since we've got a QObject up at the top which wants mocing
#include "SequenceModel.moc"
