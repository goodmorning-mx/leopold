#include "lvgl_renderer.h"

#include <cstring>
#include <cstdio>
#include <cmath>

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

void drawGear(lv_obj_t* parent, lv_color_t color) {
  lv_obj_t* gear = makeLabel(parent, LV_SYMBOL_SETTINGS, &lv_font_montserrat_32, color);
  lv_obj_center(gear);
}

void drawStatus(lv_obj_t* root, const SduiScreen& screen, const SharedRenderState& state) {
  const lv_color_t gray = lv_color_hex(0x7B7D7B);
  const lv_color_t green = lv_color_hex(0x30D158);
  const bool top = strcmp(screen.screenId, "sky-clock") == 0 ||
                   strcmp(screen.screenId, "sky-weather") == 0;
  const bool clock = strcmp(screen.screenId, "sky-clock") == 0;
  const bool showTime = !clock;
  // The clock does not repeat its primary time readout in the status pill.
  // Keep that pill compact instead of leaving a visual hole where the time
  // used to be. Weather retains the small time, but both top pills are inset
  // from the circular display edge so their rounded corners remain visible.
  const int16_t surfaceX = clock ? 143 : top ? 110 : 136;
  const int16_t surfaceY = top ? 18 : 211;
  const int16_t surfaceWidth = clock ? 190 : top ? 246 : 216;
  const int16_t surfaceHeight = top ? 34 : 39;
  const int16_t baseX = clock ? 153 : top ? 119 : 145;
  const int16_t wifiCenterX = clock ? 184 : top ? 153 : 181;
  const int16_t cloudX = clock ? 209 : top ? 178 : 194;
  const int16_t timeX = clock ? 0 : top ? 204 : 237;
  const int16_t batteryX = clock ? 279 : top ? 307 : 300;
  lv_obj_t* statusSurface = makePanel(root, surfaceX, surfaceY, surfaceWidth, surfaceHeight, surfaceHeight / 2, lv_color_hex(0x090B0E));
  lv_obj_set_style_bg_opa(statusSurface, LV_OPA_80, 0);
  lv_obj_set_style_border_width(statusSurface, 1, 0);
  lv_obj_set_style_border_color(statusSurface, lv_color_hex(0x252A30), 0);
  if (showTime) {
    lv_obj_t* time = makeLabel(root, state.timeText == nullptr ? "--:--" : state.timeText, &lv_font_montserrat_14, gray);
    if (top) {
      lv_obj_set_width(time, 78);
      lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_set_pos(time, timeX, top ? 25 : 218);
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
  const lv_color_t bluetoothColor = state.bluetoothEnabled ? green : gray;
  lv_obj_t* bluetooth = makeLabel(root, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_24, bluetoothColor);
  lv_obj_set_pos(bluetooth, baseX, top ? 25 : 219);
  const lv_color_t wifiColor = state.wifiConnected ? green : gray;
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
      points[index].y = static_cast<int16_t>(std::lround((top ? 43 : 237) + std::sin(radians) * radius));
    }
    glyphLine(points, 13, 2, wifiColor);
  }
  lv_obj_t* wifiDot = lv_obj_create(root);
  lv_obj_set_size(wifiDot, 4, 4); lv_obj_set_pos(wifiDot, wifiCenterX - 2, top ? 41 : 235);
  lv_obj_set_style_radius(wifiDot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(wifiDot, wifiColor, 0); lv_obj_set_style_bg_opa(wifiDot, LV_OPA_COVER, 0); lv_obj_set_style_border_width(wifiDot, 0, 0);
  const lv_color_t cloudColor = state.cloudConnected ? green : gray;
  lv_obj_t* cloud = makePanel(root, cloudX, top ? 38 : 232, 24, 8, 4, cloudColor);
  lv_obj_t* cloudLeft = makePanel(root, cloudX + 3, top ? 34 : 228, 10, 10, 5, cloudColor);
  lv_obj_t* cloudMid = makePanel(root, cloudX + 9, top ? 31 : 225, 14, 14, 7, cloudColor);
  LV_UNUSED(cloud); LV_UNUSED(cloudLeft); LV_UNUSED(cloudMid);
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

  if (strcmp(screen.screenId, "spotify-player") == 0) {
    renderSpotify(root, screen, resolveImage, pressedComponentId);
    return;
  }

  const bool home = strcmp(screen.screenId, "sky-home") == 0;
  const bool flower = strcmp(screen.screenId, "sky-clock") == 0 || strcmp(screen.screenId, "sky-voltage") == 0 || strcmp(screen.screenId, "sky-weather") == 0;
  if (resolveImage != nullptr && home) {
    const lv_image_dsc_t* source = resolveImage(state != nullptr && state->backgroundMode == 2 ? "chevron" : "home");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  } else if (resolveImage != nullptr && flower) {
    const lv_image_dsc_t* source = resolveImage("flower");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  }

  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
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
      lv_obj_t* button = makeButton(root, component, pressedComponentId);
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
