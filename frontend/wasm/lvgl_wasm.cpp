#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "lvgl_renderer.h"

static constexpr int kWidth = 466;
static constexpr int kHeight = 466;
static lv_display_t* display = nullptr;
static uint16_t framebuffer[kWidth * kHeight];
static uint16_t drawBuffer[kWidth * kHeight];
static leopold::SduiScreen screen;
static char pressedId[leopold::kMaxComponentId + 1];
static lv_image_dsc_t assets[24];
static bool assetReady[24];
static char renderTime[16] = "--:--";
static leopold::SharedRenderState renderState = {renderTime, 0, false, false, false, false, 0, 1};

static void flush(lv_display_t* target, const lv_area_t* area, uint8_t* pixels) {
  const int32_t width = lv_area_get_width(area);
  const int32_t height = lv_area_get_height(area);
  for (int32_t row = 0; row < height; ++row) memcpy(&framebuffer[(area->y1 + row) * kWidth + area->x1], pixels + row * width * 2, width * 2);
  lv_display_flush_ready(target);
}

static int assetIndex(const char* key) {
  static const char* keys[] = {
      "home", "flower", "chevron", "open-clock", "open-weather",
      "open-navigation", "open-whatsapp", "open-spotify", "spotify-large",
      "settings-neutral", "settings-pressed-border-mask", "settings-wifi-pressed-icon-mask", "settings-wifi-pressed-chevron-mask",
      "settings-bluetooth-pressed-icon-mask", "settings-bluetooth-pressed-chevron-mask",
      "settings-display-pressed-icon-mask", "settings-display-pressed-chevron-mask",
      "settings-sound-pressed-icon-mask", "settings-sound-pressed-chevron-mask",
      "settings-status-bluetooth-active",
      "settings-status-wifi-active", "settings-status-cloud-active", "settings-gears-base", "settings-gears-wallpaper",
  };
  for (int index = 0; index < 24; ++index) if (strcmp(keys[index], key) == 0) return index;
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
void lvgl_wasm_init() {
  lv_init();
  display = lv_display_create(kWidth, kHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, drawBuffer, nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_FULL);
}

void lvgl_wasm_set_asset(int index, const uint8_t* data, uint32_t size, uint16_t width, uint16_t height) {
  if (index < 0 || index >= 24 || data == nullptr || size == 0) return;
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

void lvgl_wasm_begin(const char* id, uint16_t background) {
  memset(&screen, 0, sizeof(screen));
  strncpy(screen.screenId, id == nullptr ? "" : id, sizeof(screen.screenId) - 1);
  screen.background = background;
}

void lvgl_wasm_add_text(const char* value, int16_t x, int16_t y, uint8_t size, uint16_t color, uint8_t align) {
  leopold::SduiComponent component; component.type = leopold::ComponentType::kText;
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

void lvgl_wasm_set_state(const char* timeText, uint8_t battery, bool wifi, bool cloud, bool bluetooth, bool bluetoothConnected, uint8_t brightness, uint8_t backgroundMode) {
  strncpy(renderTime, timeText == nullptr ? "--:--" : timeText, sizeof(renderTime) - 1);
  renderTime[sizeof(renderTime) - 1] = '\0';
  renderState.timeText = renderTime;
  renderState.batteryPercent = battery;
  renderState.wifiConnected = wifi;
  renderState.cloudConnected = cloud;
  renderState.bluetoothEnabled = bluetooth;
  renderState.bluetoothConnected = bluetoothConnected;
  renderState.brightnessPercent = brightness;
  renderState.backgroundMode = backgroundMode;
}

void lvgl_wasm_render() {
  memset(framebuffer, 0, sizeof(framebuffer));
  leopold::renderSharedLvglScreen(lv_screen_active(), screen, resolveImage, pressedId, &renderState);
  lv_obj_update_layout(lv_screen_active());
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(display);
}

uint32_t lvgl_wasm_framebuffer() { return (uint32_t)(uintptr_t)framebuffer; }
uint32_t lvgl_wasm_framebuffer_size() { return sizeof(framebuffer); }
}
