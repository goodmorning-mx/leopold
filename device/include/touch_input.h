#pragma once

#include <Arduino.h>

namespace leopold {

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
};

enum class TouchGesture : uint8_t {
  kNone,
  kTap,
  kDrag,
  kRelease,
  kLongPress,
  kSwipeUp,
  kSwipeLeft,
  kSwipeRight,
};

class TouchInput {
 public:
  void begin();
  bool pollGesture(TouchGesture& gesture, TouchPoint& point, uint32_t now);
  void setTraceEnabled(bool enabled);
  bool traceEnabled() const;

 private:
  bool read(TouchPoint& point);
  void traceSample(const char* result, uint8_t contacts, uint16_t x, uint16_t y);

  uint8_t address_ = 0;
  bool wasDown_ = false;
  TouchPoint touchStart_;
  TouchPoint touchLast_;
  uint32_t touchStartedAt_ = 0;
  uint32_t lastGestureAt_ = 0;
  uint32_t lastDragAt_ = 0;
  uint32_t lastTraceAt_ = 0;
  char lastTraceResult_[24] = {};
  bool traceEnabled_ = false;
};

}  // namespace leopold
