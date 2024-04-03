#pragma once

#include <QObject>
#include <QCoreApplication>

/**
 * \brief Utility singleton class designed to handle working with scale and key
 */
class KeyScales : public QObject {
    Q_OBJECT
public:
    static KeyScales* instance() {
        static KeyScales* instance{nullptr};
        if (!instance) {
            instance = new KeyScales(qApp);
        }
        return instance;
    };
    explicit KeyScales(QObject *parent = nullptr);
    ~KeyScales() override;

    /**
     * \brief A representation of the keys found in the Diatonic scale
     */
    enum Pitch {
        PitchC,
        PitchCSharp,
        PitchDFlat,
        PitchD,
        PitchDSharp,
        PitchEFlat,
        PitchE,
        PitchF,
        PitchFSharp,
        PitchGFlat,
        PitchG,
        PitchGSharp,
        PitchAFlat,
        PitchA,
        PitchASharp,
        PitchBFlat,
        PitchB,
    };
    Q_ENUM(Pitch)
    /**
     * \brief The human readable name for the given Pitch enumerator value
     * @param key The value you wish to get the human readable name for
     * @return They human readable name for the given value
     */
    Q_INVOKABLE QString pitchName(const Pitch &key) const;
    /**
     * \brief The enumerator entry at the given index in the Pitch enumerator
     * @param index The index to get the enumerator entry for
     * @return The enumerator entry at the given index (will return KeyC for any invalid index)
     * @see keyNames()
     */
    Q_INVOKABLE Pitch pitchIndexToEnumKey(const int& index) const;
    /**
     * \brief The index of the given entry in the Pitch enumerator
     * @param entry The key of the entry you wish to get the index of
     * @return The index of the given enumerator key
     */
    Q_INVOKABLE int pitchEnumKeyToIndex(const Pitch &entry) const;
    /**
     * \brief All the human readable names of the Pitch enumerator
     * @return A list of all the human readable names for all entries in the Pitch enumerator
     * @see keyIndexToValue(int)
     */
    Q_INVOKABLE QStringList pitchNames() const;
    /**
     * \brief Get a string shorthand for the given pitch
     * @note This is for persistence use and is guaranteed stable across releases
     * @param entry The key of the entry you wish to get a stable shorthand for
     * @return The shorthand for the given pitch entry
     */
    Q_INVOKABLE QString pitchShorthand(const Pitch &entry) const;
    /**
     * \brief Get the Pitch enumerator key for the given shorthand
     * @see pitchShorthand(const Pitch&)
     * @param shorthand The string shorthand which you want to get the enumerator key for
     * @return The Pitch enumerator key for the given shorthand (or PitchC for an invalid or unknown shorthands)
     */
    Q_INVOKABLE Pitch pitchShorthandToKey(const QString &shorthand) const;
    /**
     * \brief Get the Pitch enumerator key for a given midi note
     * @param midiNote The midi note you want to get the Pitch enumerator key for (will be clamped to within the midi note value range)
     * @return The Pitch enumerator key for the given shorthand
     */
    Q_INVOKABLE Pitch midiNoteToPitch(const int &midiNote) const;

