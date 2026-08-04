#pragma once

#include <Arduino.h>

namespace leopold {

constexpr uint8_t kMaxBluetoothScanResults = 5;

struct BluetoothScanResult {
  char name[33] = {};
  char address[18] = {};
  int32_t rssi = -127;
};

class BluetoothManager {
 public:
  bool begin(const char* advertisedName, const char* deviceId);
  void loop();
  bool enabled() const;
  bool connected() const;
  bool startScan();
  bool scanning() const;
  uint8_t scanResultCount() const;
  const BluetoothScanResult& scanResult(uint8_t index) const;
  bool connectTo(uint8_t index);
  const char* connectedPeripheralName() const;

 private:
  bool enabled_ = false;
};

}  // namespace leopold
