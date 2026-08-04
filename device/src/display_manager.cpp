#include "display_manager.h"

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <qrcode.h>

#include "board_config.h"

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
constexpr uint16_t kPanel = 0x18C3;
constexpr uint16_t kDim = 0x4208;
constexpr uint16_t kYellow = 0xFEC0;
constexpr uint16_t kSpotify = 0x1DCA;

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
lv_display_t* lvglDisplay = nullptr;
lv_obj_t* brightnessSlider = nullptr;
lv_obj_t* brightnessValue = nullptr;
uint8_t* lvglDrawBuffer = nullptr;

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
  lv_display_flush_ready(target);
}

const lv_font_t* fontForSize(uint8_t size) {
  switch (size) {
    case 1: return &lv_font_montserrat_14;
    case 2: return &lv_font_montserrat_20;
    case 3: return &lv_font_montserrat_28;
    default: return &lv_font_montserrat_36;
  }
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
  brightnessSlider = nullptr;
  brightnessValue = nullptr;
  return root;
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

}  // namespace

bool DisplayManager::begin() {
  pinMode(kDisplayEnable, OUTPUT);
  digitalWrite(kDisplayEnable, HIGH);
  if (!display->begin(80000000)) {
    return false;
  }
  displayPreferences.begin("display", false);
  brightnessPercent_ =
      constrain(displayPreferences.getUChar("brightness", 70), 5, 100);
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
  if (lvglDrawBuffer == nullptr) {
    Serial.println("LVGL draw buffer allocation failed");
    return false;
  }
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
  if (lvglDisplay != nullptr) {
    lv_timer_handler();
  }
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

void DisplayManager::drawComponent(const SduiComponent& component) {
  switch (component.type) {
    case ComponentType::kText: {
      int16_t x1 = 0;
      int16_t y1 = 0;
      uint16_t width = 0;
      uint16_t height = 0;
      display->setTextSize(component.size);
      display->getTextBounds(
          component.text,
          0,
          component.y,
          &x1,
          &y1,
          &width,
          &height);
      int16_t x = component.x;
      if (component.align == TextAlign::kCenter) {
        x -= width / 2;
      } else if (component.align == TextAlign::kRight) {
        x -= width;
      }
      display->setTextColor(component.color);
      display->setCursor(x, component.y);
      display->print(component.text);
      break;
    }
    case ComponentType::kIndicator:
      display->fillCircle(
          component.x - 8,
          component.y,
          6,
          component.connected ? kGreen : kRed);
      display->setTextSize(1);
      display->setTextColor(component.color);
      display->setCursor(component.x + 6, component.y - 4);
      display->print(component.text);
      break;
    case ComponentType::kButton:
      display->drawRoundRect(
          component.x,
          component.y,
          component.width,
          component.height,
          component.height / 2,
          component.color);
      if (component.icon[0] != '\0') {
        drawControlIcon(
            component.icon,
            component.x + component.width / 2,
            component.y + component.height / 2,
            component.color);
      } else {
        const uint8_t textSize = component.size > 1 ? component.size : 2;
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t textWidth = 0;
        uint16_t textHeight = 0;
        display->setTextSize(textSize);
        display->getTextBounds(
            component.text,
            0,
            0,
            &x1,
            &y1,
            &textWidth,
            &textHeight);
        display->setTextColor(component.color);
        display->setCursor(
            component.x + (component.width - textWidth) / 2,
            component.y + (component.height - textHeight) / 2);
        display->print(component.text);
      }
      break;
    case ComponentType::kProgress: {
      const uint16_t filled = static_cast<uint32_t>(component.width) *
                              min(component.value, component.maximum) /
                              component.maximum;
      display->drawRoundRect(
          component.x,
          component.y,
          component.width,
          component.height,
          component.height / 2,
          component.color);
      if (filled > 2) {
        display->fillRoundRect(
            component.x + 2,
            component.y + 2,
            filled - 2,
            component.height - 4,
            max<int16_t>(1, component.height / 2 - 2),
            component.color);
      }
      break;
    }
    case ComponentType::kSlider: {
      const int16_t trackY = component.y + component.height / 2;
      const int16_t radius = component.height / 2;
      const int16_t thumbX =
          component.x +
          static_cast<uint32_t>(component.width) *
              min(component.value, component.maximum) /
              component.maximum;
      display->fillRoundRect(
          component.x,
          trackY - 4,
          component.width,
          8,
          4,
          kGray);
      if (thumbX > component.x) {
        display->fillRoundRect(
            component.x,
            trackY - 4,
            thumbX - component.x,
            8,
            4,
            component.color);
      }
      display->fillCircle(thumbX, trackY, radius, component.color);
      display->drawCircle(thumbX, trackY, radius, kWhite);
      break;
    }
    case ComponentType::kIcon:
      if (strcmp(component.icon, "spotify") == 0) {
        drawSpotifyIcon(
            component.x,
            component.y,
            component.width,
            component.color);
      } else if (strcmp(component.icon, "bluetooth") == 0) {
        const int16_t scale = max<int16_t>(2, component.width / 32);
        const int16_t x = component.x;
        const int16_t y = component.y;
        display->drawLine(x, y - 12 * scale, x, y + 12 * scale, component.color);
        display->drawLine(x, y - 12 * scale, x + 7 * scale, y - 5 * scale, component.color);
        display->drawLine(x + 7 * scale, y - 5 * scale, x - 6 * scale, y + 7 * scale, component.color);
        display->drawLine(x - 6 * scale, y - 7 * scale, x + 7 * scale, y + 5 * scale, component.color);
        display->drawLine(x + 7 * scale, y + 5 * scale, x, y + 12 * scale, component.color);
      }
      break;
  }
}

void DisplayManager::setBrightnessPercent(uint8_t percent) {
  brightnessPercent_ = constrain(percent, 5, 100);
  display->setBrightness(map(brightnessPercent_, 5, 100, 13, 255));
  displayPreferences.putUChar("brightness", brightnessPercent_);
}

uint8_t DisplayManager::brightnessPercent() const {
  return brightnessPercent_;
}

void DisplayManager::drawControlIcon(
    const char* icon,
    int16_t centerX,
    int16_t centerY,
    uint16_t color) {
  if (strcmp(icon, "play") == 0) {
    display->fillTriangle(
        centerX - 9,
        centerY - 18,
        centerX - 9,
        centerY + 18,
        centerX + 19,
        centerY,
        color);
  } else if (strcmp(icon, "pause") == 0) {
    display->fillRoundRect(centerX - 14, centerY - 18, 10, 36, 3, color);
    display->fillRoundRect(centerX + 4, centerY - 18, 10, 36, 3, color);
  } else if (strcmp(icon, "previous") == 0) {
    display->fillRoundRect(centerX - 17, centerY - 16, 5, 32, 2, color);
    display->fillTriangle(
        centerX - 10, centerY, centerX + 15, centerY - 17,
        centerX + 15, centerY + 17, color);
  } else if (strcmp(icon, "next") == 0) {
    display->fillRoundRect(centerX + 12, centerY - 16, 5, 32, 2, color);
    display->fillTriangle(
        centerX + 10, centerY, centerX - 15, centerY - 17,
        centerX - 15, centerY + 17, color);
  }
}

void DisplayManager::drawSpotifyIcon(
    int16_t centerX,
    int16_t centerY,
    int16_t size,
    uint16_t color) {
  const int16_t radius = size / 2;
  display->fillCircle(centerX, centerY, radius, color);
  const int16_t outer = max<int16_t>(8, radius - 9);
  for (int8_t thickness = 0; thickness < 3; ++thickness) {
    display->drawArc(
        centerX,
        centerY - 5 + thickness,
        outer,
        outer - 3,
        205,
        335,
        kBlack);
    display->drawArc(
        centerX,
        centerY + 6 + thickness,
        outer - 6,
        outer - 9,
        205,
        335,
        kBlack);
    display->drawArc(
        centerX,
        centerY + 15 + thickness,
        outer - 12,
        max<int16_t>(2, outer - 15),
        205,
        335,
        kBlack);
  }
}

void DisplayManager::drawSpotifyPlayer(const SduiScreen& screen) {
  const SduiComponent* track = nullptr;
  const SduiComponent* artist = nullptr;
  const SduiComponent* album = nullptr;
  const SduiComponent* progress = nullptr;
  const SduiComponent* elapsed = nullptr;
  const SduiComponent* duration = nullptr;
  const SduiComponent* toggle = nullptr;
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (strcmp(component.id, "track") == 0) track = &component;
    else if (strcmp(component.id, "artist") == 0) artist = &component;
    else if (strcmp(component.id, "album") == 0) album = &component;
    else if (strcmp(component.id, "progress") == 0 ||
             strcmp(component.id, "spotify-seek") == 0) progress = &component;
    else if (strcmp(component.id, "elapsed") == 0) elapsed = &component;
    else if (strcmp(component.id, "duration") == 0) duration = &component;
    else if (strcmp(component.id, "spotify-toggle") == 0) toggle = &component;
  }

  display->fillScreen(kBlack);
  display->fillRoundRect(43, 43, 380, 352, 46, kPanel);
  drawSpotifyIcon(82, 82, 42, kSpotify);
  display->setTextSize(1);
  display->setTextColor(kSpotify);
  display->setCursor(112, 78);
  display->print("SPOTIFY");
  display->setTextColor(kGray);
  display->setCursor(112, 92);
  display->print("REPRODUCIENDO AHORA");

  if (track != nullptr) centered(track->text, 126, 2, kWhite);
  if (artist != nullptr) centered(artist->text, 157, 1, 0xBDF7);
  if (album != nullptr) centered(album->text, 177, 1, kGray);

  constexpr int16_t barX = 77;
  constexpr int16_t barY = 221;
  constexpr int16_t barWidth = 312;
  display->fillRoundRect(barX, barY, barWidth, 6, 3, kDim);
  uint16_t filled = 0;
  if (progress != nullptr && progress->maximum > 0) {
    filled = static_cast<uint32_t>(barWidth) *
             min(progress->value, progress->maximum) / progress->maximum;
  }
  if (filled > 0) {
    display->fillRoundRect(barX, barY, filled, 6, 3, kWhite);
    display->fillCircle(barX + filled, barY + 3, 5, kWhite);
  }
  display->setTextSize(1);
  display->setTextColor(kGray);
  if (elapsed != nullptr) {
    display->setCursor(barX, 240);
    display->print(elapsed->text);
  }
  if (duration != nullptr) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    display->getTextBounds(duration->text, 0, 0, &x1, &y1, &width, &height);
    display->setCursor(barX + barWidth - width, 240);
    display->print(duration->text);
  }

  display->fillCircle(233, 311, 45, kWhite);
  drawControlIcon(
      toggle != nullptr && strcmp(toggle->icon, "pause") == 0
          ? "pause" : "play",
      233,
      311,
      kBlack);
  display->fillCircle(111, 311, 29, kDim);
  display->fillCircle(355, 311, 29, kDim);
  drawControlIcon("previous", 111, 311, kWhite);
  drawControlIcon("next", 355, 311, kWhite);

  display->setTextSize(1);
  display->setTextColor(kGray);
  display->setCursor(88, 359);
  display->print("ANTERIOR");
  display->setCursor(331, 359);
  display->print("SIGUIENTE");
}

