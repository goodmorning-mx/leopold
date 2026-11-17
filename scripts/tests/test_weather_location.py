"""Exercise the native postal record and normalization without network/device I/O."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class WeatherLocationTests(unittest.TestCase):
    def test_validated_location_roundtrip_and_failure_retains_output(self):
        source = r'''
#include "weather_location.h"
#include "weather_location_response.h"
#include <cassert>
#include <cstring>
using namespace leopold;
struct Store {
  weather_location::Record active{}, staged{};
  bool failStage = false, failVerify = false, failPromote = false;
  bool stage(const weather_location::Record& record) { if (failStage) return false; staged = record; return true; }
  bool readStaged(weather_location::Record& record) { if (failVerify) return false; record = staged; return true; }
  bool promote(const weather_location::Record& record) { if (failPromote) return false; active = record; return true; }
};
int main() {
  const char* response = R"({"country":"US","postal_code":"00501","name":"Holtsville","latitude":40.8,"longitude":-73.04})";
  WeatherLocation parsed{};
  assert(weather_location::parseResponse(response, strlen(response), "US", "00501", parsed));
  assert(fabs(parsed.latitude - 40.8) < 0.00001 && fabs(parsed.longitude + 73.04) < 0.00001);
  const auto originalParsed = parsed;
  assert(!weather_location::parseResponse(response, strlen(response), "MX", "00501", parsed));
  assert(!weather_location::parseResponse(response, strlen(response), "US", "00502", parsed));
  assert(!weather_location::parseResponse(response, 1025, "US", "00501", parsed));
  const char* bad = R"({"country":"US","postal_code":"00501","name":"A","latitude":91,"longitude":0})";
  assert(!weather_location::parseResponse(bad, strlen(bad), "US", "00501", parsed));
  assert(memcmp(&parsed, &originalParsed, sizeof(parsed)) == 0);
  Store storage;
  assert(weather_location::save(storage, parsed));
  WeatherLocation rebooted{};
  assert(weather_location::decode(storage.active, rebooted));
  double lat = 0, lon = 0;
  assert(weather_location::coordinates(rebooted, lat, lon));
  assert(lat == parsed.latitude && lon == parsed.longitude);
  const auto activeBefore = storage.active;
  auto replacement = parsed; strcpy(replacement.city, "Replacement");
  storage.failStage = true;
  assert(!weather_location::save(storage, replacement));
  storage.failStage = false; storage.failVerify = true;
  assert(!weather_location::save(storage, replacement));
  storage.failVerify = false; storage.failPromote = true;
  assert(!weather_location::save(storage, replacement));
  assert(memcmp(&storage.active, &activeBefore, sizeof(activeBefore)) == 0);
  WeatherLocation absent{};
  assert(!weather_location::coordinates(absent, lat, lon));
  assert(lat == parsed.latitude && lon == parsed.longitude);
  char postal[11] = {};
  assert(weather_location::normalize(1, "00501", postal));
  assert(strcmp(postal, "00501") == 0);
  assert(weather_location::normalize(1, "021391234", postal));
  assert(strcmp(postal, "02139-1234") == 0);
  assert(weather_location::normalize(1, "02139-1234", postal));
  assert(!weather_location::normalize(2, "021391234", postal));
  assert(!weather_location::normalize(1, "0213x", postal));
  assert(!weather_location::normalize(0, "06000", postal));
  weather_location::Record saved{};
  strcpy(saved.value.country, "MX");
  strcpy(saved.value.postalCode, "06000");
  strcpy(saved.value.city, "Ciudad de Mexico");
  saved.value.latitude = 19.43; saved.value.longitude = -99.13;
  WeatherLocation restored{};
  assert(weather_location::decode(saved, restored));
  assert(restored.latitude == 19.43 && restored.longitude == -99.13);
  assert(weather_location::matches(2, "06000", restored));
  assert(!weather_location::matches(1, "06000", restored));
  char draft[16] = {};
  strcpy(saved.value.country, "US");
  strcpy(saved.value.postalCode, "02139-1234");
  assert(weather_location::decode(saved, restored));
  weather_location::draft(restored, draft, sizeof(draft));
  assert(strcmp(draft, "021391234") == 0);
  assert(weather_location::matches(1, draft, restored));
  const auto previous = restored;
  saved.version = 99;
  assert(!weather_location::decode(saved, restored));
  assert(memcmp(&restored, &previous, sizeof(previous)) == 0);
  saved.version = 1; saved.value.latitude = NAN;
  assert(!weather_location::decode(saved, restored));
  saved.value.latitude = 91;
  assert(!weather_location::decode(saved, restored));
  saved.value.latitude = 0; saved.value.longitude = -181;
  assert(!weather_location::decode(saved, restored));
  saved.value.longitude = 0; memset(saved.value.city, 'x', sizeof(saved.value.city));
  assert(!weather_location::decode(saved, restored));
  assert(memcmp(&restored, &previous, sizeof(previous)) == 0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "location.cpp"
            binary = Path(directory) / "location"
            cpp.write_text(source)
            subprocess.run([shutil.which("c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(ROOT / "shared"), "-I", str(ROOT / "device/include"),
                            "-I", str(ROOT / "device/.pio/libdeps/leopold/ArduinoJson/src"), str(cpp), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
