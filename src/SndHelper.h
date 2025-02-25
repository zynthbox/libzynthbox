#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>


/**
 * @brief The SndHelper class provides helper methods to manage, index and lookup `.snd` files
 */
class SndHelper : public QObject
{
    Q_OBJECT
public:
    static SndHelper* instance() {
        static SndHelper* instance{nullptr};
        if (!instance) {
            instance = new SndHelper(qApp);
        }
        return instance;
    }
    // Delete the methods we dont want to avoid having copies of the singleton class
    SndHelper(SndHelper const&) = delete;
    void operator=(SndHelper const&) = delete;

    Q_INVOKABLE void serializeTo(const QString sourceDir, const QString outputFile);
private:
    explicit SndHelper(QObject *parent = nullptr);
};
