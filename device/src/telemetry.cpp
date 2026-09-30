#include "telemetry.h"

#include <Preferences.h>
#include <time.h>

#include "board_config.h"

namespace leopold {

void Telemetry::begin() {
  analogReadResolution(12);
  pinMode(board::kBatteryAdc, INPUT);
  Preferences preferences;
  if (preferences.begin("leopold-time", true)) {
    twentyFourHour_ = preferences.getBool("24h", true);
    preferences.end();
  }
}

uint16_t Telemetry::batteryMillivolts() {
  constexpr uint8_t kSamples = 8;
  uint32_t sum = 0;
  for (uint8_t sample = 0; sample < kSamples; ++sample) {
    sum += analogReadMilliVolts(board::kBatteryAdc);
  }
  const float pinMillivolts = static_cast<float>(sum) / kSamples;
  return static_cast<uint16_t>(
      pinMillivolts * board::kBatteryDividerRatio);
}

uint8_t Telemetry::batteryPercent() {
  const uint16_t millivolts = batteryMillivolts();
  if (millivolts <= board::kBatteryEmptyMillivolts) {
    return 0;
  }
  if (millivolts >= board::kBatteryFullMillivolts) {
    return 100;
  }
  return static_cast<uint8_t>(
      (millivolts - board::kBatteryEmptyMillivolts) * 100UL /
      (board::kBatteryFullMillivolts - board::kBatteryEmptyMillivolts));
}

bool Telemetry::timeSynchronized() const {
  return time(nullptr) > 1700000000;
}

void Telemetry::formattedTime(char output[9]) const {
  if (!timeSynchronized()) {
    memcpy(output, "--:--", 6);
    return;
  }
  time_t now = time(nullptr);
  struct tm local = {};
  localtime_r(&now, &local);
  strftime(output, 9, twentyFourHour_ ? "%H:%M" : "%I:%M %p", &local);
}

void Telemetry::toggleTimeFormat() {
  twentyFourHour_ = !twentyFourHour_;
  Preferences preferences;
  if (preferences.begin("leopold-time", false)) {
    preferences.putBool("24h", twentyFourHour_);
    preferences.end();
  }
}

}  // namespace leopold
