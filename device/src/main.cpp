#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include "board_config.h"

namespace {

using namespace leopold::board;

Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    kDisplayCs,
    kDisplayClock,
    kDisplayData0,
    kDisplayData1,
    kDisplayData2,
    kDisplayData3);

Arduino_SH8601* display = new Arduino_SH8601(
    bus,
    kDisplayReset,
    0,
    false,
    kScreenWidth,
    kScreenHeight);

enum class Mood : uint8_t {
  kReady,
  kListening,
  kThinking,
};

Mood mood = Mood::kReady;
bool touchWasDown = false;
bool buttonWasDown = false;
uint32_t lastInputAt = 0;
uint32_t lastBlinkAt = 0;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kMorning = 0xFD20;
constexpr uint16_t kBlue = 0x3D7F;

bool readTouch(uint16_t& x, uint16_t& y) {
  Wire.beginTransmission(kTouchAddress);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(kTouchAddress, static_cast<uint8_t>(5)) != 5) {
    return false;
  }

  const uint8_t points = Wire.read() & 0x0F;
  const uint8_t xHigh = Wire.read();
  const uint8_t xLow = Wire.read();
  const uint8_t yHigh = Wire.read();
  const uint8_t yLow = Wire.read();

  if (points == 0) {
    return false;
  }

  x = ((xHigh & 0x0F) << 8) | xLow;
  y = ((yHigh & 0x0F) << 8) | yLow;
  return x < kScreenWidth && y < kScreenHeight;
}

void drawCenteredText(const char* text, int16_t y, uint8_t size, uint16_t color) {
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

void drawEyes(bool closed) {
  display->fillRect(115, 174, 236, 72, kBlack);
  if (closed) {
    display->fillRoundRect(128, 208, 72, 8, 4, kWhite);
    display->fillRoundRect(266, 208, 72, 8, 4, kWhite);
    return;
  }

  const uint16_t color = mood == Mood::kListening ? kMorning : kWhite;
  display->fillCircle(164, 207, 25, color);
  display->fillCircle(302, 207, 25, color);
}

void render() {
  display->fillScreen(kBlack);
  drawCenteredText("GOODMORNING", 66, 2, kMorning);
  drawCenteredText("LEOPOLD", 104, 3, kWhite);
  drawEyes(false);

  switch (mood) {
    case Mood::kReady:
      display->drawRoundRect(147, 285, 172, 56, 28, kBlue);
      drawCenteredText("LISTO", 305, 2, kWhite);
      drawCenteredText("TOCAME", 372, 1, kWhite);
      break;
    case Mood::kListening:
      display->drawRoundRect(113, 285, 240, 56, 28, kMorning);
      drawCenteredText("TE ESCUCHO", 305, 2, kWhite);
      drawCenteredText("AUDIO PROXIMAMENTE", 372, 1, kWhite);
      break;
    case Mood::kThinking:
      display->drawRoundRect(125, 285, 216, 56, 28, kBlue);
      drawCenteredText("PENSANDO", 305, 2, kWhite);
      drawCenteredText("GOODMORNING CLOUD", 372, 1, kWhite);
      break;
  }
}

void advanceMood() {
  mood = static_cast<Mood>((static_cast<uint8_t>(mood) + 1) % 3);
  lastInputAt = millis();
  Serial.printf("Leopold state: %u\n", static_cast<unsigned>(mood));
  render();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("Leopold device firmware starting");

  pinMode(kDisplayEnable, OUTPUT);
  digitalWrite(kDisplayEnable, HIGH);
  pinMode(kBootButton, INPUT_PULLUP);

  Wire.begin(kTouchSda, kTouchScl, 300000);

  if (!display->begin(80000000)) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  display->Display_Brightness(180);
  render();
}

void loop() {
  uint16_t touchX = 0;
  uint16_t touchY = 0;
  const bool touchDown = readTouch(touchX, touchY);
  const bool buttonDown = digitalRead(kBootButton) == LOW;
  const uint32_t now = millis();

  if (((touchDown && !touchWasDown) || (buttonDown && !buttonWasDown)) &&
      now - lastInputAt > 250) {
    advanceMood();
  }

  touchWasDown = touchDown;
  buttonWasDown = buttonDown;

  if (now - lastBlinkAt > 4200) {
    drawEyes(true);
    delay(90);
    drawEyes(false);
    lastBlinkAt = millis();
  }

  delay(16);
}
