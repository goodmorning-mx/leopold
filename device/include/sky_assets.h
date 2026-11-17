#pragma once

#include <cstddef>
#include <lvgl.h>
#include "persistent_asset_cache.h"

namespace leopold::sky {

extern lv_image_dsc_t kFlowerBackground;
extern lv_image_dsc_t kChevronBackground;
extern lv_image_dsc_t kHomeBackground;
extern lv_image_dsc_t kBackgroundFlower;
extern lv_image_dsc_t kBackgroundChevron;
extern lv_image_dsc_t kBackgroundMechanicalFlower;
extern lv_image_dsc_t kBackgroundBlackWhiteCircle;
extern lv_image_dsc_t kClockIcon;
extern lv_image_dsc_t kBoltIcon;
extern lv_image_dsc_t kWeatherIcon;
extern lv_image_dsc_t kNavigationIcon;
extern lv_image_dsc_t kWhatsappIcon;
extern lv_image_dsc_t kSpotifyIcon;
extern lv_image_dsc_t kSpotifyLargeIcon;
extern lv_image_dsc_t kStatusCloudIcon;
extern lv_image_dsc_t kStatusBluetoothIcon;
extern lv_image_dsc_t kStatusWifiIcon;
extern lv_image_dsc_t kHomeUsbIcon;
extern lv_image_dsc_t kHomeGlassRim;
extern lv_image_dsc_t kSettingsNeutral;
extern lv_image_dsc_t kSettingsGearsWallpaper;
extern lv_image_dsc_t kSettingsStatusBluetoothActive;
extern lv_image_dsc_t kSettingsStatusWifiActive;
extern lv_image_dsc_t kSettingsStatusCloudActive;
extern lv_image_dsc_t kSettingsWifiPressed;
extern lv_image_dsc_t kSettingsBluetoothPressed;
extern lv_image_dsc_t kSettingsDisplayPressed;
extern lv_image_dsc_t kSettingsSoundPressed;
extern lv_image_dsc_t kSettingsWifiPressedIconMask;
extern lv_image_dsc_t kSettingsWifiPressedChevronMask;
extern lv_image_dsc_t kSettingsBluetoothPressedIconMask;
extern lv_image_dsc_t kSettingsBluetoothPressedChevronMask;
extern lv_image_dsc_t kSettingsDisplayPressedIconMask;
extern lv_image_dsc_t kSettingsDisplayPressedChevronMask;
extern lv_image_dsc_t kSettingsSoundPressedIconMask;
extern lv_image_dsc_t kSettingsSoundPressedChevronMask;

// Legacy SD loader retained for diagnostics. Normal boot loads canonical
// embedded Home/Settings fallbacks; cloud refresh uses the same manifest files.
bool loadSdAssets();
bool loadEmbeddedHomeAsset();
bool loadEmbeddedSettingsAsset();
bool loadCloudAsset(const char* file, const uint8_t* data, size_t bytes);
bool loadCachedAsset(const char* file, leopold::asset_cache::Pin pin);
const lv_image_dsc_t* resolveAsset(const char* id);

}  // namespace leopold::sky
