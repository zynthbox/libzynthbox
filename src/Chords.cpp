#include "Chords.h"
#include "Note.h"

#include <QDebug>
#include <QMap>
#include <QVariant>

// Most of these come from tracktion's Musicality implementation, but as that's buried, we can't use that directly
// plus we have different needs, so rolling our own stuff here a bit
static const QMap<QStringList, QList<int>> chordData{
    {{QLatin1String{"M"}, QLatin1String{"major"}, QLatin1String{"Major Triad"}}, {0, 4, 7}},
    {{QLatin1String{"m"}, QLatin1String{"minor"}, QLatin1String{"Minor Triad"}}, {0, 3, 7}},
    {{QLatin1String{"o"}, QLatin1String{"dim"}, QLatin1String{"Diminished"}}, {0, 3, 6}},
    {{QLatin1String{"+M"}, QLatin1String{"aug"}, QLatin1String{"Augmented"}}, {0, 4, 8}},
    {{QLatin1String{"M6"}, QLatin1String{"major 6"}, QLatin1String{"Major Sixth"}}, {0, 4, 7, 9}},
    {{QLatin1String{"m6"}, QLatin1String{"minor 6"}, QLatin1String{"Minor Sixth"}}, {0, 3, 7, 9}},
    {{QLatin1String{"7"}, QLatin1String{"dom 7"}, QLatin1String{"Dominant Seventh"}}, {0, 4, 7, 10}},
    {{QLatin1String{"M7"}, QLatin1String{"major 7"}, QLatin1String{"Major Seventh"}}, {0, 4, 7, 11}},
    {{QLatin1String{"m7"}, QLatin1String{"minor 7"}, QLatin1String{"Minor Seventh"}}, {0, 3, 7, 10}},
    {{QLatin1String{"+7"}, QLatin1String{"aug 7"}, QLatin1String{"Augmented Seventh"}}, {0, 4, 8, 10}},
    {{QLatin1String{"o7"}, QLatin1String{"dim 7"}, QLatin1String{"Diminished Seventh"}}, {0, 3, 6, 9}},
    {{QString::fromUtf8("ø7"), QLatin1String{"half dim 7"}, QLatin1String{"Half-diminished Seventh"}}, {0, 3, 6, 10}},
    {{QLatin1String{"mM7"}, QLatin1String{"min maj 7"}, QLatin1String{"Minor-major Seventh"}}, {0, 3, 7, 11}},
    {{QLatin1String{"+M7"}, QLatin1String{"aug maj 7"}, QLatin1String{"Augmented Major Seventh"}}, {0, 4, 8, 11}},
    {{QLatin1String{"5"}, QLatin1String{"power"}, QLatin1String{"Power Chord"}}, {0, 7}},
    {{QLatin1String{"sus2"}, QLatin1String{"sus 2"}, QLatin1String{"Suspended Second"}}, {0, 2, 7}},
    {{QLatin1String{"sus4"}, QLatin1String{"sus 4"}, QLatin1String{"Suspended Fourth"}}, {0, 5, 7}},
    {{QLatin1String{"M9"}, QLatin1String{"major 9"}, QLatin1String{"Major Ninth"}}, {0, 4, 7, 11, 14}},
    {{QLatin1String{"9"}, QLatin1String{"dom 9"}, QLatin1String{"Dominant Ninth"}}, {0, 4, 7, 10, 14}},
    {{QLatin1String{"mM9"}, QLatin1String{"min maj 9"}, QLatin1String{"Minor Major Ninth"}}, {0, 3, 7, 11, 14}},
    {{QLatin1String{"m9"}, QLatin1String{"min dom 9"}, QLatin1String{"Minor Dominant Ninth"}}, {0, 3, 7, 10, 14}},
    {{QLatin1String{"+M9"}, QLatin1String{"aug maj 9"}, QLatin1String{"Augmented Major Ninth"}}, {0, 4, 8, 11, 14}},
    {{QLatin1String{"+9"}, QLatin1String{"aug dom 9"}, QLatin1String{"Augmented Dominant Ninth"}}, {0, 4, 8, 10, 14}},
    {{QString::fromUtf8("ø9"), QLatin1String{"half dim 9"}, QLatin1String{"Half Diminished Ninth"}}, {0, 3, 6, 10, 14}},
    {{QString::fromUtf8("ø♭9"), QLatin1String{"half dim min 9"}, QLatin1String{"Half Diminished Minor Ninth"}}, {0, 3, 6, 10, 13}},
    {{QLatin1String{"o9"}, QLatin1String{"dim 9"}, QLatin1String{"Diminished Ninth"}}, {0, 3, 6, 9, 14}},
    {{QString::fromUtf8("o♭9"), QLatin1String{"dim min 9"}, QLatin1String{"Diminished Minor Ninth"}}, {0, 3, 6, 9, 13}},
};

