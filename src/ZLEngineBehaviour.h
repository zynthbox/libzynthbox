#pragma once

#include "JUCEHeaders.h"
#include <QDebug>

class ZLEngineBehavior : public tracktion::engine::EngineBehaviour {
    bool autoInitialiseDeviceManager() override { qDebug() << "autoInitialiseDeviceManager? : Absolutely NOT !!!"; return false; }
    bool addSystemAudioIODeviceTypes() override { qDebug() << "addSystemAudioIODeviceTypes? : Absolutely NOT !!!"; return false; }
};
