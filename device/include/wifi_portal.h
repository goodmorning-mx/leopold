#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

namespace leopold {

class WifiPortal {
 public:
  void begin(const char* deviceId);
  void loop(uint32_t now);
  bool connected() const;
  bool portalActive() const;
  const char* accessPointName() const;
  const char* setupToken() const;
  const char* setupUrl() const;
  void reconnectNow(uint32_t now);
  void startConfiguration(uint32_t now);
  void prepareForScan();
  void cancelSelection(uint32_t now);
  bool connectSavedNetwork(const char* networkName, uint32_t now);
  bool hasSavedNetwork(const char* networkName) const;
  void connectToNetwork(
      const char* networkName,
      const char* password,
      uint32_t now);
  void completeSetup();
  void clearCredentialsAndRestart();

 private:
  static constexpr uint8_t kBackupNetworkCount = 3;

  struct SavedNetwork {
    char ssid[33] = {};
    char password[65] = {};
  };

  void startPortal(uint32_t now);
  void loadBackupNetworks();
  void saveBackupNetworks();
  void importPortalNetworks();
  void tryNextBackupNetwork(uint32_t now);
  void rememberNetwork(const char* networkName, const char* password);

  WiFiManager manager_;
  // WiFiManagerParameter retains these pointers for the lifetime of the portal.
  char backupSsidId_[kBackupNetworkCount][24] = {};
  char backupPasswordId_[kBackupNetworkCount][24] = {};
  char backupSsidLabel_[kBackupNetworkCount][40] = {};
  char backupPasswordLabel_[kBackupNetworkCount][40] = {};
  WiFiManagerParameter* backupSsid_[kBackupNetworkCount] = {};
  WiFiManagerParameter* backupPassword_[kBackupNetworkCount] = {};
  SavedNetwork backupNetworks_[kBackupNetworkCount];
  uint8_t nextBackupNetwork_ = 0;
  SavedNetwork pendingNetwork_;
  bool pendingConnection_ = false;
  bool manualSelection_ = false;
  char accessPointName_[24] = {};
  char setupToken_[33] = {};
  char setupUrl_[256] = {};
  char customHead_[1800] = {};
  bool portalActive_ = false;
  uint32_t connectStartedAt_ = 0;
  uint32_t disconnectedAt_ = 0;
};

}  // namespace leopold