void DisplayManager::buildLvglControlCenter() {
  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* title = createLabel(
      root,
      "Centro de control",
      &lv_font_montserrat_24,
      lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 49);

  lv_obj_t* connectivity = createRoundPanel(
      root, 54, 108, 238, 240, 42, lv_color_hex(0x1C1C1E));
  struct ConnectivityItem {
    int16_t x;
    int16_t y;
    const char* symbol;
    const char* label;
    bool active;
    uint32_t color;
  };
  const ConnectivityItem items[] = {
      {28, 22, LV_SYMBOL_WIFI, "Wi-Fi", wifiConnected_, 0x0A84FF},
      {132, 22, LV_SYMBOL_BLUETOOTH, "Bluetooth", bluetoothEnabled_, 0x0A84FF},
      {80, 126, LV_SYMBOL_UPLOAD, "Nube", cloudConnected_, 0x30D158},
  };
  for (const ConnectivityItem& item : items) {
    lv_obj_t* circle = createRoundPanel(
        connectivity,
        item.x,
        item.y,
        78,
        78,
        LV_RADIUS_CIRCLE,
        lv_color_hex(item.active ? item.color : 0x3A3A3C));
    lv_obj_t* symbol = createLabel(
        circle,
        item.symbol,
        &lv_font_montserrat_28,
        lv_color_white());
    lv_obj_center(symbol);
    lv_obj_t* label = createLabel(
        connectivity,
        item.label,
        &lv_font_montserrat_12,
        item.active ? lv_color_white() : lv_color_hex(0x8E8E93));
    lv_obj_align_to(label, circle, LV_ALIGN_OUT_BOTTOM_MID, 0, 7);
  }

  brightnessSlider = lv_slider_create(root);
  lv_obj_set_pos(brightnessSlider, 326, 116);
  lv_obj_set_size(brightnessSlider, 78, 232);
  lv_slider_set_range(brightnessSlider, 5, 100);
  lv_slider_set_value(brightnessSlider, brightnessPercent_, LV_ANIM_OFF);
  lv_obj_remove_flag(brightnessSlider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(brightnessSlider, 39, LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      brightnessSlider, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(brightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(brightnessSlider, 39, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(
      brightnessSlider, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(
      brightnessSlider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(brightnessSlider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightnessSlider, 0, LV_PART_KNOB);

  lv_obj_t* sun = createLabel(
      root,
      LV_SYMBOL_SETTINGS,
      &lv_font_montserrat_22,
      lv_color_black());
  lv_obj_align_to(sun, brightnessSlider, LV_ALIGN_BOTTOM_MID, 0, -22);
  brightnessValue = createLabel(
      root,
      "",
      &lv_font_montserrat_20,
      lv_color_white());
  lv_obj_align_to(brightnessValue, brightnessSlider, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
  lv_obj_t* caption = createLabel(
      root,
      "BRILLO",
      &lv_font_montserrat_12,
      lv_color_hex(0x8E8E93));
  lv_obj_align_to(caption, brightnessValue, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
  updateControlCenterBrightness();
}

void DisplayManager::buildLvglConnectivityDetails(
    bool wifiScreen,
    bool primaryConnected,
    bool secondaryState,
    const char* primaryDetail,
    const char* secondaryDetail) {
  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* back = createRoundPanel(
      root, 28, 43, 74, 48, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_24, lv_color_white());
  lv_obj_center(backIcon);

  lv_obj_t* title = createLabel(
      root,
      wifiScreen ? "Wi-Fi" : "GoodMorning Cloud",
      &lv_font_montserrat_24,
      lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 51);

  const uint32_t accent = primaryConnected ? 0x30D158 : 0xFF453A;
  lv_obj_t* status = createRoundPanel(
      root, 149, 119, 168, 168, LV_RADIUS_CIRCLE, lv_color_hex(0x1C1C1E));
  lv_obj_set_style_border_width(status, 5, 0);
  lv_obj_set_style_border_color(status, lv_color_hex(accent), 0);
  lv_obj_t* symbol = createLabel(
      status,
      wifiScreen ? LV_SYMBOL_WIFI : LV_SYMBOL_UPLOAD,
      &lv_font_montserrat_36,
      lv_color_hex(accent));
  lv_obj_align(symbol, LV_ALIGN_CENTER, 0, -20);
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

  lv_obj_t* retry = createRoundPanel(
      root, 47, 372, wifiScreen ? 174 : 372, 62, 31, lv_color_hex(0x0A84FF));
  lv_obj_t* retryLabel = createLabel(
      retry,
      "Reintentar",
      &lv_font_montserrat_16,
      lv_color_white());
  lv_obj_center(retryLabel);
  if (wifiScreen) {
    lv_obj_t* configure = createRoundPanel(
        root, 245, 372, 174, 62, 31, lv_color_hex(0x2C2C2E));
    lv_obj_t* configureLabel = createLabel(
        configure,
        "Cambiar red",
        &lv_font_montserrat_16,
        lv_color_white());
    lv_obj_center(configureLabel);
  }
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
  lv_obj_t* back = createRoundPanel(
      root, 28, 43, 74, 48, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_24, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Bluetooth", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 51);
  const uint32_t accent = connected ? 0x30D158 : 0x0A84FF;
  lv_obj_t* status = createRoundPanel(
      root, 149, 112, 168, 168, LV_RADIUS_CIRCLE, lv_color_hex(0x1C1C1E));
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
}

void DisplayManager::showBluetoothDevices(
    const BluetoothScanResult* devices,
    uint8_t count,
    bool scanning,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* back = createRoundPanel(
      root, 28, 35, 62, 46, 23, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_22, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Bluetooth", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 43);
  lv_obj_t* subtitle = createLabel(
      root,
      scanning ? "Buscando accesorios BLE..."
               : (message != nullptr ? message : "Toca uno para conectar"),
      &lv_font_montserrat_12,
      lv_color_hex(scanning ? 0x0A84FF : 0x8E8E93));
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 80);
  for (uint8_t index = 0; index < count && index < 5; ++index) {
    const int16_t y = 111 + index * 57;
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
}

void DisplayManager::showWifiNetworks(
    const char* const* names,
    const int32_t* signalStrengths,
    const bool* secured,
    uint8_t count,
    bool scanning,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* back = createRoundPanel(
      root, 28, 35, 62, 46, 23, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_22, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, "Elegir Wi-Fi", &lv_font_montserrat_24, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 43);
  lv_obj_t* subtitle = createLabel(
      root,
      scanning ? "Buscando redes..." :
          (message != nullptr ? message : "Redes disponibles"),
      &lv_font_montserrat_12,
      lv_color_hex(scanning ? 0x0A84FF : 0x8E8E93));
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 80);

  for (uint8_t index = 0; index < count && index < 5; ++index) {
    const int16_t y = 111 + index * 57;
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
  lv_obj_t* back = createRoundPanel(
      root, 25, 25, 64, 48, 24, lv_color_hex(0x1C1C1E));
  lv_obj_t* backIcon = createLabel(
      back, LV_SYMBOL_LEFT, &lv_font_montserrat_20, lv_color_white());
  lv_obj_center(backIcon);
  lv_obj_t* title = createLabel(
      root, networkName, &lv_font_montserrat_18, lv_color_white());
  lv_obj_set_width(title, 280);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 31);

  lv_obj_t* field = createRoundPanel(
      root, 34, 83, 398, 61, 18, lv_color_hex(0x1C1C1E));
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
}

void DisplayManager::showWifiConnecting(
    const char* networkName,
    const char* message) {
  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* back = createRoundPanel(
      root, 25, 25, 64, 48, 24, lv_color_hex(0x1C1C1E));
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
}

void DisplayManager::buildLvglSpotifyPlayer(const SduiScreen& screen) {
  const SduiComponent* track = nullptr;
  const SduiComponent* artist = nullptr;
  const SduiComponent* album = nullptr;
  const SduiComponent* progress = nullptr;
  const SduiComponent* elapsed = nullptr;
  const SduiComponent* duration = nullptr;
  const SduiComponent* toggle = nullptr;
  const SduiComponent* volume = nullptr;
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (strcmp(component.id, "track") == 0) track = &component;
    else if (strcmp(component.id, "artist") == 0) artist = &component;
    else if (strcmp(component.id, "album") == 0) album = &component;
    else if (strcmp(component.id, "progress") == 0 ||
             strcmp(component.id, "spotify-seek") == 0) progress = &component;
    else if (strcmp(component.id, "elapsed") == 0) elapsed = &component;
    else if (strcmp(component.id, "duration") == 0) duration = &component;
    else if (strcmp(component.id, "spotify-toggle") == 0) toggle = &component;
    else if (strcmp(component.id, "spotify-volume") == 0) volume = &component;
  }

  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* card = createRoundPanel(
      root, 38, 39, 390, 360, 48, lv_color_hex(0x151517));
  lv_obj_t* brand = createLabel(
      card,
      "SPOTIFY",
      &lv_font_montserrat_16,
      lv_color_hex(0x1DB954));
  lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 42, 28);
  lv_obj_t* nowPlaying = createLabel(
      card,
      "REPRODUCIENDO AHORA",
      &lv_font_montserrat_12,
      lv_color_hex(0x8E8E93));
  lv_obj_align(nowPlaying, LV_ALIGN_TOP_LEFT, 42, 51);

  lv_obj_t* trackLabel = createLabel(
      card,
      track == nullptr ? "Sin reproducción" : track->text,
      &lv_font_montserrat_22,
      lv_color_white());
  lv_obj_set_width(trackLabel, 330);
  lv_label_set_long_mode(trackLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(trackLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(trackLabel, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_t* artistLabel = createLabel(
      card,
      artist == nullptr ? "Abre Spotify" : artist->text,
      &lv_font_montserrat_14,
      lv_color_hex(0xD1D1D6));
  lv_obj_align(artistLabel, LV_ALIGN_TOP_MID, 0, 121);
  lv_obj_t* albumLabel = createLabel(
      card,
      album == nullptr ? "" : album->text,
      &lv_font_montserrat_12,
      lv_color_hex(0x636366));
  lv_obj_align(albumLabel, LV_ALIGN_TOP_MID, 0, 144);

  lv_obj_t* bar = lv_bar_create(card);
  lv_obj_set_pos(bar, 34, 190);
  lv_obj_set_size(bar, 322, 6);
  lv_bar_set_range(bar, 0, progress == nullptr ? 100 : progress->maximum);
  lv_bar_set_value(bar, progress == nullptr ? 0 : progress->value, LV_ANIM_ON);
  lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);

  lv_obj_t* elapsedLabel = createLabel(
      card,
      elapsed == nullptr ? "0:00" : elapsed->text,
      &lv_font_montserrat_12,
      lv_color_hex(0x8E8E93));
  lv_obj_align_to(elapsedLabel, bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
  lv_obj_t* durationLabel = createLabel(
      card,
      duration == nullptr ? "0:00" : duration->text,
      &lv_font_montserrat_12,
      lv_color_hex(0x8E8E93));
  lv_obj_align_to(durationLabel, bar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 8);

  if (volume != nullptr) {
    lv_obj_t* volumeSlider = lv_slider_create(root);
    lv_obj_set_pos(volumeSlider, volume->x, volume->y);
    lv_obj_set_size(volumeSlider, volume->width, volume->height);
    lv_slider_set_range(volumeSlider, 0, 100);
    lv_slider_set_value(volumeSlider, volume->value, LV_ANIM_OFF);
    lv_obj_remove_flag(volumeSlider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(volumeSlider, volume->width / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        volumeSlider, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volumeSlider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(
        volumeSlider, volume->width / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(
        volumeSlider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(
        volumeSlider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volumeSlider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(volumeSlider, 0, LV_PART_KNOB);
    lv_obj_t* volumeIcon = createLabel(
        root, LV_SYMBOL_VOLUME_MAX, &lv_font_montserrat_14, lv_color_black());
    lv_obj_align_to(volumeIcon, volumeSlider, LV_ALIGN_BOTTOM_MID, 0, -15);
  }

  struct MediaButton {
    int16_t x;
    int16_t size;
    const char* symbol;
    uint32_t background;
    uint32_t foreground;
  };
  const bool playing = toggle != nullptr && strcmp(toggle->icon, "pause") == 0;
  const MediaButton buttons[] = {
      {70, 62, LV_SYMBOL_PREV, 0x2C2C2E, 0xFFFFFF},
      {153, 84, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, 0xFFFFFF, 0x000000},
      {278, 62, LV_SYMBOL_NEXT, 0x2C2C2E, 0xFFFFFF},
  };
  for (const MediaButton& item : buttons) {
    lv_obj_t* button = createRoundPanel(
        card,
        item.x,
        243,
        item.size,
        item.size,
        LV_RADIUS_CIRCLE,
        lv_color_hex(item.background));
    lv_obj_t* icon = createLabel(
        button,
        item.symbol,
        item.size > 70 ? &lv_font_montserrat_32 : &lv_font_montserrat_24,
        lv_color_hex(item.foreground));
    lv_obj_center(icon);
  }
}

void DisplayManager::buildLvglYoutubeFeed(const SduiScreen& screen) {
  const SduiComponent* account = nullptr;
  const SduiComponent* titles[3] = {};
  const SduiComponent* channels[3] = {};
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (strcmp(component.id, "youtube-account") == 0) {
      account = &component;
    } else if (strncmp(component.id, "youtube-title-", 14) == 0) {
      const uint8_t slot = component.id[14] - '0';
      if (slot < 3) titles[slot] = &component;
    } else if (strncmp(component.id, "youtube-channel-", 16) == 0) {
      const uint8_t slot = component.id[16] - '0';
      if (slot < 3) channels[slot] = &component;
    }
  }

  lv_obj_t* root = cleanLvglScreen();
  lv_obj_t* brand = createRoundPanel(
      root, 172, 42, 122, 54, 18, lv_color_hex(0xFF453A));
  lv_obj_t* play = createLabel(
      brand, LV_SYMBOL_PLAY, &lv_font_montserrat_22, lv_color_white());
  lv_obj_align(play, LV_ALIGN_LEFT_MID, 17, 0);
  lv_obj_t* youtube = createLabel(
      brand, "YouTube", &lv_font_montserrat_16, lv_color_white());
  lv_obj_align(youtube, LV_ALIGN_RIGHT_MID, -13, 0);
  lv_obj_t* accountLabel = createLabel(
      root,
      account == nullptr ? "Tu feed" : account->text,
      &lv_font_montserrat_14,
      lv_color_hex(0xA7A7A7));
  lv_obj_align(accountLabel, LV_ALIGN_TOP_MID, 0, 105);

  for (uint8_t index = 0; index < 3; ++index) {
    if (titles[index] == nullptr) continue;
    lv_obj_t* card = createRoundPanel(
        root,
        48,
        139 + index * 76,
        370,
        66,
        23,
        lv_color_hex(index == 0 ? 0x211617 : 0x1C1C1E));
    lv_obj_t* dot = createRoundPanel(
        card, 16, 18, 30, 30, LV_RADIUS_CIRCLE, lv_color_hex(0xFF453A));
    lv_obj_t* dotPlay = createLabel(
        dot, LV_SYMBOL_PLAY, &lv_font_montserrat_12, lv_color_white());
    lv_obj_center(dotPlay);
    lv_obj_t* title = createLabel(
        card, titles[index]->text, &lv_font_montserrat_14, lv_color_white());
    lv_obj_set_width(title, 285);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 58, 11);
    lv_obj_t* channel = createLabel(
        card,
        channels[index] == nullptr ? "YouTube" : channels[index]->text,
        &lv_font_montserrat_12,
        lv_color_hex(0x8E8E93));
    lv_obj_set_width(channel, 285);
    lv_label_set_long_mode(channel, LV_LABEL_LONG_DOT);
    lv_obj_align(channel, LV_ALIGN_TOP_LEFT, 58, 36);
  }

  lv_obj_t* refresh = createRoundPanel(
      root, 139, 381, 188, 54, 27, lv_color_hex(0xFF453A));
  lv_obj_t* refreshLabel = createLabel(
      refresh, "Actualizar", &lv_font_montserrat_16, lv_color_white());
  lv_obj_center(refreshLabel);
}

void DisplayManager::buildLvglSduiScreen(const SduiScreen& screen) {
  lv_obj_t* root = cleanLvglScreen(screen.background);
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (component.type == ComponentType::kText) {
      lv_obj_t* label = createLabel(
          root,
          component.text,
          fontForSize(component.size),
          lvColor(component.color));
      lv_obj_set_pos(label, component.x, component.y);
      if (component.align == TextAlign::kCenter) {
        lv_obj_set_style_translate_x(label, -lv_obj_get_width(label) / 2, 0);
      }
    } else if (component.type == ComponentType::kIndicator) {
      lv_obj_t* dot = createRoundPanel(
          root,
          component.x - 12,
          component.y - 6,
          12,
          12,
          LV_RADIUS_CIRCLE,
          component.connected ? lv_color_hex(0x30D158)
                              : lv_color_hex(0xFF453A));
      LV_UNUSED(dot);
      lv_obj_t* label = createLabel(
          root, component.text, &lv_font_montserrat_14, lvColor(component.color));
      lv_obj_set_pos(label, component.x + 7, component.y - 8);
    } else if (component.type == ComponentType::kButton) {
      const bool spotifyAction =
          strcmp(screen.screenId, "spotify-actions") == 0;
      lv_obj_t* button = createRoundPanel(
          root,
          component.x,
          component.y,
          component.width,
          component.height,
          component.height / 2,
          spotifyAction ? lv_color_white() : lv_color_hex(0x2C2C2E));
      lv_obj_set_style_border_width(button, spotifyAction ? 0 : 2, 0);
      lv_obj_set_style_border_color(button, lvColor(component.color), 0);
      const char* text = component.text;
      if (strcmp(component.icon, "play") == 0) text = LV_SYMBOL_PLAY;
      else if (strcmp(component.icon, "pause") == 0) text = LV_SYMBOL_PAUSE;
      else if (strcmp(component.icon, "previous") == 0) text = LV_SYMBOL_PREV;
      else if (strcmp(component.icon, "next") == 0) text = LV_SYMBOL_NEXT;
      lv_obj_t* label = createLabel(
          button,
          text,
          spotifyAction ? &lv_font_montserrat_16 : &lv_font_montserrat_24,
          spotifyAction ? lv_color_black() : lvColor(component.color));
      lv_obj_center(label);
    } else if (component.type == ComponentType::kProgress) {
      lv_obj_t* bar = lv_bar_create(root);
      lv_obj_set_pos(bar, component.x, component.y);
      lv_obj_set_size(bar, component.width, component.height);
      lv_bar_set_range(bar, 0, component.maximum);
      lv_bar_set_value(bar, component.value, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(bar, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
      lv_obj_set_style_bg_color(bar, lvColor(component.color), LV_PART_INDICATOR);
    }
  }
}

void DisplayManager::renderSdui(const SduiDocument& document) {
  renderSduiScreen(document);
}

void DisplayManager::renderSduiScreen(const SduiDocument& document) {
  const SduiScreen& active = document.active();
  if (strcmp(active.screenId, "youtube-feed") == 0) {
    buildLvglYoutubeFeed(active);
    return;
  }
  if (strcmp(active.screenId, "spotify-player") == 0) {
    buildLvglSpotifyPlayer(active);
    if (document.screenCount > 1) {
      const int16_t gap = 18;
      const int16_t start =
          (kScreenWidth - (document.screenCount - 1) * gap) / 2;
      for (uint8_t index = 0; index < document.screenCount; ++index) {
        display->fillCircle(
            start + index * gap,
            423,
            index == document.activeScreen ? 5 : 3,
            index == document.activeScreen ? kWhite : kGray);
      }
    }
    return;
  }
  if (strcmp(active.screenId, "device-control-center") == 0) {
    buildLvglControlCenter();
    if (document.screenCount > 1) {
      const int16_t gap = 18;
      const int16_t start =
          (kScreenWidth - (document.screenCount - 1) * gap) / 2;
      for (uint8_t index = 0; index < document.screenCount; ++index) {
        display->fillCircle(
            start + index * gap,
            423,
            index == document.activeScreen ? 5 : 3,
            index == document.activeScreen ? kWhite : kGray);
      }
    }
    return;
  }
  buildLvglSduiScreen(active);
  if (document.screenCount > 1) {
    const int16_t gap = 18;
    const int16_t start =
        (kScreenWidth - (document.screenCount - 1) * gap) / 2;
    for (uint8_t index = 0; index < document.screenCount; ++index) {
      display->fillCircle(
          start + index * gap,
          423,
          index == document.activeScreen ? 5 : 3,
          index == document.activeScreen ? kWhite : kGray);
    }
  }
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
  display->fillRect(0, 0, kScreenWidth, 31, kBlack);
  display->setTextSize(1);
  drawBluetoothIcon(24, 15, bluetoothEnabled);
  drawWifiIcon(58, 15, wifiConnected);
  drawCloudIcon(420, 15, cloudConnected);
  display->setTextColor(kGray);
  display->setCursor(338, 11);
  display->printf("%u%%", batteryPercent);
  display->setCursor(210, 11);
  display->print(timeText);
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
  for (int8_t offset = 0; offset < 3; ++offset) {
    display->drawArc(x, y + 8, 19 - offset, 17 - offset, 220, 320, color);
    display->drawArc(x, y + 8, 12 - offset, 10 - offset, 220, 320, color);
  }
  display->fillCircle(x, y + 9, 3, color);
}

void DisplayManager::drawCloudGlyph(int16_t x, int16_t y, uint16_t color) {
  display->fillCircle(x - 10, y + 3, 8, color);
  display->fillCircle(x, y - 4, 12, color);
  display->fillCircle(x + 12, y + 3, 8, color);
  display->fillRoundRect(x - 18, y + 2, 38, 12, 6, color);
}

void DisplayManager::drawBluetoothGlyph(
    int16_t x,
    int16_t y,
    uint16_t color) {
  for (int8_t offset = -1; offset <= 1; ++offset) {
    display->drawLine(x + offset, y - 20, x + offset, y + 20, color);
    display->drawLine(x + offset, y - 20, x + 12 + offset, y - 8, color);
    display->drawLine(x + 12 + offset, y - 8, x - 10 + offset, y + 11, color);
    display->drawLine(x - 10 + offset, y - 11, x + 12 + offset, y + 8, color);
    display->drawLine(x + 12 + offset, y + 8, x + offset, y + 20, color);
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

void DisplayManager::drawControlCenter() {
  display->fillScreen(kBlack);
  centered("CENTRO DE CONTROL", 52, 2, kWhite);

  // Connectivity module, inspired by the compact grouped control in iOS.
  display->fillRoundRect(58, 112, 232, 232, 42, kPanel);
  const bool states[] = {
      wifiConnected_, bluetoothEnabled_, cloudConnected_};
  const int16_t centersX[] = {123, 225, 174};
  const int16_t centersY[] = {174, 174, 276};
  for (uint8_t index = 0; index < 3; ++index) {
    const uint16_t activeColor = index == 2 ? kGreen : kBlue;
    display->fillCircle(
        centersX[index], centersY[index], 38,
        states[index] ? activeColor : kDim);
  }
  drawWifiGlyph(123, 168, kWhite);
  drawBluetoothGlyph(225, 174, kWhite);
  drawCloudGlyph(174, 272, kWhite);

  display->setTextSize(1);
  display->setTextColor(wifiConnected_ ? kWhite : kGray);
  display->setCursor(105, 218);
  display->print("WIFI");
  display->setTextColor(bluetoothEnabled_ ? kWhite : kGray);
  display->setCursor(198, 218);
  display->print(bluetoothConnected_ ? "ENLAZADO" : "BLUETOOTH");
  display->setTextColor(cloudConnected_ ? kWhite : kGray);
  display->setCursor(159, 318);
  display->print("NUBE");

  updateControlCenterBrightness();
}

void DisplayManager::updateControlCenterBrightness() {
  if (brightnessSlider != nullptr && brightnessValue != nullptr) {
    lv_slider_set_value(brightnessSlider, brightnessPercent_, LV_ANIM_OFF);
    char value[8] = {};
    snprintf(value, sizeof(value), "%u%%", brightnessPercent_);
    lv_label_set_text(brightnessValue, value);
    return;
  }
  // Redraw only this region for smooth touch updates over QSPI.
  constexpr int16_t sliderX = 326;
  constexpr int16_t sliderY = 120;
  constexpr int16_t sliderWidth = 76;
  constexpr int16_t sliderHeight = 224;
  constexpr int16_t radius = sliderWidth / 2;
  display->fillRoundRect(
      sliderX, sliderY, sliderWidth, sliderHeight, radius, kPanel);
  const int16_t fillHeight =
      max<int16_t>(sliderWidth, brightnessPercent_ * sliderHeight / 100);
  const int16_t fillY = sliderY + sliderHeight - fillHeight;
  display->fillRoundRect(
      sliderX, fillY, sliderWidth, fillHeight, radius, kWhite);
  drawSunGlyph(sliderX + sliderWidth / 2, sliderY + sliderHeight - 36, kBlack);

  display->fillRect(318, 360, 96, 52, kBlack);
  display->setTextSize(2);
  display->setTextColor(kWhite);
  display->setCursor(328, 365);
  display->printf("%u%%", brightnessPercent_);
  display->setTextSize(1);
  display->setTextColor(kGray);
  display->setCursor(336, 391);
  display->print("BRILLO");
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