    /**
     * \brief A representation of the scales Zynthbox understands how to work with
     */
    enum Scale {
        ScaleAdonaiMalakh,
        ScaleAeolian,
        ScaleAlgerian,
        ScaleAugmented,
        ScaleBeebopDominant,
        ScaleBlues,
        ScaleChromatic,
        ScaleDorian,
        ScaleDoubleHarmonic,
        ScaleEnigmatic,
        ScaleFlamenco,
        ScaleGypsy,
        ScaleHalfDiminished,
        ScaleHarmonicMajor,
        ScaleHarmonicMinor,
        ScaleHarmonics,
        ScaleHirajoshi,
        ScaleHungarianMajor,
        ScaleHungarianMinor,
        ScaleIn,
        ScaleInsen,
        ScaleIonian,
        ScaleIstrian,
        ScaleIwato,
        ScaleLydian,
        ScaleLydianAugmented,
        ScaleLydianDiminished,
        ScaleLydianDominant,
        ScaleLocrian,
        ScaleMajor,
        ScaleMajorBebop,
        ScaleMajorLocrian,
        ScaleMajorPentatonic,
        ScaleMelodicMinorAscending,
        ScaleMelodicMinorDescending,
        ScaleMelodicMinorAscendingDescending,
        ScaleMelodicMinorDescendingAscending,
        ScaleMinorPentatonic,
        ScaleMixolydian,
        ScaleNaturalMinor,
        ScaleNeopolitanMajor,
        ScaleNeopolitanMinor,
        ScalePersian,
        ScalePhrygian,
        ScalePhrygianDominant,
        ScalePrometheus,
        ScaleSuperLocrian,
        ScaleTritone,
        ScaleTwoSemitoneTritone,
        ScaleUkranianDorian,
        ScaleWholeTone,
        ScaleYo,
    };
    Q_ENUM(Scale)
    /**
     * \brief The human readable name for the given Scale enumerator value
     * @param key The value you wish to get the human readable name for
     * @return They human readable name for the given value
     */
    Q_INVOKABLE QString scaleName(const Scale &scale) const;
    /**
     * \brief The enumerator entry at the given index in the Scale enumerator
     * @param index The index to get the enumerator entry for
     * @return The enumerator entry at the given index (will return ScaleChromatic for any invalid index)
     * @see scaleNames()
     */
    Q_INVOKABLE Scale scaleIndexToEnumKey(const int& index) const;
    /**
     * \brief The index of the given entry in the Scale enumerator
     * @param entry The key of the entry you wish to get the index of
     * @return The index of the given enumerator key
     */
    Q_INVOKABLE int scaleEnumKeyToIndex(const Scale &entry) const;
    /**
     * \brief All the human readable names of the Scale enumerator
     * @return A list of all the human readable names for all entries in the Scale enumerator
     * @see scaleIndexToValue(int)
     */
    Q_INVOKABLE QStringList scaleNames() const;
    /**
     * \brief Get a string shorthand for the given scale
     * @note This is for persistence use and is guaranteed stable across releases
     * @param entry The key of the entry you wish to get a stable shorthand for
     * @return The shorthand for the given scale entry
     */
    Q_INVOKABLE QString scaleShorthand(const Scale &entry) const;
    /**
     * \brief Get the Scale enumerator key for the given shorthand
     * @see scaleShorthand(const Scale&)
     * @param shorthand The string shorthand which you want to get the enumerator key for
     * @return The Scale enumerator key for the given shorthand (or ScaleChromatic for an invalid or unknown shorthands)
     */
    Q_INVOKABLE Scale scaleShorthandToKey(const QString &shorthand) const;

    // The logic here being that the octaves are defined by their offset from
    // the midi root note, and we have to make a choice on what that means. In
    // our case, that means we decide that octave 4 starts at midi note 60.
    /**
     * \brief A representation of the octaves found in the midi standard, with C4 at note 60
     */
    enum Octave {
        OctaveNegative1 = 0,
        Octave0 = 12,
        Octave1 = 24,
        Octave2 = 36,
        Octave3 = 48,
        Octave4 = 60,
        Octave5 = 72,
        Octave6 = 84,
        Octave7 = 96,
        Octave8 = 108,
        Octave9 = 120,
    };
    Q_ENUM(Octave)
    /**
     * \brief The human readable name for the given Octave enumerator value
     * @param octave The value you wish to get the human readable name for
     * @return They human readable name for the given value
     */
    Q_INVOKABLE QString octaveName(const Octave &octave) const;
    /**
     * \brief The enumerator entry at the given index in the Octave enumerator
     * @param index the index to get the enumerator entry for
     * @return The enumerator entry at the given index (will return OctaveNegative1 for any invalid index)
     * @see octaveNames()
     */
    Q_INVOKABLE Octave octaveIndexToEnumKey(const int &index) const;
    /**
     * \brief The index of the given entry in the Octave enumerator
     * @param entry The key of the entry you wish to get the index of
     * @return The index of the given enumerator key
     */
    Q_INVOKABLE int octaveEnumKeyToIndex(const Octave &entry) const;
    /**
     * \brief All the human readable names of the Octave enumerator
     * @return A list of all the human readable names for all entries in the Octave enumerator
     * @see octaveIndexToEnumKey(int)
     */
    Q_INVOKABLE QStringList octaveNames() const;
    /**
     * \brief Get a string shorthand for the given octave
     * @note This is for persistence use and is guaranteed stable across releases
     * @param entry The key of the entry you wish to get a stable shorthand for
     * @return The shorthand for the given octave entry
     */
    Q_INVOKABLE QString octaveShorthand(const Octave &entry) const;
    /**
     * \brief Get the Octave enumerator key for the given shorthand
     * @see pitchShorthand(const Octave&)
     * @param shorthand The string shorthand which you want to get the enumerator key for
     * @return The Octave enumerator key for the given shorthand (or Octave4 for an invalid or unknown shorthands)
     */
    Q_INVOKABLE Octave octaveShorthandToKey(const QString &shorthand) const;

