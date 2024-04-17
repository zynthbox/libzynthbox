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
    {KeyScales::PitchAFlat, QLatin1String("aflat")},
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
static const KeyScales::Pitch pitchForMidiValue[12]{
    KeyScales::PitchC,
    KeyScales::PitchDFlat,
    KeyScales::PitchD,
    KeyScales::PitchDSharp,
    KeyScales::PitchE,
    KeyScales::PitchF,
    KeyScales::PitchFSharp,
    KeyScales::PitchG,
    KeyScales::PitchGSharp,
    KeyScales::PitchA,
    KeyScales::PitchASharp,
    KeyScales::PitchB
};

static const QList<KeyScales::Scale> scaleIndices{
    KeyScales::ScaleAdonaiMalakh,
    KeyScales::ScaleAeolian,
    KeyScales::ScaleAlgerian,
    KeyScales::ScaleAugmented,
    KeyScales::ScaleBeebopDominant,
    KeyScales::ScaleBlues,
    KeyScales::ScaleChromatic,
    KeyScales::ScaleDorian,
    KeyScales::ScaleDoubleHarmonic,
    KeyScales::ScaleEnigmatic,
    KeyScales::ScaleFlamenco,
    KeyScales::ScaleGypsy,
    KeyScales::ScaleHalfDiminished,
    KeyScales::ScaleHarmonicMajor,
    KeyScales::ScaleHarmonicMinor,
    KeyScales::ScaleHarmonics,
    KeyScales::ScaleHirajoshi,
    KeyScales::ScaleHungarianMajor,
    KeyScales::ScaleHungarianMinor,
    KeyScales::ScaleIn,
    KeyScales::ScaleInsen,
    KeyScales::ScaleIonian,
    KeyScales::ScaleIstrian,
    KeyScales::ScaleIwato,
    KeyScales::ScaleLydian,
    KeyScales::ScaleLydianAugmented,
    KeyScales::ScaleLydianDiminished,
    KeyScales::ScaleLydianDominant,
    KeyScales::ScaleLocrian,
    KeyScales::ScaleMajor,
    KeyScales::ScaleMajorBebop,
    KeyScales::ScaleMajorLocrian,
    KeyScales::ScaleMajorPentatonic,
    KeyScales::ScaleMelodicMinorAscending,
    KeyScales::ScaleMelodicMinorDescending,
    KeyScales::ScaleMelodicMinorAscendingDescending,
    KeyScales::ScaleMelodicMinorDescendingAscending,
    KeyScales::ScaleMinorPentatonic,
    KeyScales::ScaleMixolydian,
    KeyScales::ScaleNaturalMinor,
    KeyScales::ScaleNeopolitanMajor,
    KeyScales::ScaleNeopolitanMinor,
    KeyScales::ScalePersian,
    KeyScales::ScalePhrygian,
    KeyScales::ScalePhrygianDominant,
    KeyScales::ScalePrometheus,
    KeyScales::ScaleSuperLocrian,
    KeyScales::ScaleTritone,
    KeyScales::ScaleTwoSemitoneTritone,
    KeyScales::ScaleUkranianDorian,
    KeyScales::ScaleWholeTone,
    KeyScales::ScaleYo
};

