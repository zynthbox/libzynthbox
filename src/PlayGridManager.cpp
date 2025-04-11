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

#include "PlayGridManager.h"
#include "ZynthboxBasics.h"
#include "Note.h"
#include "NotesModel.h"
#include "PatternModel.h"
#include "SegmentHandler.h"
#include "SettingsContainer.h"

// Sketchpad library
// Hackety hack - we don't need all the thing, just need some storage things (MidiBuffer and MidiNote specifically)
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_audio_formats/juce_audio_formats.h>
#include "ClipAudioSource.h"
#include "MidiRouter.h"
#include "SyncTimer.h"
#include "Plugin.h"
#include "PlayfieldManager.h"

#include <QQmlEngine>
#include <QColor>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileSystemWatcher>
#include <QList>
#include <QQmlComponent>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include "KeyScales.h"

static const QString midiNoteNames[128]{
    "C-1", "C#-1", "D-1", "D#-1", "E-1", "F-1", "F#-1", "G-1", "G#-1", "A-1", "A#-1", "B-1",
    "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",
    "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",
    "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
    "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",
    "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
    "C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",
    "C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9"
};

PlayGridManager* timer_callback_ticker{nullptr};
void timer_callback(int beat) {
    if (timer_callback_ticker) {
        timer_callback_ticker->handleMetronomeTick(beat);
    }
}

class ZLPGMSynchronisationManager : public QObject {
Q_OBJECT
public:
    explicit ZLPGMSynchronisationManager(PlayGridManager *parent = nullptr)
        : QObject(parent)
        , q(parent)
    {
    };
    PlayGridManager *q{nullptr};
    QObject *zlSketchpad{nullptr};
    void setZlSketchpad(QObject *newZlSketchpad) {
        if (zlSketchpad != newZlSketchpad) {
            if (zlSketchpad) {
                zlSketchpad->disconnect(this);
            }
            zlSketchpad = newZlSketchpad;
            if (zlSketchpad) {
                connect(zlSketchpad, SIGNAL(selected_track_id_changed()), this, SLOT(selectedChannelChanged()), Qt::QueuedConnection);
                selectedChannelChanged();
            }
        }
    }
public Q_SLOTS:
    void selectedChannelChanged() {
        if (zlSketchpad) {
            const int selectedTrackId = zlSketchpad->property("selectedTrackId").toInt();
            q->setCurrentSketchpadTrack(selectedTrackId);
            // Do not set program change command when track changes. This causes all kind of wrong behaviour like
            // automatically changing preset on fluidsynth engines when switching to some other track and back to fluidsynth
            // SyncTimer::instance()->sendProgramChangeImmediately(MidiRouter::instance()->masterChannel(), 64);
        }
    }
};

