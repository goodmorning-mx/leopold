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
  void clearCredentialsAndRestart();

 private:
  void startPortal(uint32_t now);

  WiFiManager manager_;
  char accessPointName_[24] = {};
  char setupToken_[33] = {};
  char customHead_[1400] = {};
  bool portalActive_ = false;
  uint32_t connectStartedAt_ = 0;
  uint32_t disconnectedAt_ = 0;
};

}  // namespace leopold