// Notation for these is essentially a kind of Italian abbreviation:
// Octave above (that is, one note at the octave above at the same time as the original note is also played) is notated 8va for 'ottava'
// Octave below (that is, one note at the octave below at the same time as the original note is also played) is notated 8vb 'ottava bassa'
// Two octaves above (that is, one note at the octave two above at the same time as the original note is also played) is notated 15ma for 'quindicesima'
// Two octaves below (that is, one note at the octave two below at the same time as the original note is also played) is notated 15mb for 'quindicesima bassa'
// To achieve this in a computational manner, we have the following extra options for each note, in each chord (that is, all combinations of the -24, -12, +12, and +24 notes from the tonic)
// 8va, 8va8vb, 8va8vb15ma, 8va8vb15mb, 8va8vb15ma15mb, 8va15ma, 8va15mb, 8va15ma15mb, 8vb, 8vb15ma, 8vb15mb, 8vb15ma15mb, 15ma, 15ma15mb, 15mb
// The structure here holds the shorthand addition in the .first, full name in .second, and the extra note positions
// Note that these are, in fact, not really correct and should be considered initial test work (as the octaving is only considered for
// the first note of the chord, not for subsequent notes, and the notation is imprecise)
// FIXME Disabling for now, as it causes clashes with polychord discovery
static const QMap<QStringList, QList<int>> octavingData{
    {{QString::fromUtf8(""), QLatin1String{""}, QLatin1String{""}}, {}}, // First entry is "no octaving", the base chord
    // {{QString::fromUtf8(" 8ᵛᵃ"), QString::fromUtf8(" 8ᵛᵃ"), QLatin1String{" Ottava"}}, {12}},
    // {{QString::fromUtf8(" 8ᵛᵃᵇ"), QString::fromUtf8(" 8ᵛᵃ 8ᵛᵇ"), QLatin1String{" Ottava Ottava bassa"}}, {-12, 12}},
    // {{QString::fromUtf8(" 8ᵛᵃ 15ᵐᵃ"), QString::fromUtf8(" 8ᵛᵃ 15ᵐᵃ"), QLatin1String{" Ottava Quindicesima"}}, {12, 24}},
    // {{QString::fromUtf8(" 8ᵛᵃ 15ᵐᵇ"), QString::fromUtf8(" 8ᵛᵃ 15ᵐᵇ"), QLatin1String{" Ottava Quindicesima bassa"}}, {-24, 12}},
    // {{QString::fromUtf8(" 8ᵛᵃ 15ᵐᵃᵇ"), QString::fromUtf8("8ᵛᵃ 15ma 15ᵐᵇ"), QLatin1String{" Ottava Quindicesima Quindicesima bassa"}}, {-24, 12, 24}},
    // {{QString::fromUtf8(" 8ᵛᵃᵇ 15ᵐᵃ"), QString::fromUtf8(" 8ᵛᵃ 8ᵛᵇ 15ᵐᵃ"), QLatin1String{" Ottava Ottava bassa Quindicesima"}}, {-12, 12, 24}},
    // {{QString::fromUtf8(" 8ᵛᵃᵇ 15ᵐᵇ"), QString::fromUtf8(" 8ᵛᵃ 8ᵛᵇ 15ᵐᵇ"), QLatin1String{" Ottava Ottava bassa Quindicesima bassa"}}, {-24, -12, 12}},
    // {{QString::fromUtf8(" 8ᵛᵃᵇ 15ᵐᵃᵇ"), QString::fromUtf8(" 8ᵛᵃ 8ᵛᵇ 15ᵐᵃ 15vb"), QLatin1String{" Ottava Ottava bassa Quindicesima Quindicesima bassa"}}, {-24, -12, 12, 24}},
    // {{QString::fromUtf8(" 8ᵛᵇ"), QString::fromUtf8(" 8ᵛᵇ"), QLatin1String{" Ottava bassa"}}, {-12}},
    // {{QString::fromUtf8(" 8ᵛᵇ 15ᵐᵃ"), QString::fromUtf8(" 8ᵛᵇ 15ᵐᵃ"), QLatin1String{" Ottava bassa Quindicesima"}}, {-12, 24}},
    // {{QString::fromUtf8(" 8ᵛᵇ 15ᵐᵇ"), QString::fromUtf8(" 8ᵛᵇ 15ᵐᵇ"), QLatin1String{" Ottava bassa Quindicesima bassa"}}, {-24, -12}},
    // {{QString::fromUtf8(" 8ᵛᵇ 15ᵐᵃᵇ"), QString::fromUtf8(" 8ᵛᵇ 15ᵐᵃ 15ᵐᵇ"), QLatin1String{" Ottava bassa Quindicesima Quindicesima bassa"}}, {-24, 12, 24}},
    // {{QString::fromUtf8(" 15ᵐᵃ"), QString::fromUtf8(" 15ᵐᵃ"), QLatin1String{" Quindicesima"}}, {24}},
    // {{QString::fromUtf8(" 15ᵐᵃᵇ"), QString::fromUtf8(" 15ᵐᵃ 15ᵐᵇ"), QLatin1String{" Quindicesima Quindicesima bassa"}}, {-24, 24}},
    // {{QString::fromUtf8(" 15ᵐᵇ"), QString::fromUtf8(" 15ᵐᵇ"), QLatin1String{" Quindicesima bassa"}}, {-24}},
};

