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

#ifndef SEQUENCEMODEL_H
#define SEQUENCEMODEL_H

#include <QAbstractListModel>
#include "PlayGridManager.h"

class PatternModel;
class SequenceModel : public QAbstractListModel
{
    Q_OBJECT
    /**
     * \brief The Beats Per Minute set on the song this Sequence was most recently associated with
     */
    Q_PROPERTY(int bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    /**
     * \brief The currently selected pattern
     * This is not functionally important to playback, but important for keeping the state between
     * saves and whatnot
     */
    Q_PROPERTY(int activePattern READ activePattern WRITE setActivePattern NOTIFY activePatternChanged)
    /**
     * \brief The pattern object at the position of the activePattern property
     */
    Q_PROPERTY(QObject* activePatternObject READ activePatternObject NOTIFY activePatternChanged)
    /**
     * \brief Whether or not the sequence is currently included in playback and playing is running
     * That is to say, whether or not the sequence is currently playing
     */
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    /**
     * \brief Sets a reference to the song this Sequence is associated with
     * @note Setting this will cause the Sequence to be reloaded to match that song (and clearing it will load the unassociated sequence)
     */
    Q_PROPERTY(QObject* song READ song WRITE setSong NOTIFY songChanged)
    /**
     * \brief Index of a pattern which will become the only one played
     * This property which will override all other playback settings. That is, the pattern will be
     * played whether or not it is enabled, and no other patterns will be played while there is a
     * pattern set here. Set to the default value (-1) to disable soloing.
     * \default -1
     */
    Q_PROPERTY(int soloPattern READ soloPattern WRITE setSoloPattern NOTIFY soloPatternChanged)
    /**
     * \brief Convenience property for getting the actual pattern set as the solo pattern
     * @see soloPattern
     * \default null
     */
    Q_PROPERTY(QObject* soloPatternObject READ soloPatternObject NOTIFY soloPatternChanged)

    /**
     * \brief The location this Sequence is stored in
     * If this is not set, using save() or load() without passing a filename will create a filename
     * for you (using the current song's location if set, or a generic location if none was set)
     *
     * The file system layout of a Sequence is as follows (allowing for granular upload and control)
     * (sequence name in lower case) (a folder)
     * - metadata.seq.json (a json file containing the basic information about the sequence)
     * - patterns (a folder)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-0.pat.json (a json file containing the notes and other data for the first pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-1.pat.json (a json file containing the notes and other data for the second pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-2.pat.json (a json file containing the notes and other data for the third pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-3.pat.json (a json file containing the notes and other data for the fourth pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-4.pat.json (a json file containing the notes and other data for the fifth pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-5.pat.json (a json file containing the notes and other data for the sixth pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-6.pat.json (a json file containing the notes and other data for the seventh pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-7.pat.json (a json file containing the notes and other data for the eights pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-8.pat.json (a json file containing the notes and other data for the ninth pattern)
     *   - (lower-case-sequence-name-with-dashes-instead-of-spaces)-9.pat.json (a json file containing the notes and other data for the tenth pattern)
     */
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)

    /**
     * \brief Information on the index of the scene this sequence is associated with
     * @default -1
     */
    Q_PROPERTY(int sceneIndex READ sceneIndex WRITE setSceneIndex NOTIFY sceneIndexChanged)
    /**
     * \brief Whether or not this sequence's patterns are supposed to be making any sounds
     * @default True
     */
    Q_PROPERTY(bool shouldMakeSounds READ shouldMakeSounds WRITE setShouldMakeSounds NOTIFY shouldMakeSoundsChanged)

    /**
     * \brief Whether there are unsaved changes in the sequence
     * This will automatically be reset to false when loading and saving
     */
    Q_PROPERTY(bool isDirty READ isDirty WRITE setIsDirty NOTIFY isDirtyChanged)
    /**
     * \brief Whether or not the model is currently loading
     */
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

    /**
     * \brief The number of patterns in this sequence
     */
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    explicit SequenceModel(PlayGridManager *parent = nullptr);
    ~SequenceModel() override;

    enum Roles {
        PatternRole = Qt::UserRole + 1,
        TextRole,
        NameRole,
        LayerRole,
        BankRole,
        PlaybackPositionRole,
        BankPlaybackPositionRole
    };

    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

    /**
     * \brief Get the pattern object for the given position (or null if none exists)
     * @param patternIndex The index of the pattern to fetch from the model
     * @return The PatternModel at the given index, or null
     */
    Q_INVOKABLE QObject *get(int patternIndex) const;
    /**
     * \brief Get the pattern object for the given part in a logical channel
     * The concept here is that each channel is a part of a channel, and a channel can have multiple
     * patterns associated with it. To be able to operate directly on the channel's patterns, you
     * can use this function to fetch them given their positions by channel/part, instead of their
     * linear position in the Sequence.
     * @param channelIndex The index of the logical channel to fetch a pattern in
     * @param partIndex The index of the part in the logical channel for which the pattern should be fetched
     * @return A pattern object (or null if none was found at the given position)
     */
    Q_INVOKABLE QObject *getByPart(int channelIndex, int partIndex) const;
    /**
     * \brief Insert a pattern into the sequence at the desired location (or at the end if undefined)
     * @param pattern The pattern to insert into the model
     * @param row The row at which the pattern should be inserted (if -1, it will be added at the end of the model)
     */
    Q_INVOKABLE void insertPattern(PatternModel *pattern, int row = -1);
    /**
     * \brief Remove a pattern from the sequence
     * @param pattern The pattern that should be removed from the model
     */
    Q_INVOKABLE void removePattern(PatternModel *pattern);
    /**
     * \brief Check whether a pattern already exists in this sequence
     * @return True if the pattern is already in the sequence, false if not
     */
    Q_INVOKABLE bool contains(QObject *pattern) const;
    /**
     * \brief Gets the index of the given model in this sequence (or -1 if not found)
     * @param pattern The pattern you wish to find the index of
     * @return The index of the pattern (or -1 if not found)
     */
    Q_INVOKABLE int indexOf(QObject *pattern) const;