class PlayGridManager::Private
{
public:
    Private(PlayGridManager *q) : q(q) {
        zlSyncManager = new ZLPGMSynchronisationManager(q);
        // Let's try and avoid any unnecessary things here...
        midiMessage.push_back(0);
        midiMessage.push_back(0);
        midiMessage.push_back(0);

        // pre-generate all possible notes (that is, all the note options for all tracks)
        Note *note{nullptr};
        for (int track = 0; track < ZynthboxTrackCount; ++track) {
            for (int midiNote = 0; midiNote < 128; ++midiNote) {
                static const QString note_int_to_str_map[12]{"C", "C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                note = new Note(q);
                note->setName(note_int_to_str_map[midiNote % 12]);
                note->setMidiNote(midiNote);
                note->setSketchpadTrack(track);
                QQmlEngine::setObjectOwnership(note, QQmlEngine::CppOwnership);
                notes << note;
            }
        }

        updatePlaygrids();
        connect(&watcher, &QFileSystemWatcher::directoryChanged, q, [this](){
            updatePlaygrids();
        });
        activeNotesUpdater = new QTimer(q);
        activeNotesUpdater->setSingleShot(true);
        activeNotesUpdater->setInterval(0);
        connect(activeNotesUpdater, &QTimer::timeout, q, [this, q](){
            QStringList activated;
            for (int i = 0; i < 128; ++i) {
                if (noteActivations[i]) {
                    activated << midiNoteNames[i];
                }
            }
            activeNotes = activated;
            Q_EMIT q->activeNotesChanged();
        });
        internalPassthroughActiveNotesUpdater = new QTimer(q);
        internalPassthroughActiveNotesUpdater->setSingleShot(true);
        internalPassthroughActiveNotesUpdater->setInterval(0);
        connect(internalPassthroughActiveNotesUpdater, &QTimer::timeout, q, [this, q](){
            QStringList activated;
            for (int i = 0; i < 128; ++i) {
                if (internalPassthroughNoteActivations[i]) {
                    activated << midiNoteNames[i];
                }
            }
            internalPassthroughActiveNotes = activated;
            Q_EMIT q->internalPassthroughActiveNotesChanged();
        });
        internalControllerPassthroughActiveNotesUpdater = new QTimer(q);
        internalControllerPassthroughActiveNotesUpdater->setSingleShot(true);
        internalControllerPassthroughActiveNotesUpdater->setInterval(0);
        connect(internalControllerPassthroughActiveNotesUpdater, &QTimer::timeout, q, [this, q](){
            QStringList activated;
            for (int i = 0; i < 128; ++i) {
                if (internalControllerPassthroughNoteActivations[i]) {
                    activated << midiNoteNames[i];
                }
            }
            internalControllerPassthroughActiveNotes = activated;
            Q_EMIT q->internalControllerPassthroughActiveNotesChanged();
        });
        hardwareInActiveNotesUpdater = new QTimer(q);
        hardwareInActiveNotesUpdater->setSingleShot(true);
        hardwareInActiveNotesUpdater->setInterval(0);
        connect(hardwareInActiveNotesUpdater, &QTimer::timeout, q, [this, q](){
            QStringList activated;
            for (int i = 0; i < 128; ++i) {
                if (hardwareInNoteActivations[i]) {
                    activated << midiNoteNames[i];
                }
            }
            hardwareInActiveNotes = activated;
            Q_EMIT q->hardwareInActiveNotesChanged();
        });
        hardwareOutActiveNotesUpdater = new QTimer(q);
        hardwareOutActiveNotesUpdater->setSingleShot(true);
        hardwareOutActiveNotesUpdater->setInterval(0);
        connect(hardwareOutActiveNotesUpdater, &QTimer::timeout, q, [this, q](){
            QStringList activated;
            for (int i = 0; i < 128; ++i) {
                if (hardwareOutNoteActivations[i]) {
                    activated << midiNoteNames[i];
                }
            }
            hardwareOutActiveNotes = activated;
            Q_EMIT q->hardwareOutActiveNotesChanged();
        });
        for (int i = 0; i < 128; ++i) {
            noteActivations[i] = 0;
            internalPassthroughNoteActivations[i] = 0;
            internalControllerPassthroughNoteActivations[i] = 0;
            hardwareInNoteActivations[i] = 0;
            hardwareOutNoteActivations[i] = 0;
        }
        QObject::connect(midiRouter, &MidiRouter::noteChanged, q, [this](const MidiRouter::ListenerPort &port, const int &/*midiNote*/, const int &/*midiChannel*/, const int &/*velocity*/, const bool &/*setOn*/, const quint64 &timestamp, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const int &sketchpadTrack, const QString& hardwareDeviceId){ emitMidiMessage(port, timestamp, byte1, byte2, byte3, sketchpadTrack, hardwareDeviceId); }, Qt::DirectConnection);
        QObject::connect(midiRouter, &MidiRouter::midiMessage, q, [this](int port, int size, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const int& sketchpadTrack, bool fromInternal ){ handleMidiMessage(port, size, byte1, byte2, byte3, sketchpadTrack, fromInternal); }, Qt::QueuedConnection);
        currentPlaygrids = {
            {"minigrid", 0}, // As these are sorted alphabetically, notesgrid for minigrid and
            {"playgrid", 1}, // stepsequencer for playgrid
        };

        beatSubdivision = syncTimer->getMultiplier();
        beatSubdivision2 = beatSubdivision / 2;
        beatSubdivision3 = beatSubdivision2 / 2;
        beatSubdivision4 = beatSubdivision3 / 2;
        beatSubdivision5 = beatSubdivision4 / 2;
        beatSubdivision6 = beatSubdivision5 / 2;
    }
    ~Private() {
    }
    PlayGridManager *q;
    ZLPGMSynchronisationManager *zlSyncManager{nullptr};
    QQmlEngine *engine{nullptr};
    SegmentHandler *segmentHandler{nullptr};
    PlayfieldManager *playfieldManager{nullptr};
    QStringList playgrids;
    QVariantMap currentPlaygrids;
    QString preferredSequencer;
    int pitch{0};
    int modulation{0};
    QHash<QString, SequenceModel*> sequenceModels;
    QHash<QString, PatternModel*> patternModels;
    QHash<QString, NotesModel*> notesModels;
    QList<Note*> notes;
    QHash<QString, SettingsContainer*> settingsContainers;
    QHash<QString, QObject*> namedInstances;
    QHash<Note*, int> noteStateMap;
    QVariantList mostRecentlyChangedNotes;

    int noteActivations[128];
    QTimer *activeNotesUpdater;
    QStringList activeNotes;
    int internalPassthroughNoteActivations[128];
    QTimer *internalPassthroughActiveNotesUpdater;
    QStringList internalPassthroughActiveNotes;
    int internalControllerPassthroughNoteActivations[128];
    QTimer *internalControllerPassthroughActiveNotesUpdater;
    QStringList internalControllerPassthroughActiveNotes;
    int hardwareInNoteActivations[128];
    QTimer *hardwareInActiveNotesUpdater;
    QStringList hardwareInActiveNotes;
    int hardwareOutNoteActivations[128];
    QTimer *hardwareOutActiveNotesUpdater;
    QStringList hardwareOutActiveNotes;

    int currentSketchpadTrack{0};

    std::vector<unsigned char> midiMessage;
    MidiRouter* midiRouter{MidiRouter::instance()};

    SyncTimer *syncTimer{nullptr};
    int beatSubdivision{0};
    int beatSubdivision2{0};
    int beatSubdivision3{0};
    int beatSubdivision4{0};
    int beatSubdivision5{0};
    int beatSubdivision6{0};
    int metronomeBeat4th{0};
    int metronomeBeat8th{0};
    int metronomeBeat16th{0};
    int metronomeBeat32nd{0};
    int metronomeBeat64th{0};
    int metronomeBeat128th{0};

    QFileSystemWatcher watcher;

    void emitMidiMessage(const MidiRouter::ListenerPort &port, const quint64 &timestamp, const unsigned char &byte1, const unsigned char &byte2, const unsigned char &byte3, const int &sketchpadTrack, const QString& hardwareDeviceId) {
        Q_EMIT q->midiMessage(port, timestamp, byte1, byte2, byte3, sketchpadTrack, hardwareDeviceId);
    }

    void handleMidiMessage(int port, int size, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3, const int& sketchpadTrack, bool /*fromInternal*/) {
        switch(port) {
            case MidiRouter::PassthroughPort:
                if (size == 3) {
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        static const QLatin1String note_on{"note_on"};
                        static const QLatin1String note_off{"note_off"};
                        const bool setOn{0x8F < byte1 && byte3 > 0};
                        const int midiChannel = (byte1 & 0xF);
                        const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                        QVariantMap metadata;
                        metadata["note"] = byte2;
                        metadata["channel"] = midiChannel;
                        metadata["velocity"] = byte3;
                        metadata["type"] = setOn ? note_on : note_off;
                        metadata["sketchpadTrack"] = sketchpadTrack;
                        metadata.insert("timestamp", QVariant::fromValue<qint64>(currentTime));
                        mostRecentlyChangedNotes << metadata;
                        while (mostRecentlyChangedNotes.count() > 100) {
                            mostRecentlyChangedNotes.removeFirst();
                        }
                        QMetaObject::invokeMethod(q, &PlayGridManager::mostRecentlyChangedNotesChanged, Qt::QueuedConnection);
                        noteActivations[byte2] = setOn ? 1 : 0;
                        activeNotesUpdater->start();
                        Note *note = findExistingNote(byte2, sketchpadTrack);
                        if (note) {
                            if (setOn) {
                                note->registerOn(midiChannel);
                            } else {
                                note->registerOff(midiChannel);
                            }
                        }
                    } else if (0xAF < byte1 && byte1 < 0xC0) {
                        if (byte2 == 0x7B) {
                            // All Notes Off
                            for (Note *note : qAsConst(notes)) {
                                if (note->sketchpadTrack() == sketchpadTrack) {
                                    note->resetRegistrations();
                                }
                            }
                            for (int note = 0; note < 128; ++note) {
                                noteActivations[note] = 0;
                            }
                            activeNotesUpdater->start();
                        }
                    } else if (0xDF < byte1 && byte1 < 0xF0) {
                        const int midiChannel = (byte1 & 0xF);
                        const int pitch = (byte3 * 128) + byte2;
                        for (Note *note : qAsConst(notes)) {
                            if (note->sketchpadTrack() == sketchpadTrack && note->activeChannel() == midiChannel) {
                                note->registerPitchChange(pitch);
                            }
                        }
                    }
                }
                break;
            case MidiRouter::InternalPassthroughPort:
                if (size == 3) {
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        const bool setOn{0x8F < byte1 && byte3 > 0};
                        internalPassthroughNoteActivations[byte2] = setOn ? 1 : 0;
                        internalPassthroughActiveNotesUpdater->start();
                    }
                }
                break;
            case MidiRouter::InternalControllerPassthroughPort:
                if (size == 3) {
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        const bool setOn{0x8F < byte1 && byte3 > 0};
                        internalControllerPassthroughNoteActivations[byte2] = setOn ? 1 : 0;
                        internalControllerPassthroughActiveNotesUpdater->start();
                    }
                }
                break;
            case MidiRouter::HardwareInPassthroughPort:
                if (size == 3) {
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        const bool setOn{0x8F < byte1 && byte3 > 0};
                        hardwareInNoteActivations[byte2] = setOn ? 1 : 0;
                        hardwareInActiveNotesUpdater->start();
                    }
                }
                break;
            case MidiRouter::ExternalOutPort:
                if (size == 3) {
                    if (0x79 < byte1 && byte1 < 0xA0) {
                        const bool setOn{0x8F < byte1 && byte3 > 0};
                        hardwareOutNoteActivations[byte2] = setOn ? 1 : 0;
                        hardwareOutActiveNotesUpdater->start();
                    }
                }
                break;
            case MidiRouter::UnknownPort:
            default:
                qWarning() << Q_FUNC_INFO << "Input event came in from an unknown port, somehow - no idea what to do with this";
                break;
        }
    }

    void updatePlaygrids()
    {
        static const QStringList searchlist{QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/zynthbox/playgrids", "/home/pi/zynthbox-qml/qml-ui/playgrids"};
        QStringList newPlaygrids;

        for (const QString &searchdir : searchlist) {
            QDir dir(searchdir);
            if (dir.exists()) {
                QDirIterator it(searchdir);
                while (it.hasNext()) {
                    const QFileInfo fi(it.next() + "/main.qml");
                    if (it.fileName() != "." && it.fileName() != "..") {
                        if (fi.exists()) {
                            newPlaygrids << fi.absolutePath();
                        } else {
                            qDebug() << Q_FUNC_INFO << "A stray directory that does not contain a main.qml file was found in one of the playgrid search locations: " << fi.absolutePath();
                        }
                    }
                }
            } else {
                // A little naughty, but knewstuff kind of removes directories once everything in it's gone
                dir.mkpath(searchdir);
            }
            if (!watcher.directories().contains(searchdir)) {
                watcher.addPath(searchdir);
            }
        }

        newPlaygrids.sort();
        // Start out by clearing known playgrids - it's a bit of a hack, but it ensures that for e.g. when updating a playgrid from the store, that will also be picked up and reloaded
        playgrids.clear();
        Q_EMIT q->playgridsChanged();
        playgrids = newPlaygrids;
        Q_EMIT q->playgridsChanged();
        qDebug() << Q_FUNC_INFO << "We now have the following known grids:" << playgrids;
    }

    Note *findExistingNote(int midiNote, int sketchpadTrack) {
        Note *note{nullptr};
        for (Note *aNote : qAsConst(notes)) {
            if (aNote->midiNote() == midiNote && aNote->sketchpadTrack() == sketchpadTrack) {
                note = aNote;
                break;
            }
        }
        return note;
    }

    QJsonArray generateModelNotesSection(const NotesModel* model) {
        QJsonArray modelArray;
        for (int row = 0; row < model->rowCount(); ++row) {
            QJsonArray rowArray;
            for (int column = 0; column < model->columnCount(model->index(row)); ++column) {
                QJsonObject obj;
                obj.insert("note", q->noteToJsonObject(qobject_cast<Note*>(model->getNote(row, column))));
                obj.insert("metadata", QJsonValue::fromVariant(model->getMetadata(row, column)));
                obj.insert("keyeddata", QJsonValue::fromVariant(model->getKeyedData(row, column)));
                rowArray.append(obj);
            }
            modelArray << QJsonValue(rowArray);
        }
        return modelArray;
    }
};

