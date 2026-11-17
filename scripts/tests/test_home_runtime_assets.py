"""Exercise the device asset loader against the simulator's canonical manifest bytes."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from scripts.generate_home_runtime_assets import KEYS, generate

ROOT = Path(__file__).resolve().parents[2]


def fnv64(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{value:016x}"


class HomeRuntimeAssetTests(unittest.TestCase):
    def test_generation_preserves_manifest_identity_and_is_idempotent(self):
        records = generate(ROOT)
        header = ROOT / 'device/include/generated_home_runtime_assets.h'
        before = hashlib.sha256(header.read_bytes()).hexdigest()
        self.assertEqual(generate(ROOT), records)
        self.assertEqual(hashlib.sha256(header.read_bytes()).hexdigest(), before)
        manifest = json.loads((ROOT/'shared/asset-manifest.json').read_text())['assets']
        self.assertTrue({"background-flower", "background-chevron", "background-mechanical-flower",
                         "background-black-white-circle"} <= {record["key"] for record in records})
        for record in records:
            canonical = (ROOT/'shared/assets'/record['file']).read_bytes()
            self.assertEqual(record['sha256'], hashlib.sha256(canonical).hexdigest())
            if 'device_sha256' in manifest[record['key']]:
                self.assertEqual(record['sha256'], manifest[record['key']]['device_sha256'])
            self.assertEqual(record['bytes'], len(canonical))

    @unittest.skipUnless(shutil.which('clang++') or shutil.which('g++'), 'native C++ compiler unavailable')
    def test_actual_device_loader_resolves_every_home_asset_offline(self):
        generate(ROOT)
        manifest = json.loads((ROOT/'shared/asset-manifest.json').read_text())['assets']
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'Arduino.h').write_text('''#pragma once
#include <cstddef>
#include <cstdint>
#define PROGMEM
struct TestSerial { template<class... T> int printf(const char*, T...) { return 0; } };
inline TestSerial Serial;
''')
            (root/'lvgl.h').write_text('''#pragma once
#include <cstdint>
#define LV_COLOR_FORMAT_RGB565A8 1
#define LV_IMAGE_HEADER_MAGIC 0x19
struct lv_image_header_t { uint32_t magic, cf, flags, w, h, stride, reserved_2; };
struct lv_image_dsc_t { lv_image_header_t header; uint32_t data_size; const uint8_t* data; };
extern "C" inline void lv_image_cache_drop(const void*) {}
''')
            (root/'esp_heap_caps.h').write_text('''#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void* heap_caps_malloc(size_t n, int) { return malloc(n); }
inline size_t freedAssetBuffers = 0;
inline void heap_caps_free(void* p) { if (p) ++freedAssetBuffers; free(p); }
''')
            keys = ','.join(json.dumps(key) for key in KEYS)
            (root/'main.cpp').write_text('''#include "sky_assets.h"
#include "esp_heap_caps.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
// This contract focuses on offline embedded asset resolution. The actual
// persistent-cache implementation is compiled and exercised by its dedicated
// firmware-facing fixture suite.
namespace leopold::asset_cache {
const AssetSpec* findSpec(const char* file) {
 static const AssetSpec specs[] = {
  {"background-flower", "background-flower-466.bin", 466u*466u*3u, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 466, 466},
  {"background-chevron", "background-chevron-466.bin", 466u*466u*3u, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 466, 466},
  {"background-mechanical-flower", "background-mechanical-flower-466.bin", 466u*466u*3u, "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", 466, 466},
 };
 for (const auto& spec : specs) if (strcmp(spec.file,file)==0) return &spec;
 return nullptr;
}
bool select(const AssetSpec& spec, Pin, uint8_t** data, size_t* length) {
 *data=static_cast<uint8_t*>(malloc(spec.bytes)); if (!*data) return false;
 memset(*data, 0x5a, spec.bytes); *length=spec.bytes; return true;
}
}
int main() {
 if (!leopold::sky::loadEmbeddedHomeAsset() || !leopold::sky::loadEmbeddedSettingsAsset()) return 1;
 const char* keys[] = {KEYS};
 for (const char* key : keys) {
  auto* image = leopold::sky::resolveAsset(key);
  if (!image || !image->data || image->header.cf != LV_COLOR_FORMAT_RGB565A8) return 2;
  if (image->data_size != image->header.w * image->header.h * 3) return 3;
  uint64_t hash = 14695981039346656037ULL;
  for (uint32_t i=0;i<image->data_size;++i) hash=(hash^image->data[i])*1099511628211ULL;
  printf("%s %u %016llx\\n", key, image->data_size, static_cast<unsigned long long>(hash));
 }
 auto* image = leopold::sky::resolveAsset("background-flower");
 const uint8_t* original = image->data;
 if (leopold::sky::loadCloudAsset("background-flower-466.bin", original, 1)) return 4;
 if (image->data != original) return 5;
 // A cloud refresh remains authoritative over the embedded fallback.
 if (!leopold::sky::loadCloudAsset("background-flower-466.bin", original, image->data_size)) return 6;
 if (!leopold::sky::resolveAsset("settings-title-gear") || !leopold::sky::resolveAsset("background-preview-black-white-circle")) return 7;
 // Repeated selection exercises descriptor replacement and ownership cleanup;
 // an earlier four-buffer retention limit could not survive this sequence.
 const size_t freedBeforeSelections = freedAssetBuffers;
 const char* selections[] = {"background-chevron-466.bin", "background-mechanical-flower-466.bin", "background-flower-466.bin", "background-chevron-466.bin", "background-mechanical-flower-466.bin", "background-flower-466.bin", "background-chevron-466.bin", "background-mechanical-flower-466.bin"};
 for (const char* file : selections) if (!leopold::sky::loadCachedAsset(file, leopold::asset_cache::Pin::kGlobalWallpaper)) return 8;
 if (freedAssetBuffers - freedBeforeSelections < 6) return 10;
 auto* active = leopold::sky::resolveAsset("background-mechanical-flower");
 if (!active || !active->data || active->data[0] != 0x5a) return 9;
 return 0;
}
'''.replace('KEYS', keys))
            compiler = shutil.which('clang++') or shutil.which('g++')
            compiled = subprocess.run([compiler,'-std=c++20','-O1','-I'+str(root),'-I'+str(ROOT/'device/include'),str(ROOT/'device/src/sky_assets.cpp'),str(root/'main.cpp'),'-o',str(root/'loader')],capture_output=True,text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr[-8000:])
            result = subprocess.run([str(root/'loader')],check=True,capture_output=True,text=True)
            lines = [line.split() for line in result.stdout.splitlines()]
            self.assertEqual([line[0] for line in lines], list(KEYS))
            for key, size, checksum in lines:
                payload=(ROOT/'shared/assets'/manifest[key]['file']).read_bytes()
                self.assertEqual(int(size),len(payload))
                self.assertEqual(checksum,fnv64(payload),key)


if __name__ == '__main__':
    unittest.main()
