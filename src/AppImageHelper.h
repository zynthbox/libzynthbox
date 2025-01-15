#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>


/**
 * @brief The AppImageHelper class provides helper methods to interact with appimages
 *
 * This class provides static methods to allow registering an appimage with the system
 * and unregister integrated appimages from system
 */
class AppImageHelper : public QObject
{
    Q_OBJECT
public:
    static AppImageHelper* instance() {
        static AppImageHelper* instance{nullptr};
        if (!instance) {
            instance = new AppImageHelper(qApp);
        }
        return instance;
    }
    // Delete the methods we dont want to avoid having copies of the singleton class
    AppImageHelper(AppImageHelper const&) = delete;
    void operator=(AppImageHelper const&) = delete;

    Q_INVOKABLE void registerAppImage(QString path);
    Q_INVOKABLE void unregisterAppImage(QString path);
    Q_INVOKABLE QString getAppImageMd5Hash(QString path);
private:
    explicit AppImageHelper(QObject *parent = nullptr);
};
