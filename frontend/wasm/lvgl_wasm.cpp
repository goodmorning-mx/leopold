#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "lvgl_renderer.h"
#include "display_menu.h"
#include "wasm_asset_index.h"

static constexpr int kWidth = 466;
static constexpr int kHeight = 466;
static lv_display_t* display = nullptr;
static uint16_t framebuffer[kWidth * kHeight];
static uint16_t drawBuffer[kWidth * kHeight];
static uint8_t statusBackdropPixels[kWidth * kHeight * 2];
static lv_image_dsc_t statusBackdrop;
static leopold::SduiScreen screen;
static char pressedId[leopold::kMaxComponentId + 1];
static lv_image_dsc_t assets[leopold::wasm::kAssetCount];
static bool assetReady[leopold::wasm::kAssetCount];
static char renderTime[16] = "--:--";
static char renderDate[16] = "OCT 26";
static leopold::SharedRenderState renderState = {};

static void flush(lv_display_t* target, const lv_area_t* area, uint8_t* pixels) {
  const int32_t width = lv_area_get_width(area);
  const int32_t height = lv_area_get_height(area);
  for (int32_t row = 0; row < height; ++row) memcpy(&framebuffer[(area->y1 + row) * kWidth + area->x1], pixels + row * width * 2, width * 2);
  lv_display_flush_ready(target);
}

static int assetIndex(const char* key) {
  for (int index = 0; index < leopold::wasm::kAssetCount; ++index)
    if (strcmp(leopold::wasm::kAssetKeys[index], key) == 0) return index;
  return -1;
}

static const lv_image_dsc_t* resolveImage(const char* key) {
  if (strcmp(key, "open-voltage") == 0) return nullptr;
  const int index = assetIndex(key);
  return index >= 0 && assetReady[index] ? &assets[index] : nullptr;
}

static void append(const leopold::SduiComponent& component) {
  if (screen.componentCount < leopold::kMaxSduiComponents) screen.components[screen.componentCount++] = component;
}

