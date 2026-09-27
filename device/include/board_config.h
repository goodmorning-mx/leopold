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
constexpr int kBatteryAdc = 4;
constexpr int kSdCs = 38;
constexpr int kSdMosi = 39;
constexpr int kSdMiso = 40;
constexpr int kSdSclk = 41;
constexpr int kScreenWidth = 466;
constexpr int kScreenHeight = 466;

// Battery percentage is an estimate based on a nominal 2:1 divider.
constexpr float kBatteryDividerRatio = 2.0F;
constexpr uint16_t kBatteryEmptyMillivolts = 3300;
constexpr uint16_t kBatteryFullMillivolts = 4200;

}  // namespace leopold::board
