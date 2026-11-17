#include "sky_assets.h"
#include "generated_home_runtime_assets.h"
#include "generated_settings_asset.h"
#include "persistent_asset_cache.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>

extern "C" void lv_image_cache_drop(const void* src);

namespace {

constexpr lv_image_header_t header(uint16_t width, uint16_t height) {
  return {
      .magic = LV_IMAGE_HEADER_MAGIC,
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
lv_image_dsc_t kBackgroundFlower = {};
lv_image_dsc_t kBackgroundChevron = {};
lv_image_dsc_t kBackgroundMechanicalFlower = {};
lv_image_dsc_t kBackgroundBlackWhiteCircle = {};
lv_image_dsc_t kClockIcon = {};
lv_image_dsc_t kBoltIcon = {};
lv_image_dsc_t kWeatherIcon = {};
lv_image_dsc_t kNavigationIcon = {};
lv_image_dsc_t kWhatsappIcon = {};
lv_image_dsc_t kSpotifyIcon = {};
lv_image_dsc_t kSpotifyLargeIcon = {};
lv_image_dsc_t kHomeSettingsGear = {};
lv_image_dsc_t kStatusCloudIcon = {};
lv_image_dsc_t kStatusBluetoothIcon = {};
lv_image_dsc_t kStatusWifiIcon = {};
lv_image_dsc_t kHomeUsbIcon = {};
lv_image_dsc_t kHomeGlassRim = {};
lv_image_dsc_t kSettingsNeutral = {};
lv_image_dsc_t kSettingsGearsWallpaper = {};
lv_image_dsc_t kSettingsGears = {};
lv_image_dsc_t kSettingsGearsRainbow = {};
lv_image_dsc_t kBackgroundPreviewFlower = {};
lv_image_dsc_t kBackgroundPreviewChevron = {};
lv_image_dsc_t kBackgroundPreviewMechanical = {};
lv_image_dsc_t kBackgroundPreviewBlackWhiteCircle = {};
lv_image_dsc_t kClockBackgroundPreviewFlower = {};
lv_image_dsc_t kClockBackgroundPreviewChevron = {};
lv_image_dsc_t kClockBackgroundPreviewMechanical = {};
lv_image_dsc_t kClockFacePreview0 = {};
lv_image_dsc_t kClockFacePreview1 = {};
lv_image_dsc_t kClockFacePreview2 = {};
lv_image_dsc_t kDisplayBackgroundIcon = {};
lv_image_dsc_t kDisplayAppearanceIcon = {};
lv_image_dsc_t kDisplayClockIcon = {};
lv_image_dsc_t kDisplayUnitsIcon = {};
lv_image_dsc_t kDisplayLanguageIcon = {};
lv_image_dsc_t kDisplayWeatherLocationIcon = {};
lv_image_dsc_t kDisplayIcon = {};
lv_image_dsc_t kSettingsStatusBluetoothActive = {};
lv_image_dsc_t kSettingsStatusWifiActive = {};
lv_image_dsc_t kSettingsStatusCloudActive = {};
lv_image_dsc_t kSettingsWifiPressed = {};
lv_image_dsc_t kSettingsBluetoothPressed = {};
lv_image_dsc_t kSettingsDisplayPressed = {};
lv_image_dsc_t kSettingsSoundPressed = {};
lv_image_dsc_t kSettingsWifiPressedIconMask = {};
lv_image_dsc_t kSettingsWifiPressedChevronMask = {};
lv_image_dsc_t kSettingsBluetoothPressedIconMask = {};
lv_image_dsc_t kSettingsBluetoothPressedChevronMask = {};
lv_image_dsc_t kSettingsDisplayPressedIconMask = {};
lv_image_dsc_t kSettingsDisplayPressedChevronMask = {};
lv_image_dsc_t kSettingsSoundPressedIconMask = {};
lv_image_dsc_t kSettingsSoundPressedChevronMask = {};
lv_image_dsc_t kSettingsPressedBorderMask = {};
lv_image_dsc_t kSettingsTitleGear = {};
lv_image_dsc_t kSettingsRowWifi = {};
lv_image_dsc_t kSettingsRowBluetooth = {};
lv_image_dsc_t kSettingsRowDisplay = {};
lv_image_dsc_t kSettingsRowSound = {};

namespace {

AssetSlot assets[] = {
    {"flower", "sky_flower.bin", 466, 466, &kFlowerBackground},
    {"chevron", "sky_chevron.bin", 466, 466, &kChevronBackground},
    {"home", "home-controls-outward-10-466.bin", 466, 466, &kHomeBackground},
    {"background-flower", "background-flower-466.bin", 466, 466, &kBackgroundFlower},
    {"background-chevron", "background-chevron-466.bin", 466, 466, &kBackgroundChevron},
    {"background-mechanical-flower", "background-mechanical-flower-466.bin", 466, 466, &kBackgroundMechanicalFlower},
    {"background-black-white-circle", "background-black-white-circle-466.bin", 466, 466, &kBackgroundBlackWhiteCircle},
    {"open-clock", "icon_clock.bin", 80, 80, &kClockIcon},
    {"open-weather", "icon_weather.bin", 75, 75, &kWeatherIcon},
    {"open-navigation", "icon_nav.bin", 80, 80, &kNavigationIcon},
    {"open-whatsapp", "icon_whatsapp.bin", 72, 72, &kWhatsappIcon},
    {"open-spotify", "icon_spotify.bin", 84, 84, &kSpotifyIcon},
    {"home-settings-gear", "home-settings-gear-64.bin", 64, 64, &kHomeSettingsGear},
    {"status-cloud-icon", "status-cloud-icon.bin", 14, 14, &kStatusCloudIcon},
    {"status-bluetooth-icon", "status-bluetooth-icon.bin", 10, 14, &kStatusBluetoothIcon},
    {"status-wifi-icon", "status-wifi-icon.bin", 14, 14, &kStatusWifiIcon},
    {"home-usb-icon", "home-usb-icon.bin", 32, 48, &kHomeUsbIcon},
    {"home-glass-rim", "home-glass-rim-64.bin", 64, 64, &kHomeGlassRim},
    {"spotify-large", "icon_spotify_large.bin", 97, 100, &kSpotifyLargeIcon},
    {"settings-gears-base", "settings-glass-gears-v1-466.bin", 466, 466, &kSettingsGears},
    {"settings-title-gear", "settings-title-gear.bin", 40, 40, &kSettingsTitleGear},
    {"settings-row-wifi", "settings-row-wifi.bin", 40, 40, &kSettingsRowWifi},
    {"settings-row-bluetooth", "settings-row-bluetooth.bin", 40, 40, &kSettingsRowBluetooth},
    {"settings-row-display", "settings-row-display.bin", 40, 40, &kSettingsRowDisplay},
    {"settings-row-sound", "settings-row-sound.bin", 40, 40, &kSettingsRowSound},
    {"settings-gears-rainbow", "settings-gears-rainbow-466.bin", 466, 466, &kSettingsGearsRainbow},
    {"settings-gears-wallpaper", "settings-gears-wallpaper-466.bin", 466, 466, &kSettingsGearsWallpaper},
    {"display-icon-background", "display-icon-background.bin", 30, 30, &kDisplayBackgroundIcon},
    {"display-icon-appearance", "display-icon-appearance.bin", 32, 32, &kDisplayAppearanceIcon},
    {"display-icon-clock", "display-icon-clock.bin", 31, 31, &kDisplayClockIcon},
    {"display-icon-units", "display-icon-units.bin", 33, 33, &kDisplayUnitsIcon},
    {"display-icon-language", "display-icon-language.bin", 31, 31, &kDisplayLanguageIcon},
    {"display-icon-weather-location", "display-icon-weather-location.bin", 28, 31, &kDisplayWeatherLocationIcon},
    {"display-icon-display", "display-icon-display.bin", 30, 30, &kDisplayIcon},
    {"background-preview-flower", "background-preview-flower.bin", 88, 88, &kBackgroundPreviewFlower},
    {"background-preview-chevron", "background-preview-chevron.bin", 88, 88, &kBackgroundPreviewChevron},
    {"background-preview-mechanical", "background-preview-mechanical.bin", 88, 88, &kBackgroundPreviewMechanical},
    {"background-preview-black-white-circle", "background-preview-black-white-circle.bin", 88, 88, &kBackgroundPreviewBlackWhiteCircle},
    {"clock-background-preview-flower", "clock-background-preview-flower.bin", 88, 32, &kClockBackgroundPreviewFlower},
    {"clock-background-preview-chevron", "clock-background-preview-chevron.bin", 88, 32, &kClockBackgroundPreviewChevron},
    {"clock-background-preview-mechanical", "clock-background-preview-mechanical.bin", 88, 32, &kClockBackgroundPreviewMechanical},
    {"clock-face-preview-0", "clock-face-preview-0.bin", 46, 46, &kClockFacePreview0},
    {"clock-face-preview-1", "clock-face-preview-1.bin", 46, 46, &kClockFacePreview1},
    {"clock-face-preview-2", "clock-face-preview-2.bin", 46, 46, &kClockFacePreview2},
    {"settings-neutral", "settings-neutral-466.bin", 466, 466, &kSettingsNeutral},
    {"settings-status-bluetooth-active", "settings-glass-status-bluetooth-active-22x22-v1.bin", 22, 22, &kSettingsStatusBluetoothActive},
    {"settings-status-wifi-active", "settings-glass-status-wifi-active-22x22-v1.bin", 22, 22, &kSettingsStatusWifiActive},
    {"settings-status-cloud-active", "settings-glass-status-cloud-active-24x24-v1.bin", 24, 24, &kSettingsStatusCloudActive},
    {"settings-wifi-pressed", "settings-wifi-pressed-322x56.bin", 322, 56, &kSettingsWifiPressed},
    {"settings-bluetooth-pressed", "settings-bluetooth-pressed-322x56.bin", 322, 56, &kSettingsBluetoothPressed},
    {"settings-display-pressed", "settings-display-pressed-322x56.bin", 322, 56, &kSettingsDisplayPressed},
    {"settings-sound-pressed", "settings-sound-pressed-322x56.bin", 322, 56, &kSettingsSoundPressed},
    {"settings-wifi-pressed-icon-mask", "settings-rainbow-wifi-pressed-icon-mask.bin", 61, 56, &kSettingsWifiPressedIconMask},
    {"settings-wifi-pressed-chevron-mask", "settings-rainbow-wifi-pressed-chevron-mask.bin", 39, 56, &kSettingsWifiPressedChevronMask},
    {"settings-bluetooth-pressed-icon-mask", "settings-rainbow-bluetooth-pressed-icon-mask.bin", 61, 56, &kSettingsBluetoothPressedIconMask},
    {"settings-bluetooth-pressed-chevron-mask", "settings-rainbow-bluetooth-pressed-chevron-mask.bin", 39, 56, &kSettingsBluetoothPressedChevronMask},
    {"settings-display-pressed-icon-mask", "settings-rainbow-display-pressed-icon-mask.bin", 61, 56, &kSettingsDisplayPressedIconMask},
    {"settings-display-pressed-chevron-mask", "settings-rainbow-display-pressed-chevron-mask.bin", 39, 56, &kSettingsDisplayPressedChevronMask},
    {"settings-sound-pressed-icon-mask", "settings-rainbow-sound-pressed-icon-mask.bin", 61, 56, &kSettingsSoundPressedIconMask},
    {"settings-sound-pressed-chevron-mask", "settings-rainbow-sound-pressed-chevron-mask.bin", 39, 56, &kSettingsSoundPressedChevronMask},
    {"settings-pressed-border-mask", "settings-glass-pressed-border-mask-322x56-v1.bin", 322, 56, &kSettingsPressedBorderMask},
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
    if (asset.data != nullptr) {
      // LVGL caches decoded draw buffers by descriptor pointer. Invalidate the
      // cache on the renderer thread before the old bytes are released.
      lv_image_cache_drop(asset.descriptor);
      heap_caps_free(asset.data);
    }
    asset.data = replacement;
    asset.descriptor->header = header(asset.width, asset.height);
    asset.descriptor->data_size = expected;
    asset.descriptor->data = asset.data;
    return true;
  }
  return false;
}

bool loadCachedAsset(const char* file, asset_cache::Pin pin) {
  if (!file) return false;
  AssetSlot* target = nullptr;
  for (AssetSlot& asset : assets) if (strcmp(asset.file,file)==0) { target=&asset; break; }
  const asset_cache::AssetSpec* spec = asset_cache::findSpec(file);
  if (!target || !spec || target->width != spec->width || target->height != spec->height) return false;
  uint8_t* replacement = nullptr; size_t bytes = 0;
  if (!asset_cache::select(*spec,pin,&replacement,&bytes)) return false;
  // Cache selection has fully verified a separate replacement buffer. Invalidate
  // LVGL's decoded-image entry before swapping the descriptor, then release its
  // prior owned buffer. Asset activation runs on the same renderer thread.
  lv_image_cache_drop(target->descriptor);
  uint8_t* previous = target->data;
  target->data = replacement;
  target->descriptor->header = header(target->width,target->height);
  target->descriptor->data_size = bytes;
  target->descriptor->data = replacement;
  if (previous) heap_caps_free(previous);
  return true;
}

bool loadEmbeddedHomeAsset() {
  bool loaded = true;
  for (const EmbeddedHomeAsset& asset : kEmbeddedHomeAssets) {
    loaded = loadCloudAsset(asset.file, asset.data, asset.bytes) && loaded;
  }
  return loaded;
}

bool loadEmbeddedSettingsAsset() {
  // Settings and its submenus use the selected shared screen wallpaper.
  const bool bluetooth = loadCloudAsset("settings-glass-status-bluetooth-active-22x22-v1.bin", kSettingsStatusBluetoothActiveData, kSettingsStatusBluetoothActiveDataSize);
  const bool wifi = loadCloudAsset("settings-glass-status-wifi-active-22x22-v1.bin", kSettingsStatusWifiActiveData, kSettingsStatusWifiActiveDataSize);
  const bool cloud = loadCloudAsset("settings-glass-status-cloud-active-24x24-v1.bin", kSettingsStatusCloudActiveData, kSettingsStatusCloudActiveDataSize);
  const bool suppliedGlyphs =
      loadCloudAsset("settings-title-gear.bin", kSettingsTitleGearData, kSettingsTitleGearDataSize) &&
      loadCloudAsset("settings-row-wifi.bin", kSettingsRowWifiData, kSettingsRowWifiDataSize) &&
      loadCloudAsset("settings-row-bluetooth.bin", kSettingsRowBluetoothData, kSettingsRowBluetoothDataSize) &&
      loadCloudAsset("settings-row-display.bin", kSettingsRowDisplayData, kSettingsRowDisplayDataSize) &&
      loadCloudAsset("settings-row-sound.bin", kSettingsRowSoundData, kSettingsRowSoundDataSize);
  bool pressed = true;
  pressed = loadCloudAsset("settings-rainbow-wifi-pressed-icon-mask.bin", kSettingsWifiPressedIconMaskData, kSettingsWifiPressedIconMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-wifi-pressed-chevron-mask.bin", kSettingsWifiPressedChevronMaskData, kSettingsWifiPressedChevronMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-bluetooth-pressed-icon-mask.bin", kSettingsBluetoothPressedIconMaskData, kSettingsBluetoothPressedIconMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-bluetooth-pressed-chevron-mask.bin", kSettingsBluetoothPressedChevronMaskData, kSettingsBluetoothPressedChevronMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-display-pressed-icon-mask.bin", kSettingsDisplayPressedIconMaskData, kSettingsDisplayPressedIconMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-display-pressed-chevron-mask.bin", kSettingsDisplayPressedChevronMaskData, kSettingsDisplayPressedChevronMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-sound-pressed-icon-mask.bin", kSettingsSoundPressedIconMaskData, kSettingsSoundPressedIconMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-rainbow-sound-pressed-chevron-mask.bin", kSettingsSoundPressedChevronMaskData, kSettingsSoundPressedChevronMaskDataSize) && pressed;
  pressed = loadCloudAsset("settings-glass-pressed-border-mask-322x56-v1.bin", kSettingsPressedBorderMaskData, kSettingsPressedBorderMaskDataSize) && pressed;
  const bool display =
      loadCloudAsset("display-icon-background.bin", kDisplayBackgroundIconData, kDisplayBackgroundIconDataSize) &&
      loadCloudAsset("display-icon-appearance.bin", kDisplayAppearanceIconData, kDisplayAppearanceIconDataSize) &&
      loadCloudAsset("display-icon-clock.bin", kDisplayClockIconData, kDisplayClockIconDataSize) &&
      loadCloudAsset("display-icon-units.bin", kDisplayUnitsIconData, kDisplayUnitsIconDataSize) &&
      loadCloudAsset("display-icon-language.bin", kDisplayLanguageIconData, kDisplayLanguageIconDataSize) &&
      loadCloudAsset("display-icon-weather-location.bin", kDisplayWeatherLocationIconData, kDisplayWeatherLocationIconDataSize) &&
      loadCloudAsset("display-icon-display.bin", kDisplayIconData, kDisplayIconDataSize) &&
      loadCloudAsset("background-preview-flower.bin", kBackgroundPreviewFlowerData, kBackgroundPreviewFlowerDataSize) &&
      loadCloudAsset("background-preview-chevron.bin", kBackgroundPreviewChevronData, kBackgroundPreviewChevronDataSize) &&
      loadCloudAsset("background-preview-mechanical.bin", kBackgroundPreviewMechanicalData, kBackgroundPreviewMechanicalDataSize) &&
      loadCloudAsset("background-preview-black-white-circle.bin", kBackgroundPreviewBlackWhiteCircleData, kBackgroundPreviewBlackWhiteCircleDataSize) &&
      loadCloudAsset("clock-background-preview-flower.bin", kClockBackgroundPreviewFlowerData, kClockBackgroundPreviewFlowerDataSize) &&
      loadCloudAsset("clock-background-preview-chevron.bin", kClockBackgroundPreviewChevronData, kClockBackgroundPreviewChevronDataSize) &&
      loadCloudAsset("clock-background-preview-mechanical.bin", kClockBackgroundPreviewMechanicalData, kClockBackgroundPreviewMechanicalDataSize) &&
      loadCloudAsset("clock-face-preview-0.bin", kClockFacePreview0Data, kClockFacePreview0DataSize) &&
      loadCloudAsset("clock-face-preview-1.bin", kClockFacePreview1Data, kClockFacePreview1DataSize) &&
      loadCloudAsset("clock-face-preview-2.bin", kClockFacePreview2Data, kClockFacePreview2DataSize);
  return bluetooth && wifi && cloud && suppliedGlyphs && pressed && display;
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
