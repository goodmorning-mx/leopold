#include "touch_input.h"

#include <Wire.h>

#include "board_config.h"

namespace leopold {

void TouchInput::begin() {
  Wire.begin(
      board::kTouchSda,
      board::kTouchScl,
      300000);
}

bool TouchInput::pollTap(TouchPoint& point, uint32_t now) {
  TouchPoint current;
  const bool down = read(current);
  const bool tapped = down && !wasDown_ && now - lastTapAt_ >= 250;
  wasDown_ = down;
  if (!tapped) {
    return false;
  }
  lastTapAt_ = now;
  point = current;
  return true;
}

bool TouchInput::read(TouchPoint& point) {
  Wire.beginTransmission(board::kTouchAddress);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(
          board::kTouchAddress,
          static_cast<uint8_t>(5)) != 5) {
    return false;
  }

  const uint8_t points = Wire.read() & 0x0F;
  const uint8_t xHigh = Wire.read();
  const uint8_t xLow = Wire.read();
  const uint8_t yHigh = Wire.read();
  const uint8_t yLow = Wire.read();
  if (points == 0) {
    return false;
  }

  point.x = ((xHigh & 0x0F) << 8) | xLow;
  point.y = ((yHigh & 0x0F) << 8) | yLow;
  return point.x < board::kScreenWidth && point.y < board::kScreenHeight;
}

}  // namespace leopold
