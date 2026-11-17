#pragma once

#include <Arduino.h>

#include "sdui.h"
#include "weather_location.h"
#include <atomic>

namespace leopold {

class WeatherClient {
 public:
  bool begin();
  bool ready() const { return task_ != nullptr; }
  bool savedLocation(WeatherLocation& output);
  bool requestLocationLookup(uint8_t country, const char* postal);
  bool takeLocationLookupResult(bool& success, WeatherLocation& output);
  bool requestRefresh();
  bool takeRefreshResult(bool& success);
  void render(SduiDocument& document) const;
  void setForecastDays(uint8_t days, SduiDocument& document);
  void toggleUnits(SduiDocument& document);
  bool metric() const { return celsius_; }
  void setMetric(bool metric, SduiDocument& document);

 private:
  static void taskEntry(void* context);
  void taskLoop();
  bool synchronizeClock();
  bool fetchWeather();
  bool lookupLocation();

  struct Observations {
    double latitude = 0;
    double longitude = 0;
    int weatherCode = 0;
    int dailyCodes[5] = {};
    int16_t temperatureF = 0;
    int16_t meansF[5] = {};
    int16_t highsF[5] = {};
    int16_t lowsF[5] = {};
    uint8_t humidity = 0;
    uint8_t rain[5] = {};
    bool dailyAvailable = false;
    bool rainAvailable = false;
    char dayNames[5][8] = {};
    char city[33] = "LOCATION BY IP";
  };

  Observations weather_{};
  WeatherLocation location_{};
  bool hasData_ = false;
  bool celsius_ = false;
  uint8_t selectedForecastDays_ = 3;
  TaskHandle_t task_ = nullptr;

  enum class Operation : uint8_t { kIdle, kRefresh, kLookup, kRefreshReady, kLookupReady };
  // Main is the sole request submitter and result consumer. The worker owns
  // the payload from acquiring a request until releasing the ready state.
  std::atomic<Operation> operation_{Operation::kIdle};
  Observations result_{};
  WeatherLocation operationLocation_{};
  uint8_t lookupCountry_ = 0;
  bool operationSucceeded_ = false;
};

}  // namespace leopold
