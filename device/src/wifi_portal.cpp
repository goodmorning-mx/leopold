#include "wifi_portal.h"

#include <WiFi.h>

namespace leopold {
namespace {

constexpr uint32_t kConnectWindowMs = 20000;
constexpr uint32_t kReconnectPortalMs = 60000;

}  // namespace

void WifiPortal::begin(const char* deviceId) {
  const size_t length = strlen(deviceId);
  const char* suffix = length > 4 ? deviceId + length - 4 : deviceId;
  snprintf(
      accessPointName_,
      sizeof(accessPointName_),
      "Leopold-%s",
      suffix);

  manager_.setConfigPortalBlocking(false);
  manager_.setDebugOutput(false);
  manager_.setConnectTimeout(10);
  manager_.setShowPassword(false);
  manager_.setTitle("Leopold Wi-Fi");
  manager_.setDarkMode(true);
  std::vector<const char*> menu = {"wifi", "info", "exit"};
  manager_.setMenu(menu);

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  connectStartedAt_ = millis();
  disconnectedAt_ = connectStartedAt_;
}

void WifiPortal::loop(uint32_t now) {
  if (portalActive_) {
    manager_.process();
    if (WiFi.status() == WL_CONNECTED) {
      manager_.stopConfigPortal();
      portalActive_ = false;
      disconnectedAt_ = 0;
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    disconnectedAt_ = 0;
    return;
  }

  if (disconnectedAt_ == 0) {
    disconnectedAt_ = now;
    WiFi.reconnect();
  }

  const bool initialTimeout = now - connectStartedAt_ >= kConnectWindowMs;
  const bool reconnectTimeout = now - disconnectedAt_ >= kReconnectPortalMs;
  if (initialTimeout || reconnectTimeout) {
    startPortal(now);
  }
}

bool WifiPortal::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiPortal::portalActive() const {
  return portalActive_;
}

const char* WifiPortal::accessPointName() const {
  return accessPointName_;
}

void WifiPortal::startPortal(uint32_t now) {
  manager_.startConfigPortal(accessPointName_);
  portalActive_ = true;
  connectStartedAt_ = now;
}

void WifiPortal::clearCredentialsAndRestart() {
  manager_.resetSettings();
  delay(50);
  ESP.restart();
}

}  // namespace leopold
