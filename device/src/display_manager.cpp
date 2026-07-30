#include "display_manager.h"

#include <Arduino_GFX_Library.h>
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

}  // namespace

bool DisplayManager::begin() {
  pinMode(kDisplayEnable, OUTPUT);
  digitalWrite(kDisplayEnable, HIGH);
  if (!display->begin(80000000)) {
    return false;
  }
  display->setBrightness(180);
  display->fillScreen(kBlack);
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
          min<int16_t>(component.height / 2, 24),
          component.color);
      {
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
  }
}

void DisplayManager::renderSdui(const SduiDocument& document) {
  display->fillScreen(document.background);
  for (uint8_t index = 0; index < document.componentCount; ++index) {
    drawComponent(document.components[index]);
  }
}

void DisplayManager::drawIndicators(
    bool wifiConnected,
    bool cloudConnected,
    uint8_t batteryPercent,
    const char* timeText) {
  display->fillRect(0, 0, kScreenWidth, 30, kBlack);
  display->setTextSize(1);
  display->setTextColor(wifiConnected ? kGreen : kRed);
  display->setCursor(18, 11);
  display->print(wifiConnected ? "WIFI" : "WIFI--");
  display->setTextColor(cloudConnected ? kGreen : kRed);
  display->setCursor(102, 11);
  display->print(cloudConnected ? "CLOUD" : "CLOUD--");
  display->setTextColor(kWhite);
  display->setCursor(224, 11);
  display->printf("%u%%", batteryPercent);
  display->setCursor(382, 11);
  display->print(timeText);
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
