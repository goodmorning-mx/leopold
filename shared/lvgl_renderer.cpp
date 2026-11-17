#include "lvgl_renderer.h"
#include "settings_accent.h"
#include "settings-main-icons/settings_icon_data.h"
#include "settings-main-icons/settings_display_data.h"
#include <algorithm>
#include "display_menu.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cstring>

#if __has_include("generated_firmware_ui.h")
#include "generated_firmware_ui.h"
#else
namespace leopold::generated_ui {
constexpr int16_t kSettingsStatusX = 137;
constexpr int16_t kSettingsStatusY = 39;
constexpr int16_t kSettingsStatusWidth = 195;
constexpr int16_t kSettingsStatusHeight = 34;
constexpr int16_t kSettingsStatusRadius = 17;
constexpr int16_t kSettingsMenuX = 72;
constexpr int16_t kSettingsMenuY = 127;
constexpr int16_t kSettingsMenuWidth = 322;
constexpr int16_t kSettingsMenuHeight = 216;
constexpr int16_t kSharedStatusBannerX = 143;
constexpr int16_t kSharedStatusBannerY = 43;
constexpr int16_t kSharedStatusBluetoothX = 153;
constexpr int16_t kSharedStatusBluetoothY = 49;
constexpr int16_t kSharedStatusWifiX = 181;
constexpr int16_t kSharedStatusWifiY = 49;
constexpr int16_t kSharedStatusCloudX = 264;
constexpr int16_t kSharedStatusCloudY = 48;
constexpr int16_t kSharedStatusTimeX = 217;
constexpr int16_t kSharedStatusTimeY = 50;
constexpr uint16_t kSharedStatusTimeColor = 0xF7DE;
constexpr int16_t kSharedStatusBatteryX = 291;
constexpr int16_t kSharedStatusBatteryY = 54;
constexpr uint16_t kSharedStatusBatteryColor = 0xDF5E;
constexpr int16_t kSettingsCloudX = 277;
constexpr int16_t kSettingsCloudY = 48;
constexpr int16_t kSettingsCloudGlowBlur = 4;
constexpr int16_t kSettingsBubbleXOffset = 2;
constexpr int16_t kSettingsBubbleYOffset = -1;
constexpr int16_t kSettingsBubbleDiameter = 50;
constexpr int16_t kSettingsBubbleRadius = 25;
constexpr uint32_t kSettingsBubbleInner = 0xEAF2FF;
constexpr uint32_t kSettingsBubbleOuter = 0x687993;
constexpr uint32_t kSettingsBubbleActiveInner = 0xC7E5FF;
constexpr uint32_t kSettingsBubbleActiveOuter = 0x0B5B9B;
constexpr int16_t kSettingsIconCenterXOffset = 27;
constexpr int16_t kSettingsIconCenterYOffset = 49;
constexpr float kSettingsIconScale = 1.15f;
constexpr uint32_t kSettingsIconColor = 0xFFFFFF;
constexpr uint32_t kSettingsIconActiveColor = 0x00D2FF;
constexpr uint32_t kSettingsIconActiveInnerGlow = 0x0084C8;
constexpr uint32_t kThemeAccentBright[] = {0xF04452, 0xF07832, 0xD5A800, 0x22A861, 0x00D2FF, 0x7771F0, 0xC46CE6};
constexpr uint32_t kThemeAccent[] = {0xD92D3A, 0xC65313, 0x8A6800, 0x16834A, 0x00739B, 0x514CC2, 0x9A45BD};
constexpr uint8_t kThemeAccentCount = 7;
constexpr int16_t kSettingsChevronXOffset = 293;
constexpr int16_t kSettingsChevronYOffset = 19;
constexpr int16_t kSettingsChevronMidX = 7;
constexpr int16_t kSettingsChevronMidY = 7;
constexpr int16_t kSettingsChevronEndY = 14;
constexpr int16_t kSettingsChevronStroke = 3;
constexpr int16_t kSettingsGearX = 166;
constexpr int16_t kSettingsGearY = 102;
constexpr int16_t kSettingsGearDiameter = 41;
constexpr int16_t kSettingsTitleX = 196;
constexpr int16_t kSettingsTitleY = 92;
constexpr int16_t kSettingsTitleFontSize = 25;
constexpr int16_t kSettingsRowX[] = {73, 73, 73, 73};
constexpr int16_t kSettingsRowY[] = {130, 184, 238, 292};
constexpr int16_t kSettingsRowWidth[] = {320, 320, 320, 320};
constexpr int16_t kSettingsRowHeight[] = {49, 49, 49, 49};
constexpr uint8_t kSettingsRowCount = 4;
constexpr int16_t kSettingsSliderX = 98;
constexpr int16_t kSettingsSliderY = 358;
constexpr int16_t kSettingsSliderWidth = 272;
constexpr int16_t kSettingsSliderHeight = 28;
constexpr int16_t kSettingsSliderRadius = 14;
constexpr int16_t kSettingsSliderKnobRadius = 17;
constexpr uint16_t kSettingsSliderTrack = 0x11E5;
constexpr uint16_t kSettingsSliderFill = 0xB6EB;
constexpr uint16_t kSettingsSliderFillStart = 0xC71F;
constexpr uint16_t kSettingsSliderFillEnd = 0x13FA;
constexpr uint16_t kSettingsSliderAccent = 0x357F;
constexpr int16_t kSettingsBrightnessLabelX = 233;
constexpr int16_t kSettingsBrightnessLabelY = 400;
constexpr int16_t kSettingsBrightnessLabelFontSize = 16;
constexpr uint16_t kHomeGlassFill = 0xE7BF;
constexpr uint8_t kHomeGlassFillOpacity = 16;
constexpr uint16_t kHomeGlassBorder = 0xF7BF;
constexpr uint8_t kHomeGlassBorderOpacity = 76;
constexpr uint16_t kHomeGlassLightFill = 0xBEDC;
constexpr uint8_t kHomeGlassLightFillOpacity = 7;
constexpr uint16_t kHomeGlassLightBorder = 0x7473;
constexpr uint8_t kHomeGlassLightBorderOpacity = 30;
constexpr uint16_t kHomeGlassLightInset = 0xFFFF;
constexpr uint8_t kHomeGlassLightInsetOpacity = 18;
constexpr uint16_t kHomeLightNeutralIcon = 0x21A9;
constexpr uint8_t kHomeGlassBorderWidth = 1;
constexpr uint8_t kHomeGlassInsetOpacity = 20;
constexpr uint8_t kHomeGlassPressedShadowWidth = 3;
constexpr uint8_t kHomeGlassPressedShadowOpacity = 24;
constexpr uint8_t kHomeInnerHighlightOpacity = 65;
constexpr uint16_t kHomeGlassReflectionColor = 0xFFFF;
constexpr uint8_t kHomeGlassReflectionWidth = 2;
constexpr uint8_t kHomeGlassReflectionOpacity = 82;
}  // namespace leopold::generated_ui
#endif

