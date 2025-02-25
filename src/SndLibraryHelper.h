#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>


/**
 * @brief The SndLibraryHelper class provides helper methods to manage, index and lookup `.snd` files
 */
class SndLibraryHelper : public QObject
{
    Q_OBJECT
public:
    static SndLibraryHelper* instance() {
        static SndLibraryHelper* instance{nullptr};
        if (!instance) {
            instance = new SndLibraryHelper(qApp);
        }
        return instance;
    }
    // Delete the methods we dont want to avoid having copies of the singleton class
    SndLibraryHelper(SndLibraryHelper const&) = delete;
    void operator=(SndLibraryHelper const&) = delete;

    Q_INVOKABLE void serializeTo(const QString sourceDir, const QString outputFile);
private:
    explicit SndLibraryHelper(QObject *parent = nullptr);
};