static const QHash<KeyScales::Scale, QString> scaleNamesHash{
    {KeyScales::ScaleAdonaiMalakh, QLatin1String{"Adonai Malakh Mode"}},
    {KeyScales::ScaleAeolian, QLatin1String{"Aeolian Mode"}},
    {KeyScales::ScaleAlgerian, QLatin1String{"Algerian Scale"}},
    {KeyScales::ScaleAugmented, QLatin1String{"Augmented Scale"}},
    {KeyScales::ScaleBeebopDominant, QLatin1String{"Beebop Dominant Scale"}},
    {KeyScales::ScaleBlues, QLatin1String{"Blues Scale"}},
    {KeyScales::ScaleChromatic, QLatin1String{"Chromatic Scale"}},
    {KeyScales::ScaleDorian, QLatin1String{"Dorian Mode"}},
    {KeyScales::ScaleDoubleHarmonic, QLatin1String{"Double Harmonic Scale"}},
    {KeyScales::ScaleEnigmatic, QLatin1String{"Enigmatic Scale"}},
    {KeyScales::ScaleFlamenco, QLatin1String{"Flamenco Mode"}},
    {KeyScales::ScaleGypsy, QLatin1String{"'Gypsy' Scale"}},
    {KeyScales::ScaleHalfDiminished, QLatin1String{"Half Diminished Scale"}},
    {KeyScales::ScaleHarmonicMajor, QLatin1String{"Harmonic Major Scale"}},
    {KeyScales::ScaleHarmonicMinor, QLatin1String{"Harmonic Minor Scale"}},
    {KeyScales::ScaleHarmonics, QLatin1String{"Scale of Harmonics"}},
    {KeyScales::ScaleHirajoshi, QLatin1String{"Hirajoshi Scale"}},
    {KeyScales::ScaleHungarianMajor, QLatin1String{"Hungarian Major Scale"}},
    {KeyScales::ScaleHungarianMinor, QLatin1String{"Hungarian Minor Scale"}},
    {KeyScales::ScaleIn, QLatin1String{"In (Sakura Pentatonic) Scale"}},
    {KeyScales::ScaleInsen, QLatin1String{"Insen Scale"}},
    {KeyScales::ScaleIonian, QLatin1String{"Ionian Mode"}},
    {KeyScales::ScaleIstrian, QLatin1String{"Istrian Scale"}},
    {KeyScales::ScaleIwato, QLatin1String{"Iwato Scale"}},
    {KeyScales::ScaleLydian, QLatin1String{"Lydian Mode"}},
    {KeyScales::ScaleLydianAugmented, QLatin1String{"Lydian Augmented Scale"}},
    {KeyScales::ScaleLydianDiminished, QLatin1String{"Lydian Diminished Scale"}},
    {KeyScales::ScaleLydianDominant, QLatin1String{"Lydian Dominant Scale"}},
    {KeyScales::ScaleLocrian, QLatin1String{"Locrian Mode"}},
    {KeyScales::ScaleMajor,  QLatin1String{"Major Scale"}},
    {KeyScales::ScaleMajorBebop, QLatin1String{"Major Bebop Scale"}},
    {KeyScales::ScaleMajorLocrian, QLatin1String{"Major Locrian Scale"}},
    {KeyScales::ScaleMajorPentatonic, QLatin1String{"Major Pentatonic Scale"}},
    {KeyScales::ScaleMelodicMinorAscending, QLatin1String{"Melodic Minor Scale Ascending"}},
    {KeyScales::ScaleMelodicMinorDescending, QLatin1String{"Melodic Minor Scale Descending"}},
    {KeyScales::ScaleMelodicMinorAscendingDescending, QLatin1String{"Melodic Minor Scale Ascending then Descending"}},
    {KeyScales::ScaleMelodicMinorDescendingAscending, QLatin1String{"Melodic Minor Scale Descending then Ascending"}},
    {KeyScales::ScaleMinorPentatonic, QLatin1String{"Minor Pentatonic Scale"}},
    {KeyScales::ScaleMixolydian, QLatin1String{"Mixolydian Mode"}},
    {KeyScales::ScaleNaturalMinor,  QLatin1String{"Natural Minor Scale"}},
    {KeyScales::ScaleNeopolitanMajor, QLatin1String{"Neopolitan Major Scale"}},
    {KeyScales::ScaleNeopolitanMinor, QLatin1String{"Neopolitan Minor Scale"}},
    {KeyScales::ScalePersian, QLatin1String{"Persian Scale"}},
    {KeyScales::ScalePhrygian, QLatin1String{"Phrygian Mode"}},
    {KeyScales::ScalePhrygianDominant, QLatin1String{"Phrygian Dominant Scale"}},
    {KeyScales::ScalePrometheus, QLatin1String{"Prometheus Scale"}},
    {KeyScales::ScaleSuperLocrian, QLatin1String{"Super Locrian Scale"}},
    {KeyScales::ScaleTritone, QLatin1String{"Tritone Scale"}},
    {KeyScales::ScaleTwoSemitoneTritone, QLatin1String{"Two Semi-tone Tritone Scale"}},
    {KeyScales::ScaleUkranianDorian, QLatin1String{"Ukranian Dorian Scale"}},
    {KeyScales::ScaleWholeTone, QLatin1String{"Whole Tone Scale"}},
    {KeyScales::ScaleYo, QLatin1String{"Yo Scale"}},
};

