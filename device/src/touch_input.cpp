#include "touch_input.h"

#include <Wire.h>

#include "board_config.h"

namespace leopold {
namespace {

TwoWire touchBus(1);
constexpr uint8_t kCst820Address = 0x15;

bool respondsAt(uint8_t address) {
  touchBus.beginTransmission(address);
  return touchBus.endTransmission(true) == 0;
}

}  // namespace

void TouchInput::begin() {
  if (!touchBus.begin(
      board::kTouchSda,
      board::kTouchScl,
      100000)) {
    Serial.println("Touch I2C initialization failed");
    return;
  }
  if (respondsAt(board::kTouchAddress)) {
    address_ = board::kTouchAddress;
    Serial.println("Touch controller detected: FT3x68");
  } else if (respondsAt(kCst820Address)) {
    address_ = kCst820Address;
    Serial.println("Touch controller detected: CST820");
  } else {
    Serial.println("Touch controller not detected");
  }
}

bool TouchInput::pollGesture(
    TouchGesture& gesture,
    TouchPoint& point,
    uint32_t now) {
  gesture = TouchGesture::kNone;
  TouchPoint current;
  const bool down = read(current);
  if (down) {
    if (!wasDown_) {
      touchStart_ = current;
      touchStartedAt_ = now;
    }
    touchLast_ = current;
    wasDown_ = true;
    if (now - lastDragAt_ >= 16) {
      gesture = TouchGesture::kDrag;
      point = current;
      lastDragAt_ = now;
      return true;
    }
    return false;
  }

  if (!wasDown_) {
    return false;
  }
  wasDown_ = false;
  if (now - lastGestureAt_ < 180) {
    return false;
  }

  const int16_t deltaX =
      static_cast<int16_t>(touchLast_.x) -
      static_cast<int16_t>(touchStart_.x);
  const int16_t deltaY =
      static_cast<int16_t>(touchLast_.y) -
      static_cast<int16_t>(touchStart_.y);
  const uint16_t horizontalDistance = abs(deltaX);
  const uint16_t verticalDistance = abs(deltaY);
  if (horizontalDistance >= 55 && horizontalDistance > verticalDistance) {
    gesture =
        deltaX < 0 ? TouchGesture::kSwipeLeft : TouchGesture::kSwipeRight;
  } else if (now - touchStartedAt_ <= 650 &&
             horizontalDistance < 24 && verticalDistance < 24) {
    gesture = TouchGesture::kTap;
  } else {
    return false;
  }
  lastGestureAt_ = now;
  point = touchLast_;
  return true;
}

bool TouchInput::read(TouchPoint& point) {
  if (address_ == 0) {
    return false;
  }
  touchBus.beginTransmission(address_);
  touchBus.write(0x02);
  if (touchBus.endTransmission(true) != 0) {
    return false;
  }
  if (touchBus.requestFrom(
          address_,
          static_cast<uint8_t>(5)) != 5) {
    return false;
  }

  const uint8_t points = touchBus.read() & 0x0F;
  const uint8_t xHigh = touchBus.read();
  const uint8_t xLow = touchBus.read();
  const uint8_t yHigh = touchBus.read();
  const uint8_t yLow = touchBus.read();
  if (points == 0) {
    return false;
  }

  point.x = ((xHigh & 0x0F) << 8) | xLow;
  point.y = ((yHigh & 0x0F) << 8) | yLow;
  return point.x < board::kScreenWidth && point.y < board::kScreenHeight;
}

}  // namespace leopold