void PlayGridManager::setEngine(QQmlEngine* engine)
{
    d->engine = engine;
}

PlayGridManager::PlayGridManager(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    d->syncTimer = qobject_cast<SyncTimer*>(SyncTimer::instance());
    connect(d->syncTimer, &SyncTimer::timerTick, this, &timer_callback, Qt::DirectConnection);
    connect(d->syncTimer, &SyncTimer::timerRunningChanged, this, &PlayGridManager::metronomeActiveChanged);

    // QDir defaultSequenceLocation{QString("%1/sequences/default-sequences").arg(QString(qgetenv("ZYNTHIAN_MY_DATA_DIR")))};
    // if (!defaultSequenceLocation.exists()) {
    //     defaultSequenceLocation.mkpath(defaultSequenceLocation.path());
    // }
    // QDir mySequenceLocation{QString("%1/sequences/my-sequences").arg(QString(qgetenv("ZYNTHIAN_MY_DATA_DIR")))};
    // if (!mySequenceLocation.exists()) {
    //     mySequenceLocation.mkpath(mySequenceLocation.path());
    // }
    // QDir communitySequenceLocation{QString("%1/sequences/community-sequences").arg(QString(qgetenv("ZYNTHIAN_MY_DATA_DIR")))};
    // if (!communitySequenceLocation.exists()) {
    //     communitySequenceLocation.mkpath(communitySequenceLocation.path());
    // }

    QSettings settings;
    settings.beginGroup("PlayGridManager");
    d->preferredSequencer = settings.value("preferredSequencer", "").toString();
    connect(this, &PlayGridManager::sequenceEditorIndexChanged, [this](){
        QSettings settings;
        settings.beginGroup("PlayGridManager");
        settings.setValue("preferredSequencer", d->preferredSequencer);
    });
}

PlayGridManager::~PlayGridManager()
{
    delete d;
}

QStringList PlayGridManager::playgrids() const
{
    return d->playgrids;
}