// NOTE The shorthand-to-key pairs here MUST remain stable across releases (as they are our persistence values)
static const QHash<KeyScales::Scale, QString> scaleShorthandHash{
    {KeyScales::ScaleAdonaiMalakh, QLatin1String{"adonaimalakh"}},
    {KeyScales::ScaleAeolian, QLatin1String{"aeolian"}},
    {KeyScales::ScaleAlgerian, QLatin1String{"algerian"}},
    {KeyScales::ScaleAugmented, QLatin1String{"augmented"}},
    {KeyScales::ScaleBeebopDominant, QLatin1String{"beebopdominant"}},
    {KeyScales::ScaleBlues, QLatin1String{"blues"}},
    {KeyScales::ScaleChromatic, QLatin1String{"chromatic"}},
    {KeyScales::ScaleDorian, QLatin1String{"dorian"}},
    {KeyScales::ScaleDoubleHarmonic, QLatin1String{"doubleharmonic"}},
    {KeyScales::ScaleEnigmatic, QLatin1String{"enigmatic"}},
    {KeyScales::ScaleFlamenco, QLatin1String{"flamenco"}},
    {KeyScales::ScaleGypsy, QLatin1String{"gypsy"}},
    {KeyScales::ScaleHalfDiminished, QLatin1String{"halfdiminished"}},
    {KeyScales::ScaleHarmonicMajor, QLatin1String{"harmonicmajor"}},
    {KeyScales::ScaleHarmonicMinor, QLatin1String{"harmonicminor"}},
    {KeyScales::ScaleHarmonics, QLatin1String{"harmonics"}},
    {KeyScales::ScaleHirajoshi, QLatin1String{"hirajoshi"}},
    {KeyScales::ScaleHungarianMajor, QLatin1String{"hungarianmajor"}},
    {KeyScales::ScaleHungarianMinor, QLatin1String{"hungarianminor"}},
    {KeyScales::ScaleIn, QLatin1String{"in"}},
    {KeyScales::ScaleInsen, QLatin1String{"insen"}},
    {KeyScales::ScaleIonian, QLatin1String{"ionian"}},
    {KeyScales::ScaleIstrian, QLatin1String{"istrian"}},
    {KeyScales::ScaleIwato, QLatin1String{"iwato"}},
    {KeyScales::ScaleLydian, QLatin1String{"lydian"}},
    {KeyScales::ScaleLydianAugmented, QLatin1String{"lydianaugmented"}},
    {KeyScales::ScaleLydianDiminished, QLatin1String{"lydiandiminished"}},
    {KeyScales::ScaleLydianDominant, QLatin1String{"lydiandominant"}},
    {KeyScales::ScaleLocrian, QLatin1String{"locrian"}},
    {KeyScales::ScaleMajor,  QLatin1String{"major"}},
    {KeyScales::ScaleMajorBebop, QLatin1String{"majorbebop"}},
    {KeyScales::ScaleMajorLocrian, QLatin1String{"majorlocrian"}},
    {KeyScales::ScaleMajorPentatonic, QLatin1String{"majorpentatonic"}},
    {KeyScales::ScaleMelodicMinorAscending, QLatin1String{"melodicminorascending"}},
    {KeyScales::ScaleMelodicMinorDescending, QLatin1String{"melodicminordescending"}},
    {KeyScales::ScaleMelodicMinorAscendingDescending, QLatin1String{"melodicminorascendingdescending"}},
    {KeyScales::ScaleMelodicMinorDescendingAscending, QLatin1String{"melodicminordescendingascending"}},
    {KeyScales::ScaleMinorPentatonic, QLatin1String{"minorpentatonic"}},
    {KeyScales::ScaleMixolydian, QLatin1String{"mixolydian"}},
    {KeyScales::ScaleNaturalMinor,  QLatin1String{"naturalminor"}},
    {KeyScales::ScaleNeopolitanMajor, QLatin1String{"neopolitanmajor"}},
    {KeyScales::ScaleNeopolitanMinor, QLatin1String{"neopolitanminor"}},
    {KeyScales::ScalePersian, QLatin1String{"persian"}},
    {KeyScales::ScalePhrygian, QLatin1String{"phrygian"}},
    {KeyScales::ScalePhrygianDominant, QLatin1String{"phrygiandominant"}},
    {KeyScales::ScalePrometheus, QLatin1String{"prometheus"}},
    {KeyScales::ScaleSuperLocrian, QLatin1String{"superlocrian"}},
    {KeyScales::ScaleTritone, QLatin1String{"tritone"}},
    {KeyScales::ScaleTwoSemitoneTritone, QLatin1String{"twosemitonetritone"}},
    {KeyScales::ScaleUkranianDorian, QLatin1String{"ukraniandorian"}},
    {KeyScales::ScaleWholeTone, QLatin1String{"wholetone"}},
    {KeyScales::ScaleYo, QLatin1String{"yo"}},
};

