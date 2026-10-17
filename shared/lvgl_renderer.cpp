#include "lvgl_renderer.h"

#include <cstring>
#include <cstdio>
#include <cmath>

#if __has_include("generated_firmware_ui.h")
#include "generated_firmware_ui.h"
#else
namespace leopold::generated_ui {
constexpr int16_t kSettingsStatusX = 137;
constexpr int16_t kSettingsStatusY = 39;
constexpr int16_t kSettingsStatusWidth = 195;
constexpr int16_t kSettingsStatusHeight = 34;
constexpr int16_t kSettingsStatusRadius = 17;
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
constexpr uint32_t kSettingsIconActiveColor = 0x0DE3FF;
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
}  // namespace leopold::generated_ui
#endif

namespace leopold {
namespace {

lv_color_t color565(uint16_t value) {
  const uint8_t red = (value >> 11) & 0x1F;
  const uint8_t green = (value >> 5) & 0x3F;
  const uint8_t blue = value & 0x1F;
  return lv_color_make((red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2));
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

lv_obj_t* makeButton(lv_obj_t* parent, const SduiComponent& component, const char* pressedComponentId) {
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  lv_obj_t* object = makePanel(parent, component.x, component.y, component.width, component.height, component.height / 2, lv_color_hex(0x0D0F12));
  lv_obj_set_style_border_width(object, 3, 0);
  lv_obj_set_style_border_color(object, color565(component.color), 0);
  lv_obj_set_style_shadow_width(object, pressed ? 2 : 6, 0);
  lv_obj_set_style_shadow_opa(object, pressed ? LV_OPA_20 : LV_OPA_40, 0);
  lv_obj_set_style_shadow_color(object, color565(component.color), 0);
  lv_obj_t* inset = makePanel(object, 5, 5, component.width - 10, component.height - 10, (component.height - 10) / 2, pressed ? lv_color_hex(0x2C2C30) : lv_color_hex(0x171A1F));
  lv_obj_set_style_bg_opa(inset, LV_OPA_COVER, 0);
  return object;
}

lv_obj_t* makeHomeButton(lv_obj_t* parent, const SduiComponent& component, const char* pressedComponentId) {
  const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, component.id) == 0;
  const lv_color_t glass = lv_color_hex(0xBFC9D8);
  const lv_color_t white = lv_color_hex(0xF3F6FA);
  const lv_color_t accent = color565(component.color);
  const bool greenAccent = strcmp(component.id, "open-whatsapp") == 0 || strcmp(component.id, "open-spotify") == 0;
  const bool blueAccent = strcmp(component.id, "open-navigation") == 0 || strcmp(component.id, "open-usb") == 0;
  lv_obj_t* object = makePanel(
      parent,
      component.x,
      component.y,
      component.width,
      component.height,
      component.height / 2,
      glass);
  // The reference uses a luminous frosted-glass disc rather than a dark
  // outlined button. Keep the color translucent so the active background
  // still shows through, but make the glass itself readable on both modes.
  lv_obj_set_style_bg_opa(object, pressed ? LV_OPA_90 : greenAccent ? LV_OPA_50 : blueAccent ? LV_OPA_70 : LV_OPA_90, 0);
  lv_obj_set_style_border_width(object, 2, 0);
  lv_obj_set_style_border_color(object, greenAccent || blueAccent ? accent : white, 0);
  lv_obj_set_style_border_opa(object, greenAccent ? LV_OPA_90 : LV_OPA_80, 0);
  lv_obj_set_style_shadow_width(object, pressed ? 7 : 20, 0);
  lv_obj_set_style_shadow_opa(object, pressed ? LV_OPA_60 : LV_OPA_90, 0);
  lv_obj_set_style_shadow_color(object, accent, 0);
  lv_obj_t* inset = makePanel(
      object,
      6,
      6,
      component.width - 12,
      component.height - 12,
      (component.height - 12) / 2,
      white);
  lv_obj_set_style_bg_opa(inset, pressed ? LV_OPA_70 : greenAccent ? LV_OPA_10 : LV_OPA_60, 0);
  lv_obj_set_style_border_width(inset, 1, 0);
  lv_obj_set_style_border_color(inset, greenAccent ? accent : white, 0);
  lv_obj_set_style_border_opa(inset, greenAccent ? LV_OPA_40 : LV_OPA_50, 0);
  return object;
}

void drawUsb(lv_obj_t* parent, lv_color_t color) {
  static const lv_point_precise_t segments[][2] = {
      {{36, 52}, {36, 20}}, {{36, 20}, {27, 28}}, {{36, 20}, {45, 28}},
      {{36, 35}, {22, 35}}, {{22, 35}, {22, 28}}, {{36, 44}, {50, 44}},
      {{50, 44}, {50, 37}},
  };
  for (const auto& segment : segments) {
    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, segment, 2);
    lv_obj_set_size(line, 72, 72);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 3, 0);
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

void drawPairedCircles(lv_obj_t* root, int16_t x, int16_t y, lv_color_t color) {
  const auto disc = [root, color](int16_t left, int16_t top, int16_t size) {
    lv_obj_t* object = makePanel(root, left, top, size, size, size / 2, color);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(0xF3F6FA), 0);
    lv_obj_set_style_shadow_width(object, generated_ui::kSettingsCloudGlowBlur, 0);
    lv_obj_set_style_shadow_opa(object, LV_OPA_80, 0);
    lv_obj_set_style_shadow_color(object, lv_color_hex(0xA9C9F4), 0);
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

void drawChevron(lv_obj_t* parent, int16_t x, int16_t y) {
  static lv_point_precise_t points[] = {{0, 0}, {generated_ui::kSettingsChevronMidX, generated_ui::kSettingsChevronMidY}, {0, generated_ui::kSettingsChevronEndY}};
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 3);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_size(line, 10, 18);
  lv_obj_set_style_line_color(line, lv_color_hex(0xD5E3FA), 0);
  lv_obj_set_style_line_width(line, generated_ui::kSettingsChevronStroke, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

void drawSettingsIcon(lv_obj_t* parent, uint8_t index, int16_t centerX, int16_t centerY, bool active) {
  constexpr float pi = 3.14159265358979323846f;
  const lv_color_t color = lv_color_hex(active ? generated_ui::kSettingsIconActiveColor : generated_ui::kSettingsIconColor);
  const auto line = [parent, color](const lv_point_precise_t* points, uint32_t count, int16_t width = 3) {
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
    static lv_point_precise_t inner[13];
    for (int16_t i = 0; i < 13; ++i) {
      const float angle = 1.15f * pi + (0.70f * pi * i / 12.0f);
      outer[i] = point(std::cos(angle) * 12.0f, std::sin(angle) * 12.0f);
      inner[i] = point(std::cos(angle) * 7.0f, std::sin(angle) * 7.0f);
    }
    line(outer, 13, 3);
    line(inner, 13, 3);
    const int16_t dotSize = static_cast<int16_t>(std::lround(4 * generated_ui::kSettingsIconScale));
    lv_obj_t* dot = makePanel(parent, static_cast<int16_t>(std::lround(centerX - dotSize / 2.0f)), static_cast<int16_t>(std::lround(centerY + 4 * generated_ui::kSettingsIconScale - dotSize / 2.0f)), dotSize, dotSize, dotSize / 2, color);
    LV_UNUSED(dot);
    return;
  }
  if (index == 1) {
    static lv_point_precise_t upper[3];
    static lv_point_precise_t lower[3];
    static lv_point_precise_t stem[2];
    upper[0] = point(0, -12); upper[1] = point(8, -5); upper[2] = point(-7, 5);
    lower[0] = point(-7, -5); lower[1] = point(8, 5); lower[2] = point(0, 12);
    stem[0] = point(0, -12); stem[1] = point(0, 12);
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
  static lv_point_precise_t sound[13];
  speaker[0] = point(-12, -4); speaker[1] = point(-6, -4); speaker[2] = point(2, -11); speaker[3] = point(2, 11); speaker[4] = point(-6, 4); speaker[5] = point(-12, 4);
  speaker[6] = speaker[0];
  for (int16_t i = 0; i < 13; ++i) {
    const float angle = -0.9f + (1.8f * i / 12.0f);
    sound[i] = point(std::cos(angle) * 11.0f, std::sin(angle) * 11.0f);
  }
  line(speaker, 7, 3); line(sound, 13, 3);
}

void drawStatus(lv_obj_t* root, const SduiScreen& screen, const SharedRenderState& state) {
  const lv_color_t gray = lv_color_hex(0xD2D8E2);
  const lv_color_t active = lv_color_hex(0xF3F6FA);
  if (strcmp(screen.screenId, "sky-home") == 0) {
    // sky_home.bin contains the reference bezel, pill, icons, and battery.
    // Keep the physical renderer from painting a second pill over that asset;
    // only the time remains live, matching the browser renderer.
    const char* live = state.timeText == nullptr ? "--:--" : state.timeText;
    char clock[8] = {};
    char period[3] = {};
    const char* separator = strstr(live, " AM");
    if (separator == nullptr) separator = strstr(live, " PM");
    if (separator != nullptr) {
      const size_t length = static_cast<size_t>(separator - live);
      snprintf(clock, sizeof(clock), "%.*s", static_cast<int>(length), live);
      snprintf(period, sizeof(period), "%c%c", separator[1], separator[2]);
    } else {
      snprintf(clock, sizeof(clock), "%s", live);
    }
    lv_obj_t* timeSlot = makePanel(root, 211, 208, 68, 31, 0, lv_color_hex(0x555C6A));
    lv_obj_set_style_border_width(timeSlot, 0, 0);
    lv_obj_t* time = makeLabel(root, clock, &lv_font_montserrat_20, gray);
    lv_obj_set_pos(time, 218, 214);
    if (period[0] != '\0') {
      lv_obj_t* meridiem = makeLabel(root, period, &lv_font_montserrat_12, gray);
      lv_obj_set_pos(meridiem, 247, 234);
    }
    return;
  }
  const bool top = strcmp(screen.screenId, "sky-clock") == 0 ||
                   strcmp(screen.screenId, "sky-weather") == 0;
  const bool clock = strcmp(screen.screenId, "sky-clock") == 0;
  const bool showTime = !clock;
  // The clock does not repeat its primary time readout in the status pill.
  // Keep that pill compact instead of leaving a visual hole where the time
  // used to be. Weather retains the small time, but both top pills are inset
  // from the circular display edge so their rounded corners remain visible.
  const int16_t surfaceX = clock ? 143 : top ? 110 : 122;
  const int16_t surfaceY = top ? 18 : 202;
  const int16_t surfaceWidth = clock ? 190 : top ? 246 : 214;
  const int16_t surfaceHeight = top ? 34 : 56;
  const int16_t baseX = clock ? 153 : top ? 119 : 145;
  const int16_t wifiCenterX = clock ? 184 : top ? 153 : 179;
  // The reference pill reads left-to-right as Bluetooth, Wi-Fi, time,
  // paired-circles, battery. These coordinates are intentionally shared by
  // the native and WASM renderers through this source file.
  const int16_t cloudX = clock ? 209 : top ? 238 : 279;
  const int16_t timeX = clock ? 0 : top ? 172 : 198;
  const int16_t batteryX = clock ? 279 : top ? 270 : 294;
  lv_obj_t* statusSurface = makePanel(root, surfaceX, surfaceY, surfaceWidth, surfaceHeight, surfaceHeight / 2, lv_color_hex(0x555C6A));
  lv_obj_set_style_bg_opa(statusSurface, static_cast<lv_opa_t>(235), 0);
  lv_obj_set_style_border_width(statusSurface, 1, 0);
  lv_obj_set_style_border_color(statusSurface, lv_color_hex(0xB8C1CE), 0);
  lv_obj_set_style_border_opa(statusSurface, LV_OPA_50, 0);
  lv_obj_set_style_shadow_width(statusSurface, 0, 0);
  lv_obj_set_style_shadow_opa(statusSurface, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_width(statusSurface, 0, 0);
  lv_obj_set_style_outline_opa(statusSurface, LV_OPA_TRANSP, 0);
  if (showTime) {
    lv_obj_t* time = makeLabel(root, state.timeText == nullptr ? "--:--" : state.timeText, top ? &lv_font_montserrat_14 : &lv_font_montserrat_20, gray);
    if (top) {
      lv_obj_set_width(time, 78);
      lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_set_pos(time, timeX, top ? 25 : 211);
  }
  lv_obj_t* batteryPercent = makeLabel(root, "", &lv_font_montserrat_14, gray);
  char batteryText[8] = {};
  snprintf(batteryText, sizeof(batteryText), "%u%%", state.batteryPercent);
  lv_label_set_text(batteryPercent, batteryText);
  if (top) {
    lv_obj_set_width(batteryPercent, 42);
    lv_obj_set_style_text_align(batteryPercent, LV_TEXT_ALIGN_RIGHT, 0);
  }
  lv_obj_set_pos(batteryPercent, batteryX, top ? 25 : 218);

  const auto glyphLine = [root](const lv_point_precise_t* points, uint32_t count, int16_t width, lv_color_t color) {
    lv_obj_t* line = lv_line_create(root);
    lv_line_set_points(line, points, count);
    lv_obj_set_size(line, 466, 466);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  };
  const lv_color_t bluetoothColor = state.bluetoothEnabled ? active : gray;
  lv_obj_t* bluetooth = makeLabel(root, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_24, bluetoothColor);
  lv_obj_set_pos(bluetooth, baseX, top ? 25 : 220);
  const lv_color_t wifiColor = state.wifiConnected ? active : gray;
  // Draw sampled arches instead of LVGL arcs. This keeps the Wi-Fi geometry
  // stable across LVGL versions and the browser WASM build.
  const int16_t wifiRadii[] = {10, 6};
  static lv_point_precise_t wifiPoints[2][13] = {};
  for (uint8_t radiusIndex = 0; radiusIndex < 2; ++radiusIndex) {
    const int16_t radius = wifiRadii[radiusIndex];
    lv_point_precise_t* points = wifiPoints[radiusIndex];
    for (int index = 0; index <= 12; ++index) {
      const double degrees = 225.0 + (90.0 * index / 12.0);
      const double radians = degrees * 3.14159265358979323846 / 180.0;
      points[index].x = static_cast<int16_t>(std::lround(wifiCenterX + std::cos(radians) * radius));
      points[index].y = static_cast<int16_t>(std::lround((top ? 43 : 230) + std::sin(radians) * radius));
    }
    glyphLine(points, 13, 2, wifiColor);
  }
  lv_obj_t* wifiDot = lv_obj_create(root);
  lv_obj_set_size(wifiDot, 4, 4); lv_obj_set_pos(wifiDot, wifiCenterX - 2, top ? 41 : 228);
  lv_obj_set_style_radius(wifiDot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(wifiDot, wifiColor, 0); lv_obj_set_style_bg_opa(wifiDot, LV_OPA_COVER, 0); lv_obj_set_style_border_width(wifiDot, 0, 0);
  const lv_color_t cloudColor = state.cloudConnected ? active : gray;
  drawPairedCircles(root, cloudX, top ? 31 : 216, cloudColor);
}

const SduiComponent* findComponent(const SduiScreen& screen, const char* id);

void renderSettings(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const SharedRenderState& state, const char* pressedComponentId) {
  using namespace generated_ui;
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);

  const lv_image_dsc_t* neutral = resolveImage == nullptr ? nullptr : resolveImage("settings-neutral");
  if (neutral != nullptr) {
    lv_obj_t* base = lv_image_create(root);
    lv_image_set_src(base, neutral);
    lv_obj_set_pos(base, 0, 0);
    const char* patchId = nullptr;
    int16_t patchX = 0;
    int16_t patchY = 0;
    if (pressedComponentId != nullptr) {
      if (strcmp(pressedComponentId, "settings-wifi") == 0) { patchId = "settings-wifi-pressed"; patchX = 72; patchY = 127; }
      else if (strcmp(pressedComponentId, "settings-bluetooth") == 0) { patchId = "settings-bluetooth-pressed"; patchX = 72; patchY = 181; }
      else if (strcmp(pressedComponentId, "settings-display") == 0) { patchId = "settings-display-pressed"; patchX = 72; patchY = 235; }
      else if (strcmp(pressedComponentId, "settings-sound") == 0) { patchId = "settings-sound-pressed"; patchX = 72; patchY = 289; }
    }
    const lv_image_dsc_t* patch = patchId == nullptr || resolveImage == nullptr ? nullptr : resolveImage(patchId);
    if (patch != nullptr) {
      lv_obj_t* overlay = lv_image_create(root);
      lv_image_set_src(overlay, patch);
      lv_obj_set_pos(overlay, patchX, patchY);
    }
  }

  lv_obj_t* status = makePanel(root, kSettingsStatusX, kSettingsStatusY, kSettingsStatusWidth, kSettingsStatusHeight, kSettingsStatusRadius, lv_color_hex(0x293A50));
  lv_obj_set_style_border_width(status, 1, 0);
  lv_obj_set_style_border_color(status, lv_color_hex(0x9CB3CD), 0);
  lv_obj_set_style_shadow_width(status, 15, 0);
  lv_obj_set_style_shadow_opa(status, LV_OPA_70, 0);
  lv_obj_set_style_shadow_color(status, lv_color_hex(0xA8C9F2), 0);

  lv_obj_t* bluetooth = makeLabel(root, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_20, lv_color_hex(0xF3F6FA));
  lv_obj_set_pos(bluetooth, 148, 43);
  lv_obj_t* wifi = makeLabel(root, LV_SYMBOL_WIFI, &lv_font_montserrat_20, lv_color_hex(0xF3F6FA));
  lv_obj_set_pos(wifi, 180, 43);
  lv_obj_t* time = makeLabel(root, state.timeText == nullptr ? "--:--" : state.timeText, &lv_font_montserrat_20, lv_color_hex(0xF4F7FC));
  lv_obj_set_pos(time, 216, 47);
  drawPairedCircles(root, generated_ui::kSettingsCloudX, generated_ui::kSettingsCloudY, lv_color_hex(0xE6EDF8));
  char batteryText[8] = {};
  snprintf(batteryText, sizeof(batteryText), "%u%%", state.batteryPercent);
  lv_obj_t* battery = makeLabel(root, batteryText, &lv_font_montserrat_14, lv_color_hex(0xDFE8F4));
  lv_obj_set_pos(battery, 301, 48);

  lv_obj_t* gear = makePanel(root, kSettingsGearX - kSettingsGearDiameter / 2, kSettingsGearY - kSettingsGearDiameter / 2, kSettingsGearDiameter, kSettingsGearDiameter, kSettingsGearDiameter / 2, lv_color_hex(0xEEF5FF));
  lv_obj_set_style_shadow_width(gear, 18, 0);
  lv_obj_set_style_shadow_opa(gear, LV_OPA_90, 0);
  lv_obj_set_style_shadow_color(gear, lv_color_hex(0xEAF2FF), 0);
  lv_obj_t* gearGlyph = makeLabel(gear, LV_SYMBOL_SETTINGS, &lv_font_montserrat_22, lv_color_hex(0x66758A));
  lv_obj_center(gearGlyph);
  lv_obj_t* title = makeLabel(root, "SETTINGS", &lv_font_montserrat_28, lv_color_hex(0xF6F7FB));
  lv_obj_set_pos(title, kSettingsTitleX, kSettingsTitleY);

  const char* ids[] = {"settings-wifi", "settings-bluetooth", "settings-display", "settings-sound"};
  const char* fallbackLabels[] = {"WiFi", "Bluetooth", "Display", "Sound & Vibration"};
  for (uint8_t index = 0; index < kSettingsRowCount && neutral == nullptr; ++index) {
    const SduiComponent* component = findComponent(screen, ids[index]);
    const bool active = pressedComponentId != nullptr && strcmp(pressedComponentId, ids[index]) == 0;
    const int16_t x = kSettingsRowX[index];
    const int16_t y = kSettingsRowY[index];
    lv_obj_t* row = makePanel(root, x, y, kSettingsRowWidth[index], kSettingsRowHeight[index], 24, lv_color_hex(active ? 0x087EBD : 0x263342));
    lv_obj_set_style_bg_grad_color(row, lv_color_hex(active ? 0x06446F : 0x101823), 0);
    lv_obj_set_style_bg_grad_dir(row, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(row, active ? 2 : 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(active ? 0x08A8FF : 0x6C7C91), 0);
    lv_obj_set_style_shadow_width(row, active ? 18 : 11, 0);
    lv_obj_set_style_shadow_opa(row, LV_OPA_80, 0);
    lv_obj_set_style_shadow_color(row, lv_color_hex(active ? 0x079BFF : 0xBFD5F5), 0);
    lv_obj_t* iconCircle = makePanel(root, x + generated_ui::kSettingsBubbleXOffset, y + generated_ui::kSettingsBubbleYOffset, generated_ui::kSettingsBubbleDiameter, generated_ui::kSettingsBubbleDiameter, generated_ui::kSettingsBubbleRadius, lv_color_hex(active ? generated_ui::kSettingsBubbleActiveInner : generated_ui::kSettingsBubbleInner));
    lv_obj_set_style_bg_grad_color(iconCircle, lv_color_hex(active ? generated_ui::kSettingsBubbleActiveOuter : generated_ui::kSettingsBubbleOuter), 0);
    lv_obj_set_style_bg_grad_dir(iconCircle, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_width(iconCircle, 18, 0);
    lv_obj_set_style_shadow_opa(iconCircle, LV_OPA_90, 0);
    lv_obj_set_style_shadow_color(iconCircle, lv_color_hex(0xE7F0FF), 0);
    drawSettingsIcon(root, index, x + generated_ui::kSettingsIconCenterXOffset, y + generated_ui::kSettingsIconCenterYOffset / 2, active);
    lv_obj_t* label = makeLabel(root, component == nullptr ? fallbackLabels[index] : component->text, &lv_font_montserrat_20, lv_color_hex(0xF4F6FA));
    lv_obj_set_pos(label, x + 63, y + 13);
    drawChevron(root, x + generated_ui::kSettingsChevronXOffset, y + generated_ui::kSettingsChevronYOffset);
  }

  lv_obj_t* sliderMask = makePanel(root, 98, 336, 272, 84, 0, lv_color_black());
  lv_obj_set_style_border_width(sliderMask, 0, 0);
  lv_obj_t* slider = lv_slider_create(root);
  lv_obj_set_pos(slider, kSettingsSliderX, kSettingsSliderY);
  lv_obj_set_size(slider, kSettingsSliderWidth, kSettingsSliderHeight);
  lv_slider_set_range(slider, 5, 100);
  lv_slider_set_value(slider, state.brightnessPercent, LV_ANIM_OFF);
  lv_obj_remove_flag(slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(slider, kSettingsSliderRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(kSettingsSliderTrack), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, kSettingsSliderRadius, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(kSettingsSliderFillStart), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(slider, lv_color_hex(kSettingsSliderFillEnd), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(kSettingsSliderAccent), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_width(slider, kSettingsSliderKnobRadius * 2, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSettingsSliderKnobRadius * 2, LV_PART_KNOB);

  lv_obj_t* brightness = makeLabel(root, "Brightness", &lv_font_montserrat_16, lv_color_hex(0xF2F3F7));
  lv_obj_set_width(brightness, 160);
  lv_obj_set_style_text_align(brightness, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(brightness, kSettingsBrightnessLabelX - 80, kSettingsBrightnessLabelY);
}

const SduiComponent* findComponent(const SduiScreen& screen, const char* id) {
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    if (strcmp(screen.components[index].id, id) == 0) return &screen.components[index];
  }
  return nullptr;
}

void renderSpotify(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const char* pressedComponentId) {
  lv_obj_t* arc = lv_arc_create(root);
  lv_obj_set_pos(arc, 42, 42);
  lv_obj_set_size(arc, 382, 382);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, 100);
  const SduiComponent* volume = findComponent(screen, "spotify-volume");
  lv_arc_set_value(arc, volume == nullptr ? 0 : volume->value);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 11, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x20252A), LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 11, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x0A84FF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_bg_color(arc, lv_color_white(), LV_PART_KNOB);

  const char* symbols[] = {LV_SYMBOL_LIST, LV_SYMBOL_SHUFFLE, LV_SYMBOL_OK};
  const int16_t actionX[] = {120, 221, 322};
  for (uint8_t index = 0; index < 3; ++index) {
    lv_obj_t* action = makeLabel(root, symbols[index], &lv_font_montserrat_24, lv_color_white());
    lv_obj_set_pos(action, actionX[index], 82);
  }

  if (resolveImage != nullptr) {
    const lv_image_dsc_t* coverSource = resolveImage("spotify-large");
    if (coverSource != nullptr) {
      lv_obj_t* cover = lv_image_create(root);
      lv_image_set_src(cover, coverSource);
      lv_obj_set_style_image_opa(cover, LV_OPA_COVER, 0);
      lv_obj_set_pos(cover, 184, 137);
    }
  }
  const SduiComponent* track = findComponent(screen, "track");
  const SduiComponent* artist = findComponent(screen, "artist");
  lv_obj_t* trackLabel = makeLabel(root, track == nullptr ? "Sin reproducción" : track->text, &lv_font_montserrat_22, lv_color_white());
  lv_obj_set_width(trackLabel, 320);
  lv_obj_set_style_text_align(trackLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(trackLabel, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_t* artistLabel = makeLabel(root, artist == nullptr ? "Abre Spotify" : artist->text, &lv_font_montserrat_14, lv_color_hex(0xD1D1D6));
  lv_obj_align(artistLabel, LV_ALIGN_TOP_MID, 0, 286);

  const SduiComponent* toggle = findComponent(screen, "spotify-toggle");
  const bool playing = toggle != nullptr && strcmp(toggle->icon, "pause") == 0;
  const int16_t controlX[] = {83, 191, 313};
  const int16_t controlSize[] = {70, 84, 70};
  const char* controlSymbols[] = {LV_SYMBOL_PREV, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, LV_SYMBOL_NEXT};
  for (uint8_t index = 0; index < 3; ++index) {
    const bool pressed = pressedComponentId != nullptr && strcmp(pressedComponentId, index == 0 ? "spotify-prev" : index == 1 ? "spotify-toggle" : "spotify-next") == 0;
    lv_obj_t* control = makeLabel(root, controlSymbols[index], controlSize[index] > 70 ? &lv_font_montserrat_32 : &lv_font_montserrat_24, pressed ? lv_color_hex(0xA7A7A7) : lv_color_white());
    lv_obj_set_pos(control, controlX[index] + controlSize[index] / 2 - 16, 326);
  }
  const SduiComponent* seek = findComponent(screen, "spotify-seek");
  lv_obj_t* bar = lv_bar_create(root);
  lv_obj_set_pos(bar, 83, 392);
  lv_obj_set_size(bar, 300, 8);
  lv_bar_set_value(bar, seek == nullptr ? 0 : seek->value, LV_ANIM_OFF);
  lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x343438), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_INDICATOR);
}

}  // namespace

void renderSharedLvglScreen(lv_obj_t* root, const SduiScreen& screen, SharedImageResolver resolveImage, const char* pressedComponentId, const SharedRenderState* state) {
  lv_obj_clean(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(root, color565(screen.background), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root, 0, 0);

  if (state != nullptr && strcmp(screen.screenId, "device-control-center") == 0) {
    renderSettings(root, screen, resolveImage, *state, pressedComponentId);
    return;
  }

  if (strcmp(screen.screenId, "spotify-player") == 0) {
    renderSpotify(root, screen, resolveImage, pressedComponentId);
    return;
  }

  const bool home = strcmp(screen.screenId, "sky-home") == 0;
  const bool flower = strcmp(screen.screenId, "sky-clock") == 0 || strcmp(screen.screenId, "sky-voltage") == 0 || strcmp(screen.screenId, "sky-weather") == 0;
  const bool backgroundEnabled = state == nullptr || state->backgroundMode != 0;
  // The reference home artwork contains the static glass controls. Keep the
  // SDUI components as live hit targets, but let the shared artwork supply
  // their pixels whenever the home screen is in its reference presentation.
  const bool referenceHome = home && (state == nullptr || state->backgroundMode != 2);
  if (resolveImage != nullptr && home && referenceHome) {
    const lv_image_dsc_t* source = resolveImage(state != nullptr && state->backgroundMode == 2 ? "chevron" : "home");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  } else if (resolveImage != nullptr && home && backgroundEnabled) {
    const lv_image_dsc_t* source = resolveImage("chevron");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  } else if (resolveImage != nullptr && flower && backgroundEnabled) {
    const lv_image_dsc_t* source = resolveImage("flower");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  }

  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (referenceHome) continue;
    if (component.type == ComponentType::kText) {
      lv_obj_t* text = makeLabel(root, component.text, fontForSize(component.size), color565(component.color));
      if (component.align == TextAlign::kCenter) {
        // LVGL's intrinsic label width is not valid until layout has run.
        // Positioning by that width caused the simulator and device to drift.
        // A full-width, centered label uses the same text-align operation in
        // both the native and WASM LVGL renderers.
        lv_obj_set_width(text, 466);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(text, component.x - 233, component.y);
        if (strcmp(screen.screenId, "sky-clock") == 0 && strcmp(component.id, "sky-time") == 0) {
          lv_obj_update_layout(text);
          lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
        }
      } else {
        lv_obj_set_pos(text, component.x, component.y);
      }
    } else if (component.type == ComponentType::kButton) {
      lv_obj_t* button = home
          ? makeHomeButton(root, component, pressedComponentId)
          : makeButton(root, component, pressedComponentId);
      const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage(component.id);
      if (strcmp(component.icon, "x") == 0) {
        lv_obj_t* placeholder = makeLabel(button, "X", &lv_font_montserrat_28, color565(component.color));
        lv_obj_center(placeholder);
      } else if (strcmp(component.icon, "gear") == 0) {
        drawGear(button, color565(component.color));
      } else if (source != nullptr && strcmp(component.id, "open-spotify") != 0) {
        lv_obj_t* image = lv_image_create(button);
        lv_image_set_src(image, source);
        lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0);
        lv_obj_center(image);
      } else if (strcmp(component.icon, "usb") == 0) {
        drawUsb(button, color565(component.color));
      } else if (strcmp(component.icon, "whatsapp") == 0) {
        drawWhatsapp(button, color565(component.color));
      } else {
        const lv_font_t* buttonFont = strcmp(component.id, "open-spotify") == 0
            ? &lv_font_montserrat_14
            : &lv_font_montserrat_20;
        lv_obj_t* text = makeLabel(button, component.text, buttonFont, color565(component.color));
        lv_obj_center(text);
      }
    } else if (component.type == ComponentType::kSlider) {
      lv_obj_t* slider = lv_slider_create(root);
      lv_obj_set_pos(slider, component.x, component.y);
      lv_obj_set_size(slider, component.width, component.height);
      lv_slider_set_range(slider, 0, component.maximum);
      lv_slider_set_value(slider, component.value, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(slider, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
      lv_obj_set_style_bg_color(slider, color565(component.color), LV_PART_INDICATOR);
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
  if (state != nullptr && (strcmp(screen.screenId, "sky-home") == 0 || strcmp(screen.screenId, "sky-clock") == 0 || strcmp(screen.screenId, "sky-voltage") == 0 || strcmp(screen.screenId, "sky-weather") == 0 || strcmp(screen.screenId, "device-control-center") == 0)) {
    drawStatus(root, screen, *state);
  }
}

}  // namespace leopold
