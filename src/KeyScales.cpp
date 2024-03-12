#include "KeyScales.h"

#include <QDebug>
#include <QHash>
#include <QMetaObject>
#include <QMetaEnum>

static const QList<KeyScales::Pitch> pitchIndices{
    KeyScales::PitchC,
    KeyScales::PitchCSharp,
    KeyScales::PitchDFlat,
    KeyScales::PitchD,
    KeyScales::PitchDSharp,
    KeyScales::PitchEFlat,
    KeyScales::PitchE,
    KeyScales::PitchF,
    KeyScales::PitchFSharp,
    KeyScales::PitchGFlat,
    KeyScales::PitchG,
    KeyScales::PitchGSharp,
    KeyScales::PitchAFlat,
    KeyScales::PitchA,
    KeyScales::PitchASharp,
    KeyScales::PitchBFlat,
    KeyScales::PitchB,
};
static const QHash<KeyScales::Pitch, QString> pitchNamesHash{
    {KeyScales::PitchC, QLatin1String{"C"}},
    {KeyScales::PitchCSharp, QLatin1String{"C#"}},
    {KeyScales::PitchDFlat, QString::fromUtf8("D♭")},
    {KeyScales::PitchD, QLatin1String{"D"}},
    {KeyScales::PitchDSharp, QLatin1String{"D#"}},
    {KeyScales::PitchEFlat, QString::fromUtf8("E♭")},
    {KeyScales::PitchE, QLatin1String{"E"}},
    {KeyScales::PitchF, QLatin1String{"F"}},
    {KeyScales::PitchFSharp, QLatin1String{"F#"}},
    {KeyScales::PitchGFlat, QString::fromUtf8("G♭")},
    {KeyScales::PitchG, QLatin1String{"G"}},
    {KeyScales::PitchGSharp, QLatin1String{"G#"}},
    {KeyScales::PitchAFlat, QString::fromUtf8("A♭")},
    {KeyScales::PitchA, QLatin1String{"A"}},
    {KeyScales::PitchASharp, QLatin1String{"A#"}},
    {KeyScales::PitchBFlat, QString::fromUtf8("B♭")},
    {KeyScales::PitchB, QLatin1String{"B"}},
};
// NOTE The shorthand-to-key pairs here MUST remain stable across releases (as they are our persistence values)
static const QHash<KeyScales::Pitch, QString> pitchShorthandHash{
    {KeyScales::PitchC, QLatin1String{"c"}},
    {KeyScales::PitchCSharp, QLatin1String{"csharp"}},
    {KeyScales::PitchDFlat, QLatin1String("dflat")},
    {KeyScales::PitchD, QLatin1String{"d"}},
    {KeyScales::PitchDSharp, QLatin1String{"dsharp"}},
    {KeyScales::PitchEFlat, QLatin1String("eflat")},
    {KeyScales::PitchE, QLatin1String{"e"}},
    {KeyScales::PitchF, QLatin1String{"f"}},
    {KeyScales::PitchFSharp, QLatin1String{"fsharp"}},
    {KeyScales::PitchGFlat, QLatin1String("gflat")},
    {KeyScales::PitchG, QLatin1String{"g"}},
    {KeyScales::PitchGSharp, QLatin1String{"gsharp"}},
    {KeyScales::PitchAFlat, QLatin1String("asharp")},
    {KeyScales::PitchA, QLatin1String{"a"}},
    {KeyScales::PitchASharp, QLatin1String{"asharp"}},
    {KeyScales::PitchBFlat, QLatin1String("bflat")},
    {KeyScales::PitchB, QLatin1String{"b"}},
};

static const QHash<KeyScales::Pitch, int> pitchValuesHash{
    {KeyScales::PitchC, 0},
    {KeyScales::PitchCSharp, 1},
    {KeyScales::PitchDFlat, 1},
    {KeyScales::PitchD, 2},
    {KeyScales::PitchDSharp, 3},
    {KeyScales::PitchEFlat, 3},
    {KeyScales::PitchE, 4},
    {KeyScales::PitchF, 5},
    {KeyScales::PitchFSharp, 6},
    {KeyScales::PitchGFlat, 6},
    {KeyScales::PitchG, 7},
    {KeyScales::PitchGSharp, 8},
    {KeyScales::PitchAFlat, 8},
    {KeyScales::PitchA, 9},
    {KeyScales::PitchASharp, 10},
    {KeyScales::PitchBFlat, 10},
    {KeyScales::PitchB, 11},
};

static const QList<KeyScales::Scale> scaleIndices{
    KeyScales::ScaleChromatic,
    KeyScales::ScaleIonian,
    KeyScales::ScaleMajor,
    KeyScales::ScaleDorian,
    KeyScales::ScalePhrygian,
    KeyScales::ScaleLydian,
    KeyScales::ScaleMixolydian,
    KeyScales::ScaleAeolian,
    KeyScales::ScaleNaturalMinor,
    KeyScales::ScaleLocrian,
};

