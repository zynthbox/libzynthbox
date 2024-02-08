/*
 * Copyright (C) 20214 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#ifndef PLAYFIELDMANAGER_H
#define PLAYFIELDMANAGER_H

#include <QObject>
#include <QCoreApplication>

class PlayfieldManagerPrivate;
/**
 * \brief A singleton class designed to manage the playfield's state, primarily during live performance play
 *
 * This is a central location which holds and manages the playfield information:
 * * Which clips are currently playing
 * * Which clips will be playing in the next bar
 * * Methods for setting clips to play or not, both immediately and in the next bar
 * * Signals to listen to for when the playfield information changes
 */
class PlayfieldManager : public QObject
{
    Q_OBJECT
    /**
     * \brief Sets a reference to the currently active sketchpad
     */
    Q_PROPERTY(QObject* sketchpad READ sketchpad WRITE setSketchpad NOTIFY sketchpadChanged)
public:
    static PlayfieldManager* instance() {
        static PlayfieldManager* instance{nullptr};
        if (!instance) {
            instance = new PlayfieldManager(qApp);
        }
        return instance;
    };
    explicit PlayfieldManager(QObject *parent = nullptr);
    ~PlayfieldManager() override;

    void setSketchpad(QObject *sketchpad);
    QObject *sketchpad() const;
    Q_SIGNAL void sketchpadChanged();

    enum PlaybackState {
        StoppedState,
        PlayingState
    };
    Q_ENUM(PlaybackState)

    enum PlayfieldStatePosition {
        CurrentPosition, ///<@ The current state of the playfield
        NextBarPosition ///<@ The state as it will be when the next bar arrives
    };
    Q_ENUM(PlayfieldStatePosition)

    /**
     * \brief Set whether or not a clip should play (optionally immediately)
     *
     * @param sketchpadSong The song in the sketchpad that should have its state changed (this will invariably be 0 at the moment)
     * @param sketchpadTrack The sketchpad track the clip is on
     * @param clip The clip in the track whose state needs changing
     * @param newState The state the clip should be changed to
     * @param position What position to set the state for (if set to CurrentPosition, we don't guarantee clip alignment)
     * @param offset Set the offset at the given position explicitly (only applies when setting CurrentPosition)
     */
    Q_INVOKABLE void setClipPlaystate(const int &sketchpadSong, const int &sketchpadTrack, const int &clip, const PlaybackState &newState, const PlayfieldStatePosition &position = NextBarPosition, const qint64 &offset = -1);

    /**
     * \brief The current state of the given clip (optionally the scheduled one)
     * @param sketchpadSong The song in the sketchpad that should have its state changed (this will invariably be 0 at the moment)
     * @param sketchpadTrack The sketchpad track the clip is on
     * @param clip The clip in the track whose state needs changing
     * @param position What position you want the state for
     */
    Q_INVOKABLE const PlaybackState clipPlaystate(const int &sketchpadSong, const int &sketchpadTrack, const int &clip, const PlayfieldStatePosition &position = CurrentPosition) const;

    /**
     * \brief The number of timer ticks the playback of the clip is offset
     * This is used to ensure that pattern playback, when triggered during playback, will happen aligned to the beat
     * @param sketchpadSong The song in the sketchpad that should have its state changed (this will invariably be 0 at the moment)
     * @param sketchpadTrack The sketchpad track the clip is on
     * @param clip The clip in the track whose state needs changing
     * @return The playback offset of the clip in timer ticks (essentially, at which tick did the clip start playback)
     */
    Q_INVOKABLE const qint64 clipOffset(const int &sketchpadSong, const int &sketchpadTrack, const int &clip) const;

    /**
     * \brief Emitted after the playfield state has changed
     * @note This signal is emitted in a queued fashion, and should ONLY be used for visual feedback, not playback management
     * @param sketchpadSong The song in the sketchpad that has changed (this will invariably be 0 at the moment)
     * @param sketchpadTrack The sketchpad track the clip is on
     * @param clip The clip in the track whose state has changed
     * @param position Which position has changed state (the value will be one of PlayfieldStatePosition)
     */
    Q_SIGNAL void playfieldStateChanged(const int &sketchpadSong, const int &sketchpadTrack, const int &clip, const int &position);

    void startPlayback();
    void progressPlayback();
    void stopPlayback();

    /**
     * \brief Emitted when the playfield state is changed
     * @note DANGER Note that this is called directly from the process manager. Unless you must have the information immediately, use playfieldStateChanged
     * @param sketchpadSong The song in the sketchpad that has changed (this will invariably be 0 at the moment)
     * @param sketchpadTrack The sketchpad track the clip is on
     * @param clip The clip in the track whose state has changed
     * @param position Which position has changed state (the value will be one of PlayfieldStatePosition)
     */
    Q_SIGNAL void directPlayfieldStateChanged(const int &sketchpadSong, const int &sketchpadTrack, const int &clip, const int &position);
private:
    PlayfieldManagerPrivate *d;
};

#endif//PLAYFIELDMANAGER_H