#define scaleCount 52
// These are stored so that, given a root note, you can add these intervals in order to get the
// next pitch (and conversely, starting from a root note, you can rotate through backwards
// starting at the last entry in the list to complete the scale downwards)
static const QHash<KeyScales::Scale, QList<int>> scaleIntervals{
    {KeyScales::ScaleAdonaiMalakh, {2, 2, 1, 2, 2, 1, 2}}, // 0,2,4,5,7,9,10
    {KeyScales::ScaleAeolian, {2, 1, 2, 2, 1, 2, 2}}, // 0,2,3,5,7,8,10
    {KeyScales::ScaleAlgerian, {2,1,3,1,1,3,1,2,1,2,2,1,3,1}}, // alternates between two different types of octave layout (nominally W, H, WH, H, H, WH, H, with every second octave being W, H, W, W, H, WH, H instead) // 0,2,3,6,7,9,11,12,14,15,17
    {KeyScales::ScaleAugmented, {3,1,3,1,3,2}}, // 0,3,4,7,8,11
    {KeyScales::ScaleBeebopDominant, {2, 2, 1, 2, 2, 1, 1, 1}}, // 0,2,4,5,7,9,10,11
    {KeyScales::ScaleBlues, {3, 2, 1, 1, 3, 2}}, // 0,3,5,6,7,10
    {KeyScales::ScaleChromatic, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 0,1,2,3,4,5,6,7,8,9,10,11
    {KeyScales::ScaleDorian, {2, 1, 2, 2, 2, 1, 2}}, // 0,2,3,5,7,9,10
    {KeyScales::ScaleDoubleHarmonic, {1, 3, 1, 2, 1, 3, 1}}, // 0,1,4,5,7,8,11
    {KeyScales::ScaleEnigmatic, {1, 3, 2, 2, 2, 1, 1}}, // 0,1,4,6,8,10,11
    {KeyScales::ScaleFlamenco, {1, 3, 1, 2, 1, 3, 1}}, // 0,1,4,5,7,8,11
    {KeyScales::ScaleGypsy, {2, 1, 3, 1, 1, 2, 2}}, // 0,2,3,6,7,8,10
    {KeyScales::ScaleHalfDiminished, {2, 1, 2, 1, 2, 2, 2}}, // 0,2,3,5,6,8,10
    {KeyScales::ScaleHarmonicMajor, {2, 2, 1, 2, 1, 3, 1}}, // 0,2,4,5,7,8,11
    {KeyScales::ScaleHarmonicMinor, {2, 1, 2, 2, 1, 3, 1}}, // 0,2,3,5,7,8,11
    {KeyScales::ScaleHarmonics, {3, 1, 1, 2, 2, 3}}, // 0,3,4,5,7,9
    {KeyScales::ScaleHirajoshi, {4, 2, 1, 4, 1}}, // 0,4,6,7,11
    {KeyScales::ScaleHungarianMajor, {3, 1, 2, 1, 2, 1, 2}}, // 0,3,4,6,7,9,10
    {KeyScales::ScaleHungarianMinor, {2, 1, 3, 1, 1, 3, 1}}, // 0,2,3,6,7,8,11
    {KeyScales::ScaleIn, {1, 4, 2, 1, 4}}, // 0,1,5,7,8
    {KeyScales::ScaleInsen, {1, 4, 2, 3, 2}}, // 0,1,5,7,10
    {KeyScales::ScaleIonian, {2, 2, 1, 2, 2, 2, 1}}, // 0,2,4,5,7,9,11
    {KeyScales::ScaleIstrian, {1, 2, 1, 2, 1, 5}}, // 0,1,3,4,6,7
    {KeyScales::ScaleIwato, {1, 4, 1, 4, 2}}, // 0,1,5,6,10
    {KeyScales::ScaleLydian, {2, 2, 2, 1, 2, 2, 1}}, // 0,2,4,6,7,9,11
    {KeyScales::ScaleLydianAugmented, {2, 2, 2, 2, 1, 2, 1}}, // 0,2,4,6,8,9,11
    {KeyScales::ScaleLydianDiminished, {2, 1, 3, 1, 2, 2, 1}}, // 0,2,3,6,7,9,11
    {KeyScales::ScaleLydianDominant, {2, 2, 2, 1, 2, 1, 2}}, // 0,2,4,6,7,9,10
    {KeyScales::ScaleLocrian, {1, 2, 2, 1, 2, 2, 2}}, // 0,1,3,5,6,8,10
    {KeyScales::ScaleMajor, {2, 2, 1, 2, 2, 2, 1}}, // 0,2,4,5,7,9,11
    {KeyScales::ScaleMajorBebop, {2, 2, 1, 2, 1, 1, 2, 1}}, // 0,2,4,5,7,(8),9,11
    {KeyScales::ScaleMajorLocrian, {2, 2, 1, 1, 2, 2, 2}}, // 0,2,4,5,6,8,10
    {KeyScales::ScaleMajorPentatonic, {2, 2, 3, 2, 3}}, // 0,2,4,7,9
    {KeyScales::ScaleMelodicMinorAscending, {2, 1, 2, 2, 2, 2, 1}}, // 0,2,3,5,7,9,11
    {KeyScales::ScaleMelodicMinorDescending, {2, 1, 2, 2, 1, 2, 2}}, // 12,10,8,7,5,3,2
    {KeyScales::ScaleMelodicMinorAscendingDescending, {2, 1, 2, 2, 2, 2, 1, 2, 1, 2, 2, 1, 2, 2}}, // 0,2,3,5,7,9,11
    {KeyScales::ScaleMelodicMinorDescendingAscending, {2, 1, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 2, 1}}, // 12,10,8,7,5,3,2
    {KeyScales::ScaleMinorPentatonic, {3, 2, 2, 3, 2}}, // 0,3,5,7,10
    {KeyScales::ScaleMixolydian, {2, 2, 1, 2, 2, 1, 2}}, // 0,2,4,5,7,9,10
    {KeyScales::ScaleNaturalMinor,  {2, 1, 2, 2, 1, 2, 2}}, // 0,2,3,5,7,8,10
    {KeyScales::ScaleNeopolitanMajor, {1, 2, 2, 2, 2, 2, 1}}, // 0,1,3,5,7,9,11
    {KeyScales::ScaleNeopolitanMinor, {1, 2, 2, 2, 1, 3, 1}}, // 0,1,3,5,7,8,11
    {KeyScales::ScalePersian, {1, 3, 1, 1, 2, 3, 1}}, // 0,1,4,5,6,8,11
    {KeyScales::ScalePhrygian, {1, 2, 2, 2, 1, 2, 2}}, // 0,1,3,5,7,8,10
    {KeyScales::ScalePhrygianDominant, {1, 3, 1, 2, 1, 2, 2}}, // 0,1,4,5,7,8,10
    {KeyScales::ScalePrometheus, {2, 2, 2, 3, 1, 2}}, // 0,2,4,6,9,10
    {KeyScales::ScaleSuperLocrian, {1,2,1,2,2,2,2}}, // 0,1,3,4,6,8,10
    {KeyScales::ScaleTritone, {1, 3, 2, 1, 3, 2}}, // 0,1,4,6,7,10
    {KeyScales::ScaleTwoSemitoneTritone, {1, 1, 4, 1, 1, 4}}, // 0,1,2,6,7,8
    {KeyScales::ScaleUkranianDorian, {2, 1, 3, 1, 2, 1, 2}}, // 0,2,3,6,7,9,10
    {KeyScales::ScaleWholeTone, {2, 2, 2, 2, 2, 2}}, // 0,2,4,6,8,10
    {KeyScales::ScaleYo, {3, 2, 2, 3, 2}}, // 0,3,5,7,10
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
                // - the notes which exist in the scale, for which we use a convenience QList, as it makes building the list a bit easier
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
                int noteIndex{0};
                for (const int &scaleNote : qAsConst(scaleRootAllNotes)) {
                    noteIndices[scaleName][rootNote][scaleNote] = noteIndex;
                    ++noteIndex;
                    while (anyNote <= scaleNote) {
                        nearestNote[scaleName][rootNote][anyNote] = scaleNote;
                        ++anyNote;
                    }
                }
                // And finally, add these lists to our scale data
                for (int index = 0; index < scaleRootAllNotes.count(); ++index) {
                    allNotes[scaleName][rootNote][index] = scaleRootAllNotes[index];
                }
                // And fill out the array, for good measure
                for (int index = scaleRootAllNotes.count(); index < 128; ++index) {
                    allNotes[scaleName][rootNote][index] = scaleRootAllNotes.constLast();
                }
            }
        }
    }
    // Pre-calculated table of what the nearest note is for any valid midi note, for a given root note, in the known scales
    int nearestNote[scaleCount][128][128]{{{0}}};
    // Pre-calculated table of all notes in a scale, for any given root note, in the known scales
    int allNotes[scaleCount][128][128]{{{0}}};
    // Pre-calculated table of the indices in allNotes of for any given note
    int noteIndices[scaleCount][128][128]{{{-1}}};
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
    static QStringList names;
    if (names.length() == 0) {
        for (int index = 0; index < pitchIndices.length(); ++index) {
            names << pitchNamesHash[pitchIndices[index]];
        }
    }
    return names;
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