/**
 * \brief A node in a search graph for chords
 */
class ChordStepNode {
public:
    explicit ChordStepNode() {}
    ~ChordStepNode() {
        for (int note = 0; note < 128; ++note) {
            delete nextSteps[note];
        }
    }
    QString symbolicName; // The symbolic name for the chord this node in the tree represents
    QString shorthandName; // The shorthand name for the chord this node in the tree represents
    QString fullName; // The name of the chord that this node in the tree represents
    int rootOffset{0}; // In case of an bassa-octaved chord, this is used to define the "real" root note of the chord (that is, how much do you need to add to the root note to get that root)
    QList<int> notes; // The notes which make up this chord
    bool isValidChord{false}; // Whether or not this node represents a valid chord

    ChordStepNode* nextSteps[128]{nullptr};
    /**
     * @param notes A list of notes in chromatic order to search and match against the chord tree
     * @param lastEntryIndex The last entry in the list to consider for chord discovery
     * @param depth The current position in the list
     * @param rootNoteOffset This is subtracted from the note values in `notes` when searching
     */
    ChordStepNode* getChord(const QList<int> &notes, const int &lastEntryIndex, const int &depth = 0, const int &rootNoteOffset = 0) {
        if (depth == lastEntryIndex) {
            // In this case, this is the end of the line and we have found our chord
            if (isValidChord) {
                // qDebug() << Q_FUNC_INFO << "We've found our chord!" << fullName;
                return this;
            }
        } else {
            const int nextDepth{depth + 1};
            ChordStepNode *nextStep{nextSteps[notes[nextDepth] - rootNoteOffset]};
            if (nextStep) {
                // qDebug() << Q_FUNC_INFO << "Found a step on the way, we have to go deeper..." << notes << lastEntryIndex << depth;
                return nextStep->getChord(notes, lastEntryIndex, nextDepth, rootNoteOffset);
            }
        }
        // If we've arrived here, then this list of notes does not represent a chord we are aware of
        // qDebug() << "Aw maaaaan no chord found for" << notes << lastEntryIndex << depth;
        return nullptr;
    }
    void addNode(const QList<int> &notes, const int &lastEntryIndex, const int &depth, const QString &symbolicName, const QString &shorthandName, const QString &fullName, const int &rootOffset) {
        const int nextDepth{depth + 1};
        const int nextNote = notes[nextDepth];
        if (nextSteps[nextNote] == nullptr) {
            // qDebug() << Q_FUNC_INFO << "Missing step for" << nextNote;
            nextSteps[nextNote] = new ChordStepNode();
        }
        if (nextDepth == lastEntryIndex) {
            ChordStepNode *node = nextSteps[nextNote];
            // qDebug() << Q_FUNC_INFO << "Reached last step for" << notes << "named" << symbolicName << "so setting data on" << node;
            node->symbolicName = symbolicName;
            node->shorthandName = shorthandName;
            node->fullName = fullName;
            node->rootOffset = rootOffset;
            node->notes = notes;
            node->isValidChord = true;
        } else if (nextDepth < lastEntryIndex) {
            nextSteps[nextNote]->addNode(notes, lastEntryIndex, nextDepth, symbolicName, shorthandName, fullName, rootOffset);
        }
    }
};

