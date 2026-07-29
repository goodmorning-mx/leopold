#pragma once

#include <Arduino.h>

namespace leopold::board {

constexpr int kDisplayCs = 9;
constexpr int kDisplayClock = 10;
constexpr int kDisplayData0 = 11;
constexpr int kDisplayData1 = 12;
constexpr int kDisplayData2 = 13;
constexpr int kDisplayData3 = 14;
constexpr int kDisplayReset = 21;
constexpr int kDisplayEnable = 42;

constexpr int kTouchSda = 47;
constexpr int kTouchScl = 48;
constexpr uint8_t kTouchAddress = 0x38;

constexpr int kBootButton = 0;
constexpr int kScreenWidth = 466;
constexpr int kScreenHeight = 466;

}  // namespace leopold::board
