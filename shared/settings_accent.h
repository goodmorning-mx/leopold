#pragma once
#include <stdint.h>

namespace leopold {
// The selection overlays contain neutral replacement pixels as well as cyan
// light. Preserve the neutral backing and white highlights; map only the
// cyan contribution to the selected palette, retaining its intensity.
inline uint16_t tintSettingsAccentPixel(uint16_t pixel, uint16_t accent) {
  const int r = ((pixel >> 11) & 31) * 255 / 31;
  const int g = ((pixel >> 5) & 63) * 255 / 63;
  const int b = (pixel & 31) * 255 / 31;
  // Rainbow wallpaper is capped at 58% (148/255). Its pixels are backing,
  // not palette highlights. Reference cyan light is brighter than this cap.
  if (b <= 160 || g <= 80 || g <= r + 12 || b <= r + 16) return pixel;
  const int strength = b - r;
  const int ar = ((accent >> 11) & 31) * 255 / 31;
  const int ag = ((accent >> 5) & 63) * 255 / 63;
  const int ab = (accent & 31) * 255 / 31;
  const int red = r + ar * strength / 255;
  const int green = r + ag * strength / 255;
  const int blue = r + ab * strength / 255;
  return static_cast<uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}
}