static const QHash<KeyScales::Scale, QString> scaleNamesHash{
    {KeyScales::ScaleChromatic, QLatin1String{"Chromatic"}},
    {KeyScales::ScaleIonian, QLatin1String{"Ionian"}},
    {KeyScales::ScaleMajor,  QLatin1String{"Major"}},
    {KeyScales::ScaleDorian, QLatin1String{"Dorian"}},
    {KeyScales::ScalePhrygian, QLatin1String{"Phrygian"}},
    {KeyScales::ScaleLydian, QLatin1String{"Lydian"}},
    {KeyScales::ScaleMixolydian, QLatin1String{"Mixolydian"}},
    {KeyScales::ScaleAeolian, QLatin1String{"Aeolian"}},
    {KeyScales::ScaleNaturalMinor,  QLatin1String{"Natural Minor"}},
    {KeyScales::ScaleLocrian, QLatin1String{"Locrian"}},
};

// NOTE The shorthand-to-key pairs here MUST remain stable across releases (as they are our persistence values)
static const QHash<KeyScales::Scale, QString> scaleShorthandHash{
    {KeyScales::ScaleChromatic, QLatin1String{"chromatic"}},
    {KeyScales::ScaleIonian, QLatin1String{"ionian"}},
    {KeyScales::ScaleMajor,  QLatin1String{"major"}},
    {KeyScales::ScaleDorian, QLatin1String{"dorian"}},
    {KeyScales::ScalePhrygian, QLatin1String{"phrygian"}},
    {KeyScales::ScaleLydian, QLatin1String{"lydian"}},
    {KeyScales::ScaleMixolydian, QLatin1String{"mixolydian"}},
    {KeyScales::ScaleAeolian, QLatin1String{"aeolian"}},
    {KeyScales::ScaleNaturalMinor,  QLatin1String{"natural Minor"}},
    {KeyScales::ScaleLocrian, QLatin1String{"locrian"}},
};

