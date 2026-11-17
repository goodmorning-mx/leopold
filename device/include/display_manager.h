#pragma once

#include <Arduino.h>

#include "app_state.h"
#include "bluetooth_manager.h"
#include "lvgl_renderer.h"
#include "sdui.h"

namespace leopold {

class DisplayManager {
 public:
  bool begin();
  void loop();
  uint32_t flushCallbackCount() const;
  uint64_t flushPixelCount() const;
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
  void renderSduiScreen(const SduiDocument& document);
  bool setPressedButton(const char* id);
  bool clearPressedButton();
  void setTouchTraceEnabled(bool enabled);
  void setBrightnessPercent(uint8_t percent);
  uint8_t brightnessPercent() const;
  void cycleBackground();
  void toggleLanguage();
  void setAppearanceDark(bool dark);
  void setClockShowWeather(bool enabled);
  void setAccentIndex(uint8_t index);
  uint8_t accentIndex() const;
  void setBackgroundArt(uint8_t index);
  void setClockBackground(uint8_t index);
  uint8_t backgroundArt() const;
  uint8_t clockBackground() const;
  void setClockFace(uint8_t index);
  uint8_t clockFace() const;
  uint8_t backgroundMode() const;
  bool spanishLanguage() const;
  const SharedRenderState& sharedRenderState() const;
  void showWifiDetails(
      bool connected,
      bool portalActive,
      const char* networkName,
      const char* accessPointName,
      int32_t rssi);
  void showCloudDetails(
      bool wifiConnected,
      bool cloudConnected,
      bool retrying,
      uint32_t retryInSeconds);
  void showBluetoothDetails(
      bool enabled,
      bool connected,
      const char* peripheralName);
  void showBluetoothDevices(
      const BluetoothScanResult* devices,
      uint8_t count,
      bool scanning,
      const char* message = nullptr);
  void showWifiNetworks(
      const char* const* names,
      const int32_t* signalStrengths,
      const bool* secured,
      uint8_t count,
      bool scanning,
      const char* message = nullptr);
  void showWifiPassword(
      const char* networkName,
      uint8_t passwordLength,
      int8_t selectedGroup,
      bool uppercase);
  void showWifiConnecting(const char* networkName, const char* message);
  void drawIndicators(
      bool wifiConnected,
      bool cloudConnected,
      bool bluetoothEnabled,
      bool bluetoothConnected,
      uint8_t batteryPercent,
      const char* timeText);
  void showLocalFace(uint32_t now, bool force = false);
  void cycleLocalExpression();
  void showCalibration();
  void exitCalibration();
  bool calibrationMode() const;
  bool writeLvglScreenshot(Stream& output);
  void writeDisplayInfo(Stream& output) const;

 private:
  void centered(const char* text, int16_t y, uint8_t size, uint16_t color);
  void drawEye(int16_t x, int16_t y, int8_t pupilX, int8_t pupilY);
  void drawInternetIcon(int16_t x, int16_t y, bool connected);
  void drawWifiIcon(int16_t x, int16_t y, bool connected);
  void drawCloudIcon(int16_t x, int16_t y, bool connected);
  void drawBluetoothIcon(int16_t x, int16_t y, bool enabled);
  void drawWifiGlyph(int16_t x, int16_t y, uint16_t color);
  void drawCloudGlyph(int16_t x, int16_t y, uint16_t color);
  void drawBluetoothGlyph(int16_t x, int16_t y, uint16_t color);
  void drawSunGlyph(int16_t x, int16_t y, uint16_t color);
  void buildLvglConnectivityDetails(
      bool wifiScreen,
      bool primaryConnected,
      bool secondaryState,
      const char* primaryDetail,
      const char* secondaryDetail);

  uint8_t localExpression_ = 0;
  uint8_t brightnessPercent_ = 70;
  uint8_t backgroundMode_ = 1;
  uint8_t backgroundArt_ = 3;
  uint8_t clockBackground_ = 0;
  uint8_t accentIndex_ = 4;
  uint8_t clockFace_ = 0;
  bool spanishLanguage_ = false;
  uint32_t lastFaceFrameAt_ = 0;
  bool wifiConnected_ = false;
  bool cloudConnected_ = false;
  bool bluetoothEnabled_ = false;
  bool bluetoothConnected_ = false;
  bool calibrationMode_ = false;
  uint8_t batteryPercent_ = 0;
  char timeText_[16] = "--:--";
  char dateText_[16] = "";
  SharedRenderState sharedRenderState_{};
  char pressedButtonId_[kMaxComponentId + 1] = {};
  bool touchTraceEnabled_ = false;
};

}  // namespace leopold