namespace leopold {
namespace {

uint8_t gAccentIndex = 4;
bool gDarkAppearance = true;
uint8_t gHomeArt = 255;

lv_color_t accentBright() {
  const uint8_t index = gAccentIndex < generated_ui::kThemeAccentCount ? gAccentIndex : 4;
  return lv_color_hex(gDarkAppearance ? generated_ui::kThemeAccentBright[index] : generated_ui::kThemeAccent[index]);
}

bool isThemeAccentBright(uint16_t value) {
  for (uint8_t index = 0; index < generated_ui::kThemeAccentCount; ++index) {
    const uint32_t rgb = generated_ui::kThemeAccentBright[index];
    const uint16_t packed = static_cast<uint16_t>(((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F));
    if (value == packed) return true;
  }
  return false;
}

bool isThemeAccentToken(uint16_t value) {
  if (isThemeAccentBright(value)) return true;
  for (uint8_t index = 0; index < generated_ui::kThemeAccentCount; ++index) {
    const uint32_t rgb = generated_ui::kThemeAccent[index];
    const uint16_t packed = static_cast<uint16_t>(((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F));
    if (value == packed) return true;
  }
  return false;
}

lv_color_t rawColor565(uint16_t value) {
  const uint8_t red = (value >> 11) & 0x1F;
  const uint8_t green = (value >> 5) & 0x3F;
  const uint8_t blue = value & 0x1F;
  return lv_color_make((red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2));
}

lv_color_t color565(uint16_t value) {
  return isThemeAccentBright(value) ? accentBright() : rawColor565(value);
}

uint16_t selectedAccent565() {
  const uint8_t index = gAccentIndex < generated_ui::kThemeAccentCount ? gAccentIndex : 4;
  return generated_ui::kThemeAccent565[index];
}

uint16_t accentBright565() {
  const uint8_t index = gAccentIndex < generated_ui::kThemeAccentCount ? gAccentIndex : 4;
  const uint32_t rgb = generated_ui::kThemeAccentBright[index];
  const uint8_t red = static_cast<uint8_t>((rgb >> 16) & 0xFF);
  const uint8_t green = static_cast<uint8_t>((rgb >> 8) & 0xFF);
  const uint8_t blue = static_cast<uint8_t>(rgb & 0xFF);
  return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}

const lv_image_dsc_t* tintAccentMask(const lv_image_dsc_t* source, uint8_t slot) {
  if (source == nullptr || source->header.cf != LV_COLOR_FORMAT_RGB565A8 || slot >= 3) return source;
  struct Cache { lv_image_dsc_t descriptor = {}; uint8_t* bytes = nullptr; uint32_t size = 0; const lv_image_dsc_t* source = nullptr; uint16_t color = 0; };
  static Cache cache[3];
  Cache& item = cache[slot];
  const uint16_t accent = selectedAccent565();
  if (item.source == source && item.color == accent && item.bytes != nullptr) return &item.descriptor;
  if (item.size != source->data_size || item.bytes == nullptr) {
    item.bytes = static_cast<uint8_t*>(realloc(item.bytes, source->data_size));
    if (item.bytes == nullptr) { item.size = 0; item.source = nullptr; return source; }
    item.size = source->data_size;
  }
  memcpy(item.bytes, source->data, source->data_size);
  const uint32_t pixels = static_cast<uint32_t>(source->header.w) * source->header.h;
  for (uint32_t index = 0; index < pixels; ++index) {
    const uint16_t pixel = static_cast<uint16_t>(source->data[index * 2] | (source->data[index * 2 + 1] << 8));
    const uint16_t tinted = tintSettingsAccentPixel(pixel, accent);
    item.bytes[index * 2] = static_cast<uint8_t>(tinted & 0xFF);
    item.bytes[index * 2 + 1] = static_cast<uint8_t>(tinted >> 8);
  }
  item.descriptor = *source;
  item.descriptor.data = item.bytes;
  item.source = source;
  item.color = accent;
  return &item.descriptor;
}

uint16_t appearanceAccent565() {
  const uint8_t index = gAccentIndex < generated_ui::kThemeAccentCount ? gAccentIndex : 4;
  const uint32_t rgb = gDarkAppearance ? generated_ui::kThemeAccentBright[index] : generated_ui::kThemeAccent[index];
  const uint8_t red = static_cast<uint8_t>((rgb >> 16) & 0xFF);
  const uint8_t green = static_cast<uint8_t>((rgb >> 8) & 0xFF);
  const uint8_t blue = static_cast<uint8_t>(rgb & 0xFF);
  return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}

const lv_image_dsc_t* settingsMainGlyph(const char* id, bool useAppearanceAccent = true) {
  if (id == nullptr) return nullptr;
  const uint8_t accentIndex = gAccentIndex < generated_ui::kThemeAccentCount ? gAccentIndex : 4;
  const uint32_t accent = useAppearanceAccent
      ? (gDarkAppearance ? generated_ui::kThemeAccentBright[accentIndex] : generated_ui::kThemeAccent[accentIndex])
      : 0xFFFFFF;
  struct Cache { lv_image_dsc_t descriptor = {}; uint8_t* bytes = nullptr; uint32_t size = 0; const leopold_settings_icons::Material* material = nullptr; uint32_t accent = 0; bool appearanceTint = false; };
  static Cache cache[12];
  const leopold_settings_icons::Material* material = nullptr;
  if (strcmp(id, leopold_settings_icons::kSettingsDisplayMaterial.id) == 0) {
    material = &leopold_settings_icons::kSettingsDisplayMaterial;
  } else {
    for (size_t i = 0; i < leopold_settings_icons::kMaterialCount; ++i) {
      const auto& candidate = leopold_settings_icons::kMaterials[i];
      if (candidate.width == 40 && strcmp(candidate.id, id) == 0) { material = &candidate; break; }
    }
  }
  if (material == nullptr) return nullptr;
  size_t slot = 12;
  size_t emptySlot = 12;
  for (size_t index = 0; index < 12; ++index) {
    if (cache[index].material == material && cache[index].appearanceTint == useAppearanceAccent) {
      if (cache[index].accent == accent && cache[index].bytes != nullptr) return &cache[index].descriptor;
      slot = index;
      break;
    }
    if (cache[index].material == nullptr && emptySlot == 12) emptySlot = index;
  }
  if (slot == 12) slot = emptySlot;
  if (slot == 12) return nullptr;
  Cache& item = cache[slot];
  const uint32_t bytes = uint32_t(material->width) * material->height * 3u;
  // LVGL caches decoded image data by descriptor address. Drop the old decode
  // before changing pixels behind this stable descriptor pointer.
  if (item.material != nullptr && item.bytes != nullptr) lv_image_cache_drop(&item.descriptor);
  if (item.size != bytes || item.bytes == nullptr) {
    auto* allocation = static_cast<uint8_t*>(realloc(item.bytes, bytes));
    if (allocation == nullptr) return nullptr;
    item.bytes = allocation;
    item.size = bytes;
  }
  leopold_settings_icons::renderRgb565A8(*material,
      static_cast<uint8_t>(accent >> 16), static_cast<uint8_t>(accent >> 8), static_cast<uint8_t>(accent), item.bytes);
  item.descriptor = {};
  item.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  item.descriptor.header.cf = LV_COLOR_FORMAT_RGB565A8;
  item.descriptor.header.w = material->width;
  item.descriptor.header.h = material->height;
  item.descriptor.header.stride = static_cast<uint16_t>(material->width * 2);
  item.descriptor.data_size = bytes;
  item.descriptor.data = item.bytes;
  item.material = material;
  item.accent = accent;
  item.appearanceTint = useAppearanceAccent;
  return &item.descriptor;
}

const lv_image_dsc_t* tintHomeGlassRim(const lv_image_dsc_t* source) {
  if (source == nullptr || source->header.cf != LV_COLOR_FORMAT_RGB565A8) return source;
  const uint32_t pixels = static_cast<uint32_t>(source->header.w) * source->header.h;
  if (source->data_size < pixels * 3) return source;
  struct Cache { lv_image_dsc_t descriptor = {}; uint8_t* bytes = nullptr; uint32_t size = 0; const lv_image_dsc_t* source = nullptr; uint16_t accent = 0; };
  static Cache cache;
  const uint16_t accent = appearanceAccent565();
  if (cache.source == source && cache.accent == accent && cache.bytes != nullptr) return &cache.descriptor;
  if (cache.size != source->data_size || cache.bytes == nullptr) {
    cache.bytes = static_cast<uint8_t*>(realloc(cache.bytes, source->data_size));
    if (cache.bytes == nullptr) { cache.size = 0; cache.source = nullptr; return source; }
    cache.size = source->data_size;
  }
  memcpy(cache.bytes, source->data, source->data_size);
  for (uint32_t index = 0; index < pixels; ++index) {
    const uint16_t pixel = static_cast<uint16_t>(source->data[index * 2] | (source->data[index * 2 + 1] << 8));
    const uint16_t tinted = tintSettingsAccentPixel(pixel, accent);
    cache.bytes[index * 2] = static_cast<uint8_t>(tinted & 0xFF);
    cache.bytes[index * 2 + 1] = static_cast<uint8_t>(tinted >> 8);
  }
  cache.descriptor = *source;
  cache.descriptor.data = cache.bytes;
  cache.source = source;
  cache.accent = accent;
  return &cache.descriptor;
}

const lv_image_dsc_t* neutralHomeGlassRim(const lv_image_dsc_t* source) {
  if (source == nullptr || source->header.cf != LV_COLOR_FORMAT_RGB565A8) return source;
  const uint32_t pixels = static_cast<uint32_t>(source->header.w) * source->header.h;
  if (source->data_size < pixels * 3) return source;
  struct Cache { lv_image_dsc_t descriptor = {}; uint8_t* bytes = nullptr; uint32_t size = 0; const lv_image_dsc_t* source = nullptr; };
  static Cache cache;
  if (cache.source == source && cache.bytes != nullptr) return &cache.descriptor;
  if (cache.size != source->data_size || cache.bytes == nullptr) {
    cache.bytes = static_cast<uint8_t*>(realloc(cache.bytes, source->data_size));
    if (cache.bytes == nullptr) { cache.size = 0; cache.source = nullptr; return source; }
    cache.size = source->data_size;
  }
  memcpy(cache.bytes, source->data, source->data_size);
  for (uint32_t index = 0; index < pixels; ++index) {
    const uint16_t pixel = static_cast<uint16_t>(source->data[index * 2] | (source->data[index * 2 + 1] << 8));
    const uint8_t red = static_cast<uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
    const uint8_t green = static_cast<uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
    const uint8_t blue = static_cast<uint8_t>((pixel & 0x1F) * 255 / 31);
    const uint8_t luminance = static_cast<uint8_t>((red * 30u + green * 59u + blue * 11u + 50u) / 100u);
    const uint16_t neutral = static_cast<uint16_t>(((luminance & 0xF8) << 8) | ((luminance & 0xFC) << 3) | (luminance >> 3));
    cache.bytes[index * 2] = static_cast<uint8_t>(neutral & 0xFF);
    cache.bytes[index * 2 + 1] = static_cast<uint8_t>(neutral >> 8);
  }
  cache.descriptor = *source;
  cache.descriptor.data = cache.bytes;
  cache.source = source;
  return &cache.descriptor;
}

const lv_image_dsc_t* contrastHomeIcon(const lv_image_dsc_t* source) {
  if (gDarkAppearance || gHomeArt != 1 || source == nullptr || source->header.cf != LV_COLOR_FORMAT_RGB565A8) return source;
  const uint32_t pixels = static_cast<uint32_t>(source->header.w) * source->header.h;
  if (source->data_size < pixels * 3) return source;
  struct Cache { lv_image_dsc_t descriptor = {}; uint8_t* bytes = nullptr; uint32_t size = 0; const lv_image_dsc_t* source = nullptr; };
  static Cache cache[2];
  for (Cache& item : cache) {
    if (item.source == source && item.bytes != nullptr) return &item.descriptor;
    if (item.source != nullptr) continue;
    item.bytes = static_cast<uint8_t*>(realloc(item.bytes, source->data_size));
    if (item.bytes == nullptr) return source;
    item.size = source->data_size;
    memcpy(item.bytes, source->data, source->data_size);
    const uint16_t neutral = generated_ui::kHomeLightNeutralIcon;
    const uint8_t* alpha = source->data + pixels * 2;
    for (uint32_t index = 0; index < pixels; ++index) {
      if (alpha[index] == 0) continue;
      const uint16_t pixel = static_cast<uint16_t>(source->data[index * 2] | (source->data[index * 2 + 1] << 8));
      const int red = (pixel >> 11) & 0x1F;
      const int green = (pixel >> 5) & 0x3F;
      const int blue = pixel & 0x1F;
      if (red >= 20 && green >= 40 && blue >= 20 && std::abs(red * 2 - green) < 10 && std::abs(green - blue * 2) < 10) {
        item.bytes[index * 2] = static_cast<uint8_t>(neutral & 0xFF);
        item.bytes[index * 2 + 1] = static_cast<uint8_t>(neutral >> 8);
      }
    }
    item.descriptor = *source;
    item.descriptor.data = item.bytes;
    item.source = source;
    return &item.descriptor;
  }
  return source;
}

// Normalize visible artwork rather than scaling transparent source padding.
// The reference is the existing 32px Settings gear within the 72px Home bubble.
void placeHomeIcon(lv_obj_t* image, const lv_image_dsc_t* source, int width, int height) {
  int left = 0, top = 0, right = source->header.w - 1, bottom = source->header.h - 1;
  const uint32_t count = static_cast<uint32_t>(source->header.w) * source->header.h;
  if (source->header.cf == LV_COLOR_FORMAT_RGB565A8 && source->data_size >= count * 3) {
    const uint8_t* alpha = source->data + count * 2;
    left = source->header.w; top = source->header.h; right = bottom = -1;
    for (int y = 0; y < source->header.h; ++y) for (int x = 0; x < source->header.w; ++x) {
      if (alpha[y * source->header.w + x] <= 128) continue;
      if (x < left) left = x; if (x > right) right = x;
      if (y < top) top = y; if (y > bottom) bottom = y;
    }
    if (right < left) { left = top = 0; right = source->header.w - 1; bottom = source->header.h - 1; }
  }
  const int extent = std::max(right - left + 1, bottom - top + 1);
  const uint16_t scale = static_cast<uint16_t>(generated_ui::kHomeIconVisibleDiameter * 256 / std::max(1, extent));
  lv_image_set_pivot(image, 0, 0);
  lv_image_set_scale(image, scale);
  lv_image_set_antialias(image, true);
  lv_obj_set_pos(image, (width * 256 - (left + right + 1) * scale) / 512,
                       (height * 256 - (top + bottom + 1) * scale) / 512);
}

const lv_font_t* fontForSize(uint8_t size) {
  switch (size) {
    case 1: return &lv_font_montserrat_14;
    case 2: return &lv_font_montserrat_20;
    case 3: return &lv_font_montserrat_28;
    case 5: return &lv_font_montserrat_48;
    default: return &lv_font_montserrat_36;
  }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* value, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* object = lv_label_create(parent);
  lv_label_set_text(object, value == nullptr ? "" : value);
  lv_obj_set_style_text_font(object, font, 0);
  lv_obj_set_style_text_color(object, color, 0);
  lv_obj_set_style_text_opa(object, LV_OPA_COVER, 0);
  return object;
}

struct SpanishAccentMark { uint8_t index; bool tilde; };

uint8_t spanishAscii(const char* source, char* output, size_t capacity, SpanishAccentMark* marks, uint8_t markCapacity) {
  if (capacity == 0) return 0;
  size_t out = 0;
  uint8_t markCount = 0;
  for (size_t in = 0; source != nullptr && source[in] != '\0' && out + 1 < capacity;) {
    const uint8_t first = static_cast<uint8_t>(source[in]);
    if (first == 0xC3 && source[in + 1] != '\0') {
      const uint8_t second = static_cast<uint8_t>(source[in + 1]);
      char base = 0;
      bool tilde = false;
      switch (second) {
        case 0x81: base = 'A'; break; case 0x89: base = 'E'; break;
        case 0x8D: base = 'I'; break; case 0x93: base = 'O'; break;
        case 0x9A: base = 'U'; break; case 0x91: base = 'N'; tilde = true; break;
        case 0xA1: base = 'a'; break; case 0xA9: base = 'e'; break;
        case 0xAD: base = 'i'; break; case 0xB3: base = 'o'; break;
        case 0xBA: base = 'u'; break; case 0xB1: base = 'n'; tilde = true; break;
        default: break;
      }
      if (base != 0) {
        if (markCount < markCapacity) marks[markCount++] = {static_cast<uint8_t>(out), tilde};
        output[out++] = base;
        in += 2;
        continue;
      }
    }
    output[out++] = source[in++];
  }
  output[out] = '\0';
  return markCount;
}

void drawSpanishAccents(lv_obj_t* parent, const char* ascii, const SpanishAccentMark* marks,
    uint8_t count, const lv_font_t* font, int16_t startX, int16_t topY, lv_color_t color) {
  static lv_point_precise_t acute[] = {{0, 3}, {4, 0}};
  static lv_point_precise_t tilde[] = {{0, 1}, {2, 3}, {4, 1}};
  for (uint8_t mark = 0; mark < count; ++mark) {
    const uint8_t index = marks[mark].index;
    const int32_t prefix = lv_text_get_width(ascii, index, font, 0);
    const int32_t glyph = index < strlen(ascii) ? lv_font_get_glyph_width(font, static_cast<uint8_t>(ascii[index]), static_cast<uint8_t>(ascii[index + 1])) : 0;
    lv_obj_t* stroke = lv_line_create(parent);
    lv_line_set_points(stroke, marks[mark].tilde ? tilde : acute, marks[mark].tilde ? 3 : 2);
    lv_obj_set_pos(stroke, static_cast<int16_t>(startX + prefix + glyph / 2 - 2), topY + 1);
    lv_obj_set_size(stroke, 5, 4);
    lv_obj_set_style_line_color(stroke, color, 0);
    lv_obj_set_style_line_width(stroke, 1, 0);
    lv_obj_set_style_line_rounded(stroke, true, 0);
  }
}

lv_obj_t* makePanel(lv_obj_t* parent, int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius, lv_color_t color) {
  lv_obj_t* object = lv_obj_create(parent);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, width, height);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(object, radius, 0);
  lv_obj_set_style_bg_color(object, color, 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_pad_all(object, 0, 0);
  return object;
}

lv_obj_t* makeButton(lv_obj_t* parent, const SduiComponent& component, const char* pressedComponentId,
    bool accentEligible = true) {
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  lv_obj_t* object = makePanel(parent, component.x, component.y, component.width, component.height, component.height / 2, lv_color_hex(0x0D0F12));
  lv_obj_set_style_border_width(object, 3, 0);
  lv_obj_set_style_border_color(object, accentEligible ? accentBright() : color565(component.color), 0);
  lv_obj_set_style_shadow_width(object, pressed ? 2 : 6, 0);
  lv_obj_set_style_shadow_opa(object, pressed ? LV_OPA_20 : LV_OPA_40, 0);
  lv_obj_set_style_shadow_color(object, accentEligible ? accentBright() : color565(component.color), 0);
  lv_obj_t* inset = makePanel(object, 5, 5, component.width - 10, component.height - 10, (component.height - 10) / 2, pressed ? lv_color_hex(0x2C2C30) : lv_color_hex(0x171A1F));
  lv_obj_set_style_bg_opa(inset, LV_OPA_COVER, 0);
  return object;
}

lv_obj_t* makeHomeButton(lv_obj_t* parent, const SduiComponent& component, const char* pressedComponentId,
                         bool homeControl = false, bool fixedHomeGlass = false, bool neutralGlass = false) {
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  const bool homeGlassSurface = homeControl || fixedHomeGlass;
  const bool lightSurface = !gDarkAppearance && gHomeArt == 1;
  const lv_color_t glass = neutralGlass ? rawColor565(component.color) : accentBright();
  const lv_color_t insetColor = rawColor565(lightSurface ? generated_ui::kHomeGlassLightInset : 0xFFFF);
  const int16_t diameter = homeControl ? generated_ui::kHomeBubbleDiameter : component.width;
  const int16_t x = component.x + (component.width - diameter) / 2;
  const int16_t y = component.y + (component.height - diameter) / 2;
  lv_obj_t* object = makePanel(
      parent,
      x,
      y,
      diameter,
      diameter,
      diameter / 2,
      glass);
  constexpr uint8_t kHomeRestTintPercent = generated_ui::kHomeRestTintPercent;
  constexpr uint8_t kHomePressedTintPercent = generated_ui::kHomePressedTintPercent;
  const uint16_t borderColor = homeGlassSurface
      ? (lightSurface ? generated_ui::kHomeGlassLightBorder : generated_ui::kHomeGlassBorder)
      : 0;
  const uint8_t borderOpacity = lightSurface ? generated_ui::kHomeGlassLightBorderOpacity : generated_ui::kHomeGlassBorderOpacity;
  const uint8_t insetOpacity = lightSurface ? generated_ui::kHomeGlassLightInsetOpacity : generated_ui::kHomeGlassInsetOpacity;
  lv_obj_set_style_bg_opa(object, neutralGlass ? LV_OPA_COVER : static_cast<lv_opa_t>((pressed ? kHomePressedTintPercent : kHomeRestTintPercent) * 255 / 100), 0);
  lv_obj_set_style_border_width(object, homeGlassSurface ? 0 : generated_ui::kHomeGlassBorderWidth, 0);
  lv_obj_set_style_border_color(object, homeGlassSurface ? rawColor565(borderColor) : accentBright(), 0);
  lv_obj_set_style_border_opa(object, static_cast<lv_opa_t>(borderOpacity * 255 / 100), 0);
  lv_obj_set_style_shadow_width(object, homeControl ? 0 : (fixedHomeGlass
      ? (pressed ? generated_ui::kHomeGlassPressedShadowWidth : 0)
      : (pressed ? generated_ui::kHomeGlassPressedShadowWidth : generated_ui::kHomeGlassRestShadowWidth)), 0);
  lv_obj_set_style_shadow_opa(object, homeControl ? LV_OPA_TRANSP : static_cast<lv_opa_t>((fixedHomeGlass
      ? (pressed ? generated_ui::kHomeGlassPressedShadowOpacity : 0)
      : (pressed ? generated_ui::kHomeGlassPressedShadowOpacity : generated_ui::kHomeGlassRestShadowOpacity)) * 255 / 100), 0);
  lv_obj_set_style_shadow_color(object, homeGlassSurface ? rawColor565(generated_ui::kHomeGlassShadowColor) : accentBright(), 0);
  if (pressed && homeControl) {
    lv_obj_set_style_transform_scale(object, static_cast<int16_t>(generated_ui::kHomePressedScalePercent * 256 / 100), 0);
    lv_obj_set_style_transform_pivot_x(object, diameter / 2, 0);
    lv_obj_set_style_transform_pivot_y(object, diameter / 2, 0);
  }
  // A bubble has a clear center and broken specular reflections. A second
  // complete ring and stacked solid fills made this look like a metal disc.
  if (!homeGlassSurface) {
    lv_obj_t* sheen = makePanel(object, diameter * 14 / 100, diameter * 12 / 100, diameter * 72 / 100,
        diameter / 3, diameter / 6, insetColor);
    lv_obj_set_style_bg_opa(sheen, static_cast<lv_opa_t>(insetOpacity * 255 / (pressed ? 150 : 300)), 0);
    lv_obj_set_style_bg_grad_color(sheen, insetColor, 0);
    lv_obj_set_style_bg_grad_dir(sheen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_opa(sheen, LV_OPA_TRANSP, 0);

    lv_obj_t* reflection = lv_arc_create(object);
    lv_obj_set_pos(reflection, 3, 3);
    lv_obj_set_size(reflection, diameter - 6, diameter - 6);
    lv_arc_set_rotation(reflection, 0);
    lv_arc_set_bg_angles(reflection, 208, 294);
    lv_obj_remove_flag(reflection, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(reflection, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_arc_width(reflection, generated_ui::kHomeGlassReflectionWidth, LV_PART_MAIN);
    lv_obj_set_style_arc_color(reflection, rawColor565(generated_ui::kHomeGlassReflectionColor), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(reflection, static_cast<lv_opa_t>(pressed ? 255 : generated_ui::kHomeGlassReflectionOpacity * 255 / 100), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(reflection, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(reflection, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(reflection, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(reflection, 0, 0);
  }
  return object;
}

void drawUsb(lv_obj_t* parent, lv_color_t color) {
  static lv_point_precise_t segments[generated_ui::kUsbIconSegmentCount][2] = {};
  const int16_t width = lv_obj_get_width(parent);
  const int16_t height = lv_obj_get_height(parent);
  const float scaleX = static_cast<float>(width) / generated_ui::kUsbIconWidth;
  const float scaleY = static_cast<float>(height) / generated_ui::kUsbIconHeight;
  for (uint8_t index = 0; index < generated_ui::kUsbIconSegmentCount; ++index) {
    for (uint8_t point = 0; point < 2; ++point) {
      const int16_t sourceX = generated_ui::kUsbIconSegments[index][point * 2];
      const int16_t sourceY = generated_ui::kUsbIconSegments[index][point * 2 + 1];
      segments[index][point] = {
          static_cast<int16_t>(std::lround((sourceX - generated_ui::kUsbIconWidth / 2.0f) * scaleX + width / 2.0f)),
          static_cast<int16_t>(std::lround((sourceY - generated_ui::kUsbIconHeight / 2.0f) * scaleY + height / 2.0f)),
      };
    }
    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, segments[index], 2);
    lv_obj_set_size(line, width, height);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, generated_ui::kUsbIconStroke, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
}

void drawWhatsapp(lv_obj_t* parent, lv_color_t color) {
  // WhatsApp's mark is a speech bubble containing a handset. The button
  // supplies the outer green ring; this draws the recognizable inner mark.
  lv_obj_t* ring = lv_arc_create(parent);
  lv_obj_set_pos(ring, 18, 18);
  lv_obj_set_size(ring, 36, 36);
  lv_arc_set_rotation(ring, 0);
  lv_arc_set_bg_angles(ring, 0, 360);
  lv_arc_set_range(ring, 0, 100);
  lv_arc_set_value(ring, 100);
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(ring, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ring, color, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(ring, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_KNOB);

  static const lv_point_precise_t tail[] = {{22, 49}, {18, 54}, {29, 51}};
  lv_obj_t* tailLine = lv_line_create(parent);
  lv_line_set_points(tailLine, tail, 3);
  lv_obj_set_size(tailLine, 72, 72);
  lv_obj_set_style_line_color(tailLine, color, 0);
  lv_obj_set_style_line_width(tailLine, 3, 0);
  lv_obj_set_style_line_rounded(tailLine, true, 0);

  static const lv_point_precise_t handset[] = {
      {28, 28}, {25, 31}, {29, 38}, {36, 43}, {40, 40},
  };
  lv_obj_t* handsetLine = lv_line_create(parent);
  lv_line_set_points(handsetLine, handset, 5);
  lv_obj_set_size(handsetLine, 72, 72);
  lv_obj_set_style_line_color(handsetLine, color, 0);
  lv_obj_set_style_line_width(handsetLine, 5, 0);
  lv_obj_set_style_line_rounded(handsetLine, true, 0);
}

void drawX(lv_obj_t* parent, lv_color_t color, int16_t size = 32) {
  const int16_t inset = (72 - size) / 2;
  static lv_point_precise_t first[2];
  static lv_point_precise_t second[2];
  first[0] = {inset, inset};
  first[1] = {static_cast<int16_t>(inset + size), static_cast<int16_t>(inset + size)};
  second[0] = {static_cast<int16_t>(inset + size), inset};
  second[1] = {inset, static_cast<int16_t>(inset + size)};
  const lv_point_precise_t* strokes[] = {first, second};
  for (const lv_point_precise_t* points : strokes) {
    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, points, 2);
    lv_obj_set_size(line, 72, 72);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 4, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
}

void drawPairedCircles(lv_obj_t* root, int16_t x, int16_t y, lv_color_t color, lv_color_t glow) {
  const auto disc = [root, color, glow](int16_t left, int16_t top, int16_t size) {
    lv_obj_t* object = makePanel(root, left, top, size, size, size / 2, color);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(0xF3F6FA), 0);
    lv_obj_set_style_shadow_width(object, generated_ui::kSettingsCloudGlowBlur, 0);
    lv_obj_set_style_shadow_opa(object, LV_OPA_80, 0);
    lv_obj_set_style_shadow_color(object, glow, 0);
  };
  disc(x, y + 2, 14);
  disc(x + 8, y + 5, 12);
  lv_obj_t* dot = makePanel(root, x + 12, y, 4, 4, 2, color);
  LV_UNUSED(dot);
}

void drawGear(lv_obj_t* parent, lv_color_t color) {
  lv_obj_t* gear = makeLabel(parent, LV_SYMBOL_SETTINGS, &lv_font_montserrat_32, color);
  lv_obj_center(gear);
}

void drawHomeX(lv_obj_t* parent, lv_color_t color) {
  static lv_point_precise_t points[4][2];
  for (uint8_t index = 0; index < generated_ui::kHomeXIconSegmentCount; ++index) {
    const auto& segment = generated_ui::kHomeXIconSegments[index];
    points[index][0] = {static_cast<lv_coord_t>(segment[0] * generated_ui::kHomeBubbleDiameter / generated_ui::kHomeXIconWidth),
                        static_cast<lv_coord_t>(segment[1] * generated_ui::kHomeBubbleDiameter / generated_ui::kHomeXIconHeight)};
    points[index][1] = {static_cast<lv_coord_t>(segment[2] * generated_ui::kHomeBubbleDiameter / generated_ui::kHomeXIconWidth),
                        static_cast<lv_coord_t>(segment[3] * generated_ui::kHomeBubbleDiameter / generated_ui::kHomeXIconHeight)};
    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, points[index], 2);
    lv_obj_set_size(line, generated_ui::kHomeBubbleDiameter, generated_ui::kHomeBubbleDiameter);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, generated_ui::kHomeXIconStroke, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
}

void drawHomeGlassRim(lv_obj_t* parent, SharedImageResolver resolveImage,
                      int16_t diameter = generated_ui::kHomeBubbleDiameter, bool appearanceTint = true) {
  const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage("home-glass-rim");
  if (source == nullptr) return;
  lv_obj_t* rim = lv_image_create(parent);
  lv_image_set_src(rim, appearanceTint ? tintHomeGlassRim(source) : neutralHomeGlassRim(source));
  const uint16_t scale = static_cast<uint16_t>(diameter * 256 / std::max<uint16_t>(1, source->header.w));
  lv_image_set_pivot(rim, 0, 0);
  lv_image_set_scale(rim, scale);
  lv_image_set_antialias(rim, true);
  lv_obj_set_pos(rim,
      (diameter * 256 - source->header.w * scale) / 512,
      (diameter * 256 - source->header.h * scale) / 512);
  lv_obj_remove_flag(rim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(rim, LV_OBJ_FLAG_SCROLLABLE);
}

void drawChevron(lv_obj_t* parent, int16_t x, int16_t y, bool active) {
  static lv_point_precise_t points[] = {{0, 0}, {generated_ui::kSettingsChevronMidX, generated_ui::kSettingsChevronMidY}, {0, generated_ui::kSettingsChevronEndY}};
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 3);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_size(line, 10, 18);
  lv_obj_set_style_line_color(line, active ? accentBright() : lv_color_hex(0xD5E3FA), 0);
  lv_obj_set_style_line_width(line, generated_ui::kSettingsChevronStroke, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  if (active) {
    lv_obj_set_style_shadow_width(line, 2, 0);
    lv_obj_set_style_shadow_color(line, accentBright(), 0);
    lv_obj_set_style_shadow_opa(line, LV_OPA_80, 0);
  }
}

void drawSettingsIcon(lv_obj_t* parent, uint8_t index, int16_t centerX, int16_t centerY, bool active) {
  constexpr float pi = 3.14159265358979323846f;
  const lv_color_t color = active ? accentBright() : lv_color_hex(generated_ui::kSettingsIconColor);
  const auto line = [parent, color, active](const lv_point_precise_t* points, uint32_t count, int16_t width = 3) {
    lv_obj_t* object = lv_line_create(parent);
    lv_line_set_points(object, points, count);
    lv_obj_set_size(object, 466, 466);
    lv_obj_set_style_line_color(object, color, 0);
    lv_obj_set_style_line_width(object, static_cast<int16_t>(std::lround(width * generated_ui::kSettingsIconScale)), 0);
    lv_obj_set_style_line_rounded(object, true, 0);
  };
  const auto point = [centerX, centerY](float dx, float dy) -> lv_point_precise_t {
    return {static_cast<int16_t>(std::lround(centerX + dx * generated_ui::kSettingsIconScale)), static_cast<int16_t>(std::lround(centerY + dy * generated_ui::kSettingsIconScale))};
  };
  if (index == 0) {
    static lv_point_precise_t outer[13];
    static lv_point_precise_t middle[13];
    static lv_point_precise_t inner[13];
    for (int16_t i = 0; i < 13; ++i) {
      const float angle = 1.15f * pi + (0.70f * pi * i / 12.0f);
      outer[i] = point(std::cos(angle) * 15.0f, std::sin(angle) * 15.0f + 4.0f);
      middle[i] = point(std::cos(angle) * 10.0f, std::sin(angle) * 10.0f + 4.0f);
      inner[i] = point(std::cos(angle) * 5.0f, std::sin(angle) * 5.0f + 4.0f);
    }
    line(outer, 13, 3);
    line(middle, 13, 3);
    line(inner, 13, 3);
    const int16_t dotSize = static_cast<int16_t>(std::lround(4 * generated_ui::kSettingsIconScale));
    lv_obj_t* dot = makePanel(parent, static_cast<int16_t>(std::lround(centerX - dotSize / 2.0f)), static_cast<int16_t>(std::lround(centerY + 8 * generated_ui::kSettingsIconScale - dotSize / 2.0f)), dotSize, dotSize, dotSize / 2, color);
    LV_UNUSED(dot);
    return;
  }
  if (index == 1) {
    static lv_point_precise_t upper[3];
    static lv_point_precise_t lower[3];
    static lv_point_precise_t stem[2];
    upper[0] = point(0, -14); upper[1] = point(10, -5); upper[2] = point(-8, 5);
    lower[0] = point(-8, -5); lower[1] = point(10, 5); lower[2] = point(0, 14);
    stem[0] = point(0, -14); stem[1] = point(0, 14);
    line(upper, 3, 3); line(lower, 3, 3); line(stem, 2, 3);
    return;
  }
  if (index == 2) {
    static lv_point_precise_t screen[5];
    static lv_point_precise_t stand[2];
    static lv_point_precise_t base[2];
    screen[0] = point(-11, -8); screen[1] = point(11, -8); screen[2] = point(11, 7); screen[3] = point(-11, 7);
    screen[4] = screen[0];
    stand[0] = point(0, 7); stand[1] = point(0, 12); base[0] = point(-8, 12); base[1] = point(8, 12);
    line(screen, 5, 3); line(stand, 2, 3); line(base, 2, 3);
    return;
  }
  static lv_point_precise_t speaker[7];
  static lv_point_precise_t soundInner[13];
  static lv_point_precise_t soundOuter[13];
  speaker[0] = point(-12, -4); speaker[1] = point(-6, -4); speaker[2] = point(2, -11); speaker[3] = point(2, 11); speaker[4] = point(-6, 4); speaker[5] = point(-12, 4);
  speaker[6] = speaker[0];
  for (int16_t i = 0; i < 13; ++i) {
    const float angle = -0.9f + (1.8f * i / 12.0f);
    soundInner[i] = point(std::cos(angle) * 7.0f, std::sin(angle) * 7.0f);
    soundOuter[i] = point(std::cos(angle) * 12.5f, std::sin(angle) * 12.5f);
  }
  line(speaker, 7, 3); line(soundInner, 13, 3); line(soundOuter, 13, 3);
}

void drawSelectedSettingsRow(lv_obj_t* root, uint8_t index, SharedImageResolver resolveImage) {
  const int16_t y = static_cast<int16_t>(127 + index * 54);
  const lv_image_dsc_t* borderMask = resolveImage == nullptr ? nullptr : tintAccentMask(resolveImage("settings-pressed-border-mask"), 0);
  if (borderMask != nullptr) {
    lv_obj_t* border = lv_image_create(root);
    lv_image_set_src(border, borderMask);
    lv_obj_set_pos(border, 72, y);
    lv_obj_set_style_image_opa(border, static_cast<lv_opa_t>(generated_ui::kSettingsSelectedBorderOpacity * 255 / 100), 0);
  }
  // The neutral artwork bakes white glyphs into the icon and chevron. These
  // derived transparent masks contain only the approved changed pixels for
  // those two bounded components; the row body and label remain untouched.
  static const char* iconKeys[] = {
      "settings-wifi-pressed-icon-mask", "settings-bluetooth-pressed-icon-mask",
      "settings-display-pressed-icon-mask", "settings-sound-pressed-icon-mask"};
  static const char* chevronKeys[] = {
      "settings-wifi-pressed-chevron-mask", "settings-bluetooth-pressed-chevron-mask",
      "settings-display-pressed-chevron-mask", "settings-sound-pressed-chevron-mask"};
  const lv_image_dsc_t* iconMask = resolveImage == nullptr ? nullptr : tintAccentMask(resolveImage(iconKeys[index]), 1);
  const lv_image_dsc_t* chevronMask = resolveImage == nullptr ? nullptr : tintAccentMask(resolveImage(chevronKeys[index]), 2);
  if (iconMask != nullptr && chevronMask != nullptr) {
    lv_obj_t* icon = lv_image_create(root);
    lv_image_set_src(icon, iconMask);
    lv_obj_set_pos(icon, 72, y);
    lv_obj_t* chevron = lv_image_create(root);
    lv_image_set_src(chevron, chevronMask);
    lv_obj_set_pos(chevron, 355, y);
  } else if (borderMask == nullptr) {
    drawSettingsIcon(root, index, 100, static_cast<int16_t>(y + 28), true);
    drawChevron(root, 365, static_cast<int16_t>(y + 19), true);
  }
}

lv_obj_t* drawGlassCapsule(lv_obj_t* root, int16_t x, int16_t y, int16_t width,
    int16_t height, int16_t radius, uint16_t fill, uint8_t fillOpacity,
    uint16_t gradient, uint16_t border, uint8_t borderOpacity, uint8_t borderWidth,
    uint8_t shadowWidth, uint8_t shadowOpacity, uint16_t shadowColor,
    uint16_t innerRimColor, uint8_t innerRimOpacity, int16_t reflectionX,
    int16_t reflectionY, int16_t reflectionInset, int16_t reflectionHeight,
    uint8_t reflectionOpacity, bool accentBorder);

void drawSettingsGlassRows(lv_obj_t* root, const SduiScreen& screen, const char* selectedRowId, SharedImageResolver resolveImage) {
  static const char* rowIds[] = {
      "settings-wifi", "settings-bluetooth", "settings-display", "settings-accounts", "settings-sound"};
  static const char* glyphKeys[] = {"settings-main/wifi", "settings-main/bluetooth", "settings-main/display", "settings-main/remote", "settings-main/speaker"};
  lv_obj_t* viewport = lv_obj_create(root);
  lv_obj_remove_style_all(viewport);
  lv_obj_set_pos(viewport, generated_ui::kSettingsMenuX, generated_ui::kSettingsMenuY);
  lv_obj_set_size(viewport, generated_ui::kSettingsMenuWidth, generated_ui::kSettingsMenuHeight);
  lv_obj_remove_flag(viewport, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  for (uint8_t index = 0; index < generated_ui::kSettingsRowCount && index < 5; ++index) {
    const SduiComponent* component = nullptr;
    for (uint8_t componentIndex = 0; componentIndex < screen.componentCount; ++componentIndex) {
      if (strcmp(screen.components[componentIndex].id, rowIds[index]) == 0) {
        component = &screen.components[componentIndex];
        break;
      }
    }
    if (component == nullptr) continue;
    const int16_t x = static_cast<int16_t>(component->x - generated_ui::kSettingsMenuX + 1);
    const int16_t y = static_cast<int16_t>(component->y - generated_ui::kSettingsMenuY + 3);
    const int16_t width = generated_ui::kSettingsRowWidth[index];
    const int16_t height = generated_ui::kSettingsRowHeight[index];
    // Preserve the idle chevron treatment while held; the main Settings
    // glyph glow is the row's only press-selection feedback.
    const bool active = index == 0;
    const uint16_t rowFill = gDarkAppearance ? generated_ui::kSettingsGlassRowDarkFill : generated_ui::kSettingsGlassRowLightFill;
    const uint16_t rowGradient = gDarkAppearance ? generated_ui::kSettingsGlassRowDarkGradient : generated_ui::kSettingsGlassRowLightGradient;
    const uint16_t rowShadow = gDarkAppearance ? generated_ui::kSettingsGlassRowDarkShadow : generated_ui::kSettingsGlassRowLightShadow;
    drawGlassCapsule(viewport, x, y, width, height, height / 2,
        rowFill, generated_ui::kSettingsGlassRowFillOpacity,
        rowGradient, 0xFFFF, generated_ui::kSettingsGlassRowBorderOpacity,
        generated_ui::kSettingsGlassRowBorderWidth,
        generated_ui::kSettingsGlassRowRestShadowWidth,
        generated_ui::kSettingsGlassRowRestShadowOpacity,
        rowShadow, 0xFFFF, generated_ui::kSettingsGlassRowInnerRimOpacity,
        12, 3, generated_ui::kSettingsGlassRowReflectionInset,
        generated_ui::kSettingsGlassRowReflectionHeight, generated_ui::kSettingsGlassRowReflectionOpacity, true);

    SduiComponent bubble = {};
    bubble.type = ComponentType::kButton;
    strncpy(bubble.id, rowIds[index], sizeof(bubble.id) - 1);
    bubble.x = static_cast<int16_t>(x + 1);
    bubble.y = static_cast<int16_t>(y + (height - generated_ui::kSettingsGlassRowBubbleDiameter) / 2);
    bubble.width = generated_ui::kSettingsGlassRowBubbleDiameter;
    bubble.height = generated_ui::kSettingsGlassRowBubbleDiameter;
    bubble.color = rowFill;
    const bool rowPressed = selectedRowId != nullptr && strcmp(selectedRowId, rowIds[index]) == 0;
    lv_obj_t* iconBubble = makeHomeButton(viewport, bubble, nullptr, false, true, true);
    drawHomeGlassRim(iconBubble, resolveImage, generated_ui::kSettingsGlassRowBubbleDiameter, false);
    const int16_t centerX = static_cast<int16_t>(x + generated_ui::kSettingsIconCenterXOffset);
    const int16_t centerY = static_cast<int16_t>(y + height / 2);
    const lv_image_dsc_t* glyph = glyphKeys[index] == nullptr ? nullptr : settingsMainGlyph(glyphKeys[index], false);
    if (glyph != nullptr) {
      lv_obj_t* icon = lv_image_create(viewport);
      lv_image_set_src(icon, glyph);
      lv_obj_set_pos(icon, centerX - 20, centerY - 20);
      if (rowPressed) {
        const lv_image_dsc_t* glowGlyph = settingsMainGlyph(glyphKeys[index], true);
        if (glowGlyph != nullptr) {
          lv_obj_t* glow = lv_image_create(viewport);
          lv_image_set_src(glow, glowGlyph);
          lv_obj_set_pos(glow, centerX - 20, centerY - 20);
          lv_obj_set_style_image_opa(glow, static_cast<lv_opa_t>(255 * 80 / 100), 0);
          lv_obj_set_style_transform_pivot_x(glow, 20, 0);
          lv_obj_set_style_transform_pivot_y(glow, 20, 0);
          // Keep the accent halo inside the glyph circle so the glass rim
          // itself remains neutral in the held state.
          lv_obj_set_style_transform_scale(glow, 282, 0);
          // The accent copy sits directly under the white glyph. Its own
          // alpha silhouette supplies the glow, without a rectangular shadow.
          lv_obj_move_to_index(glow, lv_obj_get_index(icon));
        }
      }
    }

    lv_obj_t* label = makeLabel(viewport, generated_ui::kSettingsRowLabels[index], &lv_font_montserrat_20,
        gDarkAppearance ? lv_color_white() : lv_color_black());
    lv_obj_set_pos(label, x + 62, y + (height - 24) / 2);
    drawChevron(viewport, static_cast<int16_t>(x + width - 28), static_cast<int16_t>(y + (height - 18) / 2), active);
  }
}

const lv_font_t* statusFont(int16_t size) {
  if (size >= 24) return &lv_font_montserrat_24;
  if (size >= 20) return &lv_font_montserrat_20;
  if (size >= 18) return &lv_font_montserrat_18;
  if (size >= 16) return &lv_font_montserrat_16;
  if (size >= 14) return &lv_font_montserrat_14;
  return &lv_font_montserrat_12;
}

void drawStatusGlyph(lv_obj_t* root, SharedImageResolver resolveImage,
                     const char* key, int16_t x, int16_t y, lv_color_t color) {
  const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage(key);
  if (source == nullptr) return;
  lv_obj_t* image = lv_image_create(root);
  lv_image_set_src(image, source);
  lv_obj_set_pos(image, x, y);
  lv_obj_set_style_image_recolor(image, color, 0);
  lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
  lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0);
}

void drawConnectivityGroup(lv_obj_t* root, const SharedRenderState& state,
                           bool home, SharedImageResolver resolveImage) {
  const lv_color_t neutral = lv_color_hex(0xDFE8F4);
  const lv_color_t connected = lv_color_hex(0x0091FF);
  const int16_t x = home ? generated_ui::kHomeStatusCloudX : generated_ui::kSharedStatusCloudX;
  const int16_t y = home ? generated_ui::kHomeStatusIconY : generated_ui::kSharedStatusCloudY;
  const int16_t btX = home ? generated_ui::kHomeStatusBluetoothX : generated_ui::kSharedStatusBluetoothX;
  const int16_t btY = home ? generated_ui::kHomeStatusIconY : generated_ui::kSharedStatusBluetoothY;
  const int16_t wifiX = home ? generated_ui::kHomeStatusWifiX : generated_ui::kSharedStatusWifiX;
  const int16_t wifiY = home ? generated_ui::kHomeStatusIconY : generated_ui::kSharedStatusWifiY;
  drawStatusGlyph(root, resolveImage, "status-cloud-icon", x, y,
      state.cloudConnected ? connected : neutral);
  drawStatusGlyph(root, resolveImage, "status-bluetooth-icon", btX, btY,
      state.bluetoothConnected ? connected : neutral);
  drawStatusGlyph(root, resolveImage, "status-wifi-icon", wifiX, wifiY,
      state.wifiConnected ? connected : neutral);
}

void drawStatusTime(lv_obj_t* root, const char* value, int16_t x, int16_t y,
                    int16_t width, int16_t fontSize, lv_color_t color) {
  const char* text = value == nullptr || value[0] == '\0' ? "--:--" : value;
  int16_t size = fontSize;
  const lv_font_t* font = statusFont(size);
  while (size > 12 && lv_text_get_width(text, strlen(text), font, 0) > width - 4) {
    size = static_cast<int16_t>(size - 2);
    font = statusFont(size);
  }
  lv_obj_t* label = makeLabel(root, text, font, color);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, x, y);
}

void drawStatusBattery(lv_obj_t* root, const SharedRenderState& state,
                       int16_t iconX, int16_t iconY, int16_t iconWidth,
                       int16_t iconHeight, int16_t textX, int16_t textY,
                       int16_t textWidth, int16_t fontSize,
                       lv_color_t textColor) {
  const uint8_t percent = state.batteryPercent > 100 ? 100 : state.batteryPercent;
  const uint32_t value = percent > generated_ui::kBatteryGreenAbove ? generated_ui::kBatteryGreen :
      percent >= generated_ui::kBatteryYellowFrom ? generated_ui::kBatteryYellow : generated_ui::kBatteryRed;
  const lv_color_t batteryColor = lv_color_hex(value);
  lv_obj_t* outline = makePanel(root, iconX, iconY, iconWidth, iconHeight, 2, lv_color_black());
  lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(outline, 1, 0);
  lv_obj_set_style_border_color(outline, batteryColor, 0);
  lv_obj_t* tip = makePanel(root, iconX + iconWidth, iconY + iconHeight / 3, 2, iconHeight / 3, 1, batteryColor);
  const int16_t fillWidth = static_cast<int16_t>((iconWidth - 4) * percent / 100);
  lv_obj_t* fill = makePanel(root, iconX + 2, iconY + 2, fillWidth, iconHeight - 4, 1, batteryColor);
  LV_UNUSED(tip); LV_UNUSED(fill);
  char batteryText[8] = {};
  snprintf(batteryText, sizeof(batteryText), "%u%%", percent);
  lv_obj_t* label = makeLabel(root, batteryText, statusFont(fontSize), textColor);
  lv_obj_set_width(label, textWidth);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, textX, textY);
}

void drawStatusDividers(lv_obj_t* root, const int16_t* xValues,
                        uint8_t count, int16_t y, int16_t height, uint8_t opacity,
                        uint16_t color) {
  for (uint8_t i = 0; i < count; ++i) {
    lv_obj_t* line = makePanel(root, xValues[i], y, 1, height, 0, rawColor565(color));
    lv_obj_set_style_bg_opa(line, static_cast<lv_opa_t>(opacity * 255 / 100), 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
  }
}

void drawStatusContent(lv_obj_t* root, const SharedRenderState& state, bool home,
                       SharedImageResolver resolveImage) {
  const lv_color_t foreground = lv_color_hex(0xF4F7FC);
  drawConnectivityGroup(root, state, home, resolveImage);
  if (home) {
    drawStatusTime(root, state.timeText, generated_ui::kHomeStatusTimeX,
        generated_ui::kHomeStatusTimeY, generated_ui::kHomeStatusTimeWidth,
        generated_ui::kHomeStatusTimeFontSize, foreground);
    drawStatusBattery(root, state, generated_ui::kHomeStatusBatteryIconX,
        generated_ui::kHomeStatusBatteryIconY, generated_ui::kHomeStatusBatteryIconWidth,
        generated_ui::kHomeStatusBatteryIconHeight, generated_ui::kHomeStatusBatteryX,
        generated_ui::kHomeStatusBatteryY, generated_ui::kHomeStatusBatteryWidth,
        generated_ui::kHomeStatusBatteryFontSize, foreground);
    drawStatusDividers(root, generated_ui::kHomeStatusDividerX,
        2,
        generated_ui::kHomeStatusDividerY, generated_ui::kHomeStatusDividerHeight,
        generated_ui::kHomeStatusDividerOpacity, generated_ui::kHomeStatusDividerColor);
    return;
  }
  drawStatusTime(root, state.timeText, generated_ui::kSharedStatusTimeX,
      generated_ui::kSharedStatusTimeY, generated_ui::kSharedStatusTimeWidth,
      generated_ui::kSharedStatusTimeFontSize, rawColor565(generated_ui::kSharedStatusTimeColor));
  drawStatusBattery(root, state, generated_ui::kSharedStatusBatteryIconX,
      generated_ui::kSharedStatusBatteryIconY, generated_ui::kSharedStatusBatteryIconWidth,
      generated_ui::kSharedStatusBatteryIconHeight, generated_ui::kSharedStatusBatteryX,
      generated_ui::kSharedStatusBatteryY, generated_ui::kSharedStatusBatteryWidth,
      generated_ui::kSharedStatusBatteryFontSize, rawColor565(generated_ui::kSharedStatusBatteryColor));
  drawStatusDividers(root, generated_ui::kSharedStatusDividerX, generated_ui::kSharedStatusDividerCount,
      generated_ui::kSharedStatusDividerY,
      generated_ui::kSharedStatusDividerHeight, generated_ui::kSharedStatusDividerOpacity,
      generated_ui::kSharedStatusDividerColor);
}

lv_obj_t* drawGlassCapsule(lv_obj_t* root, int16_t x, int16_t y, int16_t width,
    int16_t height, int16_t radius, uint16_t fill, uint8_t fillOpacity,
    uint16_t gradient, uint16_t border, uint8_t borderOpacity, uint8_t borderWidth,
    uint8_t shadowWidth, uint8_t shadowOpacity, uint16_t shadowColor,
    uint16_t innerRimColor, uint8_t innerRimOpacity, int16_t reflectionX,
    int16_t reflectionY, int16_t reflectionInset, int16_t reflectionHeight,
    uint8_t reflectionOpacity, bool accentBorder = false) {
  lv_obj_t* surface = makePanel(root, x, y, width, height, radius, rawColor565(fill));
  lv_obj_set_style_bg_opa(surface, static_cast<lv_opa_t>(fillOpacity * 255 / 100), 0);
  lv_obj_set_style_bg_grad_color(surface, rawColor565(gradient), 0);
  lv_obj_set_style_bg_grad_dir(surface, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(surface, borderWidth, 0);
  lv_obj_set_style_border_color(surface, accentBorder ? accentBright() : rawColor565(border), 0);
  lv_obj_set_style_border_opa(surface, static_cast<lv_opa_t>(borderOpacity * 255 / 100), 0);
  lv_obj_set_style_shadow_width(surface, shadowWidth, 0);
  lv_obj_set_style_shadow_color(surface, rawColor565(shadowColor), 0);
  lv_obj_set_style_shadow_opa(surface, static_cast<lv_opa_t>(shadowOpacity * 255 / 100), 0);
  lv_obj_t* rim = makePanel(surface, 2, 2, width - 4, height - 4,
      radius > 2 ? radius - 2 : 0, lv_color_white());
  lv_obj_set_style_bg_opa(rim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rim, 1, 0);
  lv_obj_set_style_border_color(rim, rawColor565(innerRimColor), 0);
  lv_obj_set_style_border_opa(rim, static_cast<lv_opa_t>(innerRimOpacity * 255 / 100), 0);
  const int16_t reflectionWidth = width - reflectionInset * 2;
  if (reflectionWidth > 0 && reflectionHeight > 0) {
    lv_obj_t* reflection = makePanel(surface, reflectionX, reflectionY,
        reflectionWidth, reflectionHeight, reflectionHeight / 2, lv_color_white());
    lv_obj_set_style_bg_opa(reflection, static_cast<lv_opa_t>(reflectionOpacity * 255 / 100), 0);
    lv_obj_set_style_bg_grad_color(reflection, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(reflection, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_opa(reflection, LV_OPA_TRANSP, 0);
  }
  return surface;
}

void drawStatus(lv_obj_t* root, const SduiScreen& screen, const SharedRenderState& state,
                SharedImageResolver resolveImage) {
  if (strcmp(screen.screenId, "sky-home") != 0) return;
  const int16_t x = generated_ui::kHomePillX;
  const int16_t y = generated_ui::kHomePillY;
  const int16_t width = generated_ui::kHomePillWidth;
  const int16_t height = generated_ui::kHomePillHeight;
  drawGlassCapsule(root, x, y, width, height, generated_ui::kHomePillRadius,
      generated_ui::kHomePillFillColor, generated_ui::kHomePillFillOpacity,
      generated_ui::kHomePillGradientColor, generated_ui::kHomePillBorder,
      generated_ui::kHomePillBorderOpacity, generated_ui::kHomePillBorderWidth,
      generated_ui::kHomePillShadowWidth, generated_ui::kHomePillShadowOpacity,
      generated_ui::kHomePillShadowColor, generated_ui::kHomePillInnerRimColor,
      generated_ui::kHomePillInnerRimOpacity, generated_ui::kHomePillReflectionX,
      generated_ui::kHomePillReflectionY, generated_ui::kHomePillReflectionInset,
      generated_ui::kHomePillReflectionHeight, generated_ui::kHomePillReflectionOpacity, true);
  drawStatusContent(root, state, true, resolveImage);
}
const SduiComponent* findComponent(const SduiScreen& screen, const char* id);
const SduiComponent* findComponent(const SduiScreen& screen, const char* id) {
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    if (strcmp(screen.components[index].id, id) == 0) return &screen.components[index];
  }
  return nullptr;
}

const SduiComponent* findComponent(const SduiScreen& screen, const char* id);

struct ClockTimeParts { int hour; int minute; int second; bool pm; bool twelveHour; };
uint8_t gClockLinePointSet = 0;
lv_point_precise_t gClockLinePoints[128][2] = {};

ClockTimeParts clockTimeParts(const SharedRenderState& state) {
  ClockTimeParts parts{10, 9, static_cast<int>(state.clockSecond % 60), false, false};
  if (state.timeText == nullptr) return parts;
  const char* colon = strchr(state.timeText, ':');
  if (colon == nullptr) return parts;
  parts.hour = atoi(state.timeText);
  parts.minute = atoi(colon + 1);
  parts.second = static_cast<int>(state.clockSecond % 60);
  parts.pm = strstr(state.timeText, "PM") != nullptr || strstr(state.timeText, "pm") != nullptr;
  parts.twelveHour = strstr(state.timeText, "AM") != nullptr || strstr(state.timeText, "PM") != nullptr ||
                     strstr(state.timeText, "am") != nullptr || strstr(state.timeText, "pm") != nullptr;
  if (parts.twelveHour) {
    parts.hour %= 12;
    if (parts.pm) parts.hour += 12;
  }
  parts.hour %= 24;
  parts.minute %= 60;
  return parts;
}

void drawClockLine(lv_obj_t* root, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t width, lv_color_t color) {
  lv_point_precise_t* linePoints = gClockLinePoints[gClockLinePointSet++ % 128];
  linePoints[0] = {x1, y1};
  linePoints[1] = {x2, y2};
  lv_obj_t* line = lv_line_create(root);
  lv_line_set_points(line, linePoints, 2);
  lv_obj_set_size(line, 466, 466);
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

void drawClockDot(lv_obj_t* root, int16_t centerX, int16_t centerY, int16_t diameter, lv_color_t color) {
  lv_obj_t* dot = lv_obj_create(root);
  lv_obj_set_size(dot, diameter, diameter);
  lv_obj_set_pos(dot, centerX - diameter / 2, centerY - diameter / 2);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, color, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
}

void drawClockText(lv_obj_t* root, const char* text, int16_t y, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* label = makeLabel(root, text, font, color);
  lv_obj_set_width(label, 466);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, 0, y);
}

void drawSevenSegmentDigit(lv_obj_t* root, int digit, int16_t x, int16_t y, lv_color_t color) {
  static const uint8_t segments[10] = {
      0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
      0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011};
  static const int16_t ends[7][4] = {
      {4, 0, 26, 0}, {29, 3, 29, 25}, {29, 31, 29, 53}, {4, 56, 26, 56},
      {1, 31, 1, 53}, {1, 3, 1, 25}, {4, 28, 26, 28}};
  const int value = digit >= 0 && digit <= 9 ? digit : 0;
  for (int segment = 0; segment < 7; ++segment) {
    if ((segments[value] & (1 << (6 - segment))) == 0) continue;
    drawClockLine(root, x + ends[segment][0], y + ends[segment][1],
                  x + ends[segment][2], y + ends[segment][3], 5, color);
  }
}

void drawWatchFace(lv_obj_t* root, const SharedRenderState& state) {
  gClockLinePointSet = 0;
  const uint8_t face = state.clockFace > 4 ? 0 : state.clockFace;
  const ClockTimeParts time = clockTimeParts(state);
  const lv_color_t white = lv_color_hex(0xF4F3EB);
  const lv_color_t muted = lv_color_hex(0xA7A7A0);
  const lv_color_t red = lv_color_hex(0xFF3547);
  const lv_color_t gold = lv_color_hex(0xE2A923);
  char date[24] = {};
  snprintf(date, sizeof(date), "%s", state.dateText != nullptr && state.dateText[0] ? state.dateText : "OCT 26");
  for (char* p = date; *p; ++p) *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
  char battery[16] = {};
  snprintf(battery, sizeof(battery), "%u%%", state.batteryPercent);

  if (face == 3) {
    drawClockText(root, date, 106, &lv_font_montserrat_20, white);
    const char* timeText = state.timeText == nullptr ? "10:09" : state.timeText;
    char largeTime[16] = {};
    const char* suffix = strstr(timeText, " AM");
    if (suffix == nullptr) suffix = strstr(timeText, " PM");
    if (suffix != nullptr) snprintf(largeTime, sizeof(largeTime), "%.*s", static_cast<int>(suffix - timeText), timeText);
    else snprintf(largeTime, sizeof(largeTime), "%s", timeText);
    drawClockText(root, largeTime, 160, &lv_font_montserrat_48, white);
    if (suffix != nullptr) drawClockText(root, suffix + 1, 218, &lv_font_montserrat_14, muted);
    drawClockText(root, battery, 339, &lv_font_montserrat_24, white);
    return;
  }

  if (face == 4) {
    const int hour = time.twelveHour ? (time.hour % 12 == 0 ? 12 : time.hour % 12) : time.hour;
    const int digits[] = {hour / 10, hour % 10, time.minute / 10, time.minute % 10};
    const int16_t startX = 154;
    drawClockText(root, date, 119, &lv_font_montserrat_18, white);
    for (int index = 0; index < 4; ++index) {
      const int16_t x = static_cast<int16_t>(startX + (index < 2 ? index * 39 : 92 + (index - 2) * 39));
      drawSevenSegmentDigit(root, digits[index], x, 190, white);
    }
    drawClockDot(root, 233, 210, 6, white);
    drawClockDot(root, 233, 232, 6, white);
    drawClockText(root, battery, 339, &lv_font_montserrat_24, white);
    return;
  }

  constexpr int16_t centerX = 233;
  constexpr int16_t centerY = 235;
  const int16_t radius = face == 0 ? 153 : 174;
  if (face == 2) {
    lv_obj_t* bezel = lv_obj_create(root);
    lv_obj_set_size(bezel, 2 * radius + 12, 2 * radius + 12);
    lv_obj_set_pos(bezel, centerX - radius - 6, centerY - radius - 6);
    lv_obj_set_style_radius(bezel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(bezel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bezel, 2, 0);
    lv_obj_set_style_border_color(bezel, gold, 0);
    lv_obj_remove_flag(bezel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bezel, LV_OBJ_FLAG_SCROLLABLE);
  }
  if (face == 1) {
    for (int mark = 0; mark < 60; ++mark) {
      if (mark >= 27 && mark <= 33) continue;  // Keep the lower date/battery window clear.
      const double angle = (mark * 6.0 - 90.0) * 3.14159265358979323846 / 180.0;
      const bool hourMark = mark % 5 == 0;
      const int16_t inner = static_cast<int16_t>(radius - (hourMark ? 20 : 9));
      drawClockLine(root,
                    static_cast<int16_t>(centerX + std::cos(angle) * inner),
                    static_cast<int16_t>(centerY + std::sin(angle) * inner),
                    static_cast<int16_t>(centerX + std::cos(angle) * (radius - 2)),
                    static_cast<int16_t>(centerY + std::sin(angle) * (radius - 2)),
                    hourMark ? 5 : 2, white);
    }
  } else if (face == 2) {
    for (int mark = 0; mark < 60; ++mark) {
      if (mark >= 27 && mark <= 33) continue;
      const double angle = (mark * 6.0 - 90.0) * 3.14159265358979323846 / 180.0;
      const bool hourMark = mark % 5 == 0;
      drawClockDot(root,
                   static_cast<int16_t>(centerX + std::cos(angle) * (radius - 3)),
                   static_cast<int16_t>(centerY + std::sin(angle) * (radius - 3)), 4, gold);
      if (hourMark) {
        drawClockLine(root,
                      static_cast<int16_t>(centerX + std::cos(angle) * (radius - 21)),
                      static_cast<int16_t>(centerY + std::sin(angle) * (radius - 21)),
                      static_cast<int16_t>(centerX + std::cos(angle) * (radius - 7)),
                      static_cast<int16_t>(centerY + std::sin(angle) * (radius - 7)), 4, gold);
        drawClockDot(root,
                     static_cast<int16_t>(centerX + std::cos(angle) * (radius - 3)),
                     static_cast<int16_t>(centerY + std::sin(angle) * (radius - 3)), 8, gold);
      }
    }
  }
  const double minuteAngle = ((time.minute + time.second / 60.0) * 6.0 - 90.0) * 3.14159265358979323846 / 180.0;
  const double hourAngle = ((time.hour % 12 + time.minute / 60.0) * 30.0 - 90.0) * 3.14159265358979323846 / 180.0;
  const double secondAngle = (time.second * 6.0 - 90.0) * 3.14159265358979323846 / 180.0;
  const lv_color_t handColor = face == 2 ? gold : white;
  drawClockLine(root, centerX, centerY,
                static_cast<int16_t>(centerX + std::cos(hourAngle) * 73),
                static_cast<int16_t>(centerY + std::sin(hourAngle) * 73), 6, handColor);
  drawClockLine(root, centerX, centerY,
                static_cast<int16_t>(centerX + std::cos(minuteAngle) * 116),
                static_cast<int16_t>(centerY + std::sin(minuteAngle) * 116), 4, handColor);
  drawClockLine(root, centerX, centerY,
                static_cast<int16_t>(centerX + std::cos(secondAngle) * 128),
                static_cast<int16_t>(centerY + std::sin(secondAngle) * 128), 2, red);
  drawClockDot(root, centerX, centerY, 12, face == 2 ? gold : muted);
  drawClockText(root, date, 360, &lv_font_montserrat_16, white);
  drawClockText(root, battery, 383, &lv_font_montserrat_16, white);
}

void drawMenuChevron(lv_obj_t* parent, int16_t x, int16_t y, bool expanded) {
  lv_obj_t* chevron = makeLabel(parent, expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT, &lv_font_montserrat_16,
                                expanded ? accentBright() : lv_color_hex(0xD5E3FA));
  lv_obj_set_pos(chevron, x, y - 1);
}

const char* displayMenuIconKey(const char* componentId) {
  if (strstr(componentId, "background") != nullptr) return "display-icon-background";
  if (strstr(componentId, "appearance") != nullptr) return "display-icon-appearance";
  if (strstr(componentId, "clock") != nullptr) return "display-icon-clock";
  if (strstr(componentId, "units") != nullptr) return "display-icon-units";
  if (strstr(componentId, "language") != nullptr) return "display-icon-language";
  if (strstr(componentId, "weather-location") != nullptr) return "display-icon-weather-location";
  return "display-icon-display";
}

void drawSubmenuRow(lv_obj_t* viewport, const SduiComponent& component, const char* pressedComponentId, SharedImageResolver resolveImage) {
  const int16_t x = static_cast<int16_t>(component.x - display_menu::kViewportX);
  const int16_t y = static_cast<int16_t>(component.y - display_menu::kViewportY);
  const bool expanded = strcmp(component.icon, "expanded") == 0;
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  lv_obj_t* row = makePanel(viewport, x, y, component.width, component.height, component.height / 2, lv_color_hex(0x121721));
  lv_obj_set_style_bg_opa(row, LV_OPA_90, 0);
  lv_obj_set_style_border_width(row, expanded || pressed ? 2 : 1, 0);
  lv_obj_set_style_border_color(row, expanded || pressed ? accentBright() : lv_color_hex(0x2B3545), 0);
  if (expanded || pressed) {
    lv_obj_set_style_shadow_width(row, 8, 0);
    lv_obj_set_style_shadow_color(row, accentBright(), 0);
    lv_obj_set_style_shadow_opa(row, LV_OPA_50, 0);
  }
  const char* iconKey = displayMenuIconKey(component.id);
  const lv_image_dsc_t* iconSource = resolveImage == nullptr ? nullptr : resolveImage(iconKey);
  if (iconSource != nullptr) {
    const int16_t diameter = generated_ui::kDisplayMenuIconBubbleDiameter;
    lv_obj_t* bubble = makePanel(row, 0, (component.height - diameter) / 2, diameter, diameter, diameter / 2, lv_color_hex(0x252C36));
    lv_obj_set_style_bg_opa(bubble, LV_OPA_50, 0);
    lv_obj_set_style_border_width(bubble, 1, 0);
    lv_obj_set_style_border_color(bubble, accentBright(), 0);
    lv_obj_t* icon = lv_image_create(bubble);
    lv_image_set_src(icon, iconSource);
    if (expanded || pressed) {
      lv_obj_set_style_image_recolor(icon, accentBright(), 0);
      lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    }
    lv_obj_center(icon);
  }
  char labelText[97] = {};
  SpanishAccentMark marks[24] = {};
  const uint8_t accentCount = spanishAscii(component.text, labelText, sizeof(labelText), marks, 24);
  lv_obj_t* label = makeLabel(row, labelText, &lv_font_montserrat_20, lv_color_white());
  lv_obj_set_width(label, component.width - generated_ui::kDisplayMenuIconBubbleDiameter - 52);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(label, generated_ui::kDisplayMenuIconBubbleDiameter + 10, (component.height - 24) / 2);
  if (accentCount) drawSpanishAccents(row, labelText, marks, accentCount, &lv_font_montserrat_20,
      generated_ui::kDisplayMenuIconBubbleDiameter + 10, (component.height - 24) / 2, lv_color_white());
  drawMenuChevron(row, component.width - 28, (component.height - 16) / 2, expanded);
}

void drawSubmenuChoice(lv_obj_t* viewport, const SduiComponent& component, const char* pressedComponentId, SharedImageResolver resolveImage) {
  const int16_t x = static_cast<int16_t>(component.x - display_menu::kViewportX);
  const int16_t y = static_cast<int16_t>(component.y - display_menu::kViewportY);
  const bool accentChoice = strncmp(component.id, "display-accent-", 15) == 0;
  const bool selected = (strncmp(component.text, "[x] ", 4) == 0) || (!accentChoice && isThemeAccentToken(component.color));
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  const bool backgroundPreview = strncmp(component.icon, "bgprev-", 7) == 0;
  const bool clockBackgroundPreview = strncmp(component.icon, "clkbg-", 6) == 0;
  const bool clockFacePreview = strncmp(component.icon, "clkface-", 8) == 0;
  const bool postalConfirm = strcmp(component.id, "display-postal-apply") == 0;
  const int16_t radius = backgroundPreview || clockBackgroundPreview || clockFacePreview ? 12 : component.height > 35 ? component.height / 2 : 9;
  lv_obj_t* choice = makePanel(viewport, x, y, component.width, component.height, radius, lv_color_hex(component.id[0] == 'p' && strstr(component.id, "postal") != nullptr ? 0x171A1F : 0x121721));
  lv_obj_set_style_bg_opa(choice, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(choice, selected || pressed ? 2 : 1, 0);
  lv_obj_set_style_border_color(choice, accentBright(), 0);
  if (postalConfirm) {
    lv_obj_set_style_bg_color(choice, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_bg_opa(choice, LV_OPA_20, 0);
    lv_obj_set_style_border_width(choice, 1, 0);
    lv_obj_set_style_border_color(choice, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_shadow_width(choice, 6, 0);
    lv_obj_set_style_shadow_color(choice, lv_color_hex(0x34C759), 0);
    lv_obj_set_style_shadow_opa(choice, LV_OPA_40, 0);
  }
  if (backgroundPreview || clockBackgroundPreview || clockFacePreview) {
    const char* imageKey = nullptr;
    const char* indexText = strrchr(component.icon, '-') + 1;
    const int index = indexText[0] - '0';
    if (backgroundPreview || clockBackgroundPreview) {
      if (clockBackgroundPreview) {
        const char* keys[] = {"clock-background-preview-flower", "clock-background-preview-chevron", "clock-background-preview-mechanical"};
        if (index >= 0 && index < 3) imageKey = keys[index];
      } else {
      const char* keys[] = {"background-preview-flower", "background-preview-chevron", "background-preview-mechanical"};
      if (index >= 0 && index < 3) imageKey = keys[index];
      }
    } else {
      // The registered crops correspond to minimal, classic, and gold analog.
      const char* keys[] = {"clock-face-preview-2", "clock-face-preview-0", "clock-face-preview-1"};
      if (index >= 0 && index < 3) imageKey = keys[index];
    }
    const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage(imageKey);
    if (source != nullptr) {
      lv_obj_t* image = lv_image_create(choice);
      lv_image_set_src(image, source);
      lv_obj_align(image, LV_ALIGN_TOP_MID, 0, 2);
    }
    lv_obj_t* caption = makeLabel(choice, component.text, &lv_font_montserrat_12, lv_color_white());
    lv_obj_set_width(caption, component.width - 4);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, 0);
  } else {
    if (strncmp(component.id, "display-accent-", 15) == 0) {
      lv_obj_t* swatch = makePanel(choice, 8, (component.height - 12) / 2, 12, 12, 6, rawColor565(component.color));
      LV_UNUSED(swatch);
      lv_obj_t* label = makeLabel(choice, component.text, &lv_font_montserrat_12, lv_color_white());
      lv_obj_set_width(label, component.width - 25);
      lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
      lv_obj_set_pos(label, 22, (component.height - 16) / 2);
      return;
    }
    char labelText[97] = {};
    SpanishAccentMark marks[24] = {};
    const uint8_t accentCount = spanishAscii(component.text, labelText, sizeof(labelText), marks, 24);
    const lv_font_t* labelFont = &lv_font_montserrat_14;
    lv_obj_t* label = makeLabel(choice, postalConfirm ? LV_SYMBOL_OK : labelText, labelFont, postalConfirm ? lv_color_hex(0x34C759) : lv_color_white());
    lv_obj_set_width(label, component.width - 8);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_center(label);
    if (accentCount) {
      lv_obj_update_layout(label);
      const int32_t textWidth = lv_text_get_width(labelText, strlen(labelText), labelFont, 0);
      const int16_t startX = static_cast<int16_t>(4 + (component.width - 8 - textWidth) / 2);
      drawSpanishAccents(choice, labelText, marks, accentCount, labelFont, startX, lv_obj_get_y(label), lv_color_white());
    }
  }
}

void renderSettingsSubmenu(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const char* pressedComponentId, const SharedRenderState* state) {
  LV_UNUSED(state);
  // Keep the opaque screen base established by renderSharedLvglScreen. With
  // transparent wallpaper, a transparent root blends over stale pixels in the
  // device's reused partial draw buffer, leaving duplicate menu/banner rows.
  const bool clockView = strcmp(screen.screenId, "device-clock-options") == 0;
  const char* titleText = clockView ? "CLOCK" : "DISPLAY";
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (strcmp(component.id, "submenu-title") == 0) titleText = component.text;
  }
  lv_obj_t* back = makePanel(root, 48, 87, 48, 46, 23, lv_color_hex(0x121721));
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, lv_color_hex(0x2B3545), 0);
  lv_obj_t* backText = makeLabel(back, LV_SYMBOL_LEFT, &lv_font_montserrat_24, lv_color_white());
  lv_obj_center(backText);
  lv_obj_t* title = makeLabel(root, titleText, &lv_font_montserrat_24, lv_color_white());
  lv_obj_set_pos(title, 196, 91);
  const lv_image_dsc_t* titleIcon = resolveImage == nullptr ? nullptr : resolveImage(clockView ? "display-icon-clock" : "display-icon-display");
  if (titleIcon != nullptr) {
    lv_obj_t* icon = lv_image_create(root);
    lv_image_set_src(icon, titleIcon);
    lv_obj_set_pos(icon, 150, 89);
  }

  lv_obj_t* viewport = lv_obj_create(root);
  lv_obj_set_pos(viewport, display_menu::kViewportX, display_menu::kViewportY);
  lv_obj_set_size(viewport, display_menu::kViewportWidth, display_menu::kViewportHeight);
  lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(viewport, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(viewport, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(viewport, 0, 0);
  lv_obj_set_style_pad_all(viewport, 0, 0);
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (strncmp(component.id, "display-row-", 12) == 0) {
      drawSubmenuRow(viewport, component, pressedComponentId, resolveImage);
    } else if (strncmp(component.id, "display-", 8) == 0 || strncmp(component.id, "postal-key-", 11) == 0 || strncmp(component.id, "clock-", 6) == 0) {
      if (component.type == ComponentType::kButton) drawSubmenuChoice(viewport, component, pressedComponentId, resolveImage);
      else if (component.type == ComponentType::kText) {
        lv_obj_t* label = makeLabel(root, component.text, fontForSize(component.size), color565(component.color));
        lv_obj_set_pos(label, component.x, component.y);
      }
    }
  }
}

void renderSharedStatusBanner(lv_obj_t* root, const SharedRenderState& state, SharedImageResolver resolveImage) {
  LV_UNUSED(resolveImage);
  drawGlassCapsule(root, generated_ui::kSharedStatusBannerX,
      generated_ui::kSharedStatusBannerY, generated_ui::kSharedStatusBannerWidth,
      generated_ui::kSharedStatusBannerHeight, generated_ui::kSharedStatusBannerRadius,
      generated_ui::kSharedStatusBannerFill, generated_ui::kSharedStatusBannerFillOpacity,
      generated_ui::kSharedStatusBannerFill, generated_ui::kSharedStatusBannerBorder,
      generated_ui::kSharedStatusBannerBorderOpacity, generated_ui::kSharedStatusBannerBorderWidth,
      generated_ui::kSharedStatusBannerShadowWidth, generated_ui::kSharedStatusBannerShadowOpacity,
      generated_ui::kSharedStatusBannerShadowColor, generated_ui::kSharedStatusBannerInnerRimColor,
      generated_ui::kSharedStatusBannerInnerRimOpacity, generated_ui::kSharedStatusBannerReflectionX,
      generated_ui::kSharedStatusBannerReflectionY, generated_ui::kSharedStatusBannerReflectionInset,
      generated_ui::kSharedStatusBannerReflectionHeight, generated_ui::kSharedStatusBannerReflectionOpacity,
      generated_ui::kSharedStatusBannerBorderUsesAccent);
  drawStatusContent(root, state, false, resolveImage);
}

void renderSettingsLiveBrightness(lv_obj_t* root, const SharedRenderState& state, bool glassBackdrop) {
  using namespace generated_ui;
  // This mask owns only the live slider treatment and label. The immutable
  // base asset remains the owner of every pixel outside this declared region.
  // The Sound & Vibration capsule and its glow extend below its nominal row.
  // Start the live-control clear below them so the slider cannot clip the row.
  // The glass plate already clears the baked slider: preserve its background.
  if (!glassBackdrop) {
    lv_obj_t* mask = makePanel(root, 63, 349, 342, 75, 0, lv_color_black());
    lv_obj_set_style_border_width(mask, 0, 0);
  }
  lv_obj_t* slider = lv_slider_create(root);
  lv_obj_set_pos(slider, kSettingsSliderX, kSettingsSliderY);
  lv_obj_set_size(slider, kSettingsSliderWidth, kSettingsSliderHeight);
  lv_slider_set_range(slider, 0, 100);
  // The reference centers 50% on the track while the usable brightness
  // range starts at 5%. Match the shared settings_geometry mapping.
  const int16_t level = state.brightnessPercent < 5 ? 5 : state.brightnessPercent > 100 ? 100 : state.brightnessPercent;
  const int16_t displayValue = level <= 50 ? (level - 5) * 50 / 45 : level;
  lv_slider_set_value(slider, displayValue, LV_ANIM_OFF);
  lv_obj_remove_flag(slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(slider, kSettingsSliderRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, color565(kSettingsSliderTrack), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, accentBright(), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(slider, 14, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(slider, accentBright(), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(slider, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, kSettingsSliderRadius, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, accentBright(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(slider, accentBright(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, accentBright(), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_70, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_border_opa(slider, LV_OPA_60, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  // LVGL's default theme adds DPI-scaled padding to LV_PART_KNOB. The browser
  // renderer disables that theme, so use an explicit 3 px on both targets.
  lv_obj_set_style_pad_all(slider, 3, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(slider, 10, LV_PART_KNOB);
  lv_obj_set_style_shadow_color(slider, accentBright(), LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(slider, LV_OPA_60, LV_PART_KNOB);
  lv_obj_set_style_width(slider, kSettingsSliderKnobRadius * 2, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSettingsSliderKnobRadius * 2, LV_PART_KNOB);
  lv_obj_t* brightness = makeLabel(root, "Brightness", &lv_font_montserrat_16, lv_color_hex(0xF2F3F7));
  lv_obj_set_width(brightness, 160);
  lv_obj_set_style_text_align(brightness, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(brightness, kSettingsBrightnessLabelX - 80, kSettingsBrightnessLabelY);
}

void drawSharedScreenBackground(lv_obj_t* root, SharedImageResolver resolveImage, const SharedRenderState* state, bool clockScreen) {
  if (resolveImage == nullptr) return;
  static const char* keys[] = {"background-flower", "background-chevron", "background-mechanical-flower", "background-black-white-circle"};
  uint8_t index = state == nullptr ? 3 : state->backgroundArt;
  if (clockScreen && state != nullptr && state->clockBackground < 3) index = state->clockBackground;
  const lv_image_dsc_t* wallpaper = resolveImage(index < 4 ? keys[index] : keys[3]);
  if (wallpaper == nullptr) wallpaper = resolveImage("background-flower");
  if (wallpaper == nullptr) return;
  lv_obj_t* image = lv_image_create(root);
  lv_image_set_src(image, wallpaper);
  lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0);
  lv_obj_set_pos(image, 0, 0);
  lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

void renderSettings(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const SharedRenderState& state, const char* selectedRowId) {
  lv_obj_t* title = makeLabel(root, "SETTINGS", &lv_font_montserrat_24, lv_color_white());
  lv_obj_set_pos(title, generated_ui::kSettingsTitleX, generated_ui::kSettingsTitleY);
  const lv_image_dsc_t* titleGear = settingsMainGlyph("settings-main/gear");
  if (titleGear != nullptr) {
    lv_obj_t* gear = lv_image_create(root);
    lv_image_set_src(gear, titleGear);
    lv_obj_set_pos(gear, generated_ui::kSettingsGearX - 20, generated_ui::kSettingsGearY - 20);
  }
  drawSettingsGlassRows(root, screen, selectedRowId, resolveImage);
  renderSettingsLiveBrightness(root, state, true);
}


const char* weatherConditionLabel(const char* code) {
  if (strcmp(code, "CL") == 0) return "CLEAR";
  if (strcmp(code, "PC") == 0) return "PARTLY";
  if (strcmp(code, "FG") == 0) return "FOG";
  if (strcmp(code, "RA") == 0) return "RAIN";
  if (strcmp(code, "SN") == 0) return "SNOW";
  if (strcmp(code, "SH") == 0) return "SHOWERS";
  if (strcmp(code, "ST") == 0) return "STORM";
  return code;
}

void renderSpotify(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const char* pressedComponentId) {
  const SduiComponent* toggle = findComponent(screen, "spotify-toggle");
  const bool playing = toggle != nullptr && strcmp(toggle->icon, "pause") == 0;
  lv_obj_t* arc = lv_arc_create(root);
  lv_obj_set_pos(arc, generated_ui::kSpotifyArcX, generated_ui::kSpotifyArcY);
  lv_obj_set_size(arc, generated_ui::kSpotifyArcSize, generated_ui::kSpotifyArcSize);
  lv_arc_set_rotation(arc, generated_ui::kSpotifyArcRotation);
  lv_arc_set_bg_angles(arc, generated_ui::kSpotifyArcBackgroundStart, generated_ui::kSpotifyArcBackgroundEnd);
  lv_arc_set_range(arc, 0, 100);
  const SduiComponent* volume = findComponent(screen, "spotify-volume");
  lv_arc_set_value(arc, volume == nullptr ? 0 : volume->value);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, generated_ui::kSpotifyArcWidth, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, rawColor565(generated_ui::kSpotifyArcBackground), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, static_cast<lv_opa_t>(generated_ui::kSpotifyArcTrackOpacity * 255 / 100), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, generated_ui::kSpotifyArcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, generated_ui::kSpotifyArcActiveUsesAccent ? accentBright() : rawColor565(generated_ui::kSpotifyArcActive), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, generated_ui::kSpotifyArcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(arc, rawColor565(generated_ui::kSpotifyArcKnobFill), LV_PART_KNOB);
  lv_obj_set_style_bg_grad_color(arc, rawColor565(generated_ui::kSpotifyArcKnobGradient), LV_PART_KNOB);
  lv_obj_set_style_bg_grad_dir(arc, LV_GRAD_DIR_VER, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_width(arc, generated_ui::kSpotifyArcKnobDiameter, LV_PART_KNOB);
  lv_obj_set_style_height(arc, generated_ui::kSpotifyArcKnobDiameter, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
  lv_obj_set_style_border_width(arc, generated_ui::kSpotifyArcKnobBorderWidth, LV_PART_KNOB);
  lv_obj_set_style_border_color(arc, accentBright(), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(arc, generated_ui::kSpotifyArcKnobShadowWidth, LV_PART_KNOB);
  lv_obj_set_style_shadow_color(arc, accentBright(), LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(arc, static_cast<lv_opa_t>(generated_ui::kSpotifyArcKnobShadowOpacity * 255 / 100), LV_PART_KNOB);
  lv_obj_set_style_radius(arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);

  const char* symbols[] = {LV_SYMBOL_LIST, LV_SYMBOL_SHUFFLE, LV_SYMBOL_OK};
  const int16_t actionX[] = {generated_ui::kSpotifyActionX[0] - 12, generated_ui::kSpotifyActionX[1] - 12, generated_ui::kSpotifyActionX[2] - 12};
  for (uint8_t index = 0; index < 3; ++index) {
    lv_obj_t* action = makeLabel(root, symbols[index], &lv_font_montserrat_24, lv_color_white());
    lv_obj_set_pos(action, actionX[index], generated_ui::kSpotifyActionY[index] - 12);
  }

  if (resolveImage != nullptr) {
    const lv_image_dsc_t* coverSource = resolveImage("spotify-large");
    if (coverSource != nullptr) {
      lv_obj_t* cover = lv_image_create(root);
      lv_image_set_src(cover, coverSource);
      lv_obj_set_style_image_opa(cover, LV_OPA_COVER, 0);
      lv_obj_set_pos(cover, generated_ui::kSpotifyCoverX, generated_ui::kSpotifyCoverY);
    }
  }
  const SduiComponent* track = findComponent(screen, "track");
  const SduiComponent* artist = findComponent(screen, "artist");
  lv_obj_t* trackLabel = makeLabel(root, track == nullptr ? "Sin reproducción" : track->text, &lv_font_montserrat_22, lv_color_white());
  lv_obj_set_width(trackLabel, 320);
  lv_obj_set_style_text_align(trackLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(trackLabel, LV_ALIGN_TOP_MID, 0, track == nullptr ? 252 : track->y);
  lv_obj_t* artistLabel = makeLabel(root, artist == nullptr ? "Abre Spotify" : artist->text, &lv_font_montserrat_14, lv_color_hex(0xD1D1D6));
  lv_obj_align(artistLabel, LV_ALIGN_TOP_MID, 0, artist == nullptr ? 286 : artist->y);

  const int16_t controlX[] = {83, 191, 313};
  const int16_t controlSize[] = {70, 84, 70};
  const char* controlSymbols[] = {LV_SYMBOL_PREV, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, LV_SYMBOL_NEXT};
  for (uint8_t index = 0; index < 3; ++index) {
    const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, index == 0 ? "spotify-prev" : index == 1 ? "spotify-toggle" : "spotify-next") == 0;
    lv_obj_t* control = makeLabel(root, controlSymbols[index], controlSize[index] > 70 ? &lv_font_montserrat_32 : &lv_font_montserrat_24, pressed ? lv_color_hex(0xA7A7A7) : lv_color_white());
    lv_obj_set_pos(control, controlX[index] + controlSize[index] / 2 - 16, generated_ui::kSpotifyControlY[index]);
  }
  const SduiComponent* seek = findComponent(screen, "spotify-seek");
  lv_obj_t* bar = lv_bar_create(root);
  lv_obj_set_pos(bar, generated_ui::kSpotifySeekX, generated_ui::kSpotifySeekY);
  lv_obj_set_size(bar, generated_ui::kSpotifySeekWidth, generated_ui::kSpotifySeekHeight);
  lv_bar_set_value(bar, seek == nullptr ? 0 : seek->value, LV_ANIM_OFF);
  lv_obj_set_style_radius(bar, generated_ui::kSpotifySeekRadius, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, generated_ui::kSpotifySeekRadius, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, rawColor565(generated_ui::kSpotifySeekBackground), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(bar, rawColor565(generated_ui::kSpotifyArcBackground), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, static_cast<lv_opa_t>(generated_ui::kSpotifySeekTrackOpacity * 255 / 100), LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(bar, accentBright(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(bar, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(bar, generated_ui::kSpotifySeekKnobShadowWidth / 2, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(bar, accentBright(), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(bar, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, generated_ui::kSpotifySeekActiveUsesAccent ? accentBright() : rawColor565(generated_ui::kSpotifySeekActive), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(bar, rawColor565(selectedAccent565()), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  const uint16_t seekRange = seek == nullptr || seek->maximum == 0 ? 100 : seek->maximum;
  const uint16_t seekValue = seek == nullptr ? 0 : (seek->value > seekRange ? seekRange : seek->value);
  const int16_t knobDiameter = generated_ui::kSpotifySeekKnobDiameter;
  const int16_t halfTrack = knobDiameter / 2;
  const int16_t travel = generated_ui::kSpotifySeekWidth - knobDiameter;
  const int16_t knobCenter = static_cast<int16_t>(generated_ui::kSpotifySeekX + halfTrack + (travel * seekValue) / seekRange);
  lv_obj_t* seekKnob = makePanel(root, static_cast<int16_t>(knobCenter - halfTrack),
      static_cast<int16_t>(generated_ui::kSpotifySeekY + generated_ui::kSpotifySeekHeight / 2 - halfTrack),
      knobDiameter, knobDiameter, halfTrack, rawColor565(generated_ui::kSpotifySeekKnobFill));
  lv_obj_set_style_bg_grad_color(seekKnob, rawColor565(generated_ui::kSpotifySeekKnobGradient), 0);
  lv_obj_set_style_bg_grad_dir(seekKnob, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(seekKnob, generated_ui::kSpotifySeekKnobBorderWidth, 0);
  lv_obj_set_style_border_color(seekKnob, accentBright(), 0);
  lv_obj_set_style_shadow_width(seekKnob, generated_ui::kSpotifySeekKnobShadowWidth, 0);
  lv_obj_set_style_shadow_color(seekKnob, accentBright(), 0);
  lv_obj_set_style_shadow_opa(seekKnob, static_cast<lv_opa_t>(generated_ui::kSpotifySeekKnobShadowOpacity * 255 / 100), 0);
}

}  // namespace

void renderSharedLvglScreen(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const char* selectedRowId, const SharedRenderState* state) {
  gAccentIndex = state == nullptr ? 4 : state->accentIndex;
  gDarkAppearance = state == nullptr || state->appearanceDark;
  gHomeArt = state == nullptr ? 255 : state->backgroundArt;
  lv_obj_clean(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(root, state != nullptr && !state->appearanceDark ? lv_color_white() : color565(screen.background), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  drawSharedScreenBackground(root, resolveImage, state, strcmp(screen.screenId, "sky-clock") == 0);

  if (state != nullptr && strcmp(screen.screenId, "device-control-center") == 0) {
    renderSettings(root, screen, resolveImage, *state, selectedRowId);
    renderSharedStatusBanner(root, *state, resolveImage);
    return;
  }

  if (strcmp(screen.screenId, "device-additional-options") == 0 || strcmp(screen.screenId, "device-clock-options") == 0 || strcmp(screen.screenId, "device-appearance-options") == 0) {
    renderSettingsSubmenu(root, screen, resolveImage, selectedRowId, state);
    if (state != nullptr) renderSharedStatusBanner(root, *state, resolveImage);
    return;
  }

  if (strcmp(screen.screenId, "spotify-player") == 0) {
    renderSpotify(root, screen, resolveImage, selectedRowId);
    if (state != nullptr) renderSharedStatusBanner(root, *state, resolveImage);
    return;
  }

  const bool home = strcmp(screen.screenId, "sky-home") == 0;
  const bool clock = strcmp(screen.screenId, "sky-clock") == 0;
  const bool computerControls = strcmp(screen.screenId, "computer-controls") == 0;
  const bool referenceHome = false;
  if (resolveImage != nullptr && strcmp(screen.screenId, "sky-weather") == 0) {
    SduiComponent glass = {};
    glass.x = generated_ui::kWeatherIconBubbleX;
    glass.y = generated_ui::kWeatherIconBubbleY;
    glass.width = generated_ui::kWeatherIconBubbleSize;
    glass.height = generated_ui::kWeatherIconBubbleSize;
    lv_obj_t* bubble = makeHomeButton(root, glass, nullptr);
    const lv_image_dsc_t* source = resolveImage("open-weather");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(bubble); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_center(image); }
    lv_obj_t* conditionsSurface = makePanel(root, 58, 198, 350, 45, 16, lv_color_hex(0x0C111A));
    lv_obj_set_style_bg_opa(conditionsSurface, LV_OPA_80, 0);
    lv_obj_set_style_border_width(conditionsSurface, 1, 0);
    lv_obj_set_style_border_color(conditionsSurface, lv_color_hex(0x526174), 0);
  }
  if (clock && state != nullptr) drawWatchFace(root, *state);
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (referenceHome) continue;
    if (clock && state != nullptr && strcmp(component.id, "sky-time") == 0) continue;
    if (strcmp(screen.screenId, "sky-weather") == 0 && (strcmp(component.id, "weather-title") == 0 || strcmp(component.id, "weather-units") == 0)) continue;
    if (strcmp(screen.screenId, "sky-weather") == 0 && strcmp(component.id, "weather-forecast-data") == 0) {
      uint8_t visibleDays = 3;
      for (uint8_t option = 0; option < screen.componentCount; ++option) {
        const SduiComponent& candidate = screen.components[option];
        if (strncmp(candidate.id, "weather-forecast-", 17) == 0 && strcmp(candidate.icon, "selected") == 0) visibleDays = static_cast<uint8_t>(atoi(candidate.id + 17));
      }
      const int16_t cardWidth = generated_ui::kWeatherForecastWidth / visibleDays;
      char data[sizeof(component.text)] = {};
      strlcpy(data, component.text, sizeof(data));
      char* saveDay = nullptr;
      char* day = strtok_r(data, ";", &saveDay);
      const SduiComponent* currentTemperature = findComponent(screen, "weather-temp");
      const size_t temperatureLength = currentTemperature == nullptr ? 0 : strlen(currentTemperature->text);
      const char unit = temperatureLength > 0 ? currentTemperature->text[temperatureLength - 1] : 'F';
      for (uint8_t dayIndex = 0; dayIndex < visibleDays; ++dayIndex) {
        char fields[5][12] = {};
        uint8_t fieldCount = 0;
        if (day != nullptr && strcmp(day, "--") != 0) {
          char* saveField = nullptr;
          char* field = strtok_r(day, ":", &saveField);
          for (; fieldCount < 5 && field != nullptr; ++fieldCount, field = strtok_r(nullptr, ":", &saveField)) strlcpy(fields[fieldCount], field, sizeof(fields[fieldCount]));
        }
        const bool validRecord = fieldCount == 5 && fields[0][0] != '\0' && fields[1][0] != '\0';
        if (!validRecord) {
          snprintf(fields[0], sizeof(fields[0]), "DAY %u", dayIndex + 1);
          strlcpy(fields[1], "NO DATA", sizeof(fields[1]));
          strlcpy(fields[2], "--", sizeof(fields[2]));
          strlcpy(fields[3], "--", sizeof(fields[3]));
          strlcpy(fields[4], "--", sizeof(fields[4]));
        }
        if (day != nullptr) day = strtok_r(nullptr, ";", &saveDay);
        const int16_t x = generated_ui::kWeatherForecastX + dayIndex * cardWidth;
        lv_obj_t* card = makePanel(root, x, generated_ui::kWeatherForecastY, cardWidth - 3, generated_ui::kWeatherForecastHeight, 13, lv_color_hex(0x121721));
        lv_obj_set_style_bg_opa(card, LV_OPA_60, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2B3545), 0);
        char meanText[20] = {};
        char highText[20] = {};
        char lowText[20] = {};
        snprintf(meanText, sizeof(meanText), "%s%c", fields[2], unit);
        snprintf(highText, sizeof(highText), "H %s%c", fields[3], unit);
        snprintf(lowText, sizeof(lowText), "L %s%c", fields[4], unit);
        const char* labels[] = {fields[0], weatherConditionLabel(fields[1]), meanText, highText, lowText};
        const int16_t ys[] = {generated_ui::kWeatherForecastY + 5, generated_ui::kWeatherForecastY + 21, generated_ui::kWeatherForecastY + 38, generated_ui::kWeatherForecastY + 61, generated_ui::kWeatherForecastY + 80};
        for (int line = 0; line < 5; ++line) {
          const lv_font_t* font = line == 2 ? &lv_font_montserrat_14 : &lv_font_montserrat_12;
          lv_obj_t* label = makeLabel(root, labels[line], font, line == 1 ? lv_color_hex(0xA0ACB9) : lv_color_white());
          lv_obj_set_width(label, cardWidth - 6);
          lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
          lv_obj_set_pos(label, x + 3, ys[line]);
        }
      }
      continue;
    }
    if (strcmp(screen.screenId, "sky-weather") == 0 && strncmp(component.id, "weather-forecast-", 17) == 0) {
      const bool selected = strcmp(component.icon, "selected") == 0;
      const bool pressed = selectedRowId != nullptr && strcmp(selectedRowId, component.id) == 0;
      SduiComponent glass = component;
      lv_obj_t* button = makeHomeButton(root, glass, pressed ? selectedRowId : nullptr);
      lv_obj_set_style_border_width(button, selected || pressed ? 2 : generated_ui::kHomeGlassBorderWidth, 0);
      lv_obj_set_style_border_color(button, selected || pressed ? accentBright() : lv_color_hex(0x46515F), 0);
      if (pressed) {
        lv_obj_set_style_shadow_width(button, generated_ui::kHomeGlassPressedShadowWidth, 0);
        lv_obj_set_style_shadow_opa(button, static_cast<lv_opa_t>(generated_ui::kHomeGlassPressedShadowOpacity * 255 / 100), 0);
      }
      lv_obj_t* label = makeLabel(button, component.text, &lv_font_montserrat_12, selected ? accentBright() : lv_color_white());
      lv_obj_center(label);
      continue;
    }
    if (strcmp(screen.screenId, "sky-weather") == 0 && strcmp(component.id, "weather-temp") == 0) {
      lv_obj_t* label = makeLabel(root, component.text, &lv_font_montserrat_24, lv_color_white());
      lv_obj_set_width(label, generated_ui::kWeatherTemperatureWidth);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(label, component.x - generated_ui::kWeatherTemperatureWidth / 2, component.y);
      continue;
    }
    if (strcmp(screen.screenId, "sky-weather") == 0 && strcmp(component.id, "weather-humidity-label") == 0) {
      lv_obj_t* label = makeLabel(root, component.text, &lv_font_montserrat_12, lv_color_hex(0xCDD4DF));
      lv_obj_set_width(label, generated_ui::kWeatherHumidityWidth);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(label, component.x - generated_ui::kWeatherHumidityWidth / 2, component.y);
      continue;
    }
    if (strcmp(screen.screenId, "sky-weather") == 0 && strcmp(component.id, "weather-humidity-value") == 0) {
      lv_obj_t* label = makeLabel(root, component.text, &lv_font_montserrat_16, lv_color_white());
      lv_obj_set_width(label, generated_ui::kWeatherHumidityWidth);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(label, component.x - generated_ui::kWeatherHumidityWidth / 2, component.y);
      continue;
    }
    if (strcmp(screen.screenId, "sky-weather") == 0 && (strcmp(component.id, "weather-rain-label") == 0 || strcmp(component.id, "weather-rain-value") == 0)) {
      const bool labelText = strcmp(component.id, "weather-rain-label") == 0;
      lv_obj_t* label = makeLabel(root, component.text, labelText ? &lv_font_montserrat_12 : &lv_font_montserrat_16,
          labelText ? lv_color_hex(0xCDD4DF) : lv_color_white());
      lv_obj_set_width(label, generated_ui::kWeatherRainWidth);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(label, component.x - generated_ui::kWeatherRainWidth / 2, component.y);
      continue;
    }
    if (component.type == ComponentType::kText) {
      const lv_font_t* textFont = fontForSize(component.size);
      if (state != nullptr && strcmp(screen.screenId, "sky-clock") == 0 && strcmp(component.id, "sky-time") == 0) {
        textFont = state->clockFace == 1 ? &lv_font_montserrat_28 : state->clockFace == 2 ? &lv_font_montserrat_32 : &lv_font_montserrat_48;
      }
      lv_color_t componentColor = color565(component.color);
      if (state != nullptr && !state->appearanceDark && component.color == 0xFFFF) componentColor = lv_color_hex(0x202833);
      if (state != nullptr && !state->appearanceDark && component.color == 0x8C72) componentColor = lv_color_hex(0x566273);
      char labelText[97] = {};
      SpanishAccentMark marks[24] = {};
      const uint8_t accentCount = spanishAscii(component.text, labelText, sizeof(labelText), marks, 24);
      lv_obj_t* text = makeLabel(root, labelText, textFont, componentColor);
      int16_t textStartX = component.x;
      if (component.align == TextAlign::kCenter) {
        // LVGL's intrinsic label width is not valid until layout has run.
        // Positioning by that width caused the simulator and device to drift.
        // A full-width, centered label uses the same text-align operation in
        // both the native and WASM LVGL renderers.
        lv_obj_set_width(text, 466);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(text, component.x - 233, component.y);
        const int32_t textWidth = lv_text_get_width(labelText, strlen(labelText), textFont, 0);
        textStartX = static_cast<int16_t>(component.x - textWidth / 2);
        if (strcmp(screen.screenId, "sky-clock") == 0 && strcmp(component.id, "sky-time") == 0) {
          lv_obj_update_layout(text);
          lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
        }
      } else {
        lv_obj_set_pos(text, component.x, component.y);
      }
      if (accentCount) drawSpanishAccents(root, labelText, marks, accentCount, textFont, textStartX, component.y, componentColor);
    } else if (component.type == ComponentType::kButton) {
      lv_obj_t* button = (home || computerControls)
          ? makeHomeButton(root, component, selectedRowId, home)
          : makeButton(root, component, selectedRowId, strcmp(screen.screenId, "sky-clock") != 0);
      const bool homeX = home && strcmp(component.id, "voltage-placeholder-home") == 0;
      const bool homeGear = home && strcmp(component.icon, "gear") == 0;
      const bool homeUsb = home && strcmp(component.icon, "usb") == 0;
      const char* iconAsset = homeGear ? "home-settings-gear" : homeUsb ? "home-usb-icon" : component.id;
      const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage(iconAsset);
      const uint16_t neutralHomeColor = !gDarkAppearance && gHomeArt == 1 && component.color == 0xFFFF
          ? generated_ui::kHomeLightNeutralIcon
          : component.color;
      if (homeX) {
        drawHomeX(button, color565(neutralHomeColor));
      } else if (strcmp(component.icon, "gear") == 0 && !homeGear) {
        drawGear(button, color565(neutralHomeColor));
      } else if (source != nullptr) {
        lv_obj_t* image = lv_image_create(button);
        if (homeUsb) {
          lv_obj_set_style_image_recolor(image, color565(component.color), 0);
          lv_obj_set_style_image_recolor_opa(image, LV_OPA_COVER, 0);
        }
        const bool neutralIcon = strcmp(component.id, "open-clock") == 0 || strcmp(component.id, "open-weather") == 0;
        lv_image_set_src(image, home && neutralIcon ? contrastHomeIcon(source) : source);
        lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0);
        if (home) placeHomeIcon(image, source, generated_ui::kHomeBubbleDiameter, generated_ui::kHomeBubbleDiameter);
        else lv_obj_center(image);
      } else if (strcmp(component.icon, "usb") == 0) {
        lv_obj_t* glyph = makePanel(button, 0, 0, component.width, component.height, 0, lv_color_black());
        lv_obj_set_style_bg_opa(glyph, LV_OPA_TRANSP, 0);
        lv_obj_set_style_transform_pivot_x(glyph, component.width / 2, 0);
        lv_obj_set_style_transform_pivot_y(glyph, component.height / 2, 0);
        lv_obj_set_style_transform_scale(glyph, generated_ui::kHomeIconVisibleDiameter * 256 / 26, 0);
        drawUsb(glyph, color565(component.color));
      } else if (strcmp(component.icon, "whatsapp") == 0) {
        drawWhatsapp(button, color565(component.color));
      } else {
        const lv_font_t* buttonFont = strcmp(component.id, "open-spotify") == 0
            ? &lv_font_montserrat_14
            : &lv_font_montserrat_20;
        lv_obj_t* text = makeLabel(button, component.text, buttonFont, color565(component.color));
        lv_obj_center(text);
      }
      if (home) drawHomeGlassRim(button, resolveImage);
    } else if (component.type == ComponentType::kSlider) {
      lv_obj_t* slider = lv_slider_create(root);
      lv_obj_set_pos(slider, component.x, component.y);
      lv_obj_set_size(slider, component.width, component.height);
      lv_slider_set_range(slider, 0, component.maximum);
      lv_slider_set_value(slider, component.value, LV_ANIM_OFF);
      lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      lv_obj_set_style_bg_color(slider, color565(generated_ui::kSettingsSliderTrack), LV_PART_MAIN);
      lv_obj_set_style_bg_grad_color(slider, rawColor565(generated_ui::kSettingsSliderFillEnd), LV_PART_MAIN);
      lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_VER, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(slider, static_cast<lv_opa_t>(generated_ui::kSettingsSliderTrackOpacity * 255 / 100), LV_PART_MAIN);
      lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(slider, accentBright(), LV_PART_MAIN);
      lv_obj_set_style_shadow_width(slider, 8, LV_PART_MAIN);
      lv_obj_set_style_shadow_color(slider, accentBright(), LV_PART_MAIN);
      lv_obj_set_style_shadow_opa(slider, LV_OPA_40, LV_PART_MAIN);
      lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(slider, accentBright(), LV_PART_INDICATOR);
      lv_obj_set_style_bg_grad_color(slider, rawColor565(selectedAccent565()), LV_PART_INDICATOR);
      lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
      lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
      lv_obj_set_style_bg_color(slider, rawColor565(generated_ui::kSettingsSliderKnobFill), LV_PART_KNOB);
      lv_obj_set_style_bg_grad_color(slider, rawColor565(generated_ui::kSettingsSliderKnobGradient), LV_PART_KNOB);
      lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_VER, LV_PART_KNOB);
      lv_obj_set_style_border_width(slider, generated_ui::kSettingsSliderKnobBorderWidth, LV_PART_KNOB);
      lv_obj_set_style_border_color(slider, accentBright(), LV_PART_KNOB);
      lv_obj_set_style_shadow_width(slider, generated_ui::kSettingsSliderKnobShadowWidth, LV_PART_KNOB);
      lv_obj_set_style_shadow_color(slider, accentBright(), LV_PART_KNOB);
      lv_obj_set_style_shadow_opa(slider, static_cast<lv_opa_t>(generated_ui::kSettingsSliderKnobShadowOpacity * 255 / 100), LV_PART_KNOB);
    } else if (component.type == ComponentType::kProgress) {
      lv_obj_t* bar = lv_bar_create(root);
      lv_obj_set_pos(bar, component.x, component.y);
      lv_obj_set_size(bar, component.width, component.height);
      lv_bar_set_range(bar, 0, component.maximum);
      lv_bar_set_value(bar, component.value, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
      lv_obj_set_style_bg_color(bar, color565(component.color), LV_PART_INDICATOR);
    }
  }
  if (state != nullptr && strcmp(screen.screenId, "sky-clock") == 0 && state->clockShowWeather) {
    lv_obj_t* weather = makeLabel(root, "WEATHER  --°", &lv_font_montserrat_14, lv_color_hex(0xA7A7A7));
    lv_obj_set_width(weather, 466);
    lv_obj_set_style_text_align(weather, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(weather, 0, 276);
  }
  if (state != nullptr && home) {
    drawStatus(root, screen, *state, resolveImage);
  } else if (state != nullptr && !clock) {
    // Home keeps its separate centered pill; Clock has no top shared banner.
    renderSharedStatusBanner(root, *state, resolveImage);
  }
}

void renderSharedLvglStatusBanner(
    lv_obj_t* root,
    const SharedRenderState& state,
    SharedImageResolver resolveImage) {
  if (root == nullptr) return;
  gAccentIndex = state.accentIndex;
  renderSharedStatusBanner(root, state, resolveImage);
}

void renderFirmwareLifecyclePreview(lv_obj_t* root, FirmwareLifecyclePreview preview, uint8_t step) {
  if (root == nullptr) return;
  lv_obj_clean(root);
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

  if (preview == FirmwareLifecyclePreview::kCalibration) {
    makePanel(root, 0, 0, 466, 1, 0, rawColor565(generated_ui::kCalibrationBorderColor));
    makePanel(root, 0, 465, 466, 1, 0, rawColor565(generated_ui::kCalibrationBorderColor));
    makePanel(root, 0, 0, 1, 466, 0, rawColor565(generated_ui::kCalibrationBorderColor));
    makePanel(root, 465, 0, 1, 466, 0, rawColor565(generated_ui::kCalibrationBorderColor));
    for (int16_t coordinate = generated_ui::kCalibrationGridSpacing; coordinate < 466; coordinate += generated_ui::kCalibrationGridSpacing) {
      makePanel(root, coordinate, 0, generated_ui::kCalibrationGridWidth, 466, 0, rawColor565(generated_ui::kCalibrationGridColor));
      makePanel(root, 0, coordinate, 466, generated_ui::kCalibrationGridWidth, 0, rawColor565(generated_ui::kCalibrationGridColor));
    }
    for (int16_t y : {0, 456}) for (int16_t x : {0, 456})
      makePanel(root, x, y, 10, 10, 0, lv_color_white());
    for (uint8_t index = 0; index < generated_ui::kCalibrationRadiusCount; ++index) {
      const int16_t diameter = generated_ui::kCalibrationRadii[index] * 2;
      lv_obj_t* ring = lv_obj_create(root);
      lv_obj_set_size(ring, diameter, diameter);
      lv_obj_set_pos(ring, 233 - generated_ui::kCalibrationRadii[index], 233 - generated_ui::kCalibrationRadii[index]);
      lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(ring, generated_ui::kCalibrationCircleWidth, 0);
      lv_obj_set_style_border_color(ring, rawColor565(generated_ui::kCalibrationCircleColor), 0);
    }
    makePanel(root, 232, 0, 2, 466, 0, rawColor565(generated_ui::kCalibrationCrosshairColor));
    makePanel(root, 0, 232, 466, 2, 0, rawColor565(generated_ui::kCalibrationCrosshairColor));
    return;
  }

  if (preview == FirmwareLifecyclePreview::kWelcome) {
    lv_obj_t* face = makePanel(root, 188, 60, 90, 90, 45, rawColor565(0xFD20));
    (void)face;
    makePanel(root, 213, 93, 10, 10, 5, lv_color_black());
    makePanel(root, 243, 93, 10, 10, 5, lv_color_black());
    static lv_point_precise_t smile[] = {{0, 0}, {9, 6}, {18, 0}};
    lv_obj_t* smileArc = lv_line_create(root); lv_line_set_points(smileArc, smile, 3);
    lv_obj_set_pos(smileArc, 224, 110); lv_obj_set_style_line_width(smileArc, 3, 0); lv_obj_set_style_line_color(smileArc, lv_color_black(), 0);
    lv_obj_t* greeting = makeLabel(root, step == 0 ? "HOLA" : "SOY TU", &lv_font_montserrat_36, lv_color_white());
    lv_obj_align(greeting, LV_ALIGN_TOP_MID, 0, step == 0 ? 195 : 205);
    lv_obj_t* name = makeLabel(root, step == 0 ? "TU NOMBRE" : "ASISTENTE", &lv_font_montserrat_36, rawColor565(0xFD20));
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, step == 0 ? 255 : 265);
    return;
  }

  const bool update = preview == FirmwareLifecyclePreview::kUpdateRequired;
  lv_obj_t* logo = makeLabel(root, "LEOPOLD", &lv_font_montserrat_36, lv_color_white());
  lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 58);
  lv_obj_t* outline = lv_obj_create(root);
  lv_obj_set_pos(outline, 73, 170); lv_obj_set_size(outline, 320, 68);
  lv_obj_remove_flag(outline, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(outline, 30, 0); lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(outline, 2, 0);
  lv_obj_set_style_border_color(outline, update ? rawColor565(0xF800) : rawColor565(0x3D7F), 0);
  lv_obj_t* title = makeLabel(root, update ? "ERROR" : (step == 0 ? "INICIANDO" : "CARGANDO"), &lv_font_montserrat_20, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 193);
  if (!update) {
    lv_obj_t* track = makePanel(root, 108, 300, 250, 10, 5, lv_color_hex(0x262A30));
    lv_obj_t* progress = makePanel(track, 0, 0, step == 0 ? 92 : 184, 10, 5, lv_color_hex(0x3D7FFF));
    (void)progress;
  } else {
    lv_obj_t* detail = makeLabel(root, "ACTUALIZACION", &lv_font_montserrat_20, lv_color_white());
    lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 280);
    lv_obj_t* secondary = makeLabel(root, "NECESARIA", &lv_font_montserrat_14, lv_color_hex(0x888888));
    lv_obj_align(secondary, LV_ALIGN_TOP_MID, 0, 328);
  }
}

}  // namespace leopold
