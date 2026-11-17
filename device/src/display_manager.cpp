#include "display_manager.h"

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <cmath>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <qrcode.h>
#include <time.h>

#include "board_config.h"
#include "sky_assets.h"
#include "generated_firmware_ui.h"
#include "lvgl_renderer.h"
#include "memory_budget.h"

namespace leopold {
namespace {

using namespace board;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kMorning = 0xFD20;
constexpr uint16_t kBlue = 0x3D7F;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kGray = 0x7BEF;
constexpr uint16_t kYellow = 0xFEC0;

Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    kDisplayCs,
    kDisplayClock,
    kDisplayData0,
    kDisplayData1,
    kDisplayData2,
    kDisplayData3);

// The validated 1.43-B unit identifies as SH8601/CO5300 and requires the
// CO5300 init sequence. Other revisions may need Arduino_SH8601 instead.
Arduino_CO5300* display = new Arduino_CO5300(
    bus,
    kDisplayReset,
    0,
    kScreenWidth,
    kScreenHeight,
    6,
    0,
    0,
    0);
Preferences displayPreferences;
bool displayPreferencesReady = false;
lv_display_t* lvglDisplay = nullptr;
uint32_t lvglFlushCallbacks = 0;
uint64_t lvglFlushPixels = 0;
uint8_t* lvglDrawBuffer = nullptr;
EXT_RAM_BSS_ATTR uint16_t* lvglCaptureBuffer = nullptr;

constexpr size_t kLvglBufferRows = 48;
constexpr size_t kLvglBufferBytes =
    kScreenWidth * kLvglBufferRows * sizeof(lv_color16_t);

uint32_t lvglTick() {
  return millis();
}

void lvglFlush(
    lv_display_t* target,
    const lv_area_t* area,
    uint8_t* pixels) {
  const int32_t width = lv_area_get_width(area);
  const int32_t height = lv_area_get_height(area);
  display->draw16bitRGBBitmap(
      area->x1,
      area->y1,
      reinterpret_cast<uint16_t*>(pixels),
      width,
      height);
  ++lvglFlushCallbacks;
  lvglFlushPixels += static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
  if (lvglCaptureBuffer != nullptr) {
    for (int32_t row = 0; row < height; ++row) {
      memcpy(
          lvglCaptureBuffer + (area->y1 + row) * kScreenWidth + area->x1,
          reinterpret_cast<const uint16_t*>(pixels) + row * width,
          static_cast<size_t>(width) * sizeof(uint16_t));
    }
  }
  lv_display_flush_ready(target);
}

lv_color_t lvColor(uint16_t rgb565) {
  const uint8_t red = (rgb565 >> 11) & 0x1F;
  const uint8_t green = (rgb565 >> 5) & 0x3F;
  const uint8_t blue = rgb565 & 0x1F;
  return lv_color_make(
      (red << 3) | (red >> 2),
      (green << 2) | (green >> 4),
      (blue << 3) | (blue >> 2));
}

lv_obj_t* cleanLvglScreen(uint16_t background = kBlack) {
  lv_obj_t* root = lv_screen_active();
  lv_obj_clean(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(root, lvColor(background), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  return root;
}

void drawSettingsWallpaper(lv_obj_t* root) {
  const lv_image_dsc_t* wallpaper = sky::resolveAsset("settings-gears-wallpaper");
  if (wallpaper == nullptr || wallpaper->data == nullptr) return;
  lv_obj_t* image = lv_image_create(root);
  lv_image_set_src(image, wallpaper);
  lv_obj_set_pos(image, 0, 0);
  lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t* createLabel(
    lv_obj_t* parent,
    const char* text,
    const lv_font_t* font,
    lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text == nullptr ? "" : text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
  return label;
}

lv_obj_t* createRoundPanel(
    lv_obj_t* parent,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    int16_t radius,
    lv_color_t color) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, width, height);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(panel, radius, 0);
  lv_obj_set_style_bg_color(panel, color, 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  return panel;
}

const lv_image_dsc_t* sharedImageResolver(const char* id) {
  return sky::resolveAsset(id);
}

// Explicit sampled arches keep the complete Wi-Fi mark independent of font glyph coverage.
void drawWifiGlyphOn(lv_obj_t* parent, int16_t centerX, int16_t arcCenterY,
                     int16_t radius, lv_color_t color, uint8_t slot) {
  static lv_point_precise_t points[6][3][13] = {};
  static const int8_t scales[] = {10, 7, 4};
  if (slot >= 6) slot = 0;
  for (uint8_t arc = 0; arc < 3; ++arc) {
    const int16_t arcRadius = static_cast<int16_t>(radius * scales[arc] / 10);
    for (int index = 0; index <= 12; ++index) {
      const double degrees = 210.0 + (120.0 * index / 12.0);
      const double radians = degrees * 3.14159265358979323846 / 180.0;
      points[slot][arc][index].x = static_cast<int16_t>(
          centerX + std::cos(radians) * arcRadius);
      points[slot][arc][index].y = static_cast<int16_t>(
          arcCenterY + std::sin(radians) * arcRadius);
    }
    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, points[slot][arc], 13);
    lv_obj_set_size(line, 466, 466);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
  createRoundPanel(parent, centerX - 2, arcCenterY + radius / 2 + 2,
                   4, 4, 2, color);
}

void createWifiActionCapsule(lv_obj_t* root, int16_t x, int16_t y,
                             int16_t width, int16_t height,
                             const char* label, bool primary) {
  lv_obj_t* capsule = createRoundPanel(
      root, x, y, width, height, height / 2,
      lv_color_hex(primary ? 0x126FD0 : 0x152335));
  lv_obj_set_style_bg_opa(capsule, primary ? LV_OPA_90 : LV_OPA_80, 0);
  lv_obj_set_style_border_width(capsule, 1, 0);
  lv_obj_set_style_border_color(capsule, lv_color_hex(primary ? 0x9BD2FF : 0x72B7F2), 0);
  lv_obj_t* gleam = lv_line_create(capsule);
  static lv_point_precise_t gleamPoints[2];
  gleamPoints[0] = {static_cast<lv_coord_t>(height / 2), 2};
  gleamPoints[1] = {static_cast<lv_coord_t>(width - height / 2), 2};
  lv_line_set_points(gleam, gleamPoints, 2);
  lv_obj_set_size(gleam, width, height);
  lv_obj_set_style_line_color(gleam, lv_color_white(), 0);
  lv_obj_set_style_line_opa(gleam, LV_OPA_50, 0);
  lv_obj_set_style_line_width(gleam, 1, 0);
  lv_obj_set_style_line_rounded(gleam, true, 0);
  lv_obj_t* text = createLabel(capsule, label, &lv_font_montserrat_14,
                               lv_color_hex(0xF5FAFF));
  lv_obj_center(text);
}

}  // namespace

bool DisplayManager::begin() {
  pinMode(kDisplayEnable, OUTPUT);
  digitalWrite(kDisplayEnable, HIGH);
  if (!display->begin(80000000)) {
    return false;
  }
  displayPreferencesReady = displayPreferences.begin("display", false);
  if (!displayPreferencesReady)
    Serial.println("Display settings unavailable; using in-memory defaults");
  brightnessPercent_ = constrain(
      displayPreferencesReady ? displayPreferences.getUChar("brightness", generated_ui::kDefaultBrightness) : generated_ui::kDefaultBrightness,
      generated_ui::kBrightnessMinimum,
      generated_ui::kBrightnessMaximum);
  if (displayPreferencesReady && displayPreferences.isKey("background_mode")) {
    backgroundMode_ = min<uint8_t>(displayPreferences.getUChar("background_mode", 1), 2);
  } else if (displayPreferencesReady) {
    backgroundMode_ = displayPreferences.getBool("dark_bg", false) ? 0 : 1;
  }
  spanishLanguage_ = displayPreferencesReady && displayPreferences.getBool("spanish", false);
  backgroundArt_ = displayPreferencesReady ? min<uint8_t>(displayPreferences.getUChar("background_art", 3), 3) : 3;
  clockBackground_ = displayPreferencesReady ? min<uint8_t>(displayPreferences.getUChar("clock_background", 0), 2) : 0;
  accentIndex_ = displayPreferencesReady ? min<uint8_t>(displayPreferences.getUChar("accent_color", 4), 6) : 4;
  clockFace_ = displayPreferencesReady ? min<uint8_t>(displayPreferences.getUChar("clock_face", 0), 4) : 0;
  sharedRenderState_.timeText = timeText_;
  sharedRenderState_.dateText = dateText_;
  sharedRenderState_.clockFace = clockFace_;
  sharedRenderState_.backgroundArt = backgroundArt_;
  sharedRenderState_.clockBackground = clockBackground_;
  sharedRenderState_.accentIndex = accentIndex_;
  sharedRenderState_.appearanceDark = !displayPreferencesReady || displayPreferences.getBool("appearance_dark", true);
  sharedRenderState_.clockShowWeather = displayPreferencesReady && displayPreferences.getBool("clock_weather", false);
  display->setBrightness(map(brightnessPercent_, 5, 100, 13, 255));
  display->fillScreen(kBlack);
  lv_init();
  lv_tick_set_cb(lvglTick);
  lvglDrawBuffer = static_cast<uint8_t*>(heap_caps_malloc(
      kLvglBufferBytes,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (lvglDrawBuffer == nullptr) {
    lvglDrawBuffer = static_cast<uint8_t*>(heap_caps_malloc(
        kLvglBufferBytes,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  lvglCaptureBuffer = static_cast<uint16_t*>(heap_caps_malloc(
      kScreenWidth * kScreenHeight * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (lvglCaptureBuffer == nullptr) {
    lvglCaptureBuffer = static_cast<uint16_t*>(heap_caps_malloc(
        kScreenWidth * kScreenHeight * sizeof(uint16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (lvglCaptureBuffer != nullptr) {
    memset(lvglCaptureBuffer, 0, kScreenWidth * kScreenHeight * sizeof(uint16_t));
  }
  if (lvglDrawBuffer == nullptr) {
    Serial.println("LVGL draw buffer allocation failed");
    return false;
  }
  Serial.printf("LEOPOLD_DISPLAY_MEMORY draw=%lu capture=%lu framebuffer_rgb565=%lu\n",
                static_cast<unsigned long>(kLvglBufferBytes),
                static_cast<unsigned long>(kScreenWidth * kScreenHeight * sizeof(uint16_t)),
                static_cast<unsigned long>(kScreenWidth * kScreenHeight * sizeof(uint16_t)));
  memory_budget::log("display-ready");
  lvglDisplay = lv_display_create(kScreenWidth, kScreenHeight);
  if (lvglDisplay == nullptr) {
    Serial.println("LVGL display creation failed");
    return false;
  }
  lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvglDisplay, lvglFlush);
  lv_display_set_buffers(
      lvglDisplay,
      lvglDrawBuffer,
      nullptr,
      kLvglBufferBytes,
      LV_DISPLAY_RENDER_MODE_PARTIAL);
  return true;
}

void DisplayManager::loop() {
  if (lvglDisplay != nullptr && !calibrationMode_) {
    lv_timer_handler();
  }
}

uint32_t DisplayManager::flushCallbackCount() const {
  return lvglFlushCallbacks;
}

uint64_t DisplayManager::flushPixelCount() const {
  return lvglFlushPixels;
}

void DisplayManager::showCalibration() {
  if (lvglCaptureBuffer == nullptr) return;
  calibrationMode_ = true;
  const int16_t centerX = generated_ui::kCalibrationWidth / 2;
  const int16_t centerY = generated_ui::kCalibrationHeight / 2;
  const auto setPixel = [](int16_t x, int16_t y, uint16_t color) {
    if (x >= 0 && x < generated_ui::kCalibrationWidth && y >= 0 && y < generated_ui::kCalibrationHeight) {
      lvglCaptureBuffer[y * generated_ui::kCalibrationWidth + x] = color;
    }
  };
  for (int16_t y = 0; y < generated_ui::kCalibrationHeight; ++y) {
    for (int16_t x = 0; x < generated_ui::kCalibrationWidth; ++x) setPixel(x, y, generated_ui::kCalibrationBackground);
  }
  for (int16_t x = 0; x < generated_ui::kCalibrationWidth; ++x) {
    for (uint8_t offset = 0; offset < generated_ui::kCalibrationBorderWidth; ++offset) {
      setPixel(x, offset, generated_ui::kCalibrationBorderColor);
      setPixel(x, generated_ui::kCalibrationHeight - 1 - offset, generated_ui::kCalibrationBorderColor);
    }
  }
  for (int16_t y = 0; y < generated_ui::kCalibrationHeight; ++y) {
    for (uint8_t offset = 0; offset < generated_ui::kCalibrationBorderWidth; ++offset) {
      setPixel(offset, y, generated_ui::kCalibrationBorderColor);
      setPixel(generated_ui::kCalibrationWidth - 1 - offset, y, generated_ui::kCalibrationBorderColor);
    }
  }
  const uint8_t marker = generated_ui::kCalibrationMarkerSize;
  for (int16_t y = 0; y < marker; ++y) {
    for (int16_t x = 0; x < marker; ++x) {
      setPixel(x, y, generated_ui::kCalibrationMarkerColor);
      setPixel(generated_ui::kCalibrationWidth - marker + x, y, generated_ui::kCalibrationMarkerColor);
      setPixel(x, generated_ui::kCalibrationHeight - marker + y, generated_ui::kCalibrationMarkerColor);
      setPixel(generated_ui::kCalibrationWidth - marker + x, generated_ui::kCalibrationHeight - marker + y, generated_ui::kCalibrationMarkerColor);
    }
  }
  for (int16_t coordinate = generated_ui::kCalibrationGridSpacing; coordinate < generated_ui::kCalibrationWidth; coordinate += generated_ui::kCalibrationGridSpacing) {
    for (int16_t offset = 0; offset < generated_ui::kCalibrationGridWidth; ++offset) {
      for (int16_t position = 0; position < generated_ui::kCalibrationHeight; ++position) {
        setPixel(coordinate + offset, position, generated_ui::kCalibrationGridColor);
        setPixel(position, coordinate + offset, generated_ui::kCalibrationGridColor);
      }
    }
  }
  for (int16_t offset = 0; offset < generated_ui::kCalibrationCrosshairWidth; ++offset) {
    for (int16_t position = 0; position < generated_ui::kCalibrationHeight; ++position) {
      setPixel(centerX - generated_ui::kCalibrationCrosshairWidth / 2 + offset, position, generated_ui::kCalibrationCrosshairColor);
      setPixel(position, centerY - generated_ui::kCalibrationCrosshairWidth / 2 + offset, generated_ui::kCalibrationCrosshairColor);
    }
  }
  for (uint8_t index = 0; index < generated_ui::kCalibrationRadiusCount; ++index) {
    const int16_t radius = generated_ui::kCalibrationRadii[index];
    for (int16_t y = centerY - radius - 2; y <= centerY + radius + 2; ++y) {
      for (int16_t x = centerX - radius - 2; x <= centerX + radius + 2; ++x) {
        const int32_t dx = x - centerX;
        const int32_t dy = y - centerY;
        const int32_t distanceSquared = dx * dx + dy * dy;
        const int32_t radiusSquared = radius * radius;
        if (abs(distanceSquared - radiusSquared) <= radius * generated_ui::kCalibrationCircleWidth) setPixel(x, y, generated_ui::kCalibrationCircleColor);
      }
    }
  }
  display->draw16bitRGBBitmap(0, 0, lvglCaptureBuffer, generated_ui::kCalibrationWidth, generated_ui::kCalibrationHeight);
}

void DisplayManager::exitCalibration() {
  if (!calibrationMode_) return;
  calibrationMode_ = false;
}

bool DisplayManager::calibrationMode() const { return calibrationMode_; }

void DisplayManager::writeDisplayInfo(Stream& output) const {
  output.printf("LEOPOLD_DISPLAY_INFO panel=%dx%d framebuffer=%ux%u lvgl=%ldx%ld origin=0,0\n",
                display->width(), display->height(), kScreenWidth, kScreenHeight,
                static_cast<long>(lvglDisplay == nullptr ? 0 : lv_display_get_horizontal_resolution(lvglDisplay)),
                static_cast<long>(lvglDisplay == nullptr ? 0 : lv_display_get_vertical_resolution(lvglDisplay)));
}

bool DisplayManager::writeLvglScreenshot(Stream& output) {
  if (lvglCaptureBuffer == nullptr) return false;
  // A USB screenshot can arrive in the same loop iteration as a screen change.
  // Flush pending LVGL invalidation first so the capture represents the
  // currently selected screen rather than the previous framebuffer contents.
  if (!calibrationMode_) lv_timer_handler();
  constexpr uint32_t byteCount = kScreenWidth * kScreenHeight * sizeof(uint16_t);
  output.printf("LEOPOLD_SCREENSHOT_BEGIN %u %u RGB565 %lu\n", kScreenWidth, kScreenHeight, static_cast<unsigned long>(byteCount));
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(lvglCaptureBuffer);
  uint32_t written = 0;
  while (written < byteCount) {
    const size_t count = output.write(bytes + written, byteCount - written);
    if (count == 0) return false;
    written += count;
  }
  output.printf("\nLEOPOLD_SCREENSHOT_END\n");
  output.flush();
  return true;
}

void DisplayManager::centered(
    const char* text,
    int16_t y,
    uint8_t size,
    uint16_t color) {
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display->setTextSize(size);
  display->getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  display->setTextColor(color);
  display->setCursor((kScreenWidth - width) / 2, y);
  display->print(text);
}

void DisplayManager::showState(
    AppState state,
    const char* detail,
    const char* secondary) {
  display->fillScreen(kBlack);
  centered("LEOPOLD", 76, 3, kWhite);

  const char* title = appStateName(state);
  uint16_t color = kBlue;
  switch (state) {
    case AppState::kBooting:
      title = "INICIANDO";
      break;
    case AppState::kWifiConnecting:
      title = "CONECTANDO WIFI";
      color = kMorning;
      break;
    case AppState::kWifiPortal:
      title = "CONFIGURAR WIFI";
      color = kMorning;
      break;
    case AppState::kCloudConnecting:
      title = "CONECTANDO NUBE";
      color = kMorning;
      break;
    case AppState::kOnline:
      title = "CONECTADO";
      color = kGreen;
      break;
    case AppState::kOffline:
      title = "MODO OFFLINE";
      break;
    case AppState::kError:
      title = "ERROR";
      color = kRed;
      break;
  }

  display->drawRoundRect(73, 170, 320, 68, 30, color);
  centered(title, 193, 2, kWhite);
  if (detail != nullptr && detail[0] != '\0') {
    centered(detail, 285, 2, kWhite);
  }
  if (secondary != nullptr && secondary[0] != '\0') {
    centered(secondary, 330, 1, kGray);
  }
}

void DisplayManager::showNotActivated() {
  showState(
      AppState::kError,
      "DISPOSITIVO",
      "NO ACTIVADO");
}

void DisplayManager::showUpdateRequired() {
  showState(
      AppState::kError,
      "ACTUALIZACION",
      "NECESARIA");
}

void DisplayManager::showQr(
    const char* title,
    const char* payload,
    const char* footer) {
  constexpr uint8_t kQrVersion = 8;
  uint8_t qrData[qrcode_getBufferSize(kQrVersion)];
  QRCode qr;
  qrcode_initText(&qr, qrData, kQrVersion, ECC_MEDIUM, payload);

  // Phone cameras need a crisp four-module quiet zone around the complete
  // symbol. Six physical pixels per module also avoids sub-pixel resampling
  // when the round display is photographed at an angle.
  constexpr int16_t scale = 6;
  constexpr int16_t quietModules = 4;
  constexpr int16_t quietPixels = quietModules * scale;
  const int16_t qrPixels = qr.size * scale;
  const int16_t originX = (kScreenWidth - qrPixels) / 2;
  const int16_t originY = 90;

  display->fillScreen(kBlack);
  centered(title, 48, 2, kWhite);
  display->fillRect(
      originX - quietPixels,
      originY - quietPixels,
      qrPixels + quietPixels * 2,
      qrPixels + quietPixels * 2,
      kWhite);
  for (uint8_t y = 0; y < qr.size; ++y) {
    for (uint8_t x = 0; x < qr.size; ++x) {
      if (qrcode_getModule(&qr, x, y)) {
        display->fillRect(
            originX + x * scale,
            originY + y * scale,
            scale,
            scale,
            kBlack);
      }
    }
  }
  if (footer != nullptr && footer[0] != '\0') {
    centered(footer, 430, 1, kGray);
  }
}

void DisplayManager::showWelcome(const char* ownerName, uint8_t step) {
  display->fillScreen(kBlack);
  display->fillCircle(kScreenWidth / 2, 105, 45, kMorning);
  display->fillCircle(kScreenWidth / 2 - 15, 98, 5, kBlack);
  display->fillCircle(kScreenWidth / 2 + 15, 98, 5, kBlack);
  display->drawArc(kScreenWidth / 2, 111, 20, 17, 20, 160, kBlack);

  switch (step) {
    case 0:
      centered("HOLA", 195, 3, kWhite);
      centered(ownerName, 255, 4, kMorning);
      break;
    default:
      centered("SOY TU", 205, 3, kWhite);
      centered("ASISTENTE", 265, 3, kMorning);
      break;
  }
}

void DisplayManager::showReadyFace() {
  display->fillScreen(kBlack);
  display->fillCircle(164, 215, 30, kWhite);
  display->fillCircle(302, 215, 30, kWhite);
  display->fillCircle(164, 215, 11, kBlack);
  display->fillCircle(302, 215, 11, kBlack);
  display->drawArc(kScreenWidth / 2, 250, 70, 54, 20, 160, kMorning);
}

void DisplayManager::setBrightnessPercent(uint8_t percent) {
  brightnessPercent_ = constrain(
      percent, generated_ui::kBrightnessMinimum, generated_ui::kBrightnessMaximum);
  sharedRenderState_.brightnessPercent = brightnessPercent_;
  display->setBrightness(map(brightnessPercent_, 5, 100, 13, 255));
  if (displayPreferencesReady) displayPreferences.putUChar("brightness", brightnessPercent_);
}

uint8_t DisplayManager::brightnessPercent() const {
  return brightnessPercent_;
}

void DisplayManager::cycleBackground() {
  backgroundMode_ = static_cast<uint8_t>((backgroundMode_ + 1) % 3);
  if (displayPreferencesReady) displayPreferences.putUChar("background_mode", backgroundMode_);
}

void DisplayManager::toggleLanguage() {
  spanishLanguage_ = !spanishLanguage_;
  if (displayPreferencesReady) displayPreferences.putBool("spanish", spanishLanguage_);
}

void DisplayManager::setAppearanceDark(bool dark) {
  sharedRenderState_.appearanceDark = dark;
  if (displayPreferencesReady) displayPreferences.putBool("appearance_dark", dark);
}

void DisplayManager::setClockShowWeather(bool enabled) {
  sharedRenderState_.clockShowWeather = enabled;
  if (displayPreferencesReady) displayPreferences.putBool("clock_weather", enabled);
}

void DisplayManager::setAccentIndex(uint8_t index) {
  accentIndex_ = min<uint8_t>(index, 6);
  sharedRenderState_.accentIndex = accentIndex_;
  if (displayPreferencesReady) displayPreferences.putUChar("accent_color", accentIndex_);
}

uint8_t DisplayManager::accentIndex() const { return accentIndex_; }

void DisplayManager::setBackgroundArt(uint8_t index) {
  backgroundArt_ = min<uint8_t>(index, 3);
  sharedRenderState_.backgroundArt = backgroundArt_;
  if (displayPreferencesReady) displayPreferences.putUChar("background_art", backgroundArt_);
}

void DisplayManager::setClockBackground(uint8_t index) {
  clockBackground_ = min<uint8_t>(index, 2);
  sharedRenderState_.clockBackground = clockBackground_;
  if (displayPreferencesReady) displayPreferences.putUChar("clock_background", clockBackground_);
}
uint8_t DisplayManager::backgroundArt() const { return backgroundArt_; }
uint8_t DisplayManager::clockBackground() const { return clockBackground_; }

void DisplayManager::setClockFace(uint8_t index) {
  clockFace_ = min<uint8_t>(index, 4);
  sharedRenderState_.clockFace = clockFace_;
  if (displayPreferencesReady) displayPreferences.putUChar("clock_face", clockFace_);
}

uint8_t DisplayManager::clockFace() const { return clockFace_; }

uint8_t DisplayManager::backgroundMode() const {
  return backgroundMode_;
}

bool DisplayManager::spanishLanguage() const {
  return spanishLanguage_;
}

void DisplayManager::buildLvglConnectivityDetails(
    bool wifiScreen,
    bool primaryConnected,
    bool secondaryState,
    const char* primaryDetail,
    const char* secondaryDetail) {
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_24, lv_color_white());
  lv_obj_center(backIcon);

  lv_obj_t* title = createLabel(
      root,
      wifiScreen ? "Wi-Fi" : "GoodMorning Cloud",
      &lv_font_montserrat_24,
      lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 84);

  const uint32_t accent = primaryConnected ? 0x30D158 : 0xFF453A;
  lv_obj_t* status = createRoundPanel(
      root, 149, 119, 168, 168, LV_RADIUS_CIRCLE, lv_color_hex(0x1C1C1E));
  lv_obj_set_style_border_width(status, 5, 0);
  lv_obj_set_style_border_color(status, lv_color_hex(accent), 0);
  if (wifiScreen) {
    drawWifiGlyphOn(status, 84, 87, 25, lv_color_hex(accent), 0);
  } else {
    lv_obj_t* symbol = createLabel(
        status, LV_SYMBOL_UPLOAD, &lv_font_montserrat_36, lv_color_hex(accent));
    lv_obj_align(symbol, LV_ALIGN_CENTER, 0, -20);
  }
  lv_obj_t* state = createLabel(
      status,
      primaryConnected ? "Conectado" : "Sin conexion",
      &lv_font_montserrat_16,
      lv_color_white());
  lv_obj_align(state, LV_ALIGN_CENTER, 0, 24);

  lv_obj_t* detail = createLabel(
      root,
      primaryDetail,
      &lv_font_montserrat_14,
      lv_color_hex(0xD1D1D6));
  lv_obj_set_width(detail, 390);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 307);
  lv_obj_t* secondary = createLabel(
      root,
      secondaryDetail,
      &lv_font_montserrat_12,
      secondaryState ? lv_color_hex(0x0A84FF) : lv_color_hex(0x8E8E93));
  lv_obj_set_width(secondary, 390);
  lv_obj_set_style_text_align(secondary, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(secondary, LV_ALIGN_TOP_MID, 0, 333);

  if (wifiScreen) {
    createWifiActionCapsule(
        root, generated_ui::kWifiScanX, generated_ui::kWifiScanY,
        generated_ui::kWifiScanWidth, generated_ui::kWifiScanHeight,
        "Scan networks", true);
    createWifiActionCapsule(
        root, generated_ui::kWifiChangeX, generated_ui::kWifiChangeY,
        generated_ui::kWifiChangeWidth, generated_ui::kWifiChangeHeight,
        "Change network", false);
  } else {
    createWifiActionCapsule(root, 47, 372, 372, 62, "Retry", true);
  }
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::showWifiDetails(
    bool connected,
    bool portalActive,
    const char* networkName,
    const char* accessPointName,
    int32_t rssi) {
  char primary[96] = {};
  char secondary[112] = {};
  if (connected) {
    snprintf(primary, sizeof(primary), "%s  |  %ld dBm", networkName, rssi);
    strlcpy(secondary, "La conexion se recupera automaticamente", sizeof(secondary));
  } else if (portalActive) {
    snprintf(primary, sizeof(primary), "Conectate a %s desde tu telefono", accessPointName);
    strlcpy(secondary, "El portal permite elegir una nueva red", sizeof(secondary));
  } else {
    strlcpy(primary, "Leopold esta buscando la red guardada", sizeof(primary));
    strlcpy(secondary, "Puedes reintentar o cambiar de red", sizeof(secondary));
  }
  buildLvglConnectivityDetails(
      true, connected, portalActive, primary, secondary);
}

void DisplayManager::showCloudDetails(
    bool wifiConnected,
    bool cloudConnected,
    bool retrying,
    uint32_t retryInSeconds) {
  char primary[96] = {};
  char secondary[112] = {};
  if (cloudConnected) {
    strlcpy(primary, "Leopold esta conectado al servidor", sizeof(primary));
    strlcpy(secondary, "Heartbeats y UI remota activos", sizeof(secondary));
  } else if (!wifiConnected) {
    strlcpy(primary, "Primero se necesita una conexion Wi-Fi", sizeof(primary));
    strlcpy(secondary, "La nube volvera cuando regrese Internet", sizeof(secondary));
  } else {
    strlcpy(primary, "Intentando conectar con GoodMorning", sizeof(primary));
    snprintf(
        secondary,
        sizeof(secondary),
        retrying && retryInSeconds > 0 ? "Siguiente intento en %lu s" : "Reconectando ahora...",
        static_cast<unsigned long>(retryInSeconds));
  }
  buildLvglConnectivityDetails(
      false, cloudConnected, retrying, primary, secondary);
}

void DisplayManager::showBluetoothDetails(
    bool enabled,
    bool connected,
    const char* peripheralName) {
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_24, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Bluetooth", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 84);
  const uint32_t accent = connected ? 0x30D158 : 0x0A84FF;
  lv_obj_t* status = createRoundPanel(
      root, 149, 120, 168, 168, LV_RADIUS_CIRCLE, lv_color_hex(0x1C1C1E));
  lv_obj_set_style_border_width(status, 5, 0);
  lv_obj_set_style_border_color(status, lv_color_hex(accent), 0);
  lv_obj_t* symbol = createLabel(
      status, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_36, lv_color_hex(accent));
  lv_obj_center(symbol);
  lv_obj_t* state = createLabel(
      root,
      connected ? "Conectado" : (enabled ? "Listo para buscar" : "No disponible"),
      &lv_font_montserrat_20,
      lv_color_white());
  lv_obj_align(state, LV_ALIGN_TOP_MID, 0, 306);
  lv_obj_t* detail = createLabel(
      root,
      connected && peripheralName != nullptr && peripheralName[0] != '\0'
          ? peripheralName
          : "Accesorios Bluetooth Low Energy",
      &lv_font_montserrat_14,
      lv_color_hex(0x8E8E93));
  lv_obj_set_width(detail, 380);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 340);
  lv_obj_t* scan = createRoundPanel(
      root, 82, 385, 302, 58, 29, lv_color_hex(0x0A84FF));
  lv_obj_t* scanLabel = createLabel(
      scan, "Buscar dispositivos", &lv_font_montserrat_16, lv_color_white());
  lv_obj_center(scanLabel);
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::showBluetoothDevices(
    const BluetoothScanResult* devices,
    uint8_t count,
    bool scanning,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 23, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_22, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Bluetooth", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_t* subtitle = createLabel(
      root,
      scanning ? "Buscando accesorios BLE..."
               : (message != nullptr ? message : "Toca uno para conectar"),
      &lv_font_montserrat_12,
      lv_color_hex(scanning ? 0x0A84FF : 0x8E8E93));
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 110);
  for (uint8_t index = 0; index < count && index < 5; ++index) {
    const int16_t y = 131 + index * 57;
    lv_obj_t* row = createRoundPanel(
        root, 34, y, 398, 49, 17, lv_color_hex(0x1C1C1E));
    lv_obj_t* icon = createLabel(
        row, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_20, lv_color_hex(0x0A84FF));
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_t* name = createLabel(
        row, devices[index].name, &lv_font_montserrat_16, lv_color_white());
    lv_obj_set_width(name, 255);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 54, 0);
    char signal[12] = {};
    snprintf(signal, sizeof(signal), "%ld", static_cast<long>(devices[index].rssi));
    lv_obj_t* rssi = createLabel(
        row, signal, &lv_font_montserrat_12, lv_color_hex(0x8E8E93));
    lv_obj_align(rssi, LV_ALIGN_RIGHT_MID, -15, 0);
  }
  lv_obj_t* scan = createRoundPanel(
      root, 111, 410, 244, 45, 23, lv_color_hex(0x2C2C2E));
  lv_obj_t* scanLabel = createLabel(
      scan,
      scanning ? "Buscando..." : "Buscar otra vez",
      &lv_font_montserrat_14,
      lv_color_white());
  lv_obj_center(scanLabel);
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::showWifiNetworks(
    const char* const* names,
    const int32_t* signalStrengths,
    const bool* secured,
    uint8_t count,
    bool scanning,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 23, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_22, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Elegir Wi-Fi", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_t* subtitle = createLabel(
      root,
      message != nullptr ? message :
          (scanning ? "Buscando redes..." : "Redes disponibles"),
      &lv_font_montserrat_12,
      lv_color_hex(scanning && message == nullptr ? 0x0A84FF : 0x8E8E93));
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 110);

  for (uint8_t index = 0; index < count && index < 5; ++index) {
    const int16_t y = 131 + index * 57;
    lv_obj_t* row = createRoundPanel(
        root, 34, y, 398, 49, 17, lv_color_hex(0x1C1C1E));
    lv_obj_t* wifi = createLabel(
        row, LV_SYMBOL_WIFI, &lv_font_montserrat_20,
        lv_color_hex(signalStrengths[index] > -68 ? 0x30D158 : 0x8E8E93));
    lv_obj_align(wifi, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_t* name = createLabel(
        row, names[index], &lv_font_montserrat_16, lv_color_white());
    lv_obj_set_width(name, 265);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 54, 0);
    lv_obj_t* lock = createLabel(
        row,
        secured[index] ? LV_SYMBOL_EYE_CLOSE : "Abierta",
        &lv_font_montserrat_12,
        lv_color_hex(0x8E8E93));
    lv_obj_align(lock, LV_ALIGN_RIGHT_MID, -15, 0);
  }
  lv_obj_t* scan = createRoundPanel(
      root, 111, 410, 244, 45, 23, lv_color_hex(0x2C2C2E));
  lv_obj_t* scanLabel = createLabel(
      scan,
      scanning ? "Buscando..." : "Buscar otra vez",
      &lv_font_montserrat_14,
      lv_color_white());
  lv_obj_center(scanLabel);
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::showWifiPassword(
    const char* networkName,
    uint8_t passwordLength,
    int8_t selectedGroup,
    bool uppercase) {
  static const char* groupLabels[] = {
      "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "123"};
  static const char* lowerGroups[] = {
      "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz", "1234567890"};
  static const char* upperGroups[] = {
      "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "1234567890"};
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_20, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, networkName, &lv_font_montserrat_18, lv_color_white());
  lv_obj_set_width(title, 280);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 84);

  lv_obj_t* field = createRoundPanel(
      root, generated_ui::kWifiPasswordFieldX, generated_ui::kWifiPasswordFieldY, generated_ui::kWifiPasswordFieldWidth, generated_ui::kWifiPasswordFieldHeight, 18, lv_color_hex(0x1C1C1E));
  char masked[65] = {};
  const uint8_t dots = min<uint8_t>(passwordLength, 32);
  memset(masked, '*', dots);
  lv_obj_t* value = createLabel(
      field,
      passwordLength == 0 ? "Contrasena" : masked,
      &lv_font_montserrat_16,
      lv_color_hex(passwordLength == 0 ? 0x8E8E93 : 0xFFFFFF));
  lv_obj_align(value, LV_ALIGN_LEFT_MID, 18, 0);
  lv_obj_t* erase = createLabel(
      field, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_22, lv_color_white());
  lv_obj_align(erase, LV_ALIGN_RIGHT_MID, -18, 0);

  if (selectedGroup < 0) {
    for (uint8_t index = 0; index < 9; ++index) {
      const uint8_t row = index / 3;
      const uint8_t column = index % 3;
      lv_obj_t* key = createRoundPanel(
          root,
          58 + column * 119,
          159 + row * 68,
          111,
          60,
          18,
          lv_color_hex(0x2C2C2E));
      lv_obj_t* label = createLabel(
          key, groupLabels[index], &lv_font_montserrat_20, lv_color_white());
      lv_obj_center(label);
    }
    const char* bottomLabels[] = {uppercase ? "minus" : "MAYUS", "Simbolos", "Unir"};
    for (uint8_t index = 0; index < 3; ++index) {
      lv_obj_t* action = createRoundPanel(
          root,
          58 + index * 119,
          368,
          111,
          54,
          18,
          lv_color_hex(index == 2 ? 0x0A84FF : 0x3A3A3C));
      lv_obj_t* label = createLabel(
          action, bottomLabels[index], &lv_font_montserrat_14, lv_color_white());
      lv_obj_center(label);
    }
  } else {
    const char* characters = selectedGroup == 9
        ? "@#$_-.!%*?"
        : (uppercase ? upperGroups[selectedGroup] : lowerGroups[selectedGroup]);
    const uint8_t count = strlen(characters);
    const uint8_t columns = count <= 5 ? count : 5;
    const int16_t width = count <= 5 ? 76 : 64;
    const int16_t gap = 8;
    const int16_t startX = (466 - columns * width - (columns - 1) * gap) / 2;
    for (uint8_t index = 0; index < count; ++index) {
      char keyText[2] = {characters[index], '\0'};
      const uint8_t row = index / 5;
      const uint8_t column = index % 5;
      lv_obj_t* key = createRoundPanel(
          root,
          startX + column * (width + gap),
          180 + row * 76,
          width,
          66,
          20,
          lv_color_hex(0x2C2C2E));
      lv_obj_t* label = createLabel(
          key, keyText, &lv_font_montserrat_24, lv_color_white());
      lv_obj_center(label);
    }
    lv_obj_t* prompt = createLabel(
        root,
        selectedGroup == 9 ? "Elige un simbolo" : "Elige un caracter",
        &lv_font_montserrat_16,
        lv_color_hex(0x8E8E93));
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 151);
  }
  lv_obj_t* hint = createLabel(
      root,
      selectedGroup < 0
          ? "Toca un grupo para elegir una letra"
          : "Atras vuelve a los grupos",
      &lv_font_montserrat_12,
      lv_color_hex(0x8E8E93));
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -26);
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::showWifiConnecting(
    const char* networkName,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  drawSettingsWallpaper(root);
  lv_obj_t* back = createRoundPanel(
      root, generated_ui::kConnectivityBackX, generated_ui::kConnectivityBackY, generated_ui::kConnectivityBackWidth, generated_ui::kConnectivityBackHeight, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_20, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* circle = createRoundPanel(
      root, 153, 91, 160, 160, LV_RADIUS_CIRCLE, lv_color_hex(0x1C1C1E));
  lv_obj_set_style_border_width(circle, 5, 0);
  lv_obj_set_style_border_color(circle, lv_color_hex(0x0A84FF), 0);
  lv_obj_t* wifi = createLabel(
      circle, LV_SYMBOL_WIFI, &lv_font_montserrat_36, lv_color_hex(0x0A84FF));
  lv_obj_center(wifi);
  lv_obj_t* title = createLabel(
      root, "Conectando", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 282);
  lv_obj_t* network = createLabel(
      root, networkName, &lv_font_montserrat_18, lv_color_hex(0xD1D1D6));
  lv_obj_set_width(network, 390);
  lv_obj_set_style_text_align(network, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(network, LV_ALIGN_TOP_MID, 0, 321);
  lv_obj_t* status = createLabel(
      root, message, &lv_font_montserrat_14, lv_color_hex(0x8E8E93));
  lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 361);
  renderSharedLvglStatusBanner(root, sharedRenderState_, sharedImageResolver);
}

void DisplayManager::renderSdui(const SduiDocument& document) {
  renderSduiScreen(document);
}

bool DisplayManager::setPressedButton(const char* id) {
  const char* next = id == nullptr ? "" : id;
  if (strcmp(pressedButtonId_, next) == 0) return false;
  if (touchTraceEnabled_) {
    Serial.printf("TOUCH_TRACE pressed old=%s new=%s\n", pressedButtonId_, next);
  }
  strlcpy(pressedButtonId_, next, sizeof(pressedButtonId_));
  return true;
}

bool DisplayManager::clearPressedButton() {
  if (pressedButtonId_[0] == '\0') return false;
  if (touchTraceEnabled_) Serial.printf("TOUCH_TRACE pressed old=%s new=none\n", pressedButtonId_);
  pressedButtonId_[0] = '\0';
  return true;
}

void DisplayManager::setTouchTraceEnabled(bool enabled) {
  touchTraceEnabled_ = enabled;
}

void DisplayManager::renderSduiScreen(const SduiDocument& document) {
  const SduiScreen& active = document.active();
  if (touchTraceEnabled_ && strcmp(active.screenId, "device-control-center") == 0 &&
      pressedButtonId_[0] != '\0') {
    Serial.printf("TOUCH_TRACE render screen=%s selectedRowId=%s bounded-treatment=shared\n",
                  active.screenId, pressedButtonId_);
  }
  renderSharedLvglScreen(lv_screen_active(), active, sharedImageResolver, pressedButtonId_, &sharedRenderState_);
}

void DisplayManager::drawIndicators(
    bool wifiConnected,
    bool cloudConnected,
    bool bluetoothEnabled,
    bool bluetoothConnected,
    uint8_t batteryPercent,
    const char* timeText) {
  wifiConnected_ = wifiConnected;
  cloudConnected_ = cloudConnected;
  bluetoothEnabled_ = bluetoothEnabled;
  bluetoothConnected_ = bluetoothConnected;
  batteryPercent_ = batteryPercent;
  strncpy(timeText_, timeText == nullptr ? "--:--" : timeText, sizeof(timeText_) - 1);
  timeText_[sizeof(timeText_) - 1] = '\0';
  sharedRenderState_.timeText = timeText_;
  time_t now = time(nullptr);
  struct tm local = {};
  if (now > 1700000000 && localtime_r(&now, &local) != nullptr) {
    strftime(dateText_, sizeof(dateText_), "%b %d", &local);
    sharedRenderState_.clockSecond = static_cast<uint8_t>(local.tm_sec);
  } else {
    dateText_[0] = '\0';
    sharedRenderState_.clockSecond = 0;
  }
  sharedRenderState_.dateText = dateText_;
  sharedRenderState_.batteryPercent = batteryPercent_;
  sharedRenderState_.wifiConnected = wifiConnected_;
  sharedRenderState_.cloudConnected = cloudConnected_;
  sharedRenderState_.bluetoothEnabled = bluetoothEnabled_;
  sharedRenderState_.bluetoothConnected = bluetoothConnected_;
  sharedRenderState_.brightnessPercent = brightnessPercent_;
  sharedRenderState_.backgroundMode = backgroundMode_;
  sharedRenderState_.backgroundArt = backgroundArt_;
  sharedRenderState_.clockBackground = clockBackground_;
  sharedRenderState_.accentIndex = accentIndex_;
  // Status UI is rendered by the shared LVGL renderer. Drawing a second
  // direct-to-panel copy here caused a flashing cloud glyph and made screens
  // without a status bar diverge from the simulator.
}

const SharedRenderState& DisplayManager::sharedRenderState() const {
  return sharedRenderState_;
}

void DisplayManager::drawWifiIcon(int16_t x, int16_t y, bool connected) {
  const uint16_t color = connected ? kGreen : kGray;
  drawWifiGlyph(x, y, color);
  if (!connected) {
    display->drawLine(x - 9, y - 7, x + 9, y + 9, kRed);
  }
}

void DisplayManager::drawCloudIcon(int16_t x, int16_t y, bool connected) {
  const uint16_t color = connected ? kGreen : kGray;
  drawCloudGlyph(x, y, color);
  if (!connected) {
    display->drawLine(x - 11, y - 9, x + 13, y + 10, kRed);
  }
}

void DisplayManager::drawWifiGlyph(int16_t x, int16_t y, uint16_t color) {
  for (int8_t offset = 0; offset < 2; ++offset) {
    display->drawArc(x, y + 4, 11 - offset, 10 - offset, 220, 320, color);
    display->drawArc(x, y + 4, 7 - offset, 6 - offset, 220, 320, color);
  }
  display->fillCircle(x, y + 7, 2, color);
}

void DisplayManager::drawCloudGlyph(int16_t x, int16_t y, uint16_t color) {
  display->fillCircle(x - 7, y + 3, 5, color);
  display->fillCircle(x, y - 1, 7, color);
  display->fillCircle(x + 7, y + 3, 5, color);
  display->fillRoundRect(x - 12, y + 2, 24, 8, 4, color);
}

void DisplayManager::drawBluetoothGlyph(
    int16_t x,
  int16_t y,
  uint16_t color) {
  for (int8_t offset = -1; offset <= 1; ++offset) {
    display->drawLine(x + offset, y - 9, x + offset, y + 9, color);
    display->drawLine(x + offset, y - 9, x + 6 + offset, y - 4, color);
    display->drawLine(x + 6 + offset, y - 4, x - 5 + offset, y + 5, color);
    display->drawLine(x - 5 + offset, y - 5, x + 6 + offset, y + 4, color);
    display->drawLine(x + 6 + offset, y + 4, x + offset, y + 9, color);
  }
}

void DisplayManager::drawSunGlyph(int16_t x, int16_t y, uint16_t color) {
  display->drawCircle(x, y, 7, color);
  display->drawCircle(x, y, 8, color);
  for (uint8_t index = 0; index < 8; ++index) {
    const float angle = index * 0.785398F;
    display->drawLine(
        x + cosf(angle) * 12,
        y + sinf(angle) * 12,
        x + cosf(angle) * 17,
        y + sinf(angle) * 17,
        color);
  }
}

void DisplayManager::drawInternetIcon(
    int16_t x,
    int16_t y,
    bool connected) {
  const uint16_t color = connected ? kGreen : kRed;
  display->drawCircle(x, y, 10, color);
  display->drawLine(x - 9, y, x + 9, y, color);
  display->drawArc(x, y, 6, 5, 90, 270, color);
  display->drawArc(x, y, 6, 5, 270, 90, color);
  if (!connected) {
    display->drawLine(x - 9, y - 9, x + 9, y + 9, kRed);
    display->drawLine(x - 8, y - 9, x + 10, y + 9, kRed);
  }
}

void DisplayManager::drawBluetoothIcon(
    int16_t x,
    int16_t y,
    bool enabled) {
  const uint16_t color = enabled ? kBlue : kGray;
  drawBluetoothGlyph(x, y, color);
}

void DisplayManager::drawEye(
    int16_t x,
    int16_t y,
    int8_t pupilX,
    int8_t pupilY) {
  display->fillCircle(x, y, 28, kWhite);
  display->fillCircle(x + pupilX, y + pupilY, 10, kBlack);
  display->fillCircle(x + pupilX - 3, y + pupilY - 3, 3, kWhite);
}

void DisplayManager::showLocalFace(uint32_t now, bool force) {
  if (!force && now - lastFaceFrameAt_ < 70) {
    return;
  }
  if (force) {
    display->fillScreen(kBlack);
    centered("GOODMORNING", 65, 2, kMorning);
    centered("LEOPOLD", 104, 3, kWhite);
  }
  display->fillRect(105, 160, 256, 120, kBlack);
  const float phase = static_cast<float>(now % 5000) * 0.001256637F;
  const int8_t x = static_cast<int8_t>(sinf(phase) * 8);
  const int8_t y = static_cast<int8_t>(cosf(phase * 1.7F) * 4);

  if (localExpression_ == 1) {
    for (int8_t offset = -2; offset <= 2; ++offset) {
      display->drawLine(130, 216 + offset, 164, 194 + offset, kMorning);
      display->drawLine(164, 194 + offset, 198, 216 + offset, kMorning);
      display->drawLine(268, 216 + offset, 302, 194 + offset, kMorning);
      display->drawLine(302, 194 + offset, 336, 216 + offset, kMorning);
    }
    display->drawLine(214, 252, 233, 262, kWhite);
    display->drawLine(233, 262, 252, 252, kWhite);
  } else if (localExpression_ == 2) {
    drawEye(164, 207, 9, -7);
    drawEye(302, 207, 9, -7);
    const uint8_t dot = (now / 280) % 3;
    for (uint8_t index = 0; index < 3; ++index) {
      display->fillCircle(
          221 + index * 12,
          258 - (index == dot ? 4 : 0),
          index == dot ? 5 : 3,
          index == dot ? kMorning : kBlue);
    }
  } else {
    drawEye(164, 207, x, y);
    drawEye(302, 207, x, y);
  }
  lastFaceFrameAt_ = now;
}

void DisplayManager::cycleLocalExpression() {
  localExpression_ = (localExpression_ + 1) % 3;
  showLocalFace(millis(), true);
}

}  // namespace leopold
