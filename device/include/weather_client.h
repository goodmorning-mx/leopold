#pragma once

#include <Arduino.h>

#include "sdui.h"

namespace leopold {

class WeatherClient {
 public:
  bool refresh(SduiDocument& document);
  void render(SduiDocument& document) const;
  void toggleUnits(SduiDocument& document);

 private:
  bool synchronizeClock();
  bool hasData_ = false;
  bool celsius_ = false;
  int16_t temperatureF_ = 0;
  uint8_t humidity_ = 0;
  int weatherCode_ = 0;
  char city_[33] = "LOCATION BY IP";
};

}  // namespace leopold
