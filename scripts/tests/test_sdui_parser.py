"""Exercise the complete production SDUI translation unit with host hardware stubs."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ARDUINO = r'''
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
using std::min;
using std::max;
#define constrain(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
class String : public std::string {
 public:
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  void concat(const char* p, size_t n) { append(p,n); }
};
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
inline int digitalRead(int) { return 1; }
inline void delay(int) {}
constexpr int INPUT_PULLUP=0,OUTPUT=1,HIGH=1;
struct SerialStub {
  void println(const char*) {}
  template<class... T> void printf(const char*,T...) {}
};
inline SerialStub Serial;
'''
HARDWARE = r'''
#pragma once
struct sdmmc_card_t {};
struct spi_bus_config_t { int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz; };
struct sdmmc_host_t { int slot,max_freq_khz; };
struct sdspi_device_config_t { int gpio_cs,host_id; };
struct esp_vfs_fat_mount_config_t { bool format_if_mount_failed; int max_files,allocation_unit_size; };
using gpio_num_t=int;
using esp_err_t=int;
constexpr int SPI3_HOST=0,SDSPI_DEFAULT_DMA=0,ESP_OK=0,ESP_ERR_INVALID_STATE=1;
#define SDSPI_HOST_DEFAULT() sdmmc_host_t{}
#define SDSPI_DEVICE_CONFIG_DEFAULT() sdspi_device_config_t{}
inline int spi_bus_initialize(int,const spi_bus_config_t*,int) { return -1; }
inline int esp_vfs_fat_sdspi_mount(const char*,const sdmmc_host_t*,const sdspi_device_config_t*,const esp_vfs_fat_mount_config_t*,sdmmc_card_t**) { return -1; }
inline const char* esp_err_to_name(int) { return "host stub"; }
'''
FILESYSTEM = r'''
#pragma once
#include "Arduino.h"
inline std::string cachedJson;
class File {
 public:
  explicit operator bool() const { return true; }
  size_t size() const { return cachedJson.size(); }
  String readString() { return String(cachedJson); }
  size_t write(const uint8_t*,size_t n) { return n; }
  void close() {}
};
struct LittleFSStub {
  bool begin(bool) { return true; }
  File open(const char*,const char*) { return File{}; }
};
inline LittleFSStub LittleFS;
'''
HARNESS = r'''
#include "sdui.h"
#include "LittleFS.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <vector>
using namespace leopold;
static SduiDocument output;
int main(int argc,char** argv) {
  assert(argc==3);
  const char* seed=R"({"schema_version":1,"revision":47,"poll_after_ms":9000,"screens":[{"id":"original","components":[{"type":"button","id":"keep","label":"Recognizable","x":4,"y":9,"width":31,"height":42}]},{"id":"other","components":[]}]})";
  assert(SduiParser::parse(seed,strlen(seed),output)==SduiParseResult::kOk);
  output.activeScreen=1;
  const auto* original=reinterpret_cast<const uint8_t*>(&output);
  std::vector<uint8_t> before(original,original+sizeof(output));
  std::string input((std::istreambuf_iterator<char>(std::cin)),{});
  SduiParseResult result;
  if (std::string(argv[2])=="store") {
    cachedJson=input; SduiStore store; String raw;
    result=store.load(output,raw)?SduiParseResult::kOk:SduiParseResult::kInvalid;
  } else result=SduiParser::parse(std::string(argv[2])=="null"?nullptr:input.data(),input.size(),output);
  if (static_cast<int>(result)!=atoi(argv[1])) { std::cerr<<"unexpected result "<<static_cast<int>(result); return 1; }
  if (result!=SduiParseResult::kOk && memcmp(before.data(),&output,sizeof(output))) {
    std::cerr<<"rejected input modified output document"; return 2;
  }
  JsonDocument report;
  report["result"]=static_cast<int>(result); report["document_bytes"]=sizeof(output);
  report["component_bytes"]=sizeof(SduiComponent); report["revision"]=output.revision;
  report["poll"]=output.pollAfterMs; report["active"]=output.activeScreen;
  auto screens=report["screens"].to<JsonArray>();
  for (size_t i=0;i<output.screenCount;++i) {
    const auto& source=output.screens[i]; auto s=screens.add<JsonObject>();
    s["id"]=source.screenId; s["background"]=source.background;
    auto components=s["components"].to<JsonArray>();
    for (size_t j=0;j<source.componentCount;++j) {
      const auto& c=source.components[j]; auto item=components.add<JsonObject>();
      item["type"]=static_cast<int>(c.type); item["id"]=c.id; item["text"]=c.text;
      item["icon"]=c.icon; item["x"]=c.x; item["y"]=c.y; item["width"]=c.width;
      item["height"]=c.height; item["value"]=c.value; item["max"]=c.maximum;
      item["color"]=c.color; item["size"]=c.size; item["align"]=static_cast<int>(c.align);
      item["connected"]=c.connected;
    }
  }
  std::string encoded; serializeJson(report,encoded); std::cout<<encoded;
}
'''


def document(components=None, **overrides):
    value = {"schema_version": 1, "revision": 7, "poll_after_ms": 5000,
             "screens": [{"id": "new", "background": "#123456", "components": components or []}]}
    return value | overrides


class SduiParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("host C++ compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="leopold-sdui-")
        cls.addClassCleanup(cls.temp.cleanup)
        root = Path(cls.temp.name)
        for name, content in {"Arduino.h": ARDUINO, "hardware.h": HARDWARE,
                              "LittleFS.h": FILESYSTEM, "parser_test.cpp": HARNESS}.items():
            (root / name).write_text(content)
        for name in ["esp_vfs_fat.h", "sdmmc_cmd.h", "driver/sdspi_host.h", "driver/spi_master.h"]:
            target = root / name
            target.parent.mkdir(exist_ok=True)
            target.write_text('#include "hardware.h"\n')
        cls.binary = root / "parser-test"
        command = [compiler, "-std=c++17", "-O1", "-g", "-fstack-usage", "-Wall", "-Wextra",
                   "-Wno-unused-function", "-DLEOPOLD_LOCAL_DEVELOPMENT",
                   "-I", str(root), "-I", str(ROOT / "device/include"),
                   "-I", str(ROOT / "device/.pio/libdeps/leopold/ArduinoJson/src"),
                   str(root / "parser_test.cpp"), str(ROOT / "device/src/sdui.cpp"), "-o", str(cls.binary)]
        if os.environ.get("LEOPOLD_SDUI_SANITIZE") == "1":
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def parse(self, value, expected=0, mode="parse"):
        raw = value if isinstance(value, str) else json.dumps(value)
        result = subprocess.run([str(self.binary), str(expected), mode], input=raw, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_late_failure_preserves_entire_live_document(self):
        invalid = {"type": "button", "id": "bad", "label": "X", "x": 2, "y": 3, "width": 0, "height": 8}
        self.parse(document([invalid]), 2)

    def test_early_failures_preserve_document_and_result_codes(self):
        cases = [("", 2, "parse"), ("{", 2, "parse"), ("x", 2, "null"),
                 ("x" * 32769, 1, "parse"), (document(schema_version=2), 3, "parse"),
                 (document(screens=[]), 2, "parse"),
                 (document(screens=[{"id": "s", "components": []}] * 14), 2, "parse")]
        for value, expected, mode in cases:
            with self.subTest(expected=expected, mode=mode, size=len(str(value))):
                self.parse(value, expected, mode)

    def test_late_screen_and_component_boundaries_preserve_document(self):
        text = {"type": "text", "id": "text", "text": "kept", "x": 1, "y": 2}
        cases = [{"id": "bad!", "components": []}, {"id": "x" * 33, "components": []},
                 {"id": "bad", "components": None}, {"id": "bad", "components": [text] * 33}]
        for change in [{"id": "x" * 33}, {"text": "x" * 97}, {"x": -1025}, {"y": 2049}, {"x": "1"}]:
            cases.append({"id": "bad", "components": [text, text | change]})
        for bad in cases:
            with self.subTest(bad=str(bad)[:100]):
                self.parse(document(screens=[{"id": "good", "components": [text]}, bad]), 2)

    def test_valid_types_defaults_and_clamps_are_compatible(self):
        base = {"id": "c", "x": -1024, "y": 2048}
        components = [base | {"type": "text", "text": "x" * 96, "size": 99, "align": "right"},
                      base | {"type": "indicator", "label": "Online", "state": "connected"},
                      base | {"type": "button", "label": "Go", "width": 10, "height": 12, "icon": "usb"},
                      base | {"type": "progress", "value": 70000, "max": 0, "width": 1, "height": 100},
                      base | {"type": "slider", "value": 101, "width": 999, "height": 1},
                      base | {"type": "icon", "name": "spotify", "size": 1},
                      {"type": "future", "id": "bad!", "x": "ignored"}]
        result = self.parse(document(components, poll_after_ms=0))
        self.assertEqual((result["revision"], result["poll"], result["active"]), (7, 2000, 0))
        self.assertEqual(result["screens"][0]["background"], 0x11AA)
        actual = result["screens"][0]["components"]
        self.assertEqual([c["type"] for c in actual], list(range(6)))
        self.assertEqual((len(actual[0]["text"]), actual[0]["size"], actual[0]["align"]), (96, 6, 2))
        self.assertTrue(actual[1]["connected"])
        self.assertEqual((actual[2]["width"], actual[2]["height"], actual[2]["icon"]), (10, 12, "usb"))
        self.assertEqual((actual[3]["value"], actual[3]["max"], actual[3]["width"], actual[3]["height"]), (65535, 1, 20, 80))
        self.assertEqual((actual[4]["value"], actual[4]["max"], actual[4]["width"], actual[4]["height"]), (100, 100, 420, 20))
        self.assertEqual(actual[5]["width"], 24)

    def test_maximum_screen_component_id_and_byte_boundaries(self):
        text = {"type": "text", "id": "x" * 32, "text": "ok", "x": 0, "y": 0}
        screens = [{"id": f"screen-{i}", "components": []} for i in range(12)]
        screens.append({"id": "s" * 32, "components": [text] * 32})
        result = self.parse(document(screens=screens, poll_after_ms=999999))
        self.assertEqual((len(result["screens"]), result["poll"]), (13, 300000))
        self.assertEqual(len(result["screens"][-1]["components"]), 32)
        raw = json.dumps(document())
        self.parse(raw + " " * (32768 - len(raw)))

    def test_store_caller_retains_output_on_failure_and_accepts_valid_cache(self):
        self.parse(document(screens=[{"id": "bad!", "components": []}]), 2, "store")
        self.assertEqual(self.parse(document(), mode="store")["screens"][0]["id"], "new")


if __name__ == "__main__":
    unittest.main()
