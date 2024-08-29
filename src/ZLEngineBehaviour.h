#pragma once

#include "JUCEHeaders.h"

class ZLEngineBehavior : public tracktion::engine::EngineBehaviour {
  bool autoInitialiseDeviceManager() override { return false; }
};
