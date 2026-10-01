#include "sky_assets.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>

namespace {

constexpr lv_image_header_t header(uint16_t width, uint16_t height) {
  return {
      .cf = LV_COLOR_FORMAT_RGB565A8,
      .flags = 0,
      .w = width,
      .h = height,
      .stride = static_cast<uint16_t>(width * 2),
      .reserved_2 = 0,
  };
}

struct AssetSlot {
  const char* id;
  const char* file;
  uint16_t width;
  uint16_t height;
  lv_image_dsc_t* descriptor;
  uint8_t* data = nullptr;
};

}  // namespace

namespace leopold::sky {

lv_image_dsc_t kFlowerBackground = {};
lv_image_dsc_t kChevronBackground = {};
lv_image_dsc_t kHomeBackground = {};
lv_image_dsc_t kClockIcon = {};
lv_image_dsc_t kBoltIcon = {};
lv_image_dsc_t kWeatherIcon = {};
lv_image_dsc_t kNavigationIcon = {};
lv_image_dsc_t kWhatsappIcon = {};
lv_image_dsc_t kSpotifyIcon = {};
lv_image_dsc_t kSpotifyLargeIcon = {};

namespace {

AssetSlot assets[] = {
    {"flower", "sky_flower.bin", 466, 466, &kFlowerBackground},
    {"chevron", "sky_chevron.bin", 466, 466, &kChevronBackground},
    {"home", "sky_home.bin", 466, 466, &kHomeBackground},
    {"open-clock", "icon_clock.bin", 80, 80, &kClockIcon},
    {"open-weather", "icon_weather.bin", 75, 75, &kWeatherIcon},
    {"open-navigation", "icon_nav.bin", 80, 80, &kNavigationIcon},
    {"open-whatsapp", "icon_whatsapp.bin", 72, 72, &kWhatsappIcon},
    {"open-spotify", "icon_spotify.bin", 84, 84, &kSpotifyIcon},
    {"spotify-large", "icon_spotify_large.bin", 97, 100, &kSpotifyLargeIcon},
};

}  // namespace

bool loadSdAssets() {
  bool allLoaded = true;
  for (AssetSlot& asset : assets) {
    char path[64] = {};
    snprintf(path, sizeof(path), "/sd/assets/%s", asset.file);
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
      Serial.printf("SD asset missing: %s\n", path);
      allLoaded = false;
      continue;
    }
    const size_t expected = static_cast<size_t>(asset.width) * asset.height * 3;
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    rewind(file);
    if (size != static_cast<long>(expected)) {
      Serial.printf("SD asset size mismatch: %s (%ld/%u)\n", path, size, static_cast<unsigned>(expected));
      fclose(file);
      allLoaded = false;
      continue;
    }
    asset.data = static_cast<uint8_t*>(heap_caps_malloc(expected, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (asset.data == nullptr) {
      asset.data = static_cast<uint8_t*>(heap_caps_malloc(expected, MALLOC_CAP_8BIT));
    }
    if (asset.data == nullptr || fread(asset.data, 1, expected, file) != expected) {
      Serial.printf("SD asset load failed: %s\n", path);
      if (asset.data != nullptr) heap_caps_free(asset.data);
      asset.data = nullptr;
      fclose(file);
      allLoaded = false;
      continue;
    }
    fclose(file);
    asset.descriptor->header = header(asset.width, asset.height);
    asset.descriptor->data_size = expected;
    asset.descriptor->data = asset.data;
  }
  return allLoaded;
}

bool loadCloudAsset(const char* file, const uint8_t* data, size_t bytes) {
  if (file == nullptr || data == nullptr) return false;
  for (AssetSlot& asset : assets) {
    const size_t expected = static_cast<size_t>(asset.width) * asset.height * 3;
    if (strcmp(asset.file, file) != 0 || bytes != expected) continue;
    uint8_t* replacement = static_cast<uint8_t*>(heap_caps_malloc(
        expected, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (replacement == nullptr) return false;
    memcpy(replacement, data, expected);
    if (asset.data != nullptr) heap_caps_free(asset.data);
    asset.data = replacement;
    asset.descriptor->header = header(asset.width, asset.height);
    asset.descriptor->data_size = expected;
    asset.descriptor->data = asset.data;
    return true;
  }
  return false;
}

bool allAssetsLoaded() {
  for (const AssetSlot& asset : assets) {
    if (asset.data == nullptr) return false;
  }
  return true;
}

const char* assetFile(uint8_t index) {
  return index < sizeof(assets) / sizeof(assets[0]) ? assets[index].file : nullptr;
}

uint8_t assetCount() {
  return static_cast<uint8_t>(sizeof(assets) / sizeof(assets[0]));
}

const lv_image_dsc_t* resolveAsset(const char* id) {
  if (id == nullptr) return nullptr;
  if (strcmp(id, "open-voltage") == 0) return nullptr;
  for (const AssetSlot& asset : assets) {
    if (strcmp(asset.id, id) == 0 && asset.data != nullptr) return asset.descriptor;
  }
  return nullptr;
}

}  // namespace leopold::sky
