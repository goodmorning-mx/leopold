#include "touch_input.h"

#include <Wire.h>

#include "board_config.h"
#include "generated_firmware_ui.h"

namespace leopold {
namespace {

// The Waveshare reference BSP initializes the FT3168 on hardware I2C port 0.
// Reuse Arduino's owner for that controller; constructing another TwoWire(0)
// leaves two objects managing one bus and can invalidate its driver handle.
TwoWire& touchBus = Wire;
constexpr uint8_t kCst820Address = 0x15;

bool respondsAt(uint8_t address) {
  touchBus.beginTransmission(address);
  return touchBus.endTransmission(true) == 0;
}

}  // namespace

void TouchInput::setTraceEnabled(bool enabled) {
  traceEnabled_ = enabled;
  lastTraceAt_ = 0;
  lastTraceResult_[0] = '\0';
}

bool TouchInput::traceEnabled() const {
  return traceEnabled_;
}

void TouchInput::traceSample(const char* result, uint8_t contacts, uint16_t x, uint16_t y) {
  if (!traceEnabled_) return;
  const uint32_t now = millis();
  if (strcmp(lastTraceResult_, result) == 0 && now - lastTraceAt_ < 250) return;
  lastTraceAt_ = now;
  strlcpy(lastTraceResult_, result, sizeof(lastTraceResult_));
  Serial.printf("TOUCH_TRACE sample result=%s detail=%u x=%u y=%u\n",
                result, contacts, x, y);
}

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
    touchBus.beginTransmission(address_);
    touchBus.write(0x00);
    touchBus.write(0x00);
    if (touchBus.endTransmission(true) != 0) {
      Serial.println("FT3168 normal-mode initialization failed");
    }
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
      // Make every new contact eligible for an immediate held-state sample.
      lastDragAt_ = now - generated_ui::kDragIntervalMs;
      if (traceEnabled_) Serial.printf("TOUCH_TRACE down x=%u y=%u\n", current.x, current.y);
    }
    touchLast_ = current;
    wasDown_ = true;
    if (now - lastDragAt_ >= generated_ui::kDragIntervalMs) {
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
  const uint32_t gestureDuration = now - touchStartedAt_;
  // A deliberate page swipe must travel far, finish quickly, and stay on a
  // clearly horizontal axis. This prevents normal slider/menu manipulation
  // from changing screens when a finger drifts before release.
  if (verticalDistance >= generated_ui::kSwipeUpDistance &&
      deltaY < 0 &&
      verticalDistance * 10 >= horizontalDistance * generated_ui::kSwipeDirectionRatio &&
      gestureDuration <= generated_ui::kSwipeUpMaxDurationMs) {
    gesture = TouchGesture::kSwipeUp;
  } else if (horizontalDistance >= generated_ui::kSwipeHorizontalDistance &&
      horizontalDistance * 10 >= verticalDistance * generated_ui::kSwipeDirectionRatio &&
      gestureDuration <= generated_ui::kSwipeHorizontalMaxDurationMs) {
    gesture =
        deltaX < 0 ? TouchGesture::kSwipeLeft : TouchGesture::kSwipeRight;
  } else if (gestureDuration >= generated_ui::kLongPressDurationMs &&
             horizontalDistance < generated_ui::kLongPressMaxMovement &&
             verticalDistance < generated_ui::kLongPressMaxMovement) {
    gesture = TouchGesture::kLongPress;
  } else if (gestureDuration <= generated_ui::kTapMaxDurationMs &&
             horizontalDistance < generated_ui::kTapMaxMovement &&
             verticalDistance < generated_ui::kTapMaxMovement) {
    gesture = TouchGesture::kTap;
  } else {
    // Report a neutral release so a widget that captured the pointer can end
    // its interaction even when the release itself is not a tap or swipe.
    gesture = TouchGesture::kRelease;
  }
  lastGestureAt_ = now;
  point = touchLast_;
  if (traceEnabled_) {
    Serial.printf("TOUCH_TRACE up gesture=%u x=%u y=%u duration_ms=%lu dx=%d dy=%d\n",
                  static_cast<unsigned>(gesture), point.x, point.y,
                  static_cast<unsigned long>(gestureDuration), deltaX, deltaY);
  }
  return true;
}

bool TouchInput::read(TouchPoint& point) {
  if (address_ == 0) {
    traceSample("no_controller", 0, 0, 0);
    return false;
  }
  touchBus.beginTransmission(address_);
  touchBus.write(0x02);
  // Keep the register pointer transaction together with the following read.
  // The FT3x68 on this board is reliable at standard-mode I2C speed but can
  // NACK the faster bus during normal touch polling.
  const uint8_t writeStatus = touchBus.endTransmission(false);
  if (writeStatus != 0) {
    traceSample("register_write_error", writeStatus, 0, 0);
    return false;
  }
  const uint8_t received = touchBus.requestFrom(address_, static_cast<uint8_t>(5), true);
  if (received != 5) {
    traceSample("short_read", received, 0, 0);
    return false;
  }

  const uint8_t points = touchBus.read() & 0x0F;
  const uint8_t xHigh = touchBus.read();
  const uint8_t xLow = touchBus.read();
  const uint8_t yHigh = touchBus.read();
  const uint8_t yLow = touchBus.read();
  if (points == 0) {
    traceSample("zero_contacts", 0, 0, 0);
    return false;
  }

  point.x = ((xHigh & 0x0F) << 8) | xLow;
  point.y = ((yHigh & 0x0F) << 8) | yLow;
  const bool inBounds = point.x < board::kScreenWidth && point.y < board::kScreenHeight;
  traceSample(inBounds ? "contact" : "out_of_bounds", points, point.x, point.y);
  return inBounds;
}

}  // namespace leopold
