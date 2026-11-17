"""Compile and drive the production WeatherClient with deterministic host shims.

The shims replace Arduino/FreeRTOS/network boundaries only. WeatherClient itself
and its request/result protocol are compiled unchanged from device/src.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class WeatherClientHostFixture(unittest.TestCase):
    def test_production_client_handoff(self) -> None:
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="weather-client-host-") as temporary:
            temp = Path(temporary)
            include = temp / "include"
            include.mkdir()
            self._write_stubs(include)
            (temp / "fixture.cpp").write_text(CPP_FIXTURE)
            command = [
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1",
                "-DLEOPOLD_API_BASE_URL=\"https://fixture.invalid\"",
                "-I", str(include), "-I", str(ROOT / "device/include"),
                "-I", str(ROOT / "shared"),
                "-I", str(ROOT / "device/.pio/libdeps/leopold/ArduinoJson/src"),
                str(temp / "fixture.cpp"), str(ROOT / "device/src/weather_client.cpp"),
                "-o", str(temp / "fixture"),
            ]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(temp / "fixture")], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("weather handoff fixture passed", result.stdout)

            if os.environ.get("LEOPOLD_WEATHER_BASELINE") == "1":
                self._run_baseline(temp, command, include)

    @staticmethod
    def _run_baseline(temp: Path, command: list[str], include: Path) -> None:
            # One-off historical reproduction against pre-F02 HEAD. Keep it
            # opt-in because the checked-out HEAD will change after review.
            baseline = temp / "baseline"
            baseline.mkdir()
            for path in ("include/weather_client.h", "src/weather_client.cpp"):
                git_path = "Code/device/" + path.replace("\\", "/")
                contents = subprocess.run(
                    ["git", "show", "c1a39922609331ac26642cf39bbd905e54c105c0:" + git_path], cwd=ROOT.parent,
                    check=True, capture_output=True, text=True,
                ).stdout
                target = baseline / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(contents)
            assert "requestRefresh(SduiDocument&" in (baseline / "include/weather_client.h").read_text()
            (temp / "baseline.cpp").write_text(BASELINE_FIXTURE)
            baseline_command = command[:]
            baseline_command[baseline_command.index(str(temp / "fixture.cpp"))] = str(temp / "baseline.cpp")
            baseline_command[baseline_command.index(str(ROOT / "device/src/weather_client.cpp"))] = str(baseline / "src/weather_client.cpp")
            stub_include_index = baseline_command.index(str(include))
            baseline_command[stub_include_index - 1:stub_include_index + 1] = [
                "-I", str(baseline / "include"), "-I", str(include),
            ]
            baseline_command[-1] = str(temp / "baseline-fixture")
            baseline_compiled = subprocess.run(baseline_command, capture_output=True, text=True, timeout=60)
            assert baseline_compiled.returncode == 0, baseline_compiled.stderr
            baseline_result = subprocess.run([str(temp / "baseline-fixture")], capture_output=True, text=True, timeout=10)
            assert baseline_result.returncode == 0, baseline_result.stderr
            assert "baseline worker mutated live document before handoff" in baseline_result.stdout

    @staticmethod
    def _write_stubs(include: Path) -> None:
        stubs = {
            "Arduino.h": ARDUINO_H,
            "WiFi.h": '#pragma once\n#include <Arduino.h>\n',
            "WiFiClientSecure.h": '#pragma once\nclass WiFiClientSecure { public: void setCACert(const char*){} void setHandshakeTimeout(int){} };\n',
            "HTTPClient.h": HTTP_H,
            "Preferences.h": PREFS_H,
            "config_guard.h": '#pragma once\n#define LEOPOLD_API_BASE_URL "https://fixture.invalid"\n#define LEOPOLD_TLS_ROOT_CA "fixture"\n',
            "device_identity.h": '#pragma once\nnamespace leopold::identity { inline const char* deviceId(){return "fixture";} inline const char* deviceSecret(){return "fixture";} }\n',
            "tls_bundle.h": '#pragma once\n',
            "tls_roots.h": '#pragma once\ninline constexpr const char* kLetsEncryptRootYrByX1="fixture";\n',
        }
        for name, contents in stubs.items():
            (include / name).write_text(contents)


ARDUINO_H = r'''#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
using TaskHandle_t = void*;
using BaseType_t = int;
using UBaseType_t=unsigned;
struct StopWorker {};
inline void (*captured_task)(void*)=nullptr;
constexpr int pdPASS=1; constexpr int WL_CONNECTED=3;
#define pdMS_TO_TICKS(x) (x)
class String { public:
 String()=default; String(const char* s):v_(s?s:""){} String(std::string s):v_(std::move(s)){}
 String(double x,int p){char b[64];std::snprintf(b,sizeof(b),"%.*f",p,x);v_=b;}
 const char* c_str()const{return v_.c_str();} size_t length()const{return v_.size();} bool isEmpty()const{return v_.empty();} operator const char*()const{return v_.c_str();}
 bool endsWith(const char*s)const{return v_.size()>=std::strlen(s)&&v_.compare(v_.size()-std::strlen(s),std::strlen(s),s)==0;}
 void remove(size_t n){if(n<v_.size())v_.erase(n);} bool concat(const char*s){v_+=s?s:"";return true;} String& operator+=(const String&s){v_+=s.v_;return *this;} String& operator+=(const char*s){v_+=s?s:"";return *this;}
 friend String operator+(const String&a,const String&b){return String(a.v_+b.v_);} friend String operator+(const String&a,const char*b){return String(a.v_+(b?b:""));}
 private: std::string v_;
};
inline size_t strlcpy(char*d,const char*s,size_t n){size_t z=s?std::strlen(s):0;if(n){size_t c=std::min(z,n-1);std::memcpy(d,s,c);d[c]=0;}return z;}
struct SerialType { template<class...A> void printf(const char*,A...){} void println(const char*){} }; inline SerialType Serial;
inline int WiFi_status_value=WL_CONNECTED; struct WiFiType{int status()const{return WiFi_status_value;}};inline WiFiType WiFi;
inline int xTaskCreatePinnedToCore(void(*entry)(void*),const char*,uint32_t,void*,UBaseType_t,TaskHandle_t*out,int){captured_task=entry;*out=(void*)1;return pdPASS;}
inline void vTaskDelay(uint32_t){throw StopWorker{};} inline void delay(uint32_t){}
inline void configTzTime(const char*,const char*,const char*,const char*){} inline bool getLocalTime(tm*,uint32_t){return false;}
'''

HTTP_H = r'''#pragma once
#include <Arduino.h>
#include <string>
constexpr int HTTP_CODE_OK = 200, HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
inline void (*beforeResponse)() = nullptr;
inline bool fullForecast = true;
inline bool geocodeSucceeds = true;
inline bool forecastSucceeds = true;
inline std::string locationCoordinates = "1.25,2.5";
inline std::string forecastUrl;
constexpr char geocodeBody[] =
    R"({"country":"US","postal_code":"12345","name":"Saved City","latitude":12,"longitude":34})";
struct Stream {
  size_t readBytes(char* out, size_t size) {
    std::memcpy(out, geocodeBody, size);
    return size;
  }
};
class HTTPClient {
 public:
  bool begin(class WiFiClientSecure&, const String& url) {
    url_ = url.c_str();
    if (isForecast()) forecastUrl = url_;
    return true;
  }
  void setConnectTimeout(int) {}
  void setTimeout(int) {}
  void setFollowRedirects(int) {}
  void addHeader(const char*, const String&) {}
  void addHeader(const char*, const char*) {}
  void collectHeaders(const char**, size_t) {}
  int GET() {
    if (beforeResponse) beforeResponse();
    return (isGeocode() && !geocodeSucceeds) || (isForecast() && !forecastSucceeds)
        ? 500 : HTTP_CODE_OK;
  }
  int getSize() { return sizeof(geocodeBody) - 1; }
  Stream& getStream() { return stream_; }
  String getString() {
    if (!isForecast()) return String("{\"loc\":\"" + locationCoordinates + "\",\"city\":\"Fixture City\"}");
    if (!fullForecast) return R"({"current":{"temperature_2m":68,"relative_humidity_2m":50,"weather_code":0}})";
    return R"({"current":{"temperature_2m":77,"relative_humidity_2m":40,"weather_code":0},"daily":{
      "weather_code":[0,1,2,3,4],"temperature_2m_mean":[77,77,77,77,77],
      "temperature_2m_max":[80,80,80,80,80],"temperature_2m_min":[65,65,65,65,65],
      "precipitation_probability_max":[30,30,30,30,30],
      "time":["2026-11-13","2026-11-14","2026-11-15","2026-11-16","2026-11-17"]}})";
  }
  String header(const char*) { return "Tue, 01 Jan 2030 00:00:00 GMT"; }
  void end() {}
 private:
  bool isForecast() const { return url_.find("forecast") != std::string::npos; }
  bool isGeocode() const { return url_.find("geocode") != std::string::npos; }
  std::string url_;
  Stream stream_;
};
'''

PREFS_H = r'''#pragma once
#include <cstring>
#include <vector>
inline bool persistSucceeds = true;
inline std::vector<unsigned char> stagedLocation, savedLocationRecord;
class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  bool getBool(const char*, bool fallback = false) { return fallback; }
  size_t getBytesLength(const char*) { return savedLocationRecord.size(); }
  size_t getBytes(const char* key, void* out, size_t size) {
    const auto& bytes = std::strcmp(key, "location_stage") == 0 ? stagedLocation : savedLocationRecord;
    if (bytes.size() != size) return 0;
    std::memcpy(out, bytes.data(), size);
    return size;
  }
  size_t putBytes(const char* key, const void* data, size_t size) {
    const auto* first = static_cast<const unsigned char*>(data);
    const bool staging = std::strcmp(key, "location_stage") == 0;
    if (!staging && !persistSucceeds) return 0;
    (staging ? stagedLocation : savedLocationRecord).assign(first, first + size);
    return size;
  }
  bool putBool(const char*, bool) { return true; }
};
'''

CPP_FIXTURE = r'''#include <cassert>
#include <cstdio>
#include "HTTPClient.h"
#include "Preferences.h"
#include "weather_client.h"

using namespace leopold;
static WeatherClient client;
static SduiDocument original, replacement;

void runWorker() {
  // Exit the perpetual worker at its next idle delay.
  try { captured_task(&client); } catch (const StopWorker&) {}
}

void prepare(SduiDocument& document) {
  document.screenCount = 2;
  auto& page = document.screens[0];
  std::strcpy(page.screenId, "sky-weather");
  std::strcpy(document.screens[1].screenId, "sky-home");
  const char* ids[] = {"weather-location", "weather-temp", "weather-humidity-value",
      "weather-rain-value", "weather-forecast-data", "weather-forecast-5"};
  for (const auto* id : ids) {
    auto& component = page.components[page.componentCount++];
    std::strcpy(component.id, id);
    std::strcpy(component.text, "UNTOUCHED");
  }
}

int main() {
  assert(client.begin());
  prepare(original);
  prepare(replacement);
  auto& fields = replacement.screens[0].components;
  assert(client.requestRefresh());
  assert(!client.requestRefresh());
  bool success = false;
  assert(!client.takeRefreshResult(success));
  beforeResponse = [] {
    bool success = false;
    assert(!client.takeRefreshResult(success));
    assert(!client.requestLocationLookup(1, "12345"));
    client.setMetric(true, replacement);
    client.setForecastDays(5, replacement);
    replacement.activeScreen = 1;
    assert(std::strcmp(replacement.screens[0].components[1].text, "-- C") == 0);
  };
  runWorker();
  beforeResponse = nullptr;
  assert(!client.requestRefresh());
  assert(!client.requestLocationLookup(1, "12345"));
  assert(std::strcmp(original.screens[0].components[0].text, "UNTOUCHED") == 0);
  assert(std::strcmp(fields[1].text, "-- C") == 0);
  assert(client.takeRefreshResult(success) && success);
  client.render(replacement);
  assert(replacement.activeScreen == 1);
  assert(std::strcmp(fields[0].text, "Fixture City") == 0);
  assert(std::strcmp(fields[1].text, "25 C") == 0);
  assert(std::strcmp(fields[2].text, "40%") == 0);
  assert(std::strcmp(fields[3].text, "30%") == 0);
  assert(std::strlen(fields[4].text) > 0);
  assert(std::strcmp(fields[5].icon, "selected") == 0);

  fullForecast = false;
  assert(client.requestRefresh());
  runWorker();
  assert(client.takeRefreshResult(success) && success);
  client.render(replacement);
  assert(std::strcmp(fields[1].text, "20 C") == 0);
  assert(std::strcmp(fields[3].text, "30%") == 0 && fields[4].text[0]);

  forecastSucceeds = false;
  assert(client.requestRefresh());
  runWorker();
  assert(client.takeRefreshResult(success) && !success);
  client.render(replacement);
  assert(std::strcmp(fields[1].text, "20 C") == 0 && fields[4].text[0]);
  forecastSucceeds = true;

  locationCoordinates = "3.75,4.5";
  assert(client.requestRefresh());
  runWorker();
  assert(client.takeRefreshResult(success) && success);
  client.render(replacement);
  assert(std::strcmp(fields[3].text, "--") == 0 && !fields[4].text[0]);

  // Restore a complete forecast to check failed location changes preserve it.
  fullForecast = true;
  assert(client.requestRefresh());
  runWorker();
  assert(client.takeRefreshResult(success) && success);
  fullForecast = false;
  WeatherLocation location{};
  geocodeSucceeds = false;
  assert(client.requestLocationLookup(1, "12345"));
  runWorker();
  assert(!client.requestRefresh());
  assert(client.takeLocationLookupResult(success, location) && !success);
  assert(!client.savedLocation(location));
  geocodeSucceeds = true;

  persistSucceeds = false;
  assert(client.requestLocationLookup(1, "12345"));
  runWorker();
  assert(client.takeLocationLookupResult(success, location) && !success);
  client.render(replacement);
  assert(std::strcmp(fields[0].text, "Fixture City") == 0 && fields[4].text[0]);

  persistSucceeds = true;
  assert(client.requestLocationLookup(1, "12345"));
  runWorker();
  assert(!client.savedLocation(location));
  assert(!savedLocationRecord.empty());
  assert(!client.requestLocationLookup(1, "12345"));
  assert(client.takeLocationLookupResult(success, location) && success);
  assert(client.savedLocation(location));
  client.render(replacement);
  assert(std::strcmp(fields[0].text, "Saved City") == 0);
  assert(std::strcmp(fields[1].text, "-- C") == 0 && !fields[4].text[0]);
  assert(client.requestRefresh());
  runWorker();
  assert(forecastUrl.find("latitude=12.0000&longitude=34.0000") != std::string::npos);
  assert(client.takeRefreshResult(success) && success);
  client.render(replacement);
  assert(std::strcmp(fields[1].text, "20 C") == 0);
  assert(std::strcmp(fields[3].text, "--") == 0 && !fields[4].text[0]);
  std::puts("weather handoff fixture passed");
}
'''

BASELINE_FIXTURE = r'''#include <cassert>
#include <cstdio>
#include <stdexcept>
#include "weather_client.h"
int main(){
  using namespace leopold;
  WeatherClient client;
  assert(client.begin());
  SduiDocument document{};
  document.screenCount=1;
  std::strcpy(document.screens[0].screenId,"sky-weather");
  std::strcpy(document.screens[0].components[0].id,"weather-location");
  std::strcpy(document.screens[0].components[0].text,"OLD");
  document.screens[0].componentCount=1;
  assert(client.requestRefresh(document));
  try { captured_task(&client); } catch (const StopWorker&) {}
  assert(std::strcmp(document.screens[0].components[0].text,"Fixture City")==0);
  std::puts("baseline worker mutated live document before handoff");
}
'''


class WeatherApplicationTests(unittest.TestCase):
    def test_editor_intent_and_completion(self) -> None:
        compiler = shutil.which("c++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("native C++ compiler unavailable")
        main = (ROOT / "device/src/main.cpp").read_text()
        # Compile the application handlers with only their display and weather
        # boundaries stubbed. The production client is covered separately.
        handlers = []
        for start, end in (
            ("void clearWeatherLookupIntent()", "void processInput("),
            ("void processWeatherResults()", "void refreshIndicators("),
        ):
            begin = main.index(start)
            handlers.append(main[begin:main.index(end, begin)])
        with tempfile.TemporaryDirectory(prefix="weather-application-host-") as temp:
            source = Path(temp) / "main.cpp"
            binary = Path(temp) / "main"
            source.write_text(APP_STUBS + "".join(handlers) + APP_CASES)
            compiled = subprocess.run([
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DLEOPOLD_WASM", "-I", str(ROOT / "device/include"),
                "-I", str(ROOT / "shared"), str(source), "-o", str(binary),
            ], capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)


APP_STUBS = r'''
#include <cassert>
#include <cstring>
#include "sdui.h"
#include "weather_location.h"
using namespace leopold;
namespace leopold::display_menu {
enum class Effect { kNone, kBack, kClock, kLookup };
struct State {
  int country = 1, expanded = 5;
  char postal[11] = "12345", status[90] = {}, location[33] = {};
  bool spanish = false;
};
void copy(char* out, size_t cap, const char* text) { snprintf(out, cap, "%s", text); }
}
using leopold::display_menu::Effect;
display_menu::State displayMenuState;
enum class LocalPanel { kNone, kWifi };
LocalPanel localPanel = LocalPanel::kNone;
bool hasUi = true, welcomeActive = false, assistantReady = false;
bool weatherLookupPending = false, weatherLookupFeedback = false, weatherRequested = true;
SduiDocument currentUi;
unsigned menuBuilds = 0;
void buildDeviceDisplayMenu(SduiScreen&) { ++menuBuilds; }
struct Screen {
  unsigned repaints = 0;
  void renderSduiScreen(const SduiDocument&) { ++repaints; }
} screen;
struct Weather {
  bool busy = true, lookupReady = false, refreshReady = false, lookupSuccess = true;
  unsigned updates = 0, submitted = 0;
  bool ready() const { return true; }
  bool takeRefreshResult(bool& success) {
    if (!refreshReady) return false;
    refreshReady = busy = false;
    success = true;
    return true;
  }
  bool takeLocationLookupResult(bool& success, WeatherLocation& result) {
    if (!lookupReady) return false;
    lookupReady = busy = false;
    success = lookupSuccess;
    strcpy(result.country, "US");
    strcpy(result.postalCode, "12345");
    strcpy(result.city, "Saved city");
    return true;
  }
  bool requestLocationLookup(uint8_t, const char*) {
    if (busy) return false;
    busy = true;
    ++submitted;
    return true;
  }
  void render(SduiDocument& doc) { assert(&doc == &currentUi); ++updates; }
} weather;
'''

APP_CASES = r'''
int main() {
  currentUi.screenCount = 2;
  strcpy(currentUi.screens[0].screenId, "device-additional-options");
  strcpy(currentUi.screens[1].screenId, "sky-weather");
  updateWeatherLookupIntent(displayMenuState, Effect::kLookup, "display-postal-apply");
  processWeatherResults();
  assert(weatherLookupPending && weather.submitted == 0);
  weather.refreshReady = true;
  processWeatherResults();
  assert(!weatherLookupPending && weatherLookupFeedback && weather.submitted == 1);
  const auto before = displayMenuState;
  strcpy(displayMenuState.postal, "54321");
  updateWeatherLookupIntent(before, Effect::kNone, "postal-key-1");
  strcpy(displayMenuState.status, "New draft");
  weather.lookupReady = true;
  processWeatherResults();
  assert(!strcmp(displayMenuState.postal, "54321"));
  assert(!strcmp(displayMenuState.status, "New draft"));
  assert(!strcmp(displayMenuState.location, "Saved city"));
  assert(!weatherRequested && weather.updates == 2);
  strcpy(displayMenuState.postal, "12345");
  updateWeatherLookupIntent(displayMenuState, Effect::kLookup, "display-postal-apply");
  processWeatherResults();
  assert(weatherLookupFeedback && weather.submitted == 2);
  updateWeatherLookupIntent(displayMenuState, Effect::kNone, "display-postal-cancel");
  assert(!weatherLookupFeedback && !displayMenuState.status[0]);
  localPanel = LocalPanel::kWifi;
  currentUi.activeScreen = 1;
  weather.lookupReady = true;
  const auto previousRepaints = screen.repaints;
  processWeatherResults();
  assert(!strcmp(displayMenuState.location, "Saved city"));
  assert(currentUi.activeScreen == 1 && screen.repaints == previousRepaints);
  localPanel = LocalPanel::kNone;
  currentUi.activeScreen = 0;
  updateWeatherLookupIntent(displayMenuState, Effect::kLookup, "display-postal-apply");
  processWeatherResults();
  weather.lookupReady = true;
  weather.lookupSuccess = false;
  weatherRequested = true;
  processWeatherResults();
  assert(strstr(displayMenuState.status, "Location not verified"));
  assert(weatherRequested && !weatherLookupFeedback);
}
'''


if __name__ == "__main__":
    unittest.main()
