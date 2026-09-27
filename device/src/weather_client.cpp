#include "weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <time.h>

#include "tls_roots.h"

namespace leopold {
namespace {

constexpr char kGeolocationUrl[] = "https://ipinfo.io/json";
constexpr char kForecastPrefix[] = "https://api.open-meteo.com/v1/forecast?latitude=";

const char* description(int code) {
  if (code == 0) return "CLEAR SKY";
  if (code <= 3) return "PARTLY CLOUDY";
  if (code <= 48) return "FOG";
  if (code <= 67) return "RAIN";
  if (code <= 77) return "SNOW";
  if (code <= 82) return "SHOWERS";
  return "STORM";
}

time_t utcEpoch(const tm& value) {
  int64_t days = 0;
  for (int year = 1970; year < value.tm_year + 1900; ++year) {
    days += (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
  }
  static constexpr uint16_t beforeMonth[] =
      {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  days += beforeMonth[value.tm_mon];
  if (value.tm_mon >= 2 &&
      ((value.tm_year + 1900) % 4 == 0 &&
       ((value.tm_year + 1900) % 100 != 0 ||
        (value.tm_year + 1900) % 400 == 0))) {
    ++days;
  }
  days += value.tm_mday - 1;
  return static_cast<time_t>(days * 86400LL + value.tm_hour * 3600L +
                             value.tm_min * 60L + value.tm_sec);
}

bool setClockFromDate(const String& header) {
  if (header.isEmpty()) return false;
  tm value = {};
  char* parsed = strptime(header.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &value);
  if (parsed == nullptr || *parsed != '\0') return false;
  const time_t epoch = utcEpoch(value);
  if (epoch <= 1700000000) return false;
  timeval now = {epoch, 0};
  settimeofday(&now, nullptr);
  return true;
}

}  // namespace

bool WeatherClient::synchronizeClock() {
  if (time(nullptr) > 1700000000) return true;
  configTzTime(
      "CST6",
      "216.239.35.0",
      "time.cloudflare.com",
      "pool.ntp.org");
  tm local = {};
  for (uint8_t attempt = 0; attempt < 30; ++attempt) {
    if (getLocalTime(&local, 500)) {
      Serial.println("Weather clock synchronized by NTP");
      return true;
    }
    delay(250);
  }
  Serial.println("Weather clock synchronization failed");
  return false;
}

bool WeatherClient::refresh(SduiDocument& document) {
  if (WiFi.status() != WL_CONNECTED || !synchronizeClock()) return false;

  WiFiClientSecure secure;
  secure.setCACert(kLetsEncryptRootYrByX1);
  secure.setHandshakeTimeout(8);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  const char* headers[] = {"Date"};

  if (!http.begin(secure, kGeolocationUrl)) {
    Serial.println("Weather geolocation setup failed");
    return false;
  }
  http.collectHeaders(headers, 1);
  const int locationStatus = http.GET();
  if (locationStatus != HTTP_CODE_OK) {
    Serial.printf("Weather geolocation failed: %d\n", locationStatus);
    http.end();
    return false;
  }
  JsonDocument location;
  const DeserializationError locationError =
      deserializeJson(location, http.getString());
  if (locationError) {
    Serial.printf("Weather geolocation JSON failed: %s\n", locationError.c_str());
    http.end();
    return false;
  }
  const char* coordinates = location["loc"] | "";
  double latitude = 0.0;
  double longitude = 0.0;
  if (sscanf(coordinates, "%lf,%lf", &latitude, &longitude) != 2) {
    Serial.println("Weather geolocation has no coordinates");
    http.end();
    return false;
  }
  strlcpy(city_, location["city"] | "LOCATION BY IP", sizeof(city_));
  http.end();

  String url = String(kForecastPrefix) + String(latitude, 4) +
      "&longitude=" + String(longitude, 4) +
      "&current=temperature_2m,relative_humidity_2m,weather_code&temperature_unit=fahrenheit";
  if (!http.begin(secure, url)) {
    Serial.println("Weather forecast setup failed");
    return false;
  }
  http.collectHeaders(headers, 1);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("Weather forecast failed: %d\n", status);
    http.end();
    return false;
  }
  JsonDocument forecast;
  const DeserializationError forecastError =
      deserializeJson(forecast, http.getString());
  if (forecastError) {
    Serial.printf("Weather forecast JSON failed: %s\n", forecastError.c_str());
    http.end();
    return false;
  }
  temperatureF_ = static_cast<int16_t>(forecast["current"]["temperature_2m"] | 0.0);
  humidity_ = forecast["current"]["relative_humidity_2m"] | 0;
  weatherCode_ = forecast["current"]["weather_code"] | 0;
  const String dateHeader = http.header("Date");
  http.end();
  if (time(nullptr) <= 1700000000) setClockFromDate(dateHeader);
  hasData_ = true;
  render(document);
  Serial.printf("Weather ready: %d F, %u%%, %s\n", temperatureF_, humidity_, city_);
  return true;
}

void WeatherClient::render(SduiDocument& document) const {
  if (!hasData_) return;
  const int temperature = celsius_
      ? static_cast<int>((temperatureF_ - 32) * 5.0 / 9.0)
      : temperatureF_;
  const char* unit = celsius_ ? "C" : "F";
  for (uint8_t i = 0; i < document.screenCount; ++i) {
    SduiScreen& screen = document.screens[i];
    if (strcmp(screen.screenId, "sky-weather") != 0) continue;
    for (uint8_t j = 0; j < screen.componentCount; ++j) {
      SduiComponent& component = screen.components[j];
      if (strcmp(component.id, "weather-temp") == 0) {
        snprintf(component.text, sizeof(component.text), "%d %s", temperature, unit);
      } else if (strcmp(component.id, "weather-condition") == 0) {
        strlcpy(component.text, description(weatherCode_), sizeof(component.text));
      } else if (strcmp(component.id, "weather-humidity") == 0) {
        snprintf(component.text, sizeof(component.text), "HUMIDITY %u%%", humidity_);
      } else if (strcmp(component.id, "weather-location") == 0) {
        strlcpy(component.text, city_, sizeof(component.text));
      }
    }
  }
}

void WeatherClient::toggleUnits(SduiDocument& document) {
  celsius_ = !celsius_;
  render(document);
  Serial.printf("Weather units: %s\n", celsius_ ? "C" : "F");
}

}  // namespace leopold
