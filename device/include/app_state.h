#pragma once

#include <Arduino.h>

namespace leopold {

enum class AppState : uint8_t {
  kBooting,
  kWifiConnecting,
  kWifiPortal,
  kCloudConnecting,
  kOnline,
  kOffline,
  kError,
};

const char* appStateName(AppState state);

}  // namespace leopold