void PlayGridManager::updatePlaygrids()
{
    d->updatePlaygrids();
}

QVariantMap PlayGridManager::currentPlaygrids() const
{
    return d->currentPlaygrids;
}

void PlayGridManager::setCurrentPlaygrid(const QString& section, int index)
{
    if (!d->currentPlaygrids.contains(section) || d->currentPlaygrids[section] != index) {
        d->currentPlaygrids[section] = index;
        Q_EMIT currentPlaygridsChanged();
    }
}

int PlayGridManager::pitch() const
{
    return d->pitch;
}

void PlayGridManager::setPitch(int pitch)
{
    int adjusted = qBound(0, pitch + 8192, 16383);
    if (d->pitch != adjusted) {
        juce::MidiBuffer buffer{juce::MidiMessage::pitchWheel(MidiRouter::instance()->masterChannel(), adjusted)};
        d->syncTimer->sendMidiBufferImmediately(buffer, SyncTimer::instance()->masterSketchpadTrack());
        d->pitch = adjusted;
        Q_EMIT pitchChanged();
    }
}

int PlayGridManager::modulation() const
{
    return d->modulation;
}

void PlayGridManager::setModulation(int modulation)
{
    int adjusted = qBound(0, modulation, 127);
    if (d->modulation != adjusted) {
        juce::MidiBuffer buffer{juce::MidiMessage::controllerEvent(MidiRouter::instance()->masterChannel(), 1, adjusted)};
        d->syncTimer->sendMidiBufferImmediately(buffer, SyncTimer::instance()->masterSketchpadTrack());
        d->modulation = adjusted;
        Q_EMIT modulationChanged();
    }
}

int PlayGridManager::sequenceEditorIndex() const
{
    int sequencerIndex{d->playgrids.indexOf(d->preferredSequencer)};
    if (sequencerIndex < 0) {
        for (int i = 0; i < d->playgrids.count(); ++i) {
            if (d->playgrids[i].contains("stepsequencer")) {
                sequencerIndex = i;
                break;
            }
        }
    }
    return sequencerIndex;
}

void PlayGridManager::setPreferredSequencer(const QString& playgridID)
{
    d->preferredSequencer = playgridID;
    Q_EMIT sequenceEditorIndexChanged();
}

QObject* PlayGridManager::getSequenceModel(const QString& name, bool loadPatterns)
{
    SequenceModel *model = d->sequenceModels.value(name.isEmpty() ? QLatin1String("global") : name);
    if (!model) {
        model = new SequenceModel(this);
        model->setObjectName(name);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        d->sequenceModels[name] = model;
        // CAUTION:
        // This causes a fair bit of IO stuff, and will also create models using getPatternModel
        // below, so make sure this happens _after_ adding it to the map above.
        if (!model->isLoading() && loadPatterns) {
            model->load();
        }
    }
    return model;
}

QObjectList PlayGridManager::getSequenceModels() const
{
    QObjectList sequenceModels;
    for (SequenceModel *model : qAsConst(d->sequenceModels)) {
        sequenceModels << model;
    }
    return sequenceModels;
}

QObject* PlayGridManager::getPatternModel(const QString&name, SequenceModel *sequence)
{
    PatternModel *model = d->patternModels.value(name);
    if (!model) {
        model = new PatternModel(sequence);
        model->setObjectName(name);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        d->patternModels[name] = model;
    }
    return model;
}

QObject* PlayGridManager::getPatternModel(const QString& name, const QString& sequenceName)
{
    // CAUTION:
    // This will potentially cause the creation of models using this same function, and so it must
    // happen here, rather than later, as otherwise it will potentially cause infinite recursion
    // in silly ways.
    SequenceModel *sequence = qobject_cast<SequenceModel*>(getSequenceModel(sequenceName));
    PatternModel *model = d->patternModels.value(name);
    if (!model) {
        model = new PatternModel(sequence);
        model->setObjectName(name);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        d->patternModels[name] = model;
    }
    if (!sequence->contains(model)) {
        sequence->insertPattern(model);
    }
    return model;
}

QObject* PlayGridManager::getNotesModel(const QString& name)
{
    NotesModel *model = d->notesModels.value(name);
    if (!model) {
        model = new NotesModel(this);
        model->setObjectName(name);
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        d->notesModels[name] = model;
    }
    return model;
}

