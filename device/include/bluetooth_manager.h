#pragma once

#include <Arduino.h>

namespace leopold {

class BluetoothManager {
 public:
  bool begin(const char* advertisedName, const char* deviceId);
  void loop();
  bool enabled() const;
  bool connected() const;

 private:
  bool enabled_ = false;
};

}  // namespace leopold