    /**
     * \brief Emitted whenever the number of patterns in the sequence changes
     */
    Q_SIGNAL void countChanged();
    PlayGridManager *playGridManager() const;

    void setBpm(int bpm);
    int bpm() const;
    Q_SIGNAL void bpmChanged();

    void setActivePattern(int activePattern);
    Q_INVOKABLE void setActiveChannel(int channelId, int partId);
    int activePattern() const;
    QObject *activePatternObject() const;
    Q_SIGNAL void activePatternChanged();

    Q_INVOKABLE QString filePath() const;
    Q_INVOKABLE void setFilePath(const QString &filePath);
    Q_SIGNAL void filePathChanged();

    Q_INVOKABLE bool isDirty() const;
    Q_INVOKABLE void setIsDirty(bool isDirty);
    Q_INVOKABLE void setDirty() { setIsDirty(true); };
    Q_SIGNAL void isDirtyChanged();

    bool isLoading() const;
    Q_SIGNAL void isLoadingChanged();

    int sceneIndex() const;
    void setSceneIndex(int sceneIndex);
    Q_SIGNAL void sceneIndexChanged();

    Q_INVOKABLE bool shouldMakeSounds() const;
    Q_INVOKABLE void setShouldMakeSounds(bool shouldMakeSounds);
    Q_SIGNAL void shouldMakeSoundsChanged();

    /**
     * \brief Load the data for this Sequence (and all Patterns contained within it) from the location indicated by filePath if none is given
     * @note Not setting filePath prior to loading will cause a default to be generated
     * @note Passing a filename to this function will reset the filePath property to that name
     * @param fileName An optional filename to be used to perform the operation in place of the automatically chosen one (pass the metadata.seq.json location)
     */
    Q_INVOKABLE void load(const QString &fileName = QString());
    /**
     * \brief Save the data for this Sequence (and all Patterns contained within it) to the location indicated by filePath if none is given
     * @note Not setting filePath prior to saving will cause a default to be generated
     * @note Passing a filename to this function will reset the filePath property to that name
     * @note Any file in the location WILL be overwritten if it already exists
     * @param fileName An optional filename to be used to perform the operation in place of the automatically chosen one (pass the metadata.seq.json location)
     * @param exportOnly If set to true, this will make this function only export the information, and not actually touch the internal state of the sequence
     * @return True if successful, false if not
     */
    Q_INVOKABLE bool save(const QString &fileName = QString(), bool exportOnly = false);

    /**
     * \brief Clear all patterns of all notes
     */
    Q_INVOKABLE void clear();

    void setSong(QObject *song);
    QObject *song() const;
    Q_SIGNAL void songChanged();

    void setSoloPattern(int soloPattern);
    int soloPattern() const;
    PatternModel *soloPatternObject() const;
    Q_SIGNAL void soloPatternChanged();

    /**
     * \brief Set the named property on the pattern with the specified index the given value
     * @param patternIndex The index in the list of the pattern to set a property on
     * @param property The name of the property you wish to change the value of
     * @param value The value you wish to set that property to
     */
    Q_INVOKABLE void setPatternProperty(int patternIndex, const QString &property, const QVariant &value);

    /**
     * \brief Set any note previously turned on using setPositionOn off
     */
    Q_INVOKABLE void setPreviousOff() const;
    /**
     * \brief Set the given position on in all enabled patterns which have the position, and turn all previous off (optionally leave them on)
     * @param row The row you wish to set to on in all enabled patterns (bankOffset will be interpreted by this function per-pattern)
     * @param column The column in the given row you wish to turn on in all enabled patterns
     * @param stopPrevious If true, the function will stop any previous notes set on on the same sequence (default is to do so, pass false to disable this automagic)
     */
    Q_INVOKABLE void setPositionOn(int row, int column, bool stopPrevious = true) const;

    /**
     * \brief Whether or not the sequence is playing
     * @see startSequencePlayback()
     * @see stopSequencePlayback()
     */
    bool isPlaying() const;
    Q_SIGNAL void isPlayingChanged();

    /**
     * \brief Prepares the sequence playback (requiring the global timer to be started manually)
     * @note If the timer is already running, playback will be joined immediately
     */
    Q_INVOKABLE void prepareSequencePlayback();
    /**
     * \brief Prepares the sequence for playback, and starts the global timer
     * @see prepareSequencePlayback()
     * @see resetSequence()
     */
    Q_INVOKABLE void startSequencePlayback();
    /**
     * \brief Disconnects the sequence from the global timer (stops playback of only this Sequence)
     * This function will also attempt to clean up any notes scheduled into the future for this sequence,
     * which means you will want to call this function /before/ starting others, to avoid potential
     * add/remove clashes.
     * @note If the sequence is not playing, this function is safe to call
     */
    Q_INVOKABLE void disconnectSequencePlayback();
    /**
     * \brief Disconnects the sequence from the global timer and stops the global timer
     */
    Q_INVOKABLE void stopSequencePlayback();
    /**
     * \brief Resets the sequence position to zero (will also work during playback)
     */
    Q_INVOKABLE void resetSequence();
    /**
     * \brief Advances the sequence position during playback (usually handled by the internal sequence playback system)
     */
    Q_SLOT void advanceSequence();
    /**
     * \brief Updates the positions in the child PatternModels during playback
     */
    Q_SLOT void updatePatternPositions();
private:
    class Private;
    Private *d;
};

#endif//SEQUENCEMODEL_H
