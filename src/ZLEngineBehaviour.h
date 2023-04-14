#include "JUCEHeaders.h"

class ZLEngineBehavior : public te::EngineBehaviour {
  bool autoInitialiseDeviceManager() override { return false; }
};

