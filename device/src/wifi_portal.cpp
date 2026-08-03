#include "wifi_portal.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config_guard.h"

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
  Preferences preferences;
  preferences.begin("leopold", false);
  const String savedToken = preferences.getString("setup_token", "");
  if (savedToken.length() == 32) {
    strlcpy(setupToken_, savedToken.c_str(), sizeof(setupToken_));
  } else {
    for (uint8_t index = 0; index < 16; ++index) {
      snprintf(
          setupToken_ + index * 2,
          sizeof(setupToken_) - index * 2,
          "%02x",
          static_cast<unsigned int>(esp_random() & 0xFF));
    }
    preferences.putString("setup_token", setupToken_);
  }
  preferences.end();
  snprintf(
      setupUrl_,
      sizeof(setupUrl_),
      "%s/v1/onboarding/%s/continue?ssid=%s",
      LEOPOLD_API_BASE_URL,
      setupToken_,
      accessPointName_);
  snprintf(
      customHead_,
      sizeof(customHead_),
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>body{background:#080808!important;color:#fff!important}"
      ".wrap{background:#111!important}.lp{padding:28px;text-align:center}"
      ".lp b{font-size:24px}.lp p{color:#aaa;line-height:1.5}"
      "button{background:#fff!important;color:#080808!important;"
      "border-radius:8px!important}</style><script>"
      "if(location.pathname=='/wifisave'){addEventListener('DOMContentLoaded',"
      "()=>{document.body.innerHTML=\"<div class='lp'><b>Conectando "
      "Leopold...</b><p>No cierres esta pagina. Continuaremos "
      "automaticamente.</p></div>\";const u='%s';const go=()=>fetch(u,"
      "{mode:'no-cors',cache:'no-store'}).then(()=>location.replace(u))"
      ".catch(()=>setTimeout(go,1200));setTimeout(go,2500)})}</script>",
      setupUrl_);

  manager_.setConfigPortalBlocking(false);
  manager_.setDebugOutput(false);
  manager_.setConnectTimeout(10);
  manager_.setShowPassword(false);
  manager_.setTitle("Leopold Wi-Fi");
  manager_.setCustomHeadElement(customHead_);
  manager_.setDarkMode(true);
  std::vector<const char*> menu = {"wifi", "info", "exit"};
  manager_.setMenu(menu);

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  connectStartedAt_ = millis();
  disconnectedAt_ = connectStartedAt_;
  if (!manager_.getWiFiIsSaved()) {
    startPortal(connectStartedAt_);
  }
}

void WifiPortal::loop(uint32_t now) {
  if (portalActive_) {
    manager_.process();
    if (WiFi.status() == WL_CONNECTED) {
      manager_.stopConfigPortal();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
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

const char* WifiPortal::setupToken() const {
  return setupToken_;
}

const char* WifiPortal::setupUrl() const {
  return setupUrl_;
}

void WifiPortal::reconnectNow(uint32_t now) {
  if (portalActive_) {
    manager_.stopConfigPortal();
    WiFi.softAPdisconnect(true);
    portalActive_ = false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.reconnect();
  connectStartedAt_ = now;
  disconnectedAt_ = now;
  Serial.println("Manual Wi-Fi reconnect requested");
}

void WifiPortal::startConfiguration(uint32_t now) {
  if (!portalActive_) {
    startPortal(now);
  }
  Serial.printf("Wi-Fi configuration portal active: %s\n", accessPointName_);
}

void WifiPortal::prepareForScan() {
  if (portalActive_) {
    manager_.stopConfigPortal();
    WiFi.softAPdisconnect(true);
    portalActive_ = false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  disconnectedAt_ = 0;
}

void WifiPortal::connectToNetwork(
    const char* networkName,
    const char* password,
    uint32_t now) {
  prepareForScan();
  WiFi.persistent(true);
  WiFi.begin(networkName, password);
  connectStartedAt_ = now;
  disconnectedAt_ = now;
  Serial.printf("Connecting to selected Wi-Fi: %s\n", networkName);
}

void WifiPortal::completeSetup() {
  Preferences preferences;
  preferences.begin("leopold", false);
  preferences.remove("setup_token");
  preferences.end();
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