    /**
     * \brief The octave the given note exists within
     * @param midiNote The note whose octave you wish to identify
     * @return The Octave enumerator entry which matches the given midi note
     */
    Q_INVOKABLE Octave midiNoteToOctave(const int &midiNote) const;

    /**
     * \brief Get the midi note value for the given pitch
     * The reason for this function is that some of the keys have the same midi note
     * value (that is, all the sharp notes have a same-value flat note, as per musical
     * theory).
     * @note This is given as an offset from the midi root. To get one adjusted for
     * octave, add the value of a KeyScales::Octave to your pitch
     * @param pitch The pitch you wish to get the midi note value for
     * @return The midi note value for the given pitch
     */
    Q_INVOKABLE int midiPitchValue(const Pitch &pitch, const Octave &octave = OctaveNegative1) const;

    /**
     * \brief Convenience function for getting the proper name of a given midi note
     * @param midiNote The midi note to get a name for (it will be clamped to the midi note range)
     * @return The name of the given midi note
     */
    Q_INVOKABLE QString midiNoteName(const int &midiNote) const;

    /**
     * \brief Returns the nearest upward on-scale note to the given note, based on the given scale and root note information
     * @param midiNote The note that you wish to transpose (this will be clamped to within the allowed midi note range of 0 through 127)
     * @param scale The scale to use for the transposition operation
     * @param pitch The pitch for the root note on which the scale is positioned
     * @param octave The octave for the root note on which the scale is positioned
     * @return The note that is closest to the given note on the scale (that is, the next note from the given note and up which is on-scale)
     */
    Q_INVOKABLE int onScaleNote(const int &midiNote, const Scale& scale = ScaleChromatic, const Pitch &pitch = PitchC, const Octave &octave = Octave4) const;
    /**
     * \brief Transpose a note by the given number of steps along the given scale and root note information
     * @note If the given note is not on-scale, the first step will be considered moving it onto the scale
     * @param midiNote The note that you wish to transpose (this will be clamped to within the allowed midi note range of 0 through 127)
     * @param steps The number of steps along the scale that the note should be transposed
     * @param scale The scale to use for the transposition operation
     * @param pitch The pitch for the root note on which the scale is positioned
     * @param octave The octave for the root note on which the scale is positioned
     * @return The transposed note (this will be clamped to within the allowed midi note range 0 through 127)
     */
    Q_INVOKABLE int transposeNote(const int &midiNote, const int &steps, const Scale& scale = ScaleChromatic, const Pitch &pitch = PitchC, const Octave &octave = Octave4) const;
    /**
     * \brief Whether the given midi note is found on the given key and scale
     * @param midiNote The midi note to check
     * @param scale The scale to check against
     * @param pitch The pitch for the root note on which the scale is positioned
     * @param octave The octave for the root note on which the scale is positioned
     * @return True if the given midi note is found on the given key and scale, false if not
     */
    Q_INVOKABLE bool midiNoteOnScale(const int &midiNote, const Scale &scale = ScaleChromatic, const Pitch &pitch = PitchC, const Octave &octave = Octave4) const;
private:
    class Private;
    Private *d{nullptr};
};
Q_DECLARE_METATYPE(KeyScales::Scale)
Q_DECLARE_METATYPE(KeyScales::Pitch)
Q_DECLARE_METATYPE(KeyScales::Octave)
