#pragma once

#include "JUCEHeaders.h"

class ZLEngineBehavior : public tracktion::EngineBehaviour {
  bool autoInitialiseDeviceManager() override { return false; }
};