KeyScales::Pitch KeyScales::midiNoteToPitch(const int& midiNote) const
{
    return pitchForMidiValue[std::clamp(midiNote, 0, 127) % 12];
}

QString KeyScales::scaleName(const Scale& scale) const
{
    return scaleNamesHash[scale];
}

QStringList KeyScales::scaleNames() const
{
    static QStringList names;
    if (names.length() == 0) {
        for (int index = 0; index < scaleIndices.length(); ++index) {
            names << scaleNamesHash[scaleIndices[index]];
        }
    }
    return names;
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
    static QStringList names;
    if (names.length() == 0) {
        for (int index = 0; index < octaveIndices.length(); ++index) {
            names << octaveNamesHash[octaveIndices[index]];
        }
    }
    return names;
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

QString KeyScales::midiNoteName(const int& midiNote) const
{
    return QString::fromUtf8("%1%2").arg(pitchName(midiNoteToPitch(midiNote))).arg(octaveName(midiNoteToOctave(midiNote)));
}

KeyScales::Octave KeyScales::midiNoteToOctave(const int& midiNote) const
{
    // static const QMetaEnum octaveNameMeta = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("Octave"));
    const int octaveValue = 12 * (std::clamp(midiNote, 0, 127) / 12);
    return static_cast<KeyScales::Octave>(octaveValue);
}

int KeyScales::onScaleNote(const int& midiNote, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    return d->nearestNote[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)][std::clamp(midiNote, 0, 127)];
}

