#pragma once

#include <QObject>
#include <QCoreApplication>

class AdjectiveNounPrivate;
/**
 * \brief Generator of pairs of adjectives and nouns, with Capital Casing
 */
class AdjectiveNoun : public QObject
{
    Q_OBJECT
public:
    static AdjectiveNoun* instance() {
        static AdjectiveNoun* instance{nullptr};
        if (!instance) {
            instance = new AdjectiveNoun(qApp);
        }
        return instance;
    };
    explicit AdjectiveNoun(QObject *parent = nullptr);
    ~AdjectiveNoun() override;

    /**
     * \brief A random Adjective from the list
     * @return A string containing one random Adjective
     */
    Q_INVOKABLE const QString & adjective() const;
    /**
     * \brief A random Noun from the list
     * @return A string containing one random Noun
     */
    Q_INVOKABLE const QString & noun() const;
    /**
     * \brief Get a string in the format "Adjective Noun"
     * @returns A string containing one adjective and one noun, with a space between them
     */
    Q_INVOKABLE QString adjectiveNoun() const;
    /**
     * \brief Fill in the %1 in the given string with Adjective, and %2 with Noun
     * @param format The string in which you wish to replace %1 and %2 with an Adjective and Noun respectively
     * @return The formatted string
     */
    Q_INVOKABLE QString formatted(const QString &format) const;
private:
    AdjectiveNounPrivate *d{nullptr};
};