struct ScaleInfo {
    ScaleInfo() {
        chordTree.fullName = "Chord Tree Root";
        chordTree.shorthandName = "Root";
        chordTree.symbolicName = "root";
        chordTree.notes = {0};
    }
    ChordStepNode chordTree; // The first note is always 0, giving us a root to search through
};

// Update whenever the number of scales changes
// Also, if we do eventually end up handling scales where the names and whatnot are different, this will need to change as well, but for now, just assume everything is on the chromatic scale
// #define scaleCount 52
class Chords::Private {
public:
    Private() {
        QMap<QStringList, QList<int>>::const_iterator octaveIterator;
        QMap<QStringList, QList<int>>::const_iterator chordIterator;
        // for (int scaleIndex = 0; scaleIndex < scaleCount; ++scaleIndex) {
            // const KeyScales::Scale scale{KeyScales::instance()->scaleIndexToEnumKey(scaleIndex)};
            for (octaveIterator = octavingData.constBegin(); octaveIterator != octavingData.constEnd(); ++octaveIterator) {
                for (chordIterator = chordData.constBegin(); chordIterator != chordData.constEnd(); ++chordIterator) {
                    QList<int> fullChordWithOctaving;
                    fullChordWithOctaving.reserve(8);
                    const QList<int> &octavingList = octaveIterator.value();
                    const QList<int> &chordList = chordIterator.value();
                    // To ensure our notes are spaced correctly with bassa octaving, offset all the chord's notes by the lowest number in the octaving's list, but only if it's negative
                    int rootOffset{0};
                    if (octavingList.length() > 0 && octavingList[0] < 0) {
                        rootOffset = abs(octavingList[0]);
                    }
                    // Construct the chord itself, given the root offset found above
                    for (const int &chordEntry : chordList) {
                        fullChordWithOctaving.append(chordEntry + rootOffset);
                    }
                    // Add the octaving data
                    const int firstNoteInChord{chordList[0] + rootOffset};
                    for (const int &octavingEntry : qAsConst(octavingList)) {
                        fullChordWithOctaving.append(firstNoteInChord + octavingEntry);
                    }
                    // Make sure the notes are in the appropriate order
                    std::sort(fullChordWithOctaving.begin(), fullChordWithOctaving.end());
                    // Build the names
                    QString chordSymbol = QString::fromUtf8("%1%2").arg(chordIterator.key()[0]).arg(octaveIterator.key()[0]);
                    QString chordShorthand = QString::fromUtf8("%1%2").arg(chordIterator.key()[1]).arg(octaveIterator.key()[1]);
                    QString chordFullName = QString::fromUtf8("%1%2").arg(chordIterator.key()[2]).arg(octaveIterator.key()[2]);
                    // And finally, add the octave to the tree
                    // qDebug() << Q_FUNC_INFO << fullChordWithOctaving << chordFullName;
                    scaleInfo.chordTree.addNode(fullChordWithOctaving, fullChordWithOctaving.count() - 1, 0, chordSymbol, chordShorthand, chordFullName, rootOffset);
                }
            }
        // }
    }
    ~Private() {}

