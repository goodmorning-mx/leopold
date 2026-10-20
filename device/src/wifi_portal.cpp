#include "wifi_portal.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config_guard.h"

namespace leopold {
namespace {

constexpr uint32_t kConnectWindowMs = 20000;
constexpr uint32_t kReconnectPortalMs = 60000;
constexpr uint8_t kMaxCredentialLength = 64;

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
  loadBackupNetworks();
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
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    auto& ssidId = backupSsidId_[index];
    auto& passwordId = backupPasswordId_[index];
    auto& ssidLabel = backupSsidLabel_[index];
    auto& passwordLabel = backupPasswordLabel_[index];
    snprintf(ssidId, sizeof(ssidId), "backup_ssid_%u", index + 1);
    snprintf(passwordId, sizeof(passwordId), "backup_password_%u", index + 1);
    snprintf(ssidLabel, sizeof(ssidLabel), "Backup Wi-Fi %u", index + 1);
    snprintf(passwordLabel, sizeof(passwordLabel), "Backup password %u", index + 1);
    backupSsid_[index] = new WiFiManagerParameter(
        ssidId, ssidLabel, backupNetworks_[index].ssid, 32);
    backupPassword_[index] = new WiFiManagerParameter(
        passwordId,
        passwordLabel,
        backupNetworks_[index].password,
        kMaxCredentialLength,
        "type='password'");
    manager_.addParameter(backupSsid_[index]);
    manager_.addParameter(backupPassword_[index]);
  }
  manager_.setSaveParamsCallback([this]() { importPortalNetworks(); });
  std::vector<const char*> menu = {"wifi", "info", "exit"};
  manager_.setMenu(menu);

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  if (backupNetworks_[0].ssid[0] != '\0') {
    WiFi.begin(backupNetworks_[0].ssid, backupNetworks_[0].password);
  } else {
    WiFi.begin();
  }
  connectStartedAt_ = millis();
  disconnectedAt_ = connectStartedAt_;
  nextBackupNetwork_ = 0;
  if (backupNetworks_[0].ssid[0] == '\0' && !manager_.getWiFiIsSaved()) {
    startPortal(connectStartedAt_);
  }
}

void WifiPortal::loop(uint32_t now) {
  // Commit credentials only after the requested network actually connects.
  if (pendingConnection_ && WiFi.status() == WL_CONNECTED &&
      WiFi.SSID() == pendingNetwork_.ssid) {
    rememberNetwork(pendingNetwork_.ssid, pendingNetwork_.password);
    saveBackupNetworks();
    memset(&pendingNetwork_, 0, sizeof(pendingNetwork_));
    pendingConnection_ = false;
    manualSelection_ = false;
    nextBackupNetwork_ = 0;
    WiFi.setAutoReconnect(true);
    connectStartedAt_ = now;
  }
  if (manualSelection_) return;
  if (portalActive_) {
    manager_.process();
    if (WiFi.status() == WL_CONNECTED) {
      rememberNetwork(WiFi.SSID().c_str(), WiFi.psk().c_str());
      saveBackupNetworks();
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
    connectStartedAt_ = now;
    nextBackupNetwork_ = 0;
    WiFi.reconnect();
  }

  const bool initialTimeout = now - connectStartedAt_ >= kConnectWindowMs;
  const bool reconnectTimeout = now - disconnectedAt_ >= kReconnectPortalMs;
  if (initialTimeout && nextBackupNetwork_ < kBackupNetworkCount) {
    tryNextBackupNetwork(now);
    return;
  }
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
  manualSelection_ = false;
  pendingConnection_ = false;
  memset(&pendingNetwork_, 0, sizeof(pendingNetwork_));
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.disconnect(false, false);
  if (backupNetworks_[0].ssid[0] != '\0') {
    WiFi.begin(backupNetworks_[0].ssid, backupNetworks_[0].password);
  } else {
    WiFi.begin(); // Existing provisioned driver credentials remain intact.
  }
  connectStartedAt_ = now;
  disconnectedAt_ = now;
  nextBackupNetwork_ = 0;
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
  // Scanning a connected station must not drop its working network.
  if (WiFi.status() == WL_CONNECTED) {
    rememberNetwork(WiFi.SSID().c_str(), WiFi.psk().c_str());
    saveBackupNetworks();
  }
  manualSelection_ = true;
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
}

void WifiPortal::cancelSelection(uint32_t now) {
  if (pendingConnection_) {
    reconnectNow(now);
    return;
  }
  manualSelection_ = false;
  WiFi.setAutoReconnect(true);
  connectStartedAt_ = now;
  disconnectedAt_ = now;
  nextBackupNetwork_ = 0;
}

bool WifiPortal::hasSavedNetwork(const char* networkName) const {
  if (networkName == nullptr) return false;
  for (const auto& network : backupNetworks_) {
    if (strcmp(network.ssid, networkName) == 0 && network.ssid[0] != '\0') return true;
  }
  return false;
}

bool WifiPortal::connectSavedNetwork(const char* networkName, uint32_t now) {
  for (const auto& network : backupNetworks_) {
    if (strcmp(network.ssid, networkName) == 0 && network.ssid[0] != '\0') {
      // Copy before prepareForScan potentially reorders the store.
      SavedNetwork saved = network;
      connectToNetwork(saved.ssid, saved.password, now);
      memset(&saved, 0, sizeof(saved));
      return true;
    }
  }
  return false;
}

void WifiPortal::connectToNetwork(
    const char* networkName,
    const char* password,
    uint32_t now) {
  if (networkName == nullptr || password == nullptr || networkName[0] == '\0' ||
      strlen(networkName) > 32 || strlen(password) > 64) return;
  prepareForScan();
  strlcpy(pendingNetwork_.ssid, networkName, sizeof(pendingNetwork_.ssid));
  strlcpy(pendingNetwork_.password, password, sizeof(pendingNetwork_.password));
  pendingConnection_ = true;
  // Leave the last provisioned driver's credentials untouched on failure.
  // The existing NVS backup store is updated only on success and preferred at boot.
  WiFi.persistent(false);
  WiFi.disconnect(false, false);
  WiFi.begin(pendingNetwork_.ssid, pendingNetwork_.password);
  connectStartedAt_ = now;
  disconnectedAt_ = now;
  Serial.println("Connecting to selected Wi-Fi");
}

void WifiPortal::completeSetup() {
  Preferences preferences;
  preferences.begin("leopold", false);
  preferences.remove("setup_token");
  preferences.end();
}

void WifiPortal::startPortal(uint32_t now) {
  // Refresh the portal fields after on-device changes, before user editing.
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    backupSsid_[index]->setValue(backupNetworks_[index].ssid, 32);
    backupPassword_[index]->setValue(backupNetworks_[index].password, kMaxCredentialLength);
  }
  manager_.startConfigPortal(accessPointName_);
  portalActive_ = true;
  connectStartedAt_ = now;
}

