#include "AppImageHelper.h"
#include <appimage/appimage.h>
#include <appimage/core/AppImage.h>
#include <appimage/desktop_integration/IntegrationManager.h>

void AppImageHelper::registerAppImage(QString path)
{
    auto manager = appimage::desktop_integration::IntegrationManager();
    if (!manager.isARegisteredAppImage(path.toStdString())) {
        manager.registerAppImage(appimage::core::AppImage(path.toStdString()));
    }
}

void AppImageHelper::unregisterAppImage(QString path)
{
    auto manager = appimage::desktop_integration::IntegrationManager();
    if (manager.isARegisteredAppImage(path.toStdString())) {
        manager.unregisterAppImage(path.toStdString());
    }
}

QString AppImageHelper::getAppImageMd5Hash(QString path)
{
    const char *path_cstr = path.toLatin1().data();
    return QString(QLatin1String(appimage_get_md5(path_cstr)));
}

AppImageHelper::AppImageHelper(QObject *parent) : QObject(parent)
{
}
