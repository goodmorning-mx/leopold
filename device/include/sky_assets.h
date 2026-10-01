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

// Loads canonical RGB565A8 assets from /sd/assets. Missing assets remain
// unavailable so the renderer can fail safely instead of using stale bytes.
bool loadSdAssets();
bool loadCloudAsset(const char* file, const uint8_t* data, size_t bytes);
bool allAssetsLoaded();
const char* assetFile(uint8_t index);
uint8_t assetCount();
const lv_image_dsc_t* resolveAsset(const char* id);

}  // namespace leopold::sky
