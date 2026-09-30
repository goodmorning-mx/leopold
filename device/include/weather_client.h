#pragma once

#include <Arduino.h>

#include "sdui.h"

namespace leopold {

class WeatherClient {
 public:
  bool begin();
  bool requestRefresh(SduiDocument& document);
  bool takeRefreshResult();
  bool refresh(SduiDocument& document);
  void render(SduiDocument& document) const;
  void toggleUnits(SduiDocument& document);

 private:
  static void taskEntry(void* context);
  void taskLoop();
  bool synchronizeClock();
  bool hasData_ = false;
  bool celsius_ = false;
  int16_t temperatureF_ = 0;
  uint8_t humidity_ = 0;
  int weatherCode_ = 0;
  char city_[33] = "LOCATION BY IP";
  TaskHandle_t task_ = nullptr;
  SduiDocument* document_ = nullptr;
  volatile bool refreshRequested_ = false;
  volatile bool refreshRunning_ = false;
  volatile bool refreshComplete_ = false;
};

}  // namespace leopold