int KeyScales::transposeNote(const int& midiNote, const int& steps, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    int remainingSteps{steps};
    int transposedNote{midiNote};
    const int rootNote{std::clamp(octave + pitchValuesHash[pitch], 0, 127)};
    // If the note is NOT on scale, perform the first transposition step as moving it onto the scale (either up or down, depending on step direction)
    if (d->noteIndices[scale][rootNote][midiNote] == -1) {
        transposedNote = onScaleNote(midiNote, scale, pitch, octave);
        // onScaleNote returns the next step upward, so if we're asking for downward movement, we need to move down by one step extra (so moving one step up, we've now done that and remainingSteps goes from 1 to 0)
        // If we are moving upward, we need to move one step less (so moving an extra step down, meaning remainingSteps goes from -1 to -2)
        // The result here is that the remaining steps should be adjusted by -1 for both cases
        --remainingSteps;
    }
    // If the note is on scale and there are still steps to be performed, do normal transposition
    int scaleNoteIndex{d->noteIndices[scale][rootNote][transposedNote]};
    // qDebug() << Q_FUNC_INFO << midiNote << steps << scale << pitch << octave << "Closest on-scale note to our origin note" << midiNote << "is" << transposedNote << "at position" << scaleNoteIndex << "meaning our transposed note should be" << d->allNotes[scale][rootNote][std::clamp(scaleNoteIndex + remainingSteps, 0, 127)];
    return d->allNotes[scale][rootNote][std::clamp(scaleNoteIndex + remainingSteps, 0, 127)];
}

bool KeyScales::midiNoteOnScale(const int& midiNote, const Scale& scale, const Pitch& pitch, const Octave& octave) const
{
    bool returnVal{false};
    for (int index = 0; index < 128; ++index) {
        if (d->allNotes[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)][index] == midiNote) {
            returnVal = true;
            break;
        }
    }
    // qDebug() << Q_FUNC_INFO << midiNote << scale << pitch << octave << returnVal << d->allNotes[scale][std::clamp(octave + pitchValuesHash[pitch], 0, 127)];
    return returnVal;
}
