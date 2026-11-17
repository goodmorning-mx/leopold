#pragma once
#include <ArduinoJson.h>
#include "weather_location.h"

namespace leopold { namespace weather_location {
inline bool parseResponse(const char* body, size_t length, const char* country,
                          const char* postal, WeatherLocation& output) {
  if (!body || !length || length > 1024 || !country || !postal) return false;
  JsonDocument response;
  if (deserializeJson(response, body, length)) return false;
  if (!response["country"].is<const char*>() || !response["postal_code"].is<const char*>() ||
      !response["name"].is<const char*>() || !response["latitude"].is<double>() ||
      !response["longitude"].is<double>()) return false;
  const char* actualCountry = response["country"].as<const char*>();
  const char* actualPostal = response["postal_code"].as<const char*>();
  const char* city = response["name"].as<const char*>();
  WeatherLocation candidate{};
  if (strcmp(actualCountry, country) || strcmp(actualPostal, postal) ||
      strlen(actualCountry) >= sizeof(candidate.country) ||
      strlen(actualPostal) >= sizeof(candidate.postalCode) ||
      strlen(city) >= sizeof(candidate.city)) return false;
  snprintf(candidate.country, sizeof(candidate.country), "%s", actualCountry);
  snprintf(candidate.postalCode, sizeof(candidate.postalCode), "%s", actualPostal);
  snprintf(candidate.city, sizeof(candidate.city), "%s", city);
  candidate.latitude = response["latitude"].as<double>();
  candidate.longitude = response["longitude"].as<double>();
  if (!valid(candidate)) return false;
  output = candidate;
  return true;
}
} }