QObject* PlayGridManager::getNote(int midiNote, int sketchpadTrack)
{
    Note *note{nullptr};
    const int theTrack{sketchpadTrack == -1 ? d->syncTimer->currentTrack() : std::clamp(sketchpadTrack, 0, ZynthboxTrackCount - 1)};
    // The channel numbers here /are/ invalid - however, we need them to distinguish "invalid" notes while still having a Note to operate with
    if (0 <= midiNote && midiNote <= 127) {
        for (Note *aNote : d->notes) {
            if (aNote->midiNote() == midiNote && aNote->sketchpadTrack() == theTrack) {
                note = aNote;
                break;
            }
        }
        if (!note) {
            static const QString note_int_to_str_map[12]{"C", "C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            note = new Note(this);
            note->setName(note_int_to_str_map[midiNote % 12]);
            note->setMidiNote(midiNote);
            note->setSketchpadTrack(theTrack);
            QQmlEngine::setObjectOwnership(note, QQmlEngine::CppOwnership);
            d->notes << note;
        }
    }
    return note;
}

QObject* PlayGridManager::getCompoundNote(const QVariantList& notes)
{
    QObjectList actualNotes;
    for (const QVariant &var : notes) {
        actualNotes << var.value<QObject*>();
    }
    Note *note{nullptr};
    // Make the compound note's fake note value...
    int fake_midi_note = 128;
    int index{1};
    for (QObject *subnote : actualNotes) {
        Note *actualSubnote = qobject_cast<Note*>(subnote);
        if (actualSubnote) {
            fake_midi_note = fake_midi_note + (index * (127 * actualSubnote->midiNote() + (actualSubnote->sketchpadTrack() + 1)));
        } else {
            // BAD CODER! THIS IS NOT A NOTE!
            fake_midi_note = -1;
            break;
        }
        ++index;
    }
    if (fake_midi_note > 127) {
        for (Note *aNote : d->notes) {
            if (aNote->midiNote() == fake_midi_note) {
                note = aNote;
                break;
            }
        }
        if (!note) {
            note = new Note(this);
            note->setMidiNote(fake_midi_note);
            note->setSubnotes(notes);
            QQmlEngine::setObjectOwnership(note, QQmlEngine::CppOwnership);
            d->notes << note;
        }
    }
    return note;
}

QObject* PlayGridManager::getSettingsStore(const QString& name)
{
    SettingsContainer *settings = d->settingsContainers.value(name);
    if (!settings) {
        settings = new SettingsContainer(name, this);
        settings->setObjectName(name);
        QQmlEngine::setObjectOwnership(settings, QQmlEngine::CppOwnership);
        d->settingsContainers[name] = settings;
    }
    return settings;
}

const QColor & PlayGridManager::noteColor(const int& midiNote) const
{
    static const QColor colors[128]{
            QColor::fromHsv(0, 80, 155), QColor::fromHsv(33, 90, 155), QColor::fromHsv(65, 100, 155), QColor::fromHsv(98, 110, 155), QColor::fromHsv(131, 120, 155),QColor::fromHsv(164, 130, 155),
            QColor::fromHsv(196, 140, 155), QColor::fromHsv(229, 150, 155), QColor::fromHsv(262, 160, 155), QColor::fromHsv(295, 170, 155), QColor::fromHsv(327, 180, 155), QColor::fromHsv(359, 190, 155),

            QColor::fromHsv(0, 80, 165), QColor::fromHsv(33, 90, 165), QColor::fromHsv(65, 100, 165), QColor::fromHsv(98, 110, 165), QColor::fromHsv(131, 120, 165),QColor::fromHsv(164, 130, 165),
            QColor::fromHsv(196, 140, 165), QColor::fromHsv(229, 150, 165), QColor::fromHsv(262, 160, 165), QColor::fromHsv(295, 170, 165), QColor::fromHsv(327, 180, 165), QColor::fromHsv(359, 190, 165),

            QColor::fromHsv(0, 80, 175), QColor::fromHsv(33, 90, 175), QColor::fromHsv(65, 100, 175), QColor::fromHsv(98, 110, 175), QColor::fromHsv(175, 120, 175),QColor::fromHsv(164, 130, 175),
            QColor::fromHsv(196, 140, 175), QColor::fromHsv(229, 150, 175), QColor::fromHsv(262, 160, 175), QColor::fromHsv(295, 170, 175), QColor::fromHsv(327, 180, 175), QColor::fromHsv(359, 190, 175),

            QColor::fromHsv(0, 80, 185), QColor::fromHsv(33, 90, 185), QColor::fromHsv(65, 100, 185), QColor::fromHsv(98, 110, 185), QColor::fromHsv(131, 120, 185),QColor::fromHsv(164, 130, 185),
            QColor::fromHsv(196, 140, 185), QColor::fromHsv(229, 150, 185), QColor::fromHsv(262, 160, 185), QColor::fromHsv(295, 170, 185), QColor::fromHsv(327, 180, 185), QColor::fromHsv(359, 190, 185),

            QColor::fromHsv(0, 80, 195), QColor::fromHsv(33, 90, 195), QColor::fromHsv(65, 100, 195), QColor::fromHsv(98, 110, 195), QColor::fromHsv(131, 120, 195),QColor::fromHsv(164, 130, 195),
            QColor::fromHsv(196, 140, 195), QColor::fromHsv(229, 150, 195), QColor::fromHsv(262, 160, 195), QColor::fromHsv(295, 170, 195), QColor::fromHsv(327, 180, 195), QColor::fromHsv(359, 190, 195),

            QColor::fromHsv(0, 80, 205), QColor::fromHsv(33, 90, 205), QColor::fromHsv(65, 100, 205), QColor::fromHsv(98, 110, 205), QColor::fromHsv(131, 120, 205),QColor::fromHsv(164, 130, 205),
            QColor::fromHsv(196, 140, 205), QColor::fromHsv(229, 150, 205), QColor::fromHsv(262, 160, 205), QColor::fromHsv(295, 170, 205), QColor::fromHsv(327, 180, 205), QColor::fromHsv(359, 190, 205),

            QColor::fromHsv(0, 80, 215), QColor::fromHsv(33, 90, 215), QColor::fromHsv(65, 100, 215), QColor::fromHsv(98, 110, 215), QColor::fromHsv(131, 120, 215),QColor::fromHsv(164, 130, 215),
            QColor::fromHsv(196, 140, 215), QColor::fromHsv(229, 150, 215), QColor::fromHsv(262, 160, 215), QColor::fromHsv(295, 170, 215), QColor::fromHsv(327, 180, 215), QColor::fromHsv(359, 190, 215),

            QColor::fromHsv(0, 80, 225), QColor::fromHsv(33, 90, 225), QColor::fromHsv(65, 100, 225), QColor::fromHsv(98, 110, 225), QColor::fromHsv(131, 120, 225),QColor::fromHsv(164, 130, 225),
            QColor::fromHsv(196, 140, 225), QColor::fromHsv(229, 150, 225), QColor::fromHsv(262, 160, 225), QColor::fromHsv(295, 170, 225), QColor::fromHsv(327, 180, 225), QColor::fromHsv(359, 190, 225),

            QColor::fromHsv(0, 80, 235), QColor::fromHsv(33, 90, 235), QColor::fromHsv(65, 100, 235), QColor::fromHsv(98, 110, 235), QColor::fromHsv(131, 120, 235),QColor::fromHsv(164, 130, 235),
            QColor::fromHsv(196, 140, 235), QColor::fromHsv(229, 150, 235), QColor::fromHsv(262, 160, 235), QColor::fromHsv(295, 170, 235), QColor::fromHsv(327, 180, 235), QColor::fromHsv(359, 190, 235),

            QColor::fromHsv(0, 80, 245), QColor::fromHsv(33, 90, 245), QColor::fromHsv(65, 100, 245), QColor::fromHsv(98, 110, 245), QColor::fromHsv(131, 120, 245),QColor::fromHsv(164, 130, 245),
            QColor::fromHsv(196, 140, 245), QColor::fromHsv(229, 150, 245), QColor::fromHsv(262, 160, 245), QColor::fromHsv(295, 170, 245), QColor::fromHsv(327, 180, 245), QColor::fromHsv(359, 190, 245),

            QColor::fromHsv(0, 80, 255), QColor::fromHsv(33, 90, 255), QColor::fromHsv(65, 100, 255), QColor::fromHsv(98, 110, 255), QColor::fromHsv(131, 120, 255),QColor::fromHsv(164, 130, 255),
            QColor::fromHsv(196, 140, 255), QColor::fromHsv(229, 150, 255)
    };
    return colors[std::clamp(midiNote, 0, 127)];
}

QObject* PlayGridManager::getNamedInstance(const QString& name, const QString& qmlTypeName)
{
    QObject *instance{nullptr};
    if (d->namedInstances.contains(name)) {
        instance = d->namedInstances[name];
    } else if (d->engine) {
        QQmlComponent component(d->engine);
        component.setData(QString("import QtQuick 2.4\n%1 { objectName: \"%2\" }").arg(qmlTypeName).arg(name).toUtf8(), QUrl());
        instance = component.create();
        QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);
        d->namedInstances.insert(name, instance);
    }
    return instance;
}

