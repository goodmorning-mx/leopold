#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace leopold {
struct WeatherLocation {
  char country[3] = {};
  char postalCode[11] = {};
  char city[33] = {};
  double latitude = 0;
  double longitude = 0;
};
namespace weather_location {
constexpr uint32_t kMagic = 0x574c4f43;
struct Record {
  uint32_t magic = kMagic;
  uint16_t version = 1;
  uint16_t bytes = sizeof(Record);
  WeatherLocation value{};
};
inline bool normalize(uint8_t country, const char* draft, char (&output)[11]) {
  if (!draft || (country != 1 && country != 2)) return false;
  const size_t size = strnlen(draft, 11);
  if ((country == 1 && size != 5 && size != 9 && size != 10) ||
      (country == 2 && size != 5)) return false;
  for (size_t i = 0; i < size; ++i)
    if (!(country == 1 && size == 10 && i == 5 && draft[i] == '-') &&
        (draft[i] < '0' || draft[i] > '9')) return false;
  if (size == 9) snprintf(output, sizeof(output), "%.5s-%.4s", draft, draft + 5);
  else { memcpy(output, draft, size); output[size] = 0; }
  return true;
}
inline bool valid(const WeatherLocation& value) {
  if (!memchr(value.country, 0, sizeof(value.country)) ||
      !memchr(value.postalCode, 0, sizeof(value.postalCode)) ||
      !memchr(value.city, 0, sizeof(value.city)) || !value.city[0]) return false;
  const uint8_t country = strcmp(value.country, "US") == 0 ? 1 : strcmp(value.country, "MX") == 0 ? 2 : 0;
  char normalized[11] = {};
  return normalize(country, value.postalCode, normalized) &&
      strcmp(normalized, value.postalCode) == 0 &&
      isfinite(value.latitude) && isfinite(value.longitude) &&
      value.latitude >= -90 && value.latitude <= 90 &&
      value.longitude >= -180 && value.longitude <= 180;
}
inline bool decode(const Record& record, WeatherLocation& output) {
  if (record.magic != kMagic || record.version != 1 ||
      record.bytes != sizeof(Record) || !valid(record.value)) return false;
  output = record.value;
  return true;
}
// Verification happens on a staging key; the previous active value is never
// replaced just because staging verification failed. Promotion is one atomic
// NVS blob commit. Staging records are never selected during boot.
template <typename Store>
bool save(Store& store, const WeatherLocation& value) {
  if (!valid(value)) return false;
  Record candidate{};
  candidate.value = value;
  Record staged{};
  WeatherLocation checked{};
  return store.stage(candidate) && store.readStaged(staged) &&
      memcmp(&candidate, &staged, sizeof(candidate)) == 0 &&
      decode(staged, checked) && store.promote(candidate);
}
inline bool coordinates(const WeatherLocation& saved, double& latitude, double& longitude) {
  if (!valid(saved)) return false;
  latitude = saved.latitude;
  longitude = saved.longitude;
  return true;
}
inline void draft(const WeatherLocation& value, char* output, size_t capacity) {
  size_t used = 0;
  if (!capacity) return;
  for (const char* p = value.postalCode; *p && used + 1 < capacity; ++p)
    if (*p != '-') output[used++] = *p;
  output[used] = 0;
}
inline bool matches(uint8_t country, const char* input, const WeatherLocation& result) {
  char canonical[11] = {};
  return normalize(country, input, canonical) &&
      strcmp(result.country, country == 1 ? "US" : "MX") == 0 &&
      strcmp(canonical, result.postalCode) == 0;
}
}  // namespace weather_location
}  // namespace leopold
