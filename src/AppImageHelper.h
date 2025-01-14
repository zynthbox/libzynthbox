#pragma once

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
    static void registerAppImage(QString path);
    static void unregisterAppImage(QString path);
};
