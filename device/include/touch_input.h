#pragma once

#include <Arduino.h>

namespace leopold {

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
};

class TouchInput {
 public:
  void begin();
  bool pollTap(TouchPoint& point, uint32_t now);

 private:
  bool read(TouchPoint& point);

  uint8_t address_ = 0;
  bool wasDown_ = false;
  uint32_t lastTapAt_ = 0;
};

}  // namespace leopold
