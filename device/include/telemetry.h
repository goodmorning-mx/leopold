#pragma once

#include <Arduino.h>

namespace leopold {

class Telemetry {
 public:
  void begin();
  uint16_t batteryMillivolts();
  uint8_t batteryPercent();
  void formattedTime(char output[9]) const;
  void toggleTimeFormat();
  bool timeSynchronized() const;

 private:
  bool twentyFourHour_ = true;
};

}  // namespace leopold
