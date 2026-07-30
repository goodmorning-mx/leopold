#pragma once

#include <Arduino.h>

namespace leopold {

class Telemetry {
 public:
  void begin();
  uint16_t batteryMillivolts();
  uint8_t batteryPercent();
  void formattedTime(char output[6]) const;
  bool timeSynchronized() const;
};

}  // namespace leopold
