/*
 * Copyright (C) 2022 Dan Leinir Turthra Jensen <admin@leinir.dk>
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

#ifndef SEGMENTHANDLER_H
#define SEGMENTHANDLER_H

#include <QObject>
#include <QCoreApplication>

class SegmentHandlerPrivate;
/**
 * \brief A method for handling song-style playback, based on the Sketchpad Segments data
 */
class SegmentHandler : public QObject
{
    Q_OBJECT
    /**
     * \brief Sets a reference to the currently active song
     */
    Q_PROPERTY(QObject* song READ song WRITE setSong NOTIFY songChanged)
    /**
     * \brief Whether or not we are in song mode (or, in other words, whether SegmentHandler should be used for playback logic)
     */
    Q_PROPERTY(bool songMode READ songMode NOTIFY songModeChanged)
    /**
     * \brief The current local playhead position for SegmentHandler
     */
    Q_PROPERTY(int playhead READ playhead NOTIFY playheadChanged)
    /**
     * \brief The total duration of the current set of segments
     */
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    /**
     * \brief The index of the segment at the current playhead position
     */
    Q_PROPERTY(int playheadSegment READ playheadSegment NOTIFY playheadSegmentChanged)
public:
    static SegmentHandler* instance() {
        static SegmentHandler* instance{nullptr};
        if (!instance) {
            instance = new SegmentHandler(qApp);
        }
        return instance;
    };
    explicit SegmentHandler(QObject *parent = nullptr);
    ~SegmentHandler() override;

    void setSong(QObject *song);
    QObject *song() const;
    Q_SIGNAL void songChanged();

    bool songMode() const;
    Q_SIGNAL void songModeChanged();

    int playhead() const;
    Q_SIGNAL void playheadChanged();

    qint64 duration() const;
    Q_SIGNAL void durationChanged();

    int playheadSegment() const;
    Q_SIGNAL void playheadSegmentChanged();

    /**
     * \brief Starts playback at the given offset
     * Starting playback won't attempt to be overly clever, and will start off with everything
     * disabled, and then apply the sequence on/off states as it progresses.
     * @param startOffset An offset in timer ticks (e.g. beat * syncTimer.getMultiplier()) to start playback at
     * @param duration How long to play for (a duration of 0 - the default - will keep playing until the end of the song)
     */
    Q_INVOKABLE void startPlayback(qint64 startOffset = 0, quint64 duration = 0);
    /**
     * \brief The offset position which was passed to startPlayback
     */
    Q_INVOKABLE qint64 startOffset() const;
    /**
     * \brief Stops playback
     */
    Q_INVOKABLE void stopPlayback();

    /**
     * \brief Called explicitly by PlayGridManager, to ensure SegmentHandler's progression happens at the right point
     */
    void progressPlayback() const;
private:
    SegmentHandlerPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(SegmentHandler*)

#endif//SEGMENTHANDLER_H