static const int scaleCount{10};
static const QHash<KeyScales::Scale, QList<int>> scaleIntervals{
    {KeyScales::ScaleChromatic, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {KeyScales::ScaleIonian, {2, 2, 1, 2, 2, 2, 1}},
    {KeyScales::ScaleMajor, {2, 2, 1, 2, 2, 2, 1}},
    {KeyScales::ScaleDorian, {2, 1, 2, 2, 2, 1, 2}},
    {KeyScales::ScalePhrygian, {1, 2, 2, 2, 1, 2, 2}},
    {KeyScales::ScaleLydian, {2, 2, 2, 1, 2, 2, 1}},
    {KeyScales::ScaleMixolydian, {2, 2, 1, 2, 2, 1, 2}},
    {KeyScales::ScaleAeolian, {2, 1, 2, 2, 1, 2, 2}},
    {KeyScales::ScaleNaturalMinor,  {2, 1, 2, 2, 1, 2, 2}},
    {KeyScales::ScaleLocrian, {1, 2, 2, 1, 2, 2, 2}},
};

static const QList<KeyScales::Octave> octaveIndices{
    KeyScales::OctaveNegative1,
    KeyScales::Octave0,
    KeyScales::Octave1,
    KeyScales::Octave2,
    KeyScales::Octave3,
    KeyScales::Octave4,
    KeyScales::Octave5,
    KeyScales::Octave6,
    KeyScales::Octave7,
    KeyScales::Octave8,
    KeyScales::Octave9,
};

static const QHash<KeyScales::Octave, QString> octaveNamesHash{
    {KeyScales::OctaveNegative1, QLatin1String{"-1"}},
    {KeyScales::Octave0, QLatin1String{"0"}},
    {KeyScales::Octave1, QLatin1String{"1"}},
    {KeyScales::Octave2, QLatin1String{"2"}},
    {KeyScales::Octave3, QLatin1String{"3"}},
    {KeyScales::Octave4, QLatin1String{"4"}},
    {KeyScales::Octave5, QLatin1String{"5"}},
    {KeyScales::Octave6, QLatin1String{"6"}},
    {KeyScales::Octave7, QLatin1String{"7"}},
    {KeyScales::Octave8, QLatin1String{"8"}},
    {KeyScales::Octave9, QLatin1String{"9"}},
};
// NOTE The shorthand-to-key pairs here MUST remain stable across releases (as they are our persistence values)
static const QHash<KeyScales::Octave, QString> octaveShorthandHash{
    {KeyScales::OctaveNegative1, QLatin1String{"octavenegative1"}},
    {KeyScales::Octave0, QLatin1String{"octave0"}},
    {KeyScales::Octave1, QLatin1String{"octave1"}},
    {KeyScales::Octave2, QLatin1String{"octave2"}},
    {KeyScales::Octave3, QLatin1String{"octave3"}},
    {KeyScales::Octave4, QLatin1String{"octave4"}},
    {KeyScales::Octave5, QLatin1String{"octave5"}},
    {KeyScales::Octave6, QLatin1String{"octave6"}},
    {KeyScales::Octave7, QLatin1String{"octave7"}},
    {KeyScales::Octave8, QLatin1String{"octave8"}},
    {KeyScales::Octave9, QLatin1String{"octave9"}},
};

class KeyScales::Private {
public:
    Private() {
        QHashIterator<KeyScales::Scale, QList<int>> scaleIntervalIterator{scaleIntervals};
        nearestNote.reserve(scaleCount);
        allNotes.reserve(scaleCount);
        while (scaleIntervalIterator.hasNext()) {
            scaleIntervalIterator.next();
            const KeyScales::Scale &scaleName = scaleIntervalIterator.key();
            const QList<int> &scaleInterval = scaleIntervalIterator.value();
            QHash<int, QList<int>> scaleNearestNote;
            scaleNearestNote.reserve(128);
            QHash<int, QList<int>> scaleAllNotes;
            scaleAllNotes.reserve(128);
            for (int rootNote = 0; rootNote < 128; ++rootNote) {
                // The lists being created here are
                // - the nearest note (that is, what should a given note become if forced onto the scale/root)
                QList<int> scaleRootNearestNote;
                scaleRootNearestNote.reserve(128);
                // - the notes which exist in the scale
                QList<int> scaleRootAllNotes;
                scaleRootAllNotes.reserve(128);
                // We start by creating the list of notes that exist in the scale
                // Add all notes that are below the root note
                const int numberOfIntervals{scaleInterval.count()};
                int scaleIntervalPosition{numberOfIntervals - 1};
                int currentlyAddingNote{rootNote - scaleInterval[scaleIntervalPosition]};
                while (currentlyAddingNote > -1) {
                    scaleRootAllNotes << currentlyAddingNote;
                    --scaleIntervalPosition;
                    if (scaleIntervalPosition < 0) {
                        scaleIntervalPosition = numberOfIntervals - 1;
                    }
                    currentlyAddingNote = currentlyAddingNote - scaleInterval[scaleIntervalPosition];
                }
                // As they are in reverse order, reverse that order
                std::reverse(scaleRootAllNotes.begin(), scaleRootAllNotes.end());
                // Then add all notes above the root note
                scaleIntervalPosition = 0;
                currentlyAddingNote = rootNote;
                while (currentlyAddingNote < 128) {
                    scaleRootAllNotes << currentlyAddingNote;
                    ++scaleIntervalPosition;
                    if (scaleIntervalPosition == numberOfIntervals) {
                        scaleIntervalPosition = 0;
                    }
                    currentlyAddingNote = currentlyAddingNote + scaleInterval[scaleIntervalPosition];
                }
                // We then run through that list, and add them in turn (and any that might be missing) to the nearest note lookup
                int anyNote{0};
                for (const int &scaleNote : qAsConst(scaleRootAllNotes)) {
                    while (anyNote <= scaleNote) {
                        scaleRootNearestNote << scaleNote;
                        ++anyNote;
                    }
                }
                // And finally, add these lists to our scale data
                scaleNearestNote[rootNote] = scaleRootNearestNote;
                scaleAllNotes[rootNote] = scaleRootAllNotes;
            }
            nearestNote[scaleName] = scaleNearestNote;
            allNotes[scaleName] = scaleAllNotes;
        }
    }
    // Pre-calculated table of what the nearest note is for any valid midi note, for a given root note, in the known scales
    QHash<KeyScales::Scale, QHash<int, QList<int>>> nearestNote;
    // Pre-calculated table of all notes in a scale, for any given root note, in the known scales
    QHash<KeyScales::Scale, QHash<int, QList<int>>> allNotes;
};

KeyScales::KeyScales(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
    qRegisterMetaType<KeyScales::Scale>("KeyScales::Scale");
    qRegisterMetaType<KeyScales::Pitch>("KeyScales::Pitch");
    qRegisterMetaType<KeyScales::Octave>("KeyScales::Octave");
}

KeyScales::~KeyScales()
{
    delete d;
}

QString KeyScales::pitchName(const Pitch& pitch) const
{
    return pitchNamesHash[pitch];
}

QStringList KeyScales::pitchNames() const
{
    return pitchNamesHash.values();
}

KeyScales::Pitch KeyScales::pitchIndexToEnumKey(const int& index) const
{
    if (-1 < index && index < pitchIndices.count()) {
        return pitchIndices[index];
    }
    return PitchC;
}

int KeyScales::pitchEnumKeyToIndex(const Pitch& entry) const
{
    return pitchIndices.indexOf(entry);
}

QString KeyScales::pitchShorthand(const Pitch& entry) const
{
    return pitchShorthandHash.value(entry);
}

KeyScales::Pitch KeyScales::pitchShorthandToKey(const QString& shorthand) const
{
    Pitch key{PitchC};
    for (auto iterator = pitchShorthandHash.cbegin(), end = pitchShorthandHash.cend(); iterator != end; ++iterator) {
        if (iterator.value() == shorthand) {
            key = iterator.key();
            break;
        }
    }
    return key;
}

QString KeyScales::scaleName(const Scale& scale) const
{
    return scaleNamesHash[scale];
}

QStringList KeyScales::scaleNames() const
{
    return scaleNamesHash.values();
}

QString KeyScales::scaleShorthand(const Scale& entry) const
{
    return scaleShorthandHash.value(entry);
}

KeyScales::Scale KeyScales::scaleShorthandToKey(const QString& shorthand) const
{
    Scale key{ScaleChromatic};
    for (auto iterator = scaleShorthandHash.cbegin(), end = scaleShorthandHash.cend(); iterator != end; ++iterator) {
        if (iterator.value() == shorthand) {
            key = iterator.key();
            break;
        }
    }
    return key;
}

KeyScales::Scale KeyScales::scaleIndexToEnumKey(const int& index) const
{
    if (-1 < index && index < scaleIndices.count()) {
        return scaleIndices[index];
    }
    return ScaleChromatic;
}

int KeyScales::scaleEnumKeyToIndex(const Scale& entry) const
{
    return scaleIndices.indexOf(entry);
}

QString KeyScales::octaveName(const Octave& octave) const
{
    return octaveNamesHash[octave];
}

KeyScales::Octave KeyScales::octaveIndexToEnumKey(const int& index) const
{
    if (-1 < index && index < octaveIndices.count()) {
        return octaveIndices[index];
    }
    return OctaveNegative1;
}

int KeyScales::octaveEnumKeyToIndex(const Octave& entry) const
{
    return octaveIndices.indexOf(entry);
}

QStringList KeyScales::octaveNames() const
{
    return octaveNamesHash.values();
}

QString KeyScales::octaveShorthand(const Octave& entry) const
{
    return octaveShorthandHash.value(entry);
}

KeyScales::Octave KeyScales::octaveShorthandToKey(const QString& shorthand) const
{
    Octave key{Octave4};
    for (auto iterator = octaveShorthandHash.cbegin(), end = octaveShorthandHash.cend(); iterator != end; ++iterator) {
        if (iterator.value() == shorthand) {
            key = iterator.key();
            break;
        }
    }
    return key;
}

int KeyScales::midiPitchValue(const Pitch& pitch, const Octave &octave) const
{
    return std::clamp(pitchValuesHash[pitch] + octave, 0, 127);
}

KeyScales::Octave KeyScales::midiNoteToOctave(const int& midiNote) const
{
    static const QMetaEnum octaveNameMeta = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("Octave"));
    const int octaveValue = 12 * (std::clamp(midiNote, 0, 127) % 12);
    return static_cast<KeyScales::Octave>(octaveNameMeta.value(octaveValue));
}

