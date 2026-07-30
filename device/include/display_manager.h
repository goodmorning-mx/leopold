#pragma once

#include <Arduino.h>

#include "app_state.h"
#include "sdui.h"

namespace leopold {

class DisplayManager {
 public:
  bool begin();
  void showState(
      AppState state,
      const char* detail = nullptr,
      const char* secondary = nullptr);
  void showNotActivated();
  void showUpdateRequired();
  void showQr(
      const char* title,
      const char* payload,
      const char* footer = nullptr);
  void showWelcome(const char* ownerName, uint8_t step);
  void showReadyFace();
  void renderSdui(const SduiDocument& document);
  void drawIndicators(
      bool wifiConnected,
      bool cloudConnected,
      bool bluetoothEnabled,
      uint8_t batteryPercent,
      const char* timeText);
  void showLocalFace(uint32_t now, bool force = false);
  void cycleLocalExpression();

 private:
  void centered(const char* text, int16_t y, uint8_t size, uint16_t color);
  void drawComponent(const SduiComponent& component);
  void drawEye(int16_t x, int16_t y, int8_t pupilX, int8_t pupilY);
  void drawControlIcon(
      const char* icon,
      int16_t centerX,
      int16_t centerY,
      uint16_t color);
  void drawSpotifyIcon(
      int16_t centerX,
      int16_t centerY,
      int16_t size,
      uint16_t color);
  void drawInternetIcon(int16_t x, int16_t y, bool connected);
  void drawBluetoothIcon(int16_t x, int16_t y, bool enabled);

  uint8_t localExpression_ = 0;
  uint32_t lastFaceFrameAt_ = 0;
};

}  // namespace leopold