extern "C" {
int lvgl_wasm_display_max_scroll(const char* expanded, int country, int keypad) {
  leopold::display_menu::State s;
  s.expanded = leopold::display_menu::rowIndex(expanded); s.country = country; s.keypad = keypad != 0;
  return leopold::display_menu::maxScroll(s);
}
const char* lvgl_wasm_display_model(const char* expanded, int scroll, int background, int galleryOffset,
    int dark, int accent, int metric, int spanish, int country, int keypad,
    const char* location, const char* postal, const char* status, int clock24,
    int face, int clockBackground, int clockWeather, int clockView, const char* action) {
  using namespace leopold::display_menu;
  State s; s.expanded = rowIndex(expanded); s.scroll = scroll; s.background = background; s.backgroundGalleryOffset = galleryOffset;
  s.dark = dark != 0; s.accent = clamp(accent, 0, 6); s.metric = metric != 0;
  s.spanish = spanish != 0; s.country = clamp(country, 0, 2); s.keypad = keypad != 0;
  copy(s.location, sizeof(s.location), location); copy(s.postal, sizeof(s.postal), postal);
  copy(s.status, sizeof(s.status), status); s.clock24 = clock24 != 0; s.clockFace = face;
  s.clockBackground = clockBackground; s.clockWeather = clockWeather != 0;
  const Effect effect = act(s, action, clockView != 0);
  static leopold::SduiScreen model;
  static char output[8192];
  if (clockView) buildClock(model, s); else build(model, s);
  if (!json(output, sizeof(output), model, s, effect)) return "";
  return output;
}

void lvgl_wasm_init() {
  lv_init();
  display = lv_display_create(kWidth, kHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, drawBuffer, nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_FULL);
}

void lvgl_wasm_set_asset(int index, const uint8_t* data, uint32_t size, uint16_t width, uint16_t height) {
  if (index < 0 || index >= leopold::wasm::kAssetCount || data == nullptr || size == 0) return;
  assets[index] = {
      .header = {
          .magic = LV_IMAGE_HEADER_MAGIC,
          .cf = LV_COLOR_FORMAT_RGB565A8,
          .flags = 0,
          .w = width,
          .h = height,
          .stride = static_cast<uint16_t>(width * 2),
          .reserved_2 = 0,
      },
      .data_size = size,
      .data = data,
  };
  assetReady[index] = true;
}

void lvgl_wasm_clear_asset(int index) {
  if (index < 0 || index >= leopold::wasm::kAssetCount || !assetReady[index]) return;
  lv_image_cache_drop(&assets[index]);
  assets[index].data = nullptr;
  assets[index].data_size = 0;
  assetReady[index] = false;
}

void lvgl_wasm_set_backdrop(const uint8_t* pixels, uint32_t size) {
  if (pixels == nullptr || size != sizeof(framebuffer)) return;
  memcpy(statusBackdropPixels, pixels, size);
  statusBackdrop = {
      .header = {
          .magic = LV_IMAGE_HEADER_MAGIC,
          .cf = LV_COLOR_FORMAT_RGB565,
          .flags = 0,
          .w = kWidth,
          .h = kHeight,
          .stride = static_cast<uint16_t>(kWidth * 2),
          .reserved_2 = 0,
      },
      .data_size = size,
      .data = statusBackdropPixels,
  };
}

void lvgl_wasm_begin(const char* id, uint16_t background) {
  memset(&screen, 0, sizeof(screen));
  strncpy(screen.screenId, id == nullptr ? "" : id, sizeof(screen.screenId) - 1);
  screen.background = background;
}

void lvgl_wasm_add_text(const char* id, const char* value, int16_t x, int16_t y, uint8_t size, uint16_t color, uint8_t align) {
  leopold::SduiComponent component; component.type = leopold::ComponentType::kText;
  strncpy(component.id, id == nullptr ? "" : id, sizeof(component.id) - 1);
  strncpy(component.text, value == nullptr ? "" : value, sizeof(component.text) - 1);
  component.x = x; component.y = y; component.size = size; component.color = color;
  component.align = align == 1 ? leopold::TextAlign::kCenter : align == 2 ? leopold::TextAlign::kRight : leopold::TextAlign::kLeft;
  append(component);
}

void lvgl_wasm_add_button(const char* id, const char* value, const char* icon, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
  leopold::SduiComponent component; component.type = leopold::ComponentType::kButton;
  strncpy(component.id, id == nullptr ? "" : id, sizeof(component.id) - 1);
  strncpy(component.text, value == nullptr ? "" : value, sizeof(component.text) - 1);
  strncpy(component.icon, icon == nullptr ? "" : icon, sizeof(component.icon) - 1);
  component.x = x; component.y = y; component.width = width; component.height = height; component.color = color;
  append(component);
}

void lvgl_wasm_add_slider(const char* id, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t value, uint16_t maximum, uint16_t color) {
  leopold::SduiComponent component; component.type = leopold::ComponentType::kSlider;
  strncpy(component.id, id == nullptr ? "" : id, sizeof(component.id) - 1);
  component.x = x; component.y = y; component.width = width; component.height = height; component.value = value; component.maximum = maximum; component.color = color;
  append(component);
}

void lvgl_wasm_set_pressed(const char* id) { strncpy(pressedId, id == nullptr ? "" : id, sizeof(pressedId) - 1); pressedId[sizeof(pressedId) - 1] = '\0'; }

void lvgl_wasm_set_state(const char* timeText, const char* dateText, uint8_t battery, uint8_t clockSecond, bool wifi, bool cloud, bool bluetooth, bool bluetoothConnected, uint8_t brightness, uint8_t backgroundMode, uint8_t backgroundArt, bool appearanceDark, uint8_t clockBackground, uint8_t clockFace, bool clockShowWeather, uint8_t accentIndex) {
  strncpy(renderTime, timeText == nullptr ? "--:--" : timeText, sizeof(renderTime) - 1);
  renderTime[sizeof(renderTime) - 1] = '\0';
  renderState.timeText = renderTime;
  strncpy(renderDate, dateText == nullptr ? "" : dateText, sizeof(renderDate) - 1);
  renderDate[sizeof(renderDate) - 1] = '\0';
  renderState.dateText = renderDate;
  renderState.batteryPercent = battery;
  renderState.clockSecond = clockSecond;
  renderState.wifiConnected = wifi;
  renderState.cloudConnected = cloud;
  renderState.bluetoothEnabled = bluetooth;
  renderState.bluetoothConnected = bluetoothConnected;
  renderState.brightnessPercent = brightness;
  renderState.backgroundMode = backgroundMode;
  renderState.backgroundArt = backgroundArt;
  renderState.appearanceDark = appearanceDark;
  renderState.clockBackground = clockBackground;
  renderState.clockFace = clockFace;
  renderState.clockShowWeather = clockShowWeather;
  renderState.accentIndex = accentIndex;
}

void lvgl_wasm_render() {
  memset(framebuffer, 0, sizeof(framebuffer));
  if (strcmp(screen.screenId, "shared-status-overlay") == 0) {
    lv_obj_t* root = lv_screen_active();
    lv_obj_clean(root);
    lv_obj_t* backdrop = lv_image_create(root);
    lv_image_set_src(backdrop, &statusBackdrop);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    leopold::renderSharedLvglStatusBanner(root, renderState, resolveImage);
  } else {
    leopold::renderSharedLvglScreen(lv_screen_active(), screen, resolveImage, pressedId, &renderState);
  }
  lv_obj_update_layout(lv_screen_active());
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(display);
}

void lvgl_wasm_render_lifecycle(int kind, int step) {
  memset(framebuffer, 0, sizeof(framebuffer));
  lv_obj_t* root = lv_screen_active();
  leopold::renderFirmwareLifecyclePreview(root,
      static_cast<leopold::FirmwareLifecyclePreview>(kind), static_cast<uint8_t>(step));
  lv_obj_update_layout(root);
  lv_obj_invalidate(root);
  lv_refr_now(display);
}

uint32_t lvgl_wasm_framebuffer() { return (uint32_t)(uintptr_t)framebuffer; }
uint32_t lvgl_wasm_framebuffer_size() { return sizeof(framebuffer); }
}
