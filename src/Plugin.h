/*
 * Copyright (C) 2021 Dan Leinir Turthra Jensen <admin@leinir.dk>
 * Copyright (C) 2023 Anupam Basak <anupam.basak27@gmail.com>
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

#pragma once
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1

#include <QCoreApplication>
#include <QObject>
#include <QList>
#include <QQmlEngine>
#include <QHash>
#include <atomic>
#include <mutex>

#include "ClipAudioSource.h"
#include "JUCEHeaders.h"
#include "JuceEventLoop.h"
#include "JackPassthrough.h"

class Plugin : public QObject {
    Q_OBJECT
    /**
     * \brief The format used for timestamps returned by the currentTimestamp() function
     */
    Q_PROPERTY(QString timeStampFormat READ timeStampFormat WRITE setTimeStampFormat NOTIFY timeStampFormatChanged)
    /**
     * \brief A JackPassthrough client used as a global playback client
     */
    Q_PROPERTY(JackPassthrough* globalPlaybackClient READ globalPlaybackClient CONSTANT)
    /**
     * \brief A list of the 10 JackPassthrough clients used as the post-fx mixer for the SketchPad tracks
     */
    Q_PROPERTY(QList<JackPassthrough*> trackMixerClients READ trackMixerClients CONSTANT)
    /**
     * \brief A list of the 16 JackPassthrough clients used by each of the synth engines
     */
    Q_PROPERTY(QList<JackPassthrough*> synthPassthroughClients READ synthPassthroughClients CONSTANT)
    /**
     * \brief A list of the 10 JackPassthrough clients used by each of the channels
     */
    Q_PROPERTY(QList<JackPassthrough*> trackPassthroughClients READ trackPassthroughClients CONSTANT)
    /**
     * \brief A list of the 10 lists. Each list contains 5 JackPasstrough clients to be used by each fx lane of a channel
     */
    Q_PROPERTY(QList<QList<JackPassthrough*>> fxPassthroughClients READ fxPassthroughClients CONSTANT)
    /**
     * \brief A list of the 10 lists. Each list contains 5 JackPasstrough clients to be used by each sketch fx lane of a channel
     */
    Q_PROPERTY(QList<QList<JackPassthrough*>> sketchFxPassthroughClients READ sketchFxPassthroughClients CONSTANT)
    /**
     * \brief The amount of Songs in a Zynthbox Sketchpad
     */
    Q_PROPERTY(int sketchpadSongCount READ sketchpadSongCount CONSTANT)
    /**
     * \brief The amount of Tracks in a Zynthbox Song
     */
    Q_PROPERTY(int sketchpadTrackCount READ sketchpadTrackCount CONSTANT)
    /**
     * \brief The amount of slots on a Zynthbox Track (whether it's clips, sound slots, or fx slots)
     */
    Q_PROPERTY(int sketchpadSlotCount READ sketchpadSlotCount CONSTANT)
    /**
     * \brief The number of positions held by a ClipAudioSourcePositionsModel
     */
    Q_PROPERTY(int clipMaximumPositionCount READ clipMaximumPositionCount CONSTANT)
public:
    static Plugin* instance();

    // Delete the methods we dont want to avoid having copies of the singleton class
    Plugin(Plugin const&) = delete;
    void operator=(Plugin const&) = delete;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void shutdown();
    // Called by zynthbox when the configuration in webconf has been changed (for example the midi setup, so our MidiRouter can pick up any changes)
    Q_INVOKABLE void reloadZynthianConfiguration();
    Q_INVOKABLE float dBFromVolume(float vol);
    Q_INVOKABLE te::Engine* getTracktionEngine();
    Q_INVOKABLE void registerTypes(QQmlEngine *engine, const char *uri);
    Q_INVOKABLE void addCreatedClipToMap(ClipAudioSource *clip);
    Q_INVOKABLE void removeCreatedClipFromMap(ClipAudioSource *clip);
    Q_INVOKABLE ClipAudioSource* getClipById(int id);
    Q_INVOKABLE int nextClipId();
    /**
     * \brief Get a text-format timestamp in the format defined by the timeStampFormat property
     * @return A formatted string for the current datetime
     */
    Q_INVOKABLE QString currentTimestamp() const;
    QString timeStampFormat() const;
    void setTimeStampFormat(const QString &timeStampFormat);
    Q_SIGNAL void timeStampFormatChanged();
    JackPassthrough* globalPlaybackClient() const;
    QList<JackPassthrough*> trackMixerClients() const;
    QList<JackPassthrough*> synthPassthroughClients() const;
    QList<JackPassthrough*> trackPassthroughClients() const;
    /**
     * \brief Fetch the specific track passthrough client indicated by the given values
     * @param trackIndex The specific track you wish to fetch a passthrough on
     * @param slotType The type of lane to get the passthrough for: 0 for the synth and sample lanes, 1 for the sketch lanes
     * @param laneIndex The specific lane you wish to fetch a passthrough for: 0 through 4
     * @return The passthrough client for the given values, or null for invalid positions
     */
    JackPassthrough* trackPassthroughClient(const int &trackIndex, const int &slotType, const int &laneIndex) const;
    QList<QList<JackPassthrough*>> fxPassthroughClients() const;
    QList<QList<JackPassthrough*>> sketchFxPassthroughClients() const;
    int sketchpadSongCount() const;
    int sketchpadTrackCount() const;
    int sketchpadSlotCount() const;
    int clipMaximumPositionCount() const;

    QQmlEngine *qmlEngine() const;
private:
    explicit Plugin(QObject *parent = nullptr);

    te::Engine *tracktionEngine{nullptr};
    JuceEventLoop juceEventLoop;
    QHash<int, ClipAudioSource *> createdClipsMap;
    int lastCreatedClipId{-1};
    QString m_timeStampFormat{"yyyyMMdd-HHmm"};
    JackPassthrough* m_globalPlaybackClient{nullptr};
    QList<JackPassthrough*> m_trackMixerClients;
    QList<JackPassthrough*> m_synthPassthroughClients;
    QList<JackPassthrough*> m_trackPassthroughClients;
    QList<QList<JackPassthrough*>> m_fxPassthroughClients;
    QList<QList<JackPassthrough*>> m_sketchFxPassthroughClients;

    static std::atomic<Plugin*> singletonInstance;
    static std::mutex singletonMutex;
    QQmlEngine *m_qmlEngine{nullptr};
};
