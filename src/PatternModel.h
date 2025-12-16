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

#ifndef PATTERNMODEL_H
#define PATTERNMODEL_H

#include "NotesModel.h"
#include "SequenceModel.h"
#include "MidiRouter.h"
#include "KeyScales.h"

class ClipCommandRing;
/**
 * \brief A way to keep channel of the notes which make up a conceptual song pattern
 *
 * This specialised NotesModel will always be a square model (that is, all rows have the same width).
 *
 * Each position in the model contains a compound note, and the metadata associated with the subnotes,
 * which is expected to be in a key/value form. If tighter control is required, you can use NotesModel
 * functions. If there are no subnotes for a position, the compound note will be removed. If a subnote
 * is set on a position where there is no compound note, one will be created for you.
 */
class PatternModel : public NotesModel
{
    Q_OBJECT
    /**
     * \brief The SequenceModel this PatternModel instance belongs to
     */
    Q_PROPERTY(QObject* sequence READ sequence CONSTANT)
    /**
     * \brief The index of the sketchpad track this model is associated with
     */
    Q_PROPERTY(int sketchpadTrack READ sketchpadTrack NOTIFY sketchpadTrackChanged)
    /**
     * \brief The index of the clip inside this pattern's associated channel associates this pattern with
     */
    Q_PROPERTY(int clipIndex READ clipIndex NOTIFY clipIndexChanged)
    /**
     * \brief The name of this pattern's associated clip (see clipIndex)
     * This will be a lower-case letter, or an empty string if there's no clip set
     */
    Q_PROPERTY(QString clipName READ clipName NOTIFY clipIndexChanged)
    /**
     * \brief A URL that you can pass to an Image item to display an up-to-date thumbnail of the pattern's current bank
     */
    Q_PROPERTY(QString thumbnailUrl READ thumbnailUrl NOTIFY thumbnailUrlChanged)
    /**
     * \brief A human-readable name for this pattern (removes the parent sequence's name from the objectName if one is set)
     */
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    /**
     * \brief The destination for notes in this pattern (currently either synth or sample)
     * This controls whether this pattern fires notes into the midi world, or whether it uses
     * the pattern to control samples being fired.
     * @default PatternModel::NoteDestination::SynthDestination
     */
    Q_PROPERTY(PatternModel::NoteDestination noteDestination READ noteDestination WRITE setNoteDestination NOTIFY noteDestinationChanged)
    /**
     * \brief The length of each row in the model (similar to column count, but for all rows)
     * @note Setting this to a value smaller than the current state will remove any notes set in the overflow columns
     * @default 16
     */
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    /**
     * \brief The amount of rows in the model (similar to rows, but actively enforced)
     * Active enforcement means that any change outside the given size will cause that change to be aborted
     * @note Setting this to a value smaller than the current state will remove any notes set in the overflow rows)
     * @default 16
     */
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
    /**
     * \brief The midi channel used to send notes to specifically when the note destination is set to ExternalDestination
     * @default -1 (which will be interpreted as "the same as sketchpadTrack")
     */
    Q_PROPERTY(int externalMidiChannel READ externalMidiChannel WRITE setExternalMidiChannel NOTIFY externalMidiChannelChanged)
    /**
     * \brief The velocity which should be given to any newly added note (unless one has been explicitly chosen)
     * @min 1
     * @max 127
     * @default 64
     */
    Q_PROPERTY(int defaultVelocity READ defaultVelocity WRITE setDefaultVelocity NOTIFY defaultVelocityChanged)
    /**
     * \brief The duration which should be any newly added note (by default 0, meaning auto-quantized)
     *
     * By default, notes on a pattern are played back in a quantized fashion, but they can also be given an explicit duration
     * instead. This property should be used to store the duration that any new note gets given (in particular by stepsequencer,
     * but also by any other sequencer which creates patterns).
     *
     * The default, 0, means that newly added notes will be auto-quantized for playback (until they are given an explicit
     * duration of their own)
     *
     * @default 0
     */
    Q_PROPERTY(int defaultNoteDuration READ defaultNoteDuration WRITE setDefaultNoteDuration NOTIFY defaultNoteDurationChanged)
    /**
     * \brief The duration of a single step in 96th of a beat (384th of a bar)
     * @see nextStepLengthStep(double, int)
     * @default 24.0
     */
    Q_PROPERTY(double stepLength READ stepLength WRITE setStepLength NOTIFY stepLengthChanged)
    /**
     * \brief A value in percent of step length defining how far each even step in the pattern will be offset during playback
     * @note If this is set to 0, the value will in fact be set to 0 (being the logical "reset" position)
     * 0 being concurrent with previous step (not a possible position, but it would be that position if it were)
     * 50 being the un-shifted position
     * 100 being concurrent with next step (not a possible position, but it would be that position if it were)
     * @minimum 1
     * @maximum 99
     * @default 50
     */
    Q_PROPERTY(int swing READ swing WRITE setSwing NOTIFY swingChanged)
    /**
     * \brief The number of bars in the pattern which should be considered for playback
     * The minimum number is 1, and the maximum is bankLength
     * @default 1
     */
    Q_PROPERTY(int availableBars READ availableBars NOTIFY patternLengthChanged)
    /**
     * \brief The number of steps in the pattern which should be considered for playback
     * The minimum number is 1, and the maximum is bankLength * width
     * @default 16 (equivalent to bankLength)
     */
    Q_PROPERTY(int patternLength READ patternLength WRITE setPatternLength NOTIFY patternLengthChanged)
    /**
     * \brief Which bar (row) should be considered current
     * This will be clamped to the available range (the lowest value is 0, maximum is height-1)
     * @default 0
     */
    Q_PROPERTY(int activeBar READ activeBar WRITE setActiveBar NOTIFY activeBarChanged)
    /**
     * \brief The alphabetical name of the current bank (an upper case A or B, for example)
     * @default A
     * @see bankOffset
     * @see bankLength
     */
    Q_PROPERTY(QString bank READ bank WRITE setBank NOTIFY bankOffsetChanged)
    /**
     * \brief An offset used to display a subsection of rows (a bank)
     * Default value is 0
     * @see bankLength
     */
    Q_PROPERTY(int bankOffset READ bankOffset WRITE setBankOffset NOTIFY bankOffsetChanged)
    /**
     * \brief The length of a bank (a subset of rows)
     * Default value is 8
     */
    Q_PROPERTY(int bankLength READ bankLength WRITE setBankLength NOTIFY bankLengthChanged)
    /**
     * \brief Whether or not there are any notes defined in the current bank
     */
    Q_PROPERTY(bool currentBankHasNotes READ currentBankHasNotes NOTIFY hasNotesChanged)
    /**
     * \brief Whether or not there are any notes defined on any step in any bank
     */
    Q_PROPERTY(bool hasNotes READ hasNotes NOTIFY hasNotesChanged)
    /**
     * \brief A toggle for setting the pattern to an enabled state (primarily used for playback purposes)
     * @default true
     */
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /**
     * \brief The IDs of the clips being used for the sample trigger and slice note destination modes
     */
    Q_PROPERTY(QVariantList clipIds READ clipIds WRITE setClipIds NOTIFY clipIdsChanged)
    /**
     * \brief The row which most recently had a note scheduled to be played
     * @default 0
     */
    Q_PROPERTY(int playingRow READ playingRow NOTIFY playingRowChanged)
    /**
     * \brief The column which most recently had a note scheduled to be played
     * @default 0
     */
    Q_PROPERTY(int playingColumn READ playingColumn NOTIFY playingColumnChanged)
    /**
     * \brief The global playback position within the pattern
     * This property will be -1 when the Pattern is not being played back
     * If played back, it will be ((playingRow * width) + playingColumn)
     * When using this for displaying a position in the UI, remember to also check the bank
     * to see whether what you are displaying should display the position. You can subtract
     * (bankOffset * width) to find a local position inside your current bank, or you can use
     * bankPlaybackPosition to get this value (though you should ensure to also check whether
     * the bank you are displaying is the one which is currently selected)
     */
    Q_PROPERTY(int playbackPosition READ playbackPosition NOTIFY playingColumnChanged)
    /**
     * \brief The bank-local playback position (see also playbackPosition)
     * This property will contain the value of playbackPosition, but with the subtraction of
     * the bank offset already done for you.
     * @note When using this for displaying positions, make sure to check you are displaying
     * the currently selected bank.
     */
    Q_PROPERTY(int bankPlaybackPosition READ bankPlaybackPosition NOTIFY playingColumnChanged)
    /**
     * \brief Whether or not this pattern is currently included in playback
     * This is essentially the same as performing a check on the parent sequence to see whether
     * that is playing, and then further checking whether this pattern is the current solo channel
     * if one is set, and if none is set then whether the pattern is enabled.
     */
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)

    /**
     * \brief The scale the pattern conceptually wants to use (not auto-enforced)
     * @default KeyScales::ScaleChromatic
     */
    Q_PROPERTY(int scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(KeyScales::Scale scaleKey READ scaleKey WRITE setScaleKey NOTIFY scaleChanged)
    /**
     * \brief The pitch of the key the pattern conceptually wants to use
     * @default KeyScales::KeyC
     */
    Q_PROPERTY(int pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(KeyScales::Pitch pitchKey READ pitchKey WRITE setPitchKey NOTIFY pitchChanged)
    /**
     * \brief The octave of the key the pattern conceptually wants to use
     * @default KeyScales::Octave4
     */
    Q_PROPERTY(int octave READ octave WRITE setOctave NOTIFY octaveChanged)
    Q_PROPERTY(KeyScales::Octave octaveKey READ octaveKey WRITE setOctaveKey NOTIFY octaveChanged)
    /**
     * \brief Whether notes handled by this pattern's clip should be locked (or rewritten) to its given key and scale
     * @default PatternModel::KeyScaleLockOff
     */
    Q_PROPERTY(PatternModel::KeyScaleLockStyle lockToKeyAndScale READ lockToKeyAndScale WRITE setLockToKeyAndScale NOTIFY lockToKeyAndScaleChanged)

    /**
     * \brief The first note used to fill out the grid model
     * @default 48
     */
    Q_PROPERTY(int gridModelStartNote READ gridModelStartNote WRITE setGridModelStartNote NOTIFY gridModelStartNoteChanged)
    /**
     * \brief The last note used to fill out the grid model
     * @default 64
     */
    Q_PROPERTY(int gridModelEndNote READ gridModelEndNote WRITE setGridModelEndNote NOTIFY gridModelEndNoteChanged)
    /**
     * \brief A NotesModel instance which shows a set of rows of notes based on the start and end note properties (in a comfortable spread)
     */
    Q_PROPERTY(QObject* gridModel READ gridModel CONSTANT)
    /**
     * \brief A NotesModel instance which shows appropriate entries for the slices in the clips associated with this pattern
     */
    Q_PROPERTY(QObject* clipSliceNotes READ clipSliceNotes CONSTANT)

    /**
     * \brief Whether or not the pattern will read messages on its channel and write them into the pattern during playback
     * This is a form of loop recording, and it will cause the pattern to change existing notes on a given step when there is
     * already a note with the same note value on that step (so it will change the velocity, duration, and delay of that note,
     * functionally replacing it with the new one)
     * @note This property will be changed to false when playback is stopped
     */
    Q_PROPERTY(bool recordLive READ recordLive WRITE setRecordLive NOTIFY recordLiveChanged)
    /**
     * \brief How tightly to quantize note start and duration points when performing live recording on the pattern
     * The value is 96th of a beat (or 1/384th of a bar), and defines the grid size to which note
     * start and end points are quantized
     *
     * 1 translates to quantizing being disabled (as that is the tightest timing we operate on), 0 (default)
     * will quantize to the pattern's step length, and the maximum of 1536 is a somewhat arbitrarily chosen 4
     * bars worth of steps
     *
     * @default 0 meaning the same as the step length
     * @minimum 0
     * @maximum 1536
     */
    Q_PROPERTY(int liveRecordingQuantizingAmount READ liveRecordingQuantizingAmount WRITE setLiveRecordingQuantizingAmount NOTIFY liveRecordingQuantizingAmountChanged)
    /**
     * \brief What source live recording should listen for midi events on (defaults to the pattern's associated track, and is not persisted)
     * This can be one of the following:
     * * sketchpadTrack:-1 - for the "current" track (the default - will also be be used as fallback if given a value that is not supported)
     * * sketchpadTrack:(0 through 9) - for a specific sketchpad track's output
     * * external:(a device index) - for an external device with the given identifying index
     * @default ""
     */
    Q_PROPERTY(QString liveRecordingSource READ liveRecordingSource WRITE setLiveRecordingSource NOTIFY liveRecordingSourceChanged)

    /**
     * \brief A reference to the model that should be used for changing the contents, depending on whether or not performance mode is active
     * If performanceActive is true, this will be the performanceClone
     * If performanceActive is false, this will be the main model
     * Essentially a convenience trick to avoid having to check which model should be operated on in the sequencer UI
     */
    Q_PROPERTY(QObject* workingModel READ workingModel NOTIFY performanceActiveChanged)
    /**
     * \brief A reference to the model's performance clone. If this is a performance clone, the value will be null
     */
    Q_PROPERTY(QObject* performanceClone READ performanceClone NOTIFY performanceCloneChanged)
    /**
     * \brief Whether or not the performance clone is active
     */
    Q_PROPERTY(bool performanceActive READ performanceActive NOTIFY performanceActiveChanged)

    /**
     * \brief A reference to the sketchpad Channel object this Pattern is associated with
     */
    Q_PROPERTY(QObject* zlChannel READ zlChannel WRITE setZlChannel NOTIFY zlChannelChanged)
    /**
     * \brief A reference to the sketchpad Clip object this Pattern is associated with
     */
    Q_PROPERTY(QObject* zlClip READ zlClip WRITE setZlClip NOTIFY zlClipChanged)
    /**
     * \brief A reference to the sketchpad Scene object this Pattern is associated with
     */
    Q_PROPERTY(QObject* zlScene READ zlScene WRITE setZlScene NOTIFY zlSceneChanged)
public:
    explicit PatternModel(SequenceModel* parent = nullptr);
    ~PatternModel() override;

    enum NoteDestination {
        SynthDestination = 0,
        SampleTriggerDestination = 1,
        SampleLoopedDestination = 2,
        ExternalDestination = 3,
    };
    Q_ENUM(NoteDestination)

    /**
     * \brief Clear this pattern and replace all contents and settings with those contained in the given pattern
     * @param otherPattern The pattern whose details you want to clone into this one
     */
    Q_INVOKABLE void cloneOther(PatternModel *otherPattern);

    /**
     * \brief The subnote position of the note with the given midi note value in the requested position in the model
     * @param row The row you wish to check in
     * @param column The column in the row you wish to check in
     * @param midiNote The note value you wish to check
     * @return The index of the subnote, or -1 if not found
     */
    Q_INVOKABLE int subnoteIndex(int row, int column, int midiNote) const;
    /**
     * \brief Add a new entry to the position
     * @param row The row you wish to add a new entry in
     * @param column The column in that row you wish to add a new entry into
     * @param note The note you wish to add to this position
     * @return The subnote position of the newly added note (for convenience with e.g. setEntryMetadata)
     */
    Q_INVOKABLE int addSubnote(int row, int column, QObject* note);
    /**
     * \brief Add a new subnote at the given subnote index
     * @note This also inserts an empty metadata entry at the same position
     * @param row The row you wish to add a new entry in
     * @param column The column in that row you wish to add a new entry into
     * @param subnoteIndex The position at which you wish to insert a subnote (if invalid, it will be appended)
     * @param note The note you wish to add to this position
     */
    Q_INVOKABLE void insertSubnote(int row, int column, int subnoteIndex, QObject* note);
    /**
     * \brief Add a new subnote at the position suggested by its midi note
     * @note The logic is such that the note will be inserted before the first note that has a higher number (or at the end if only lower ones exist)
     * @param row The row you wish to add a new entry in
     * @param column The column in that row you wish to add a new entry into
     * @param note The note you wish to add to this position
     * @return The subnote position of the newly added note (for convenience with e.g. setSubnoteMetadata)
     */
    Q_INVOKABLE int insertSubnoteSorted(int row, int column, QObject* note);

    /**
     * \brief Remove the entry at the given position in the model
     * @param row The row you wish to look in
     * @param column The column you wish to look at in that row
     * @param subnote The specific entry in that location's list of values that you wish to remove
     */
    Q_INVOKABLE void removeSubnote(int row, int column, int subnote);

    /**
     * \brief Remove all entries with any of the given note values from the model, in the given range of steps
     * @param noteValues The midi note values of the notes to be removed (anything that is not a valid integer between 0 and 127 will be ignored)
     * @param firstStep The index of the first step to remove that note from
     * @param lastStep The index of the last step to remove that note from
     */
    Q_INVOKABLE void removeSubnotesByNoteValue(const QVariantList &noteValues, const int &firstStep, const int &lastStep);
    /**
     * \brief Remove all entries with the given note value from the model, in the given range of steps
     * @param noteValue The midi note value of the note to be removed
     * @param firstStep The index of the first step to remove that note from
     * @param lastStep The index of the last step to remove that note from
     */
    Q_INVOKABLE void removeSubnoteByNoteValue(const int &noteValue, const int &firstStep, const int &lastStep);

    /**
     * \brief Set the specified metadata key to the given value for the given position
     * @param row The row you wish to look int
     * @param column The column in the given row you wish to look in
     * @param subnote The specific entry in that location's list of values that you wish to set metadata for
     * @param key The name of the specific metadata you wish to set the value for
     * @param value The new value you wish to set for the given key (pass an invalid variant to remove the key from the list)
     */
    Q_INVOKABLE void setSubnoteMetadata(int row, int column, int subnote, const QString &key, const QVariant &value);

    /**
     * \brief Get the metadata value for the specified key at the given position in the model
     * @param row The row you wish to look in
     * @param column The column in the given row you wish to look in
     * @param subnote The specific entry in that location's list of values that you wish to retrieve metadata from
     * @param key The key of the metadata you wish to fetch the value for. Pass an empty string to be given the entire hash
     * @return The requested metadata (or an invalid variant if none was found)
     */
    Q_INVOKABLE QVariant subnoteMetadata(int row, int column, int subnote, const QString &key);

    /**
     * \brief Set the indicated position in the model to the given note
     * @note This function (and setMetadata) is vital and if you must change notes in ways that are not covered in other
     * PatternModel functions, use this, not the ones on NotesModel, otherwise playback will not work correctly!
     * This sets a specified location to contain the Note object passed to the function. If the location does
     * not yet exist, rows will be appended to the model until there are that many rows, and column added to
     * the row until the position exists. Clearing the position will not remove the position from the model.
     * @note Not valid on child models (see parentModel())
     * @param row The row of the position to set to the given note
     * @param column The column of the position to set to the given note
     * @param note The new note to be set in the specified location (this may be null, to clear the position)
     */
    Q_INVOKABLE void setNote(int row, int column, QObject *note) override;

    /**
     * \brief Set an abstract piece of metadata for the given position
     * @note This function (and setNote) is vital and if you must change metadata in ways that are not covered in other
     * PatternModel functions, use this, not the ones on NotesModel, otherwise playback will not work correctly!
     * @note Not valid on child models (see parentModel())
     * @param row The row of the position to set metadata for
     * @param column The column of the position to set the metadata for
     * @param metadata The piece of metadata you wish to set
     */
    Q_INVOKABLE void setMetadata(int row, int column, QVariant metadata) override;

    /**
     * \brief Send the data for the given step to SyncTimer
     * Useful for performing test plays of a step's data
     * @param step The index of the step you wish to play
     */
    Q_INVOKABLE void playStep(const int &step);

    /**
     * \brief Move the steps between firstStep and lastStep by the given amount, shifting overflow to the opposite end of the range
     * @param firstStep The first step of the range of steps to nudge
     * @param lastStep the last step of the range of steps to nudge
     * @param amount The number of steps to nudge the steps (can be either positive or negative)
     * @param noteFilter If defined, this should be a list of midi note values (anything outside [0;127] will be ignored), and only those notes will be nudged
     */
    Q_INVOKABLE void nudge(int firstStep, int lastStep, int amount, const QVariantList &noteFilter = {});

    /**
     * \brief Resets all the model's content-related properties to their defaults
     *
     * The properties which are reset by this function are:
     * - noteDestination
     * - width
     * - height
     * - externalMidiChannel
     * - defaultNoteDuration
     * - stepLength
     * - swing
     * - patternLength
     * - bankOffset (and consequently bank)
     * - bankLength
     * - gridModelStartNote
     * - gridModelEndNote
     * - scale
     * - pitch
     * - octave
     *
     * @param clearNotes If set to true, the function will also clear notes (otherwise it will leave those alone that fit inside the default height/width)
     */
    Q_INVOKABLE void resetPattern(bool clearNotes = false);
    /**
     * \brief Removes all notes and metadata from the model
     */
    Q_INVOKABLE void clear() override;

    /**
     * \brief Removes all notes and metadata from the given row (if it exists)
     * @param row The row that you wish to clear of all data
     */
    Q_INVOKABLE void clearRow(int row);

    /**
     * \brief Clear all the rows in the given bank
     * @param bank The index of the bank (being the bank starting at the row bank * bankLength)
     */
    Q_INVOKABLE void clearBank(int bank);

    /**
     * \brief This will export a json representation of the pattern to a file with the given filename
     * @note This will overwrite anything that already exists in that location without warning
     * @param fileName The file you wish to write the pattern's json representation to
     * @return True if the file was successfully written, otherwise false
     */
    Q_INVOKABLE bool exportToFile(const QString &fileName) const;

    QObject* sequence() const;
    /**
     * \brief The index of the sketchpad track this pattern belongs to
     * @return The index of the sketchpad track this pattern belongs to (-1 if not assigned)
     */
    int sketchpadTrack() const;
    /**
     * \brief Set the index of the sketchpad track this pattern belongs to
     * @note This value is not checked, and multiple pattern objects might have the same value (though usually won't)
     * @param sketchpadTrack The index of the sketchpad track this pattern belongs to
     */
    void setSketchpadTrack(int sketchpadTrack);
    Q_SIGNAL void sketchpadTrackChanged();
    /**
     * \brief The index of the clip of the sketchpad track this pattern belongs to
     * @return The index of the clip of the sketchpad track this pattern belongs to (-1 if note assigned)
     */
    int clipIndex() const;
    QString clipName() const;
    /**
     * \brief Set the index of the clip on the sketchpad track this pattern belongs to
     * @note This value is not checked and multiple patterns in the same channel can carry the same index (though usually won't)
     * @param The index of the clip on the sketchpad track this pattern belongs to
     */
    void setClipIndex(int clipIndex);
    Q_SIGNAL void clipIndexChanged();

    QString thumbnailUrl() const;
    Q_SIGNAL void thumbnailUrlChanged();

    QString name() const;
    Q_SIGNAL void nameChanged();

    PatternModel::NoteDestination noteDestination() const;
    void setNoteDestination(const PatternModel::NoteDestination &noteDestination);
    Q_SIGNAL void noteDestinationChanged();

    int width() const;
    void setWidth(int width);
    Q_SIGNAL void widthChanged();

    int height() const;
    void setHeight(int height);
    Q_SIGNAL void heightChanged();

    void setExternalMidiChannel(int externalMidiChannel);
    int externalMidiChannel() const;
    Q_SIGNAL void externalMidiChannelChanged();

    void setDefaultVelocity(const int &defaultVelocity);
    int defaultVelocity() const;
    Q_SIGNAL void defaultVelocityChanged();

    void setDefaultNoteDuration(int defaultNoteDuration);
    int defaultNoteDuration() const;
    Q_SIGNAL void defaultNoteDurationChanged();

    void setStepLength(const double &stepLength);
    double stepLength() const;
    Q_SIGNAL void stepLengthChanged();
    /**
     * \brief Get a reasonable human-readable name for the given step length
     * @param stepLength The interval you wish to get a human-readable name for
     * @return A human-readable name for the given step length
     */
    Q_INVOKABLE QString stepLengthName(const double &stepLength) const;
    /**
     * \brief The next item in the given direction from the given starting point on a list of pre-defined step lengths
     * If the starting point is a known step, the next step will be returned. If it is between two steps in the
     * list, the nearest step in the given direction is given. If the starting point is outside the range of the
     * list, the starting point itself will be returned.
     * @param startingPoint Some number (which should reasonably be the current value of a pattern's stepLength property)
     * @param direction A positive value to go up in the list, and zero or negative to go down
     * @return The next step in the list from the given position
     */
    Q_INVOKABLE double nextStepLengthStep(const double &startingPoint, const int &direction) const;

    void setSwing(int swing);
    int swing() const;
    Q_SIGNAL void swingChanged();

    int availableBars() const;
    void setPatternLength(const int &patternLength);
    int patternLength() const;
    Q_SIGNAL void patternLengthChanged();

    void setActiveBar(int activeBar);
    int activeBar() const;
    Q_SIGNAL void activeBarChanged();

    void setBank(const QString& bank);
    QString bank() const;

    void setBankOffset(int bankOffset);
    int bankOffset() const;
    Q_SIGNAL void bankOffsetChanged();

    void setBankLength(int bankLength);
    int bankLength() const;
    Q_SIGNAL void bankLengthChanged();

    /**
     * \brief Whether the given bank contains any notes at all
     * In QML, you can "bind" to this by using the trick that the lastModified property changes. For example,
     * you might do something like:
     * <code>
       enabled: pattern.lastModified > -1 ? pattern.bankHasNotes(bankIndex) : pattern.bankHasNotes(bankIndex)
     * </code>
     * which will update the enabled property when lastModified changes, and also prefill it on the first run.
     * @param bankIndex The index of the bank to check for notes
     * @return True if the bank at the given index contains any notes
     */
    Q_INVOKABLE bool bankHasNotes(int bankIndex) const;

    /**
     * \brief Whether any bank in the model contains notes
     * @see bankHasNotes(int)
     */
    Q_INVOKABLE bool hasNotes() const;

    bool currentBankHasNotes() const;
    Q_SIGNAL void hasNotesChanged();

    /**
     * \brief Whether any bank in the model contains notes, or any persisted setting is different from its default)
     * @note To catch updates on this, listen to NotesModel::lastModifiedChanged
     * @return True if there are notes, or any persisted property is non-default
     */
    Q_INVOKABLE bool hasContent() const;

    void setEnabled(bool enabled);
    bool enabled() const;
    Q_SIGNAL void enabledChanged();

    void setClipIds(const QVariantList &ids);
    QVariantList clipIds() const;
    Q_SIGNAL void clipIdsChanged();
    QObject *clipSliceNotes() const;

    int scale() const;
    KeyScales::Scale scaleKey() const;
    void setScale(int scale);
    void setScaleKey(const KeyScales::Scale &scale);
    Q_SIGNAL void scaleChanged();
    int pitch() const;
    KeyScales::Pitch pitchKey() const;
    void setPitch(int pitch);
    void setPitchKey(const KeyScales::Pitch &pitch);
    Q_SIGNAL void pitchChanged();
    int octave() const;
    KeyScales::Octave octaveKey() const;
    void setOctave(int octave);
    void setOctaveKey(const KeyScales::Octave &octave);
    Q_SIGNAL void octaveChanged();
    enum KeyScaleLockStyle {
        KeyScaleLockOff, ///@< All notes will be accepted as their original self
        KeyScaleLockBlock, ///@< Any note which doesn't match the pattern's key and scale will be blocked
        KeyScaleLockRewrite, ///@< Any note which doesn't match the pattern's key and scale will be rewritten to match
    };
    Q_ENUM(KeyScaleLockStyle)
    KeyScaleLockStyle lockToKeyAndScale() const;
    void setLockToKeyAndScale(const KeyScaleLockStyle &lockToKeyAndScale);
    Q_SIGNAL void lockToKeyAndScaleChanged();

    int gridModelStartNote() const;
    void setGridModelStartNote(int gridModelStartNote);
    Q_SIGNAL void gridModelStartNoteChanged();
    int gridModelEndNote() const;
    void setGridModelEndNote(int gridModelEndNote);
    Q_SIGNAL void gridModelEndNoteChanged();
    QObject *gridModel() const;

    void setRecordLive(bool recordLive);
    bool recordLive() const;
    Q_SIGNAL void recordLiveChanged();
    void setLiveRecordingQuantizingAmount(const int &liveRecordingQuantizingAmount);
    int liveRecordingQuantizingAmount() const;
    Q_SIGNAL void liveRecordingQuantizingAmountChanged();
    void setLiveRecordingSource(const QString &newLiveRecordingSource);
    QString liveRecordingSource() const;
    Q_SIGNAL void liveRecordingSourceChanged();

    /**
     * \brief Begin using the performance clone
     * This will clone the current state of the pattern onto the performance clone, and mark the performance as active
     * @note If called on a performance clone, this will have no effect
     * @note If called while a performance is active, it will discard the current clone state and reapply the pattern's current state
     */
    Q_INVOKABLE void startPerformance();
    /**
     * \brief Apply the current state of the performance clone onto the pattern
     * @note Technically you can do this any time before the next call to startPerformance, but should optimally be done immediately before or after stopPerformance
     * @note If called on a performance clone, this will have no effect
     */
    Q_INVOKABLE void applyPerformance();
    /**
     * \brief Stop using the performance clone
     * @note If called on a performance clone, this will have no effect
     */
    Q_INVOKABLE void stopPerformance();
    QObject *workingModel();
    QObject *performanceClone() const;
    Q_SIGNAL void performanceCloneChanged();
    bool performanceActive() const;
    Q_SIGNAL void performanceActiveChanged();

    QObject *zlChannel() const;
    void setZlChannel(QObject *zlChannel);
    Q_SIGNAL void zlChannelChanged();
    QObject *zlClip() const;
    void setZlClip(QObject *zlClip);
    Q_SIGNAL void zlClipChanged();
    QObject *zlScene() const;
    void setZlScene(QObject *zlScene);
    Q_SIGNAL void zlSceneChanged();

    int playingRow() const;
    Q_SIGNAL void playingRowChanged();
    int playingColumn() const;
    Q_SIGNAL void playingColumnChanged();
    int playbackPosition() const;
    int bankPlaybackPosition() const;

    bool isPlaying() const;
    Q_SIGNAL void isPlayingChanged();

    /**
     * \brief Used by SequenceModel to advance the sequence position during playback
     *
     * Schedules notes to be set on and off depending on the sequence position and the note length
     * of this Pattern (notes will be scheduled for on/off on the beat preceding their location in
     * the Pattern, to ensure the lowest possible latency)
     * @param sequencePosition The position in the sequence that should be considered (literally a count of ticks)
     * @param progressionLength The number of ticks until the next position (that is, how many ticks between this and the next call of the function)
     */
    void handleSequenceAdvancement(qint64 sequencePosition, int progressionLength) const;
    /**
     * \brief Used by SequenceModel to update its patterns' positions to the actual sequence playback position during playback
     *
     * @param sequencePosition The position in the sequence that should be considered
     */
    void updateSequencePosition(qint64 sequencePosition);
    /**
     * \brief When turning off playback, this function will turn off any notes that are waiting to be turned off
     */
    void handleSequenceStop();

    Q_SLOT void handleMidiMessage(const MidiRouter::ListenerPort &port, const quint64 &timestamp, const unsigned char &byte1, const unsigned char &byte2, const unsigned char &byte3, const int& sketchpadTrack, const QString& hardwareDeviceId);
private:
    explicit PatternModel(PatternModel* parent);
    friend class ZLPatternSynchronisationManager;
    class Private;
    Private *d;
};
Q_DECLARE_METATYPE(PatternModel*)
Q_DECLARE_METATYPE(PatternModel::NoteDestination)
Q_DECLARE_METATYPE(PatternModel::KeyScaleLockStyle)

#endif//PATTERNMODEL_H
