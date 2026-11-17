#include "weather_client.h"
#include "weather_location_response.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <time.h>

#include "config_guard.h"
#include "device_identity.h"
#include "tls_bundle.h"
#include "tls_roots.h"

namespace leopold {
namespace {

constexpr char kGeolocationUrl[] = "https://ipinfo.io/json";
constexpr char kForecastPrefix[] = "https://api.open-meteo.com/v1/forecast?latitude=";
using PersistedLocation = weather_location::Record;
struct LocationPreferencesStore {
  Preferences& preferences;
  bool stage(const PersistedLocation& value) {
    return preferences.putBytes("location_stage", &value, sizeof(value)) == sizeof(value);
  }
  bool readStaged(PersistedLocation& value) {
    return preferences.getBytes("location_stage", &value, sizeof(value)) == sizeof(value);
  }
  bool promote(const PersistedLocation& value) {
    return preferences.putBytes("location_v1", &value, sizeof(value)) == sizeof(value);
  }
};


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

bool WeatherClient::begin() {
  if (task_ != nullptr) return true;
  Preferences preferences;
  if (preferences.begin("weather", true)) {
    celsius_ = preferences.getBool("metric", false);
    if (preferences.getBytesLength("location_v1") == sizeof(PersistedLocation)) {
      PersistedLocation saved{};
      WeatherLocation restored{};
      if (preferences.getBytes("location_v1", &saved, sizeof(saved)) == sizeof(saved) &&
          weather_location::decode(saved, restored)) {
        location_ = saved.value;
        strlcpy(weather_.city, location_.city, sizeof(weather_.city));
      }
    }
    preferences.end();
  }
  return xTaskCreatePinnedToCore(
             // HTTPS/DNS/TLS client calls use substantially more stack than
             // the polling loop itself. Keep this task isolated from the
             // display task's stack canary while preserving the same priority.
             taskEntry, "leopold-weather", 8192, this, 1, &task_, 0) ==
         pdPASS;
}

bool WeatherClient::savedLocation(WeatherLocation& output) {
  if (!location_.city[0]) return false;
  output = location_;
  return true;
}

bool WeatherClient::requestLocationLookup(uint8_t country, const char* postal) {
  if (!ready() || operation_.load(std::memory_order_acquire) != Operation::kIdle) return false;
  char normalized[11] = {};
  if (!weather_location::normalize(country, postal, normalized)) return false;
  operationLocation_ = WeatherLocation{};
  strlcpy(operationLocation_.postalCode, normalized, sizeof(operationLocation_.postalCode));
  lookupCountry_ = country;
  operation_.store(Operation::kLookup, std::memory_order_release);
  return true;
}

bool WeatherClient::takeLocationLookupResult(bool& success, WeatherLocation& output) {
  if (operation_.load(std::memory_order_acquire) != Operation::kLookupReady) return false;
  success = operationSucceeded_;
  output = operationLocation_;
  if (success) {
    location_ = output;
    strlcpy(weather_.city, output.city, sizeof(weather_.city));
    hasData_ = false;
    weather_.dailyAvailable = false;
    weather_.rainAvailable = false;
  }
  operation_.store(Operation::kIdle, std::memory_order_release);
  return true;
}

bool WeatherClient::requestRefresh() {
  if (!ready() || operation_.load(std::memory_order_acquire) != Operation::kIdle) return false;
  operationLocation_ = location_;
  operation_.store(Operation::kRefresh, std::memory_order_release);
  return true;
}

bool WeatherClient::takeRefreshResult(bool& success) {
  if (operation_.load(std::memory_order_acquire) != Operation::kRefreshReady) return false;
  success = operationSucceeded_;
  if (success) {
    const bool sameLocation = hasData_ && weather_.latitude == result_.latitude &&
        weather_.longitude == result_.longitude;
    weather_.latitude = result_.latitude;
    weather_.longitude = result_.longitude;
    weather_.temperatureF = result_.temperatureF;
    weather_.humidity = result_.humidity;
    weather_.weatherCode = result_.weatherCode;
    strlcpy(weather_.city, result_.city, sizeof(weather_.city));
    if (result_.dailyAvailable) {
      memcpy(weather_.dailyCodes, result_.dailyCodes, sizeof(weather_.dailyCodes));
      memcpy(weather_.meansF, result_.meansF, sizeof(weather_.meansF));
      memcpy(weather_.highsF, result_.highsF, sizeof(weather_.highsF));
      memcpy(weather_.lowsF, result_.lowsF, sizeof(weather_.lowsF));
      memcpy(weather_.dayNames, result_.dayNames, sizeof(weather_.dayNames));
    }
    if (result_.rainAvailable) {
      memcpy(weather_.rain, result_.rain, sizeof(weather_.rain));
    }
    weather_.dailyAvailable = result_.dailyAvailable || (sameLocation && weather_.dailyAvailable);
    weather_.rainAvailable = result_.rainAvailable || (sameLocation && weather_.rainAvailable);
    hasData_ = true;
  }
  operation_.store(Operation::kIdle, std::memory_order_release);
  return true;
}

void WeatherClient::taskEntry(void* context) {
  static_cast<WeatherClient*>(context)->taskLoop();
}

void WeatherClient::taskLoop() {
  while (true) {
    const auto operation = operation_.load(std::memory_order_acquire);
    if (operation == Operation::kRefresh) {
      operationSucceeded_ = fetchWeather();
      operation_.store(Operation::kRefreshReady, std::memory_order_release);
    } else if (operation == Operation::kLookup) {
      operationSucceeded_ = lookupLocation();
      operation_.store(Operation::kLookupReady, std::memory_order_release);
    } else {
      vTaskDelay(pdMS_TO_TICKS(25));
    }
  }
}

bool WeatherClient::lookupLocation() {
  const char* country = lookupCountry_ == 1 ? "US" : "MX";
  String base = LEOPOLD_API_BASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  const char* canonicalPostal = operationLocation_.postalCode;
  String url = base + "/v1/weather/geocode?country=" + country + "&postal_code=" + canonicalPostal;
  WiFiClientSecure secure;
  secure.setCACert(LEOPOLD_TLS_ROOT_CA);
  secure.setHandshakeTimeout(8);
  HTTPClient http;
  http.setConnectTimeout(8000); http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  bool ok = false;
  WeatherLocation& candidate = operationLocation_;
  strlcpy(candidate.country, country, sizeof(candidate.country));
  if (WiFi.status() == WL_CONNECTED && synchronizeClock() && http.begin(secure, url)) {
    http.addHeader("X-Leopold-Device-ID", identity::deviceId());
    http.addHeader("Authorization", String("Bearer ") + identity::deviceSecret());
    const int status = http.GET();
    const int bodyLength = http.getSize();
    if (status == HTTP_CODE_OK && bodyLength > 0 && bodyLength <= 1024) {
      char body[1025] = {};
      const size_t received = http.getStream().readBytes(body, static_cast<size_t>(bodyLength));
      if (received == static_cast<size_t>(bodyLength) &&
          weather_location::parseResponse(body, received, country, canonicalPostal, candidate)) {
        Preferences preferences;
        if (preferences.begin("weather", false)) {
          LocationPreferencesStore store{preferences};
          ok = weather_location::save(store, candidate);
          preferences.end();
        }
      }
    }
    http.end();
  }
  return ok;
}

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

bool WeatherClient::fetchWeather() {
  if (WiFi.status() != WL_CONNECTED || !synchronizeClock()) return false;

  WiFiClientSecure secure;
  secure.setCACert(kLetsEncryptRootYrByX1);
  secure.setHandshakeTimeout(8);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  const char* headers[] = {"Date"};

  double latitude = 0.0;
  double longitude = 0.0;
  const bool hasSavedLocation = weather_location::coordinates(operationLocation_, latitude, longitude);
  if (!hasSavedLocation && !http.begin(secure, kGeolocationUrl)) {
    Serial.println("Weather geolocation setup failed");
    return false;
  }
  if (!hasSavedLocation) http.collectHeaders(headers, 1);
  const int locationStatus = hasSavedLocation ? HTTP_CODE_OK : http.GET();
  if (locationStatus != HTTP_CODE_OK) {
    Serial.printf("Weather geolocation failed: %d\n", locationStatus);
    http.end();
    return false;
  }
  JsonDocument location;
  const DeserializationError locationError = hasSavedLocation ? DeserializationError::Ok :
      deserializeJson(location, http.getString());
  if (locationError) {
    Serial.printf("Weather geolocation JSON failed: %s\n", locationError.c_str());
    http.end();
    return false;
  }
  const char* coordinates = location["loc"] | "";
  if (!hasSavedLocation && sscanf(coordinates, "%lf,%lf", &latitude, &longitude) != 2) {
    Serial.println("Weather geolocation has no coordinates");
    http.end();
    return false;
  }
  strlcpy(result_.city, hasSavedLocation ? operationLocation_.city :
          (location["city"] | "LOCATION BY IP"), sizeof(result_.city));
  http.end();

  String url = String(kForecastPrefix) + String(latitude, 4) +
      "&longitude=" + String(longitude, 4) +
      "&current=temperature_2m,relative_humidity_2m,weather_code"
      "&daily=weather_code,temperature_2m_mean,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
      "&forecast_days=5&temperature_unit=fahrenheit";
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
  result_.temperatureF = static_cast<int16_t>(forecast["current"]["temperature_2m"] | 0.0);
  result_.humidity = forecast["current"]["relative_humidity_2m"] | 0;
  result_.weatherCode = forecast["current"]["weather_code"] | 0;
  JsonArrayConst codes = forecast["daily"]["weather_code"].as<JsonArrayConst>();
  JsonArrayConst means = forecast["daily"]["temperature_2m_mean"].as<JsonArrayConst>();
  JsonArrayConst highs = forecast["daily"]["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst lows = forecast["daily"]["temperature_2m_min"].as<JsonArrayConst>();
  JsonArrayConst rain = forecast["daily"]["precipitation_probability_max"].as<JsonArrayConst>();
  JsonArrayConst dates = forecast["daily"]["time"].as<JsonArrayConst>();
  bool completeDaily = codes.size() >= 5 && means.size() >= 5 && highs.size() >= 5 && lows.size() >= 5 && dates.size() >= 5;
  for (uint8_t day = 0; day < 5 && completeDaily; ++day) {
    completeDaily = !codes[day].isNull() && !means[day].isNull() && !highs[day].isNull() && !lows[day].isNull() && !dates[day].isNull();
    if (completeDaily) {
      const double mean = means[day].as<double>(), high = highs[day].as<double>(), low = lows[day].as<double>();
      const int code = codes[day].as<int>();
      completeDaily = mean >= -99 && mean <= 199 && high >= -99 && high <= 199 && low >= -99 && low <= 199 && code >= 0 && code <= 99;
    }
  }
  bool completeRain = rain.size() >= 5;
  for (uint8_t day = 0; day < 5 && completeRain; ++day) {
    completeRain = !rain[day].isNull() && rain[day].as<int>() <= 100;
  }
  if (completeDaily) {
    for (uint8_t day = 0; day < 5; ++day) {
      result_.dailyCodes[day] = codes[day].as<int>();
      result_.meansF[day] = static_cast<int16_t>(means[day].as<double>());
      result_.highsF[day] = static_cast<int16_t>(highs[day].as<double>());
      result_.lowsF[day] = static_cast<int16_t>(lows[day].as<double>());
      tm date = {};
      strptime(dates[day].as<const char*>(), "%Y-%m-%d", &date);
      date.tm_isdst = -1;
      mktime(&date);
      strftime(result_.dayNames[day], sizeof(result_.dayNames[day]), "%a", &date);
    }
  }
  if (completeRain) {
    for (uint8_t day = 0; day < 5; ++day) result_.rain[day] = rain[day].as<uint8_t>();
  }
  const String dateHeader = http.header("Date");
  http.end();
  if (time(nullptr) <= 1700000000) setClockFromDate(dateHeader);
  result_.latitude = latitude;
  result_.longitude = longitude;
  result_.dailyAvailable = completeDaily;
  result_.rainAvailable = completeRain;
  Serial.printf("Weather ready: %d F, %u%%, %s\n", result_.temperatureF, result_.humidity, result_.city);
  return true;
}

void WeatherClient::render(SduiDocument& document) const {
  const int temperature = celsius_
      ? static_cast<int>((weather_.temperatureF - 32) * 5.0 / 9.0)
      : weather_.temperatureF;
  const char* unit = celsius_ ? "C" : "F";
  for (uint8_t i = 0; i < document.screenCount; ++i) {
    SduiScreen& screen = document.screens[i];
    if (strcmp(screen.screenId, "sky-weather") != 0) continue;
    for (uint8_t j = 0; j < screen.componentCount; ++j) {
      SduiComponent& component = screen.components[j];
      if (strcmp(component.id, "weather-temp") == 0) {
        if (hasData_) snprintf(component.text, sizeof(component.text), "%d %s", temperature, unit);
        else snprintf(component.text, sizeof(component.text), "-- %s", unit);
      } else if (strcmp(component.id, "weather-condition") == 0) {
        strlcpy(component.text, hasData_ ? description(weather_.weatherCode) : "WEATHER UNAVAILABLE", sizeof(component.text));
      } else if (strcmp(component.id, "weather-humidity-value") == 0) {
        if (hasData_) snprintf(component.text, sizeof(component.text), "%u%%", weather_.humidity);
        else strlcpy(component.text, "--", sizeof(component.text));
      } else if (strcmp(component.id, "weather-rain-value") == 0) {
        if (weather_.rainAvailable) snprintf(component.text, sizeof(component.text), "%u%%", weather_.rain[0]);
        else strlcpy(component.text, "--", sizeof(component.text));
      } else if (strcmp(component.id, "weather-forecast-data") == 0) {
        component.text[0] = '\0';
        if (!weather_.dailyAvailable) continue;
        size_t used = 0;
        for (uint8_t day = 0; day < 5 && used < sizeof(component.text); ++day) {
          const int mean = celsius_ ? static_cast<int>((weather_.meansF[day] - 32) * 5.0 / 9.0) : weather_.meansF[day];
          const int high = celsius_ ? static_cast<int>((weather_.highsF[day] - 32) * 5.0 / 9.0) : weather_.highsF[day];
          const int low = celsius_ ? static_cast<int>((weather_.lowsF[day] - 32) * 5.0 / 9.0) : weather_.lowsF[day];
          const char* condition = weather_.dailyCodes[day] == 0 ? "CL" : weather_.dailyCodes[day] <= 3 ? "PC" : weather_.dailyCodes[day] <= 48 ? "FG" : weather_.dailyCodes[day] <= 67 ? "RA" : weather_.dailyCodes[day] <= 77 ? "SN" : weather_.dailyCodes[day] <= 82 ? "SH" : "ST";
          char record[24] = {};
          const int written = snprintf(record, sizeof(record), "%.3s:%.2s:%d:%d:%d", weather_.dayNames[day], condition, mean, high, low);
          const size_t prefix = day == 0 ? 0 : 1;
          if (written < 0 || static_cast<size_t>(written) >= sizeof(record) || used + prefix + static_cast<size_t>(written) >= sizeof(component.text)) break;
          if (prefix) component.text[used++] = ';';
          memcpy(component.text + used, record, static_cast<size_t>(written) + 1);
          used += static_cast<size_t>(written);
        }
      } else if (strncmp(component.id, "weather-forecast-", 17) == 0) {
        const uint8_t days = static_cast<uint8_t>(atoi(component.id + 17));
        if (days == selectedForecastDays_) strlcpy(component.icon, "selected", sizeof(component.icon));
        else strlcpy(component.icon, "", sizeof(component.icon));
      } else if (strcmp(component.id, "weather-humidity") == 0) {
        if (hasData_) snprintf(component.text, sizeof(component.text), "HUMIDITY %u%%", weather_.humidity);
        else strlcpy(component.text, "HUMIDITY --", sizeof(component.text));
      } else if (strcmp(component.id, "weather-location") == 0) {
        strlcpy(component.text, weather_.city, sizeof(component.text));
      }
    }
  }
}

void WeatherClient::setForecastDays(uint8_t days, SduiDocument& document) {
  if (days != 1 && days != 3 && days != 5) return;
  selectedForecastDays_ = days;
  render(document);
}

void WeatherClient::toggleUnits(SduiDocument& document) {
  setMetric(!celsius_, document);
}

void WeatherClient::setMetric(bool metric, SduiDocument& document) {
  celsius_ = metric;
  Preferences preferences;
  if (preferences.begin("weather", false)) {
    preferences.putBool("metric", celsius_);
    preferences.end();
  }
  render(document);
  Serial.printf("Weather units: %s\n", celsius_ ? "C" : "F");
}

}  // namespace leopold
