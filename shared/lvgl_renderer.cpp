#include "lvgl_renderer.h"

#include <cstring>
#include <cstdio>

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
  lv_obj_t* object = makePanel(parent, component.x, component.y, component.width, component.height, component.height / 2, pressed ? lv_color_hex(0x2C2C30) : lv_color_hex(0x141416));
  lv_obj_set_style_border_width(object, 2, 0);
  lv_obj_set_style_border_color(object, color565(component.color), 0);
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
  lv_obj_t* bubble = makePanel(parent, 17, 17, 38, 34, 14, color);
  lv_obj_t* tail = makePanel(parent, 20, 43, 10, 8, 3, color);
  LV_UNUSED(bubble);
  LV_UNUSED(tail);
}

void drawStatus(lv_obj_t* root, const SharedRenderState& state) {
  const lv_color_t gray = lv_color_hex(0x7B7D7B);
  const lv_color_t green = lv_color_hex(0x30D158);
  lv_obj_t* battery = makeLabel(root, state.timeText == nullptr ? "--:--" : state.timeText, &lv_font_montserrat_14, gray);
  lv_obj_set_pos(battery, 214, 218);
  lv_obj_t* batteryPercent = makeLabel(root, "", &lv_font_montserrat_14, gray);
  char batteryText[8] = {};
  snprintf(batteryText, sizeof(batteryText), "%u%%", state.batteryPercent);
  lv_label_set_text(batteryPercent, batteryText);
  lv_obj_set_pos(batteryPercent, 288, 218);

  const auto glyphLine = [root](const lv_point_precise_t* points, uint32_t count, int16_t width, lv_color_t color) {
    lv_obj_t* line = lv_line_create(root);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  };
  const lv_color_t bluetoothColor = state.bluetoothEnabled ? green : gray;
  static const lv_point_precise_t bluetooth[] = {{165, 224}, {165, 242}, {171, 236}, {159, 228}, {171, 224}, {159, 238}, {165, 242}};
  glyphLine(bluetooth, 7, 2, bluetoothColor);
  const lv_color_t wifiColor = state.wifiConnected ? green : gray;
  for (int16_t radius = 10; radius >= 6; radius -= 4) {
    lv_obj_t* arc = lv_arc_create(root);
    lv_obj_set_pos(arc, 190 - radius, 223);
    lv_obj_set_size(arc, radius * 2, radius * 2);
    lv_arc_set_rotation(arc, 220);
    lv_arc_set_bg_angles(arc, 220, 320);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, wifiColor, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
  }
  lv_obj_t* wifiDot = lv_obj_create(root);
  lv_obj_set_size(wifiDot, 4, 4); lv_obj_set_pos(wifiDot, 188, 236);
  lv_obj_set_style_radius(wifiDot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(wifiDot, wifiColor, 0); lv_obj_set_style_bg_opa(wifiDot, LV_OPA_COVER, 0); lv_obj_set_style_border_width(wifiDot, 0, 0);
  const lv_color_t cloudColor = state.cloudConnected ? green : gray;
  lv_obj_t* cloud = makePanel(root, 318, 232, 24, 8, 4, cloudColor);
  lv_obj_t* cloudLeft = makePanel(root, 321, 228, 10, 10, 5, cloudColor);
  lv_obj_t* cloudMid = makePanel(root, 327, 225, 14, 14, 7, cloudColor);
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
    const lv_image_dsc_t* source = resolveImage("home");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  } else if (resolveImage != nullptr && flower) {
    const lv_image_dsc_t* source = resolveImage("flower");
    if (source != nullptr) { lv_obj_t* image = lv_image_create(root); lv_image_set_src(image, source); lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0); lv_obj_set_pos(image, 0, 0); }
  }

  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (component.type == ComponentType::kText) {
      lv_obj_t* text = makeLabel(root, component.text, fontForSize(component.size), color565(component.color));
      lv_obj_set_pos(text, component.x, component.y);
      if (component.align == TextAlign::kCenter) lv_obj_set_style_translate_x(text, -lv_obj_get_width(text) / 2, 0);
    } else if (component.type == ComponentType::kButton) {
      lv_obj_t* button = makeButton(root, component, pressedComponentId);
      const lv_image_dsc_t* source = resolveImage == nullptr ? nullptr : resolveImage(component.id);
      if (source != nullptr) {
        lv_obj_t* image = lv_image_create(button);
        lv_image_set_src(image, source);
        lv_obj_set_style_image_opa(image, LV_OPA_COVER, 0);
        if (strcmp(component.id, "open-spotify") == 0) lv_image_set_scale(image, 700);
        lv_obj_center(image);
      } else if (strcmp(component.icon, "usb") == 0) {
        drawUsb(button, color565(component.color));
      } else if (strcmp(component.icon, "whatsapp") == 0) {
        drawWhatsapp(button, color565(component.color));
      } else {
        lv_obj_t* text = makeLabel(button, component.text, &lv_font_montserrat_20, color565(component.color));
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
    drawStatus(root, *state);
  }
}

}  // namespace leopold
