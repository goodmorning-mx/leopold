#include "app_state.h"

namespace leopold {

const char* appStateName(AppState state) {
  switch (state) {
    case AppState::kBooting:
      return "BOOTING";
    case AppState::kWifiConnecting:
      return "WIFI_CONNECTING";
    case AppState::kWifiPortal:
      return "WIFI_PORTAL";
    case AppState::kCloudConnecting:
      return "CLOUD_CONNECTING";
    case AppState::kOnline:
      return "ONLINE";
    case AppState::kOffline:
      return "OFFLINE";
    case AppState::kError:
      return "ERROR";
  }
  return "UNKNOWN";
}

}  // namespace leopold
