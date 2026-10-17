#pragma once

#include <lvgl.h>

namespace leopold::sky {

extern lv_image_dsc_t kFlowerBackground;
extern lv_image_dsc_t kChevronBackground;
extern lv_image_dsc_t kHomeBackground;
extern lv_image_dsc_t kClockIcon;
extern lv_image_dsc_t kBoltIcon;
extern lv_image_dsc_t kWeatherIcon;
extern lv_image_dsc_t kNavigationIcon;
extern lv_image_dsc_t kWhatsappIcon;
extern lv_image_dsc_t kSpotifyIcon;
extern lv_image_dsc_t kSpotifyLargeIcon;
extern lv_image_dsc_t kSettingsNeutral;
extern lv_image_dsc_t kSettingsWifiPressed;
extern lv_image_dsc_t kSettingsBluetoothPressed;
extern lv_image_dsc_t kSettingsDisplayPressed;
extern lv_image_dsc_t kSettingsSoundPressed;

// Legacy SD loader retained for diagnostics; normal boot loads every asset
// from GoodMorning Cloud so the device cannot render stale local bytes.
bool loadSdAssets();
bool loadEmbeddedHomeAsset();
bool loadEmbeddedSettingsAsset();
bool loadCloudAsset(const char* file, const uint8_t* data, size_t bytes);
bool allAssetsLoaded();
const char* assetFile(uint8_t index);
uint8_t assetCount();
const lv_image_dsc_t* resolveAsset(const char* id);

}  // namespace leopold::sky