void PlayGridManager::deleteNamedObject(const QString &name)
{
    QObject *instance{nullptr};
    if (d->namedInstances.contains(name)) {
        instance = d->namedInstances.take(name);
    } else if (d->sequenceModels.contains(name)) {
        instance = d->sequenceModels.take(name);
    } else if (d->patternModels.contains(name)) {
        instance = d->patternModels.take(name);
    } else if (d->settingsContainers.contains(name)) {
        instance = d->settingsContainers.take(name);
    }
    if (instance) {
        instance->deleteLater();
    }
}

QJsonObject PlayGridManager::noteToJsonObject(Note *note) const
{
    QJsonObject jsonObject;
    if (note) {
        jsonObject.insert("midiNote", note->midiNote());
        jsonObject.insert("midiChannel", note->sketchpadTrack());
        if (note->subnotes().count() > 0) {
            QJsonArray subnoteArray;
            for (const QVariant &subnote : note->subnotes()) {
                subnoteArray << noteToJsonObject(subnote.value<Note*>());
            }
            jsonObject.insert("subnotes", subnoteArray);
        }
    }
    return jsonObject;
}

Note *PlayGridManager::jsonObjectToNote(const QJsonObject &jsonObject)
{
    Note *note{nullptr};
    if (jsonObject.contains("subnotes")) {
        QJsonArray subnotes = jsonObject["subnotes"].toArray();
        QVariantList subnotesList;
        for (const QJsonValue &val : subnotes) {
            Note *subnote = jsonObjectToNote(val.toObject());
            subnotesList.append(QVariant::fromValue<QObject*>(subnote));
        }
        note = qobject_cast<Note*>(getCompoundNote(subnotesList));
    } else if (jsonObject.contains("midiNote")) {
        note = qobject_cast<Note*>(getNote(jsonObject.value("midiNote").toInt(), jsonObject.value("midiChannel").toInt()));
    }
    return note;
}

QString PlayGridManager::modelToJson(const QObject* model) const
{
    QJsonDocument json;
    const NotesModel* actualModel = qobject_cast<const NotesModel*>(model);
    const PatternModel* patternModel = qobject_cast<const PatternModel*>(model);
    if (patternModel) {
        QJsonObject modelObject;
        modelObject["height"] = patternModel->height();
        modelObject["width"] = patternModel->width();
        modelObject["noteDestination"] = int(patternModel->noteDestination());
        // This is informational for displaying in other places (like webconf), and not actually used internally
        modelObject["sketchpadTrack"] = modelObject["midiChannel"] = patternModel->sketchpadTrack();
        modelObject["defaultNoteDuration"] = patternModel->defaultNoteDuration();
        modelObject["stepLength"] = patternModel->stepLength();
        modelObject["swing"] = patternModel->swing();
        modelObject["patternLength"] = patternModel->patternLength();
        modelObject["activeBar"] = patternModel->activeBar();
        modelObject["bankOffset"] = patternModel->bankOffset();
        modelObject["bankLength"] = patternModel->bankLength();
        modelObject["enabled"] = patternModel->enabled();
        modelObject["scale"] = KeyScales::instance()->scaleShorthand(patternModel->scaleKey());
        modelObject["pitch"] = KeyScales::instance()->pitchShorthand(patternModel->pitchKey());
        modelObject["octave"] = KeyScales::instance()->octaveShorthand(patternModel->octaveKey());
        modelObject["lockToKeyAndScale"] = int(patternModel->lockToKeyAndScale());
        modelObject["gridModelStartNote"] = patternModel->gridModelStartNote();
        modelObject["gridModelEndNote"] = patternModel->gridModelEndNote();
        modelObject["hasNotes"] = patternModel->hasNotes();
        QJsonDocument notesDoc;
        notesDoc.setArray(d->generateModelNotesSection(patternModel));
        modelObject["notes"] = QString::fromUtf8(notesDoc.toJson());
        // Add in the Sound data from whatever sound is currently in use...
        json.setObject(modelObject);
    } else if (actualModel) {
        json.setArray(d->generateModelNotesSection(actualModel));
    }
    return json.toJson();
}

