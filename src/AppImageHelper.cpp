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