    enum NameType {
        SymbolicName,
        ShorthandName,
        FullName,
    };
    static const inline QString nameChord(const ChordStepNode *chordNode, const NameType &nameType, const int &firstNote) {
        return nameType == SymbolicName
            ? QString::fromUtf8("%1%2").arg(KeyScales::instance()->midiNoteName(firstNote + chordNode->rootOffset)).arg(chordNode->symbolicName)
            : nameType == ShorthandName
                ? QString::fromUtf8("%1%2").arg(KeyScales::instance()->midiNoteName(firstNote + chordNode->rootOffset)).arg(chordNode->shorthandName)
                : QString::fromUtf8("%1%2").arg(KeyScales::instance()->midiNoteName(firstNote + chordNode->rootOffset)).arg(chordNode->fullName);
    }
    // TODO Do discovery of octaving on the remaining notes (which will require some reporting back to let the previous chord search in a list of things from the above chord somehow...)
    QStringList describeNotesRecursive(const QList<int> &actualNotes, const KeyScales::Scale &scale, const KeyScales::Pitch &pitch, const KeyScales::Octave &octave, const NameType &nameType, const int &searchFrom, const int &firstNote, const int &numberOfNotes) {
        QStringList nameList;
        ChordStepNode *chordNode{nullptr};
        int startPosition{searchFrom};
        for (;startPosition < numberOfNotes; ++startPosition) {
            for (int stopPosition = numberOfNotes - 1; stopPosition > startPosition; --stopPosition) {
                // qDebug() << Q_FUNC_INFO << "Looking start position" << startPosition << "stopping at" << stopPosition << "in" << actualNotes;
                chordNode = scaleInfo.chordTree.getChord(actualNotes, stopPosition, startPosition, actualNotes[startPosition]);
                if (chordNode) {
                    // qDebug() << Q_FUNC_INFO << "Found chord" << chordNode->fullName;
                    break;
                }
            }
            if (chordNode) {
                break;
            }
        }
        if (chordNode == &scaleInfo.chordTree) {
            // Just a small sanity check, this will happen when we reach the end
            // qDebug() << Q_FUNC_INFO << "The discovered chord is the root node, which isn't a chord (it essentially represents a single stray note), and should be ignored";
            chordNode = nullptr;
        }
        if (startPosition > searchFrom || chordNode == nullptr) {
            if (searchFrom > 0) {
                // If we have stray notes past the first note in the list, we're creating a polychord description
                nameList.prepend(QString::fromUtf8("—"));
            }
            if (startPosition == searchFrom + 1 || searchFrom == numberOfNotes - 1) {
                // Then we have precisely one non-chord note at the start of the list, so add that to the name list
                nameList.prepend(KeyScales::instance()->midiNoteName(actualNotes[searchFrom] + firstNote));
            } else {
                nameList.prepend(QString::fromUtf8("%1♫").arg(startPosition - searchFrom));
            }
            // qDebug() << Q_FUNC_INFO << "Position" << searchFrom << "has stray notes" << nameList.constLast();
        }
        if (chordNode) {
            if (startPosition > 0) {
                // If we have a chord past the first note in the list, we're creating a polychord description
                nameList.prepend(QString::fromUtf8("—"));
            }
            nameList.prepend(nameChord(chordNode, nameType, firstNote + actualNotes[startPosition]));
            const int nextStartPosition{startPosition + chordNode->notes.length()};
            // qDebug() << Q_FUNC_INFO << "Position" << startPosition << "has chord" << nameList.constLast() << chordNode->fullName << "of length" << chordNode->notes.length() << "and next start position is" << nextStartPosition << "with" << numberOfNotes << "notes";
            if (nextStartPosition < numberOfNotes) {
                nameList = describeNotesRecursive(actualNotes, scale, pitch, octave, nameType, nextStartPosition, firstNote, numberOfNotes) + nameList;
            }
        }
        return nameList;
    }