void PlayGridManager::setModelFromJson(QObject* model, const QString& json)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json.toUtf8());
    if (jsonDoc.isArray()) {
        NotesModel* actualModel = qobject_cast<NotesModel*>(model);
        actualModel->startLongOperation();
        actualModel->clear();
        QJsonArray notesArray = jsonDoc.array();
        int rowPosition{0};
        for (const QJsonValue &row : notesArray) {
            if (row.isArray()) {
                QVariantList rowList;
                QVariantList rowMetadata;
                QVariantList rowKeyedData;
                QJsonArray rowArray = row.toArray();
                for (const QJsonValue &note : rowArray) {
                    rowList << QVariant::fromValue<QObject*>(jsonObjectToNote(note["note"].toObject()));
                    rowMetadata << note["metadata"].toVariant();
                    rowKeyedData << note["keyeddata"].toVariant();
                }
                actualModel->insertRow(rowPosition, rowList, rowMetadata, rowKeyedData);
            }
            ++rowPosition;
        }
        actualModel->endLongOperation();
    } else if (jsonDoc.isObject()) {
        PatternModel *pattern = qobject_cast<PatternModel*>(model);
        QJsonObject patternObject = jsonDoc.object();
        if (pattern) {
            pattern->startLongOperation();
            setModelFromJson(model, patternObject.value("notes").toString());
            pattern->setHeight(patternObject.value("height").toInt());
            pattern->setWidth(patternObject.value("width").toInt());
            if (patternObject.contains("noteLength")) {
                static const QMap<int, int> noteLengthToStepLength{{-1, 384}, {0, 192}, {1, 96}, {2, 48}, {3, 24}, {4, 12}, {5, 6}, {6, 3}};
                pattern->setStepLength(noteLengthToStepLength.value(patternObject.value("noteLength").toInt(), 24));
            } else {
                pattern->setStepLength(patternObject.value("stepLength").toDouble());
            }
            if (patternObject.contains("patternLength")) {
                pattern->setPatternLength(patternObject.value("patternLength").toInt());
            } else {
                pattern->setPatternLength(patternObject.value("availableBars").toInt() * pattern->width());
            }
            pattern->setActiveBar(patternObject.value("activeBar").toInt());
            pattern->setBankOffset(patternObject.value("bankOffset").toInt());
            pattern->setBankLength(patternObject.value("bankLength").toInt());
            // Because we've not always persisted this... probably wants to go away at some point in the near future
            if (patternObject.contains("enabled")) {
                pattern->setEnabled(patternObject.value("enabled").toBool());
            } else {
                pattern->setEnabled(true);
            }
            if (patternObject.contains("noteDestination")) {
                pattern->setNoteDestination(PatternModel::NoteDestination(patternObject.value("noteDestination").toInt()));
            } else {
                pattern->setNoteDestination(PatternModel::SynthDestination);
            }
            if (patternObject.contains("gridModelStartNote")) {
                pattern->setGridModelStartNote(patternObject.value("gridModelStartNote").toInt());
            } else {
                pattern->setGridModelStartNote(48);
            }
            if (patternObject.contains("gridModelEndNote")) {
                pattern->setGridModelEndNote(patternObject.value("gridModelEndNote").toInt());
            } else {
                pattern->setGridModelEndNote(64);
            }
            if (patternObject.contains("defaultNoteDuration")) {
                pattern->setDefaultNoteDuration(patternObject.value("defaultNoteDuration").toInt());
            } else {
                pattern->setDefaultNoteDuration(0);
            }
            if (patternObject.contains("swing")) {
                pattern->setSwing(patternObject.value("swing").toInt());
            } else {
                pattern->setSwing(50);
            }
            if (patternObject.contains("scale")) {
                pattern->setScaleKey(KeyScales::instance()->scaleShorthandToKey(patternObject.value("scale").toString()));
            } else {
                pattern->setScaleKey(KeyScales::ScaleChromatic);
            }
            if (patternObject.contains("pitch")) {
                pattern->setPitchKey(KeyScales::instance()->pitchShorthandToKey(patternObject.value("pitch").toString()));
            } else {
                pattern->setPitchKey(KeyScales::PitchC);
            }
            if (patternObject.contains("octave")) {
                pattern->setOctaveKey(KeyScales::instance()->octaveShorthandToKey(patternObject.value("octave").toString()));
            } else {
                pattern->setOctaveKey(KeyScales::Octave4);
            }
            if (patternObject.contains("lockToKeyAndScale")) {
                pattern->setLockToKeyAndScale(PatternModel::KeyScaleLockStyle(patternObject.value("lockToKeyAndScale").toInt()));
            } else {
                pattern->setLockToKeyAndScale(PatternModel::KeyScaleLockOff);
            }
            pattern->endLongOperation();
        }
    }
}

void PlayGridManager::setModelFromJsonFile(QObject *model, const QString &jsonFile)
{
    QFile file(jsonFile);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QString data = QString::fromUtf8(file.readAll());
            file.close();
            setModelFromJson(model, data);
        }
    }
}

QString PlayGridManager::notesListToJson(const QVariantList& notes) const
{
    QJsonDocument json;
    QJsonArray notesArray;
    for (const QVariant &element : notes) {
        notesArray << noteToJsonObject(element.value<Note*>());
    }
    json.setArray(notesArray);
    return json.toJson();
}

QVariantList PlayGridManager::jsonToNotesList(const QString& json)
{
    QVariantList notes;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json.toUtf8());
    if (jsonDoc.isArray()) {
        QJsonArray notesArray = jsonDoc.array();
        for (const QJsonValue &note : notesArray) {
            notes << QVariant::fromValue<QObject*>(jsonObjectToNote(note.toObject()));
        }
    }
    return notes;
}

QString PlayGridManager::noteToJson(QObject* note) const
{
    QJsonDocument doc;
    doc.setObject(noteToJsonObject(qobject_cast<Note*>(note)));
    return doc.toJson();
}

QObject* PlayGridManager::jsonToNote(const QString& json)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json.toUtf8());
    return jsonObjectToNote(jsonDoc.object());
}

QVariantList PlayGridManager::mostRecentlyChangedNotes() const
{
    return d->mostRecentlyChangedNotes;
}

QStringList PlayGridManager::activeNotes() const
{
    return d->activeNotes;
}

QStringList PlayGridManager::internalPassthroughActiveNotes() const
{
    return d->internalPassthroughActiveNotes;
}

QStringList PlayGridManager::internalControllerPassthroughActiveNotes() const
{
    return d->internalControllerPassthroughActiveNotes;
}

QStringList PlayGridManager::hardwareInActiveNotes() const
{
    return d->hardwareInActiveNotes;
}

QStringList PlayGridManager::hardwareOutActiveNotes() const
{
    return d->hardwareOutActiveNotes;
}

void PlayGridManager::updateNoteState(QVariantMap metadata)
{
    static const QLatin1String note_on{"note_on"};
    static const QLatin1String note_off{"note_off"};
    static const QLatin1String noteString{"note"};
    static const QLatin1String channelString{"channel"};
    static const QLatin1String typeString{"type"};
    static const QLatin1String sketchpadTrackString{"sketchpadTrack"};
    const int midiNote = metadata[noteString].toInt();
    const int midiChannel = metadata[channelString].toInt();
    const int sketchpadTrack = metadata[sketchpadTrackString].toInt();
    const QString messageType = metadata[typeString].toString();
    if (messageType == note_on) {
        Note *note = d->findExistingNote(midiNote, sketchpadTrack);
        if (note) {
            note->registerOn(midiChannel);
        }
    } else if (messageType == note_off) {
        Note *note = d->findExistingNote(midiNote, sketchpadTrack);
        if (note) {
            note->registerOff(midiChannel);
        }
    }
    d->mostRecentlyChangedNotes << metadata;
    Q_EMIT mostRecentlyChangedNotesChanged();
}

void PlayGridManager::midiMessageToClipCommands(ClipCommandRing *listToPopulate, const int &samplerIndex, const unsigned char& byte1, const unsigned char& byte2, const unsigned char& byte3) const
{
     for (const PatternModel *patternModel : qAsConst(d->patternModels)) {
        patternModel->midiMessageToClipCommands(listToPopulate, samplerIndex, byte1, byte2, byte3);
    }
}