int KeyScales::onScaleNote(const int& midiNote, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    return d->nearestNote[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)][std::clamp(midiNote, 0, 127)];
}

int KeyScales::transposeNote(const int& midiNote, const int& steps, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    int transposedNote{onScaleNote(midiNote, scale, pitch, octave)};
    int scaleIntervalPosition = d->allNotes[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)].indexOf(transposedNote);
    int currentStep{0};
    const QList<int> &scaleInterval = scaleIntervals[scale];
    const int intervalCount{scaleInterval.count()};
    if (steps > 0) {
        while (currentStep < steps) {
            transposedNote = transposedNote + scaleInterval[scaleIntervalPosition];
            ++scaleIntervalPosition;
            if (scaleIntervalPosition == intervalCount) {
                scaleIntervalPosition = 0;
            }
            ++currentStep;
        }
    } else if (steps < 0) {
        while (currentStep > steps) {
            --scaleIntervalPosition;
            transposedNote = transposedNote - scaleInterval[scaleIntervalPosition];
            if (scaleIntervalPosition < 0) {
                scaleIntervalPosition = intervalCount - 1;
            }
            --currentStep;
        }
    }
    return std::clamp(transposedNote, 0, 127);
}

bool KeyScales::midiNoteOnScale(const int& midiNote, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    const bool returnVal = d->allNotes[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)].contains(midiNote);
    // qDebug() << Q_FUNC_INFO << midiNote << scale << pitch << octave << returnVal << d->allNotes[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)];
    return returnVal;
}