    QString describeNotes(const QVariantList &notes, const KeyScales::Scale &scale, const KeyScales::Pitch &pitch, const KeyScales::Octave &octave, const QString &elementSeparator, const NameType &nameType, const int &searchFrom = 0) {
        // qDebug() << Q_FUNC_INFO << notes;
        const int numberOfNotes{notes.length()};
        if (numberOfNotes == 0) {
            // what are you doing that's an empty list?
            return {};
        } else if (numberOfNotes == 1) {
            const QVariant &variant = notes[0];
            const Note *note = qobject_cast<const Note*>(variant.value<QObject*>());
            if (note) {
                return KeyScales::instance()->midiNoteName(note->midiNote());
            } else {
                return KeyScales::instance()->midiNoteName(variant.toInt());
            }
        } else {
            QList<int> actualNotes;
            for (int noteIndex = 0; noteIndex < numberOfNotes; ++noteIndex) {
                const QVariant &variant = notes[noteIndex];
                const Note *note = qobject_cast<const Note*>(variant.value<QObject*>());
                if (note) {
                    actualNotes.append(note->midiNote());
                } else {
                    actualNotes.append(variant.toInt());
                }
            }
            std::sort(actualNotes.begin(), actualNotes.end());
            const int firstNote{actualNotes[0]};
            for (int noteIndex = 0; noteIndex < numberOfNotes; ++noteIndex) {
                actualNotes[noteIndex] = actualNotes[noteIndex] - firstNote;
            }
            return describeNotesRecursive(actualNotes, scale, pitch, octave, nameType, searchFrom, firstNote, numberOfNotes).join(elementSeparator);
        }
    }
    ScaleInfo scaleInfo;
    // ScaleInfo scaleInfos[scaleCount];
};

Chords::Chords(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Chords::~Chords()
{
    delete d;
}

const QString Chords::fullName(const QVariantList &notes, const KeyScales::Scale &scale, const KeyScales::Pitch &pitch, const KeyScales::Octave &octave, const QString &elementSeparator) const
{
    return d->describeNotes(notes, scale, pitch, octave, elementSeparator, Private::FullName);
}

const QString Chords::shorthand(const QVariantList &notes, const KeyScales::Scale &scale, const KeyScales::Pitch &pitch, const KeyScales::Octave &octave, const QString &elementSeparator) const
{
    return d->describeNotes(notes, scale, pitch, octave, elementSeparator, Private::ShorthandName);
}

const QString Chords::symbol(const QVariantList &notes, const KeyScales::Scale &scale, const KeyScales::Pitch &pitch, const KeyScales::Octave &octave, const QString &elementSeparator) const
{
    return d->describeNotes(notes, scale, pitch, octave, elementSeparator, Private::SymbolicName);
}