QObject * PlayGridManager::zlSketchpad() const
{
    return d->zlSyncManager->zlSketchpad;
}

void PlayGridManager::setZlSketchpad(QObject* zlSketchpad)
{
    if (d->zlSyncManager->zlSketchpad != zlSketchpad) {
        d->zlSyncManager->setZlSketchpad(zlSketchpad);
        Q_EMIT zlSketchpadChanged();
    }
}

void PlayGridManager::setCurrentSketchpadTrack(const int &sketchpadTrack)
{
    if (d->currentSketchpadTrack != sketchpadTrack) {
        d->currentSketchpadTrack = sketchpadTrack;
        SyncTimer::instance()->setCurrentTrack(sketchpadTrack);
        MidiRouter::instance()->setCurrentSketchpadTrack(sketchpadTrack);
        Q_EMIT currentSketchpadTrackChanged();
    }
}

int PlayGridManager::currentSketchpadTrack() const
{
    return d->currentSketchpadTrack;
}

void PlayGridManager::scheduleNote(unsigned char midiNote, unsigned char midiChannel, bool setOn, unsigned char velocity, quint64 duration, quint64 delay)
{
    // No need for this check, unsigned is always larger than 0
    if (d->syncTimer /*&& midiChannel >= 0*/ && midiChannel <= 15) {
        d->syncTimer->scheduleNote(midiNote, midiChannel, setOn, velocity, duration, delay);
    }
}

QObject * PlayGridManager::syncTimer()
{
    return d->syncTimer;
}

void PlayGridManager::handleMetronomeTick(int beat)
{
    if (d->playfieldManager == nullptr) {
        d->playfieldManager = PlayfieldManager::instance();
    }
    d->playfieldManager->progressPlayback();
    if (d->segmentHandler == nullptr) {
        d->segmentHandler = SegmentHandler::instance();
    }
    d->segmentHandler->progressPlayback();
    Q_EMIT metronomeTick(beat);
    if (beat % d->beatSubdivision6 == 0) {
        d->metronomeBeat128th = beat / d->beatSubdivision6;
        Q_EMIT metronomeBeat128thChanged(d->metronomeBeat128th);
    }
    if (beat % d->beatSubdivision5 == 0) {
        d->metronomeBeat64th = beat / d->beatSubdivision5;
        Q_EMIT metronomeBeat64thChanged(d->metronomeBeat64th);
    }
    if (beat % d->beatSubdivision4 == 0) {
        d->metronomeBeat32nd = beat / d->beatSubdivision4;
        Q_EMIT metronomeBeat32ndChanged(d->metronomeBeat32nd);
    }
    if (beat % d->beatSubdivision3 == 0) {
        d->metronomeBeat16th = beat / d->beatSubdivision3;
        Q_EMIT metronomeBeat16thChanged(d->metronomeBeat16th);
    }
    if (beat % d->beatSubdivision2 == 0) {
        d->metronomeBeat8th = beat / d->beatSubdivision2;
        Q_EMIT metronomeBeat8thChanged(d->metronomeBeat8th);
    }
    if (beat % d->beatSubdivision == 0) {
        d->metronomeBeat4th = beat / d->beatSubdivision;
        Q_EMIT metronomeBeat4thChanged(d->metronomeBeat4th);
    }
}

int PlayGridManager::metronomeBeat4th() const
{
    return d->metronomeBeat4th;
}

int PlayGridManager::metronomeBeat8th() const
{
    return d->metronomeBeat8th;
}

int PlayGridManager::metronomeBeat16th() const
{
    return d->metronomeBeat16th;
}

int PlayGridManager::metronomeBeat32nd() const
{
    return d->metronomeBeat32nd;
}

int PlayGridManager::metronomeBeat64th() const
{
    return d->metronomeBeat64th;
}

int PlayGridManager::metronomeBeat128th() const
{
    return d->metronomeBeat128th;
}

void hookUpAndMaybeStartTimer(PlayGridManager* pgm, bool startTimer = false)
{
    // If we've already registered ourselves to get a callback, don't do that again, it just gets silly
    if (!timer_callback_ticker) {
        // TODO Send start metronome request to libzl directly
        timer_callback_ticker = pgm;
    }
    if (startTimer) {
        Q_EMIT pgm->requestMetronomeStart();
    }
}

void PlayGridManager::hookUpTimer()
{
    hookUpAndMaybeStartTimer(this);
}

void PlayGridManager::startMetronome()
{
    hookUpAndMaybeStartTimer(this, true);
}

void PlayGridManager::stopMetronome()
{
    // TODO Send stop metronome request to libzl
    timer_callback_ticker = nullptr;
    Q_EMIT requestMetronomeStop();
    QMetaObject::invokeMethod(this, "metronomeActiveChanged", Qt::QueuedConnection);
    d->metronomeBeat4th = 0;
    d->metronomeBeat8th = 0;
    d->metronomeBeat16th = 0;
    d->metronomeBeat32nd = 0;
    d->metronomeBeat64th = 0;
    d->metronomeBeat128th = 0;
    Q_EMIT metronomeBeat4thChanged(0);
    Q_EMIT metronomeBeat8thChanged(0);
    Q_EMIT metronomeBeat16thChanged(0);
    Q_EMIT metronomeBeat32ndChanged(0);
    Q_EMIT metronomeBeat64thChanged(0);
    Q_EMIT metronomeBeat128thChanged(0);
}

bool PlayGridManager::metronomeActive() const
{
    if (d->syncTimer) {
        return d->syncTimer->timerRunning();
    }
    return false;
}

void PlayGridManager::sendAMidiNoteMessage(unsigned char midiNote, unsigned char velocity, unsigned char channel, bool setOn)
{
    // No need for this check, unsigned is always larger than 0
    if (/*channel >= 0 &&*/ channel <= 15) {
//         qDebug() << "Sending midi message" << midiNote << channel << velocity << setOn;
        d->syncTimer->sendNoteImmediately(midiNote, channel, setOn, velocity);
    }
}

QObject *PlayGridManager::getClipById(int clipID) const
{
    QObject *clip{Plugin::instance()->getClipById(clipID)};
    if (clip) {
        QQmlEngine::setObjectOwnership(clip, QQmlEngine::CppOwnership);
    }
    return clip;
}

// Since we've got a QObject up at the top which wants mocing
#include "PlayGridManager.moc"
