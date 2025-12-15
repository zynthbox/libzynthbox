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

#ifndef NOTE_H
#define NOTE_H

#include <QObject>
#include "PlayGridManager.h"

class Note : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(int sketchpadTrack READ sketchpadTrack NOTIFY sketchpadTrackChanged)
    Q_PROPERTY(int midiNote READ midiNote NOTIFY midiNoteChanged)
    Q_PROPERTY(int octave READ octave NOTIFY midiNoteChanged)
    Q_PROPERTY(int activeChannel READ activeChannel NOTIFY activeChannelChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(int pitch READ pitch NOTIFY pitchChanged)
    Q_PROPERTY(int polyphonicAftertouch READ polyphonicAftertouch NOTIFY polyphonicAftertouchChanged)
    Q_PROPERTY(QVariantList subnotes READ subnotes WRITE setSubnotes NOTIFY subnotesChanged)
    // This is arbitrary metadata... do we want to keep this?
    Q_PROPERTY(int scaleIndex READ scaleIndex WRITE setScaleIndex NOTIFY scaleIndexChanged)
public:
    explicit Note(PlayGridManager *parent = nullptr);
    ~Note() override;

    void setName(const QString& name);
    QString name() const;
    Q_SIGNAL void nameChanged();

    void setMidiNote(int midiNote);
    int midiNote() const;
    int octave() const;
    Q_SIGNAL void midiNoteChanged();

    void setSketchpadTrack(const int &sketchpadTrack);
    int sketchpadTrack() const;
    Q_SIGNAL void sketchpadTrackChanged();

    /**
     * \brief The midi channel on which the note is active, or -1 when the note is not active
     * If the note has been activated more than once, this will be the most recent channel
     */
    int activeChannel() const;
    Q_SIGNAL void activeChannelChanged();

    void resetRegistrations();
    void registerOn(const int &midiChannel);
    void registerOff(const int &midiChannel);

    bool isPlaying() const;
    Q_SIGNAL void isPlayingChanged();

    void setSubnotes(const QVariantList& subnotes);
    QVariantList subnotes() const;
    Q_SIGNAL void subnotesChanged();

    void setScaleIndex(int scaleIndex);
    int scaleIndex() const;
    Q_SIGNAL void scaleIndexChanged();

    Q_INVOKABLE void setSubnotesOn(const QVariantList &velocities);
    Q_INVOKABLE void setOn(int velocity = 64);
    Q_INVOKABLE void setOff();

    void registerPitchChange(const int &pitch);
    int pitch() const;
    Q_SIGNAL void pitchChanged();
    Q_INVOKABLE void sendPitchChange(const int &pitch);

    void registerPolyphonicAftertouch(const int &polyphonicAftertouch);
    int polyphonicAftertouch() const;
    Q_SIGNAL void polyphonicAftertouchChanged();
    Q_INVOKABLE void sendPolyphonicAftertouch(const int &polyphonicAftertouch);
private:
    class Private;
    Private* d;
};

#endif//NOTE_H
