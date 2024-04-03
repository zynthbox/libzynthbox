#pragma once

#include <QObject>
#include <QCoreApplication>

#include "KeyScales.h"

class Chords : public QObject
{
    Q_OBJECT
public:
    static Chords* instance() {
        static Chords* instance{nullptr};
        if (!instance) {
            instance = new Chords(qApp);
        }
        return instance;
    };
    explicit Chords(QObject * parent = nullptr);
    ~Chords() override;

    /**
     * \brief Returns the long-form name of the chord represented by the given list of notes (or a list of Note objects)
     * @param notes A list of midi note values
     * @param scale The scale the chord should be identified in
     * @param pitch The pitch of the root note the chord should be considered in
     * @param octave The octave of the root note the chord should be considered in
     * @param elementSeparator Will be inserted where separate elements of the name would be joined together
     * @return The common name of the chord represented by the given notes and key information
     */
    Q_INVOKABLE const QString fullName(const QVariantList &notes, const KeyScales::Scale &scale = KeyScales::ScaleChromatic, const KeyScales::Pitch &pitch = KeyScales::PitchC, const KeyScales::Octave &octave = KeyScales::Octave4, const QString &elementSeparator = " ") const;
    /**
     * \brief Returns the shorthand name of the chord represented by the given list of notes
     * @param notes A list of midi note values to find a chord shorthand for (or a list of Note objects)
     * @param scale The scale the chord should be identified in
     * @param pitch The pitch of the root note the chord should be considered in
     * @param octave The octave of the root note the chord should be considered in
     * @param elementSeparator Will be inserted where separate elements of the name would be joined together
     * @return The shorthand name of the chord represented by the given notes and key information
     */
    Q_INVOKABLE const QString shorthand(const QVariantList &notes, const KeyScales::Scale &scale = KeyScales::ScaleChromatic, const KeyScales::Pitch &pitch = KeyScales::PitchC, const KeyScales::Octave &octave = KeyScales::Octave4, const QString &elementSeparator = " ") const;
    /**
     * \brief Returns the symbolic name of the chord represented by the given list of notes
     * @param notes A list of midi note values to find a chord shorthand for (or a list of Note objects)
     * @param scale The scale the chord should be identified in
     * @param pitch The pitch of the root note the chord should be considered in
     * @param octave The octave of the root note the chord should be considered in
     * @param elementSeparator Will be inserted where separate elements of the name would be joined together
     * @return The symbolic name of the chord represented by the given notes and key information
     */
    Q_INVOKABLE const QString symbol(const QVariantList &notes, const KeyScales::Scale &scale = KeyScales::ScaleChromatic, const KeyScales::Pitch &pitch = KeyScales::PitchC, const KeyScales::Octave &octave = KeyScales::Octave4, const QString &elementSeparator = " ") const;
private:
    class Private;
    Private *d{nullptr};
};
