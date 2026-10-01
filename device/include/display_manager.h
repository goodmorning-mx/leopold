#pragma once

#include <Arduino.h>

#include "app_state.h"
#include "bluetooth_manager.h"
#include "sdui.h"

namespace leopold {

class DisplayManager {
 public:
  bool begin();
  void loop();
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
  void setBrightnessPercent(uint8_t percent);
  uint8_t brightnessPercent() const;
  void toggleBackground();
  void toggleLanguage();
  bool darkBackground() const;
  bool spanishLanguage() const;
  void updateControlCenterBrightness();
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
  bool writeLvglScreenshot(Stream& output);

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
  void drawWifiIcon(int16_t x, int16_t y, bool connected);
  void drawCloudIcon(int16_t x, int16_t y, bool connected);
  void drawBluetoothIcon(int16_t x, int16_t y, bool enabled);
  void drawWifiGlyph(int16_t x, int16_t y, uint16_t color);
  void drawCloudGlyph(int16_t x, int16_t y, uint16_t color);
  void drawBluetoothGlyph(int16_t x, int16_t y, uint16_t color);
  void drawSunGlyph(int16_t x, int16_t y, uint16_t color);
  void drawControlCenter();
  void drawSpotifyPlayer(const SduiScreen& screen);
  void buildLvglControlCenter();
  void buildLvglConnectivityDetails(
      bool wifiScreen,
      bool primaryConnected,
      bool secondaryState,
      const char* primaryDetail,
      const char* secondaryDetail);
  void buildLvglSpotifyPlayer(const SduiScreen& screen);
  void buildLvglYoutubeFeed(const SduiScreen& screen);
  void buildLvglSduiScreen(const SduiScreen& screen);

  uint8_t localExpression_ = 0;
  uint8_t brightnessPercent_ = 70;
  bool darkBackground_ = false;
  bool spanishLanguage_ = false;
  uint32_t lastFaceFrameAt_ = 0;
  bool wifiConnected_ = false;
  bool cloudConnected_ = false;
  bool bluetoothEnabled_ = false;
  bool bluetoothConnected_ = false;
  char pressedButtonId_[kMaxComponentId + 1] = {};
};

}  // namespace leopold