void WifiPortal::clearCredentialsAndRestart() {
  manager_.resetSettings();
  Preferences preferences;
  preferences.begin("wifi-backups", false);
  preferences.clear();
  preferences.end();
  delay(50);
  ESP.restart();
}

void WifiPortal::loadBackupNetworks() {
  Preferences preferences;
  preferences.begin("wifi-backups", true);
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    char ssidKey[16] = {};
    char passwordKey[16] = {};
    snprintf(ssidKey, sizeof(ssidKey), "ssid%u", index);
    snprintf(passwordKey, sizeof(passwordKey), "pass%u", index);
    preferences.getString(ssidKey, backupNetworks_[index].ssid, sizeof(backupNetworks_[index].ssid));
    preferences.getString(passwordKey, backupNetworks_[index].password, sizeof(backupNetworks_[index].password));
  }
  preferences.end();
}

void WifiPortal::importPortalNetworks() {
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    strlcpy(backupNetworks_[index].ssid, backupSsid_[index]->getValue(), sizeof(backupNetworks_[index].ssid));
    strlcpy(backupNetworks_[index].password, backupPassword_[index]->getValue(), sizeof(backupNetworks_[index].password));
  }
  saveBackupNetworks();
}

void WifiPortal::saveBackupNetworks() {
  Preferences preferences;
  preferences.begin("wifi-backups", false);
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    char ssidKey[16] = {};
    char passwordKey[16] = {};
    snprintf(ssidKey, sizeof(ssidKey), "ssid%u", index);
    snprintf(passwordKey, sizeof(passwordKey), "pass%u", index);
    preferences.putString(ssidKey, backupNetworks_[index].ssid);
    preferences.putString(passwordKey, backupNetworks_[index].password);
  }
  preferences.end();
}

void WifiPortal::tryNextBackupNetwork(uint32_t now) {
  while (nextBackupNetwork_ < kBackupNetworkCount) {
    const SavedNetwork& network = backupNetworks_[nextBackupNetwork_++];
    if (network.ssid[0] == '\0') continue;
    WiFi.persistent(false);
    WiFi.disconnect(false, false);
    WiFi.begin(network.ssid, network.password);
    connectStartedAt_ = now;
    disconnectedAt_ = now;
    Serial.printf("Trying saved Wi-Fi network %u: %s\n", nextBackupNetwork_, network.ssid);
    return;
  }
}

void WifiPortal::rememberNetwork(const char* networkName, const char* password) {
  if (networkName == nullptr || password == nullptr || networkName[0] == '\0') return;
  uint8_t target = kBackupNetworkCount;
  for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
    if (strcmp(backupNetworks_[index].ssid, networkName) == 0) { target = index; break; }
  }
  if (target == kBackupNetworkCount) {
    target = kBackupNetworkCount - 1;
    for (uint8_t index = 0; index < kBackupNetworkCount; ++index) {
      if (backupNetworks_[index].ssid[0] == '\0') { target = index; break; }
    }
  }
  // Most recently successful network first; bounded legacy store stays at 3
  // until the strategy's separate versioned ten-network migration.
  for (uint8_t index = target; index > 0; --index) backupNetworks_[index] = backupNetworks_[index - 1];
  strlcpy(backupNetworks_[0].ssid, networkName, sizeof(backupNetworks_[0].ssid));
  strlcpy(backupNetworks_[0].password, password, sizeof(backupNetworks_[0].password));
}

}  // namespace leopold
