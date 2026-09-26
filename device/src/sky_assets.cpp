#include "sky_assets.h"

namespace {

extern "C" {
extern const uint8_t skyFlowerStart[] asm("_binary_assets_sky_flower_bin_start");
extern const uint8_t skyHomeStart[] asm("_binary_assets_sky_home_bin_start");
extern const uint8_t iconClockStart[] asm("_binary_assets_icon_clock_bin_start");
extern const uint8_t iconBoltStart[] asm("_binary_assets_icon_bolt_bin_start");
extern const uint8_t iconWeatherStart[] asm("_binary_assets_icon_weather_bin_start");
extern const uint8_t iconNavStart[] asm("_binary_assets_icon_nav_bin_start");
extern const uint8_t iconSpotifyStart[] asm("_binary_assets_icon_spotify_bin_start");
extern const uint8_t iconSpotifyLargeStart[] asm("_binary_assets_icon_spotify_large_bin_start");
}

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

}  // namespace

namespace leopold::sky {

const lv_image_dsc_t kFlowerBackground = {
    .header = header(466, 466), .data_size = 651468, .data = skyFlowerStart};
const lv_image_dsc_t kHomeBackground = {
    .header = header(466, 466), .data_size = 651468, .data = skyHomeStart};
const lv_image_dsc_t kClockIcon = {
    .header = header(80, 80), .data_size = 19200, .data = iconClockStart};
const lv_image_dsc_t kBoltIcon = {
    .header = header(80, 80), .data_size = 19200, .data = iconBoltStart};
const lv_image_dsc_t kWeatherIcon = {
    .header = header(75, 75), .data_size = 16875, .data = iconWeatherStart};
const lv_image_dsc_t kNavigationIcon = {
    .header = header(80, 80), .data_size = 19200, .data = iconNavStart};
const lv_image_dsc_t kSpotifyIcon = {
    .header = header(77, 80), .data_size = 18480, .data = iconSpotifyStart};
const lv_image_dsc_t kSpotifyLargeIcon = {
    .header = header(97, 100), .data_size = 29100, .data = iconSpotifyLargeStart};

}  // namespace leopold::sky
