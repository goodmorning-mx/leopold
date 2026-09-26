#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include "app_state.h"
#include "board_config.h"
#include "bluetooth_manager.h"
#include "cloud_client.h"
#include "config_guard.h"
#include "display_manager.h"
#include "sdui.h"
#include "telemetry.h"
#include "touch_input.h"
#include "wifi_portal.h"

namespace {

using namespace leopold;

constexpr uint32_t kBootResetHoldMs = 5000;
constexpr uint32_t kIndicatorRefreshMs = 1000;
constexpr uint32_t kDefaultHeartbeatMs = 30000;
constexpr uint32_t kDefaultUiPollMs = 5000;
constexpr uint32_t kOnboardingPollMs = 5000;
constexpr uint32_t kWelcomeScreenMs = 3000;
constexpr uint8_t kEventQueueSize = 4;
constexpr uint8_t kEventMaxAttempts = 3;

enum class LocalPanel : uint8_t {
  kNone,
  kWifi,
  kCloud,
  kWifiNetworks,
  kWifiPassword,
  kWifiConnecting,
  kBluetooth,
  kBluetoothDevices,
};

constexpr uint8_t kVisibleWifiNetworks = 5;
constexpr uint32_t kWifiSelectionTimeoutMs = 20000;

struct WifiNetworkChoice {
  String name;
  int32_t rssi = -100;
  bool secured = true;
};

struct PendingEvent {
  char json[512] = {};
  uint8_t attempts = 0;
  uint32_t retryAt = 0;
  bool occupied = false;
};

DisplayManager screen;
WifiPortal wifi;
BluetoothManager bluetooth;
CloudClient cloud;
TouchInput touch;
Telemetry telemetry;
SduiStore uiStore;
EXT_RAM_BSS_ATTR SduiDocument currentUi;
String cachedRawUi;

AppState appState = AppState::kBooting;
PendingEvent events[kEventQueueSize];
char uiEtag[64] = {};
bool hasUi = false;
bool cloudConnected = false;
bool connectPending = false;
bool onboardingStatusPending = false;
bool uiPending = false;
bool heartbeatPending = false;
bool eventPending = false;
bool bootWasDown = false;
bool bootLongPressHandled = false;
bool wifiWasConnected = false;
bool timeSyncStarted = false;
bool fatalError = false;
bool onboardingPending = false;
bool welcomeActive = false;
bool welcomeShownThisBoot = false;
bool assistantReady = false;
bool indicatorsInitialized = false;
bool lastWifiIndicator = false;
bool lastCloudIndicator = false;
bool lastBluetoothEnabledIndicator = false;
bool lastBluetoothConnectedIndicator = false;
uint8_t welcomeStep = 0;
uint8_t welcomeStepsShown = 0;
uint8_t activeEventIndex = 0;
uint8_t cloudFailures = 0;
uint32_t bootPressedAt = 0;
uint32_t nextCloudAttemptAt = 0;
uint32_t nextUiPollAt = 0;
uint32_t nextOnboardingPollAt = 0;
uint32_t nextHeartbeatAt = 0;
uint32_t nextWelcomeAt = 0;
uint32_t heartbeatIntervalMs = kDefaultHeartbeatMs;
uint32_t lastIndicatorAt = 0;
uint32_t eventSequence = 0;
char onboardingUrl[256] = {};
char ownerDisplayName[81] = {};
LocalPanel localPanel = LocalPanel::kNone;
WifiNetworkChoice wifiNetworks[kVisibleWifiNetworks];
uint8_t wifiNetworkCount = 0;
String selectedWifiName;
String selectedWifiPassword;
int8_t wifiKeyGroup = -1;
bool wifiUppercase = false;
uint32_t wifiSelectionStartedAt = 0;
bool wifiScanRunning = false;
bool bluetoothScanWasRunning = false;
SduiComponent* capturedSlider = nullptr;

void renderBluetoothDevices(const char* message = nullptr) {
  const BluetoothScanResult* results =
      bluetooth.scanResultCount() > 0 ? &bluetooth.scanResult(0) : nullptr;
  screen.showBluetoothDevices(
      results,
      bluetooth.scanResultCount(),
      bluetooth.scanning(),
      message);
}

void beginBluetoothScan() {
  const bool started = bluetooth.startScan();
  bluetoothScanWasRunning = started;
  localPanel = LocalPanel::kBluetoothDevices;
  renderBluetoothDevices(started ? nullptr : "No se pudo iniciar la busqueda");
  Serial.println(started ? "BLE scan started" : "BLE scan could not start");
}

void renderWifiNetworks(const char* message = nullptr) {
  const char* names[kVisibleWifiNetworks] = {};
  int32_t signals[kVisibleWifiNetworks] = {};
  bool secured[kVisibleWifiNetworks] = {};
  for (uint8_t index = 0; index < wifiNetworkCount; ++index) {
    names[index] = wifiNetworks[index].name.c_str();
    signals[index] = wifiNetworks[index].rssi;
    secured[index] = wifiNetworks[index].secured;
  }
  screen.showWifiNetworks(
      names, signals, secured, wifiNetworkCount, wifiScanRunning, message);
}

void beginWifiScan() {
  wifi.prepareForScan();
  WiFi.scanDelete();
  wifiNetworkCount = 0;
  wifiScanRunning = WiFi.scanNetworks(true, true) == WIFI_SCAN_RUNNING;
  localPanel = LocalPanel::kWifiNetworks;
  renderWifiNetworks(wifiScanRunning ? nullptr : "No se pudo iniciar la busqueda");
  Serial.println("On-device Wi-Fi scan started");
}

void renderWifiPassword() {
  screen.showWifiPassword(
      selectedWifiName.c_str(),
      selectedWifiPassword.length(),
      wifiKeyGroup,
      wifiUppercase);
}

char selectedWifiCharacterAt(uint16_t x, uint16_t y) {
  static const char* lowerGroups[] = {
      "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz", "1234567890"};
  static const char* upperGroups[] = {
      "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "1234567890"};
  if (wifiKeyGroup < 0 || wifiKeyGroup > 9 || y < 180) return '\0';
  const char* characters = wifiKeyGroup == 9
      ? "@#$_-.!%*?"
      : (wifiUppercase ? upperGroups[wifiKeyGroup] : lowerGroups[wifiKeyGroup]);
  const uint8_t count = strlen(characters);
  const uint8_t columns = count <= 5 ? count : 5;
  const int16_t width = count <= 5 ? 76 : 64;
  const int16_t gap = 8;
  const int16_t startX = (466 - columns * width - (columns - 1) * gap) / 2;
  const uint8_t row = (y - 180) / 76;
  if (row > 1 || x < startX) return '\0';
  const uint8_t column = (x - startX) / (width + gap);
  if (column >= columns || (x - startX) % (width + gap) >= width) return '\0';
  const uint8_t index = row * 5 + column;
  return index < count ? characters[index] : '\0';
}

void showLocalConnectivityPanel(uint32_t now) {
  if (localPanel == LocalPanel::kWifi) {
    const String networkName = wifi.connected() ? WiFi.SSID() : String("Sin red");
    screen.showWifiDetails(
        wifi.connected(),
        wifi.portalActive(),
        networkName.c_str(),
        wifi.accessPointName(),
        wifi.connected() ? WiFi.RSSI() : 0);
  } else if (localPanel == LocalPanel::kCloud) {
    const uint32_t retryInSeconds =
        nextCloudAttemptAt > now ? (nextCloudAttemptAt - now + 999) / 1000 : 0;
    screen.showCloudDetails(
        wifi.connected(),
        cloudConnected,
        connectPending || !cloudConnected,
        retryInSeconds);
  } else if (localPanel == LocalPanel::kBluetooth) {
    screen.showBluetoothDetails(
        bluetooth.enabled(),
        bluetooth.connected(),
        bluetooth.connectedPeripheralName());
  }
}

void startWelcome(const char* ownerName, uint32_t now) {
  if (welcomeShownThisBoot || ownerName == nullptr || ownerName[0] == '\0') {
    return;
  }
  strlcpy(ownerDisplayName, ownerName, sizeof(ownerDisplayName));
  welcomeShownThisBoot = true;
  welcomeActive = true;
  assistantReady = true;
  welcomeStep = 0;
  welcomeStepsShown = 1;
  screen.showWelcome(ownerDisplayName, welcomeStep);
  nextWelcomeAt = now + kWelcomeScreenMs;
  Serial.printf("Starting welcome for %s\n", ownerDisplayName);
}

void processWelcome(uint32_t now) {
  if (!welcomeActive) {
    return;
  }
  if (now >= nextWelcomeAt) {
    if (welcomeStepsShown >= 2) {
      welcomeActive = false;
      assistantReady = false;
      nextUiPollAt = now;
      Serial.println("Welcome complete; loading SDUI screens");
      return;
    }
    welcomeStep = (welcomeStep + 1) % 2;
    ++welcomeStepsShown;
    screen.showWelcome(ownerDisplayName, welcomeStep);
    nextWelcomeAt = now + kWelcomeScreenMs;
  }
}

void moveWelcome(int8_t direction, uint32_t now) {
  if (!welcomeActive) {
    return;
  }
  welcomeStep =
      direction > 0 ? (welcomeStep + 1) % 2 : (welcomeStep + 1) % 2;
  screen.showWelcome(ownerDisplayName, welcomeStep);
  nextWelcomeAt = now + kWelcomeScreenMs;
}

void setState(
    AppState state,
    const char* detail = nullptr,
    const char* secondary = nullptr) {
  if (appState != state) {
    appState = state;
    Serial.printf("Leopold state: %s\n", appStateName(state));
  }
  const bool preserveUi =
      assistantReady ||
      (hasUi && (state == AppState::kCloudConnecting ||
                state == AppState::kOnline || state == AppState::kOffline));
  if (!preserveUi) {
    screen.showState(state, detail, secondary);
  }
}

uint32_t retryDelayMs(uint8_t failures) {
  const uint8_t exponent = min<uint8_t>(failures, 5);
  const uint32_t base = min<uint32_t>(2000UL << exponent, 60000UL);
  return base + random(0, 751);
}

void scheduleCloudRetry(uint32_t now) {
  cloudConnected = false;
  connectPending = false;
  onboardingStatusPending = false;
  uiPending = false;
  heartbeatPending = false;
  ++cloudFailures;
  nextCloudAttemptAt = now + retryDelayMs(cloudFailures);
  if (onboardingPending) {
    setState(AppState::kOffline, "TERMINA EN", "TU TELEFONO");
  } else {
    setState(AppState::kOffline);
  }
}

bool parseConnectResponse(const CloudResponse& response) {
  if (response.body == nullptr || response.bodyLength == 0) {
    return false;
  }
  JsonDocument document;
  if (deserializeJson(document, response.body, response.bodyLength)) {
    return false;
  }
  const uint32_t heartbeatSeconds = constrain(
      document["heartbeat_interval_seconds"] | 30,
      10,
      3600);
  const uint32_t pollSeconds = constrain(
      document["ui_poll_interval_seconds"] | 5,
      2,
      300);
  heartbeatIntervalMs = heartbeatSeconds * 1000UL;
  currentUi.pollAfterMs = pollSeconds * 1000UL;
  onboardingPending = !(document["claimed"] | false);
  const char* ownerName = document["owner_display_name"] | "";
  if (!onboardingPending && ownerName[0] != '\0') {
    strlcpy(ownerDisplayName, ownerName, sizeof(ownerDisplayName));
  }
  const char* setupUrl = document["onboarding_url"] | "";
  if (onboardingPending && setupUrl[0] != '\0') {
    strlcpy(onboardingUrl, setupUrl, sizeof(onboardingUrl));
  } else if (!onboardingPending) {
    onboardingUrl[0] = '\0';
  }
  return true;
}

EXT_RAM_BSS_ATTR SduiScreen preservedSpotify;
EXT_RAM_BSS_ATTR SduiScreen preservedSpotifyActions;
EXT_RAM_BSS_ATTR SduiScreen preservedYoutube;

SduiScreen& appendScreen(const char* id, const char* background = "#000000") {
  SduiScreen& target = currentUi.screens[currentUi.screenCount++];
  memset(&target, 0, sizeof(target));
  strlcpy(target.screenId, id, sizeof(target.screenId));
  target.background = SduiParser::rgb565(background);
  return target;
}

void addText(
    SduiScreen& target,
    const char* id,
    const char* text,
    int16_t x,
    int16_t y,
    uint8_t size = 2,
    const char* color = "#FFFFFF",
    TextAlign align = TextAlign::kCenter) {
  if (target.componentCount >= kMaxSduiComponents) return;
  SduiComponent& component = target.components[target.componentCount++];
  component.type = ComponentType::kText;
  strlcpy(component.id, id, sizeof(component.id));
  strlcpy(component.text, text, sizeof(component.text));
  component.x = x;
  component.y = y;
  component.size = size;
  component.color = SduiParser::rgb565(color);
  component.align = align;
}

void addButton(
    SduiScreen& target,
    const char* id,
    const char* label,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char* color = "#FFFFFF") {
  if (target.componentCount >= kMaxSduiComponents) return;
  SduiComponent& component = target.components[target.componentCount++];
  component.type = ComponentType::kButton;
  strlcpy(component.id, id, sizeof(component.id));
  strlcpy(component.text, label, sizeof(component.text));
  if (strcmp(id, "open-usb") == 0) {
    strlcpy(component.icon, "usb", sizeof(component.icon));
  }
  component.x = x;
  component.y = y;
  component.width = width;
  component.height = height;
  component.size = 1;
  component.color = SduiParser::rgb565(color);
}

void addBack(SduiScreen& target) {
  addButton(target, "open-home", "<", 24, 30, 66, 52, "#FFFFFF");
}

void syncControlCenterScreen() {
  memset(&preservedSpotify, 0, sizeof(preservedSpotify));
  memset(&preservedSpotifyActions, 0, sizeof(preservedSpotifyActions));
  memset(&preservedYoutube, 0, sizeof(preservedYoutube));
  bool hasSpotifyScreen = false;
  bool hasSpotifyActions = false;
  bool hasYoutubeScreen = false;
  const uint32_t revision = currentUi.revision;
  const uint32_t pollAfterMs = currentUi.pollAfterMs;
  for (uint8_t index = 0; index < currentUi.screenCount; ++index) {
    const SduiScreen& candidate = currentUi.screens[index];
    if (strcmp(candidate.screenId, "spotify-player") == 0) {
      preservedSpotify = candidate;
      hasSpotifyScreen = true;
    } else if (strcmp(candidate.screenId, "spotify-actions") == 0) {
      preservedSpotifyActions = candidate;
      hasSpotifyActions = true;
    } else if (strcmp(candidate.screenId, "youtube-feed") == 0) {
      preservedYoutube = candidate;
      hasYoutubeScreen = true;
    }
  }
  memset(&currentUi, 0, sizeof(currentUi));
  currentUi.revision = revision;
  currentUi.pollAfterMs = pollAfterMs == 0 ? 5000 : pollAfterMs;

  SduiScreen& home = appendScreen("sky-home");
  addText(home, "home-title", "HOME", 233, 208, 3);
  addText(home, "home-subtitle", "LEOPOLD", 233, 244, 1, "#D1D1D6");
  addButton(home, "open-clock", "CLOCK", 197, 34, 72, 72);
  addButton(home, "open-navigation", "NAV", 96, 91, 72, 72, "#0A84FF");
  addButton(home, "open-voltage", "VOLT", 298, 91, 72, 72);
  addButton(home, "open-weather", "WEATHER", 43, 190, 72, 72);
  addButton(home, "open-usb", "USB", 351, 190, 72, 72, "#3D7FFF");
  addButton(home, "open-rgb", "RGB", 96, 291, 72, 72);
  addButton(home, "open-spotify", "SPOTIFY", 298, 291, 72, 72, "#1DB954");
  addButton(home, "open-settings", "SET", 197, 354, 72, 72);
  addButton(home, "open-whatsapp", "CHAT", 197, 122, 72, 72, "#30D158");

  SduiScreen& clock = appendScreen("sky-clock");
  addBack(clock);
  addText(clock, "clock-icon", "CLOCK", 233, 74, 2, "#8E8E93");
  addText(clock, "sky-time", "--:--", 233, 169, 4);
  addText(clock, "clock-zone", "GOODMORNING TIME", 233, 242, 1, "#8E8E93");
  addText(clock, "clock-date", "SYNCED BY WI-FI", 233, 304, 2, "#0A84FF");

  SduiScreen& voltage = appendScreen("sky-voltage");
  addBack(voltage);
  addText(voltage, "voltage-symbol", "USB-C", 233, 77, 2, "#FFD60A");
  addText(voltage, "sky-voltage-value", "-.-- V", 233, 172, 4);
  addText(voltage, "voltage-caption", "INPUT VOLTAGE", 233, 246, 1, "#8E8E93");
  addText(voltage, "voltage-note", "POWER + BATTERY MONITOR", 233, 316, 2, "#FFD60A");

  SduiScreen& whatsapp = appendScreen("sky-whatsapp");
  addBack(whatsapp);
  addText(whatsapp, "whatsapp-title", "WHATSAPP", 233, 52, 3, "#30D158");
  addText(whatsapp, "whatsapp-name", "Sky", 112, 132, 2, "#FFFFFF", TextAlign::kLeft);
  addText(whatsapp, "whatsapp-message", "Ready for the ride?", 112, 166, 1, "#D1D1D6", TextAlign::kLeft);
  addText(whatsapp, "whatsapp-name-2", "GoodMorning", 112, 237, 2, "#FFFFFF", TextAlign::kLeft);
  addText(whatsapp, "whatsapp-message-2", "Leopold is online", 112, 271, 1, "#D1D1D6", TextAlign::kLeft);
  addButton(whatsapp, "whatsapp-reply", "VOICE REPLY", 116, 344, 234, 62, "#30D158");

  SduiScreen& settings = appendScreen("device-control-center");
  SduiComponent& slider = settings.components[settings.componentCount++];
  slider.type = ComponentType::kSlider;
  strlcpy(slider.id, "display-brightness", sizeof(slider.id));
  slider.x = 326;
  slider.y = 120;
  slider.width = 76;
  slider.height = 224;
  slider.value = screen.brightnessPercent();
  slider.maximum = 100;
  slider.color = SduiParser::rgb565("#FFD60A");

  SduiScreen& rgb = appendScreen("sky-rgb");
  addBack(rgb);
  addText(rgb, "rgb-title", "RGB", 233, 54, 3);
  addText(rgb, "rgb-red", "RED", 233, 135, 3, "#FF453A");
  addText(rgb, "rgb-green", "GREEN", 233, 206, 3, "#30D158");
  addText(rgb, "rgb-blue", "BLUE", 233, 277, 3, "#0A84FF");
  addButton(rgb, "rgb-toggle", "LIGHTS ON / OFF", 105, 350, 256, 58);

  SduiScreen& weather = appendScreen("sky-weather");
  addBack(weather);
  addText(weather, "weather-title", "WEATHER", 233, 52, 3, "#0A84FF");
  addText(weather, "weather-temp", "72 F", 233, 128, 4);
  addText(weather, "weather-condition", "CURRENT CONDITIONS", 233, 190, 1, "#8E8E93");
  addText(weather, "weather-humidity", "HUMIDITY 40%", 233, 246, 2);
  addText(weather, "weather-location", "LOCATION BY IP", 233, 298, 1, "#8E8E93");
  addButton(weather, "weather-units", "F / C", 153, 354, 160, 58, "#0A84FF");

  SduiScreen& navigation = appendScreen("sky-navigation");
  addBack(navigation);
  addText(navigation, "nav-distance", "300 m", 233, 46, 2, "#0A84FF");
  addText(navigation, "nav-arrow", ">", 233, 104, 4, "#FFFFFF");
  addText(navigation, "nav-road", "Turn right on Reforma", 233, 207, 2);
  addText(navigation, "nav-next", "THEN  KEEP LEFT", 233, 267, 1, "#8E8E93");
  addText(navigation, "nav-altitude", "ALTITUDE  2240 m", 233, 327, 2, "#0A84FF");

  if (hasSpotifyScreen) {
    currentUi.screens[currentUi.screenCount++] = preservedSpotify;
  } else {
    SduiScreen& spotify = appendScreen("spotify-player");
    addText(spotify, "track", "Connect Spotify", 233, 72, 2);
    addText(spotify, "artist", "Open Leopold on your phone", 233, 113, 1, "#A7A7A7");
    addText(spotify, "album", "Spotify Connect", 233, 141, 1, "#686868");
    addButton(spotify, "spotify-prev", "PREV", 70, 294, 72, 72);
    addButton(spotify, "spotify-toggle", "PLAY", 191, 288, 84, 84, "#1DB954");
    addButton(spotify, "spotify-next", "NEXT", 324, 294, 72, 72);
  }
  if (hasSpotifyActions && currentUi.screenCount < kMaxSduiScreens) {
    currentUi.screens[currentUi.screenCount++] = preservedSpotifyActions;
  }
  if (hasYoutubeScreen && currentUi.screenCount < kMaxSduiScreens) {
    currentUi.screens[currentUi.screenCount++] = preservedYoutube;
  }
  Serial.printf("Sky shell ready: %u screens\n", currentUi.screenCount);
}

void acceptUi(const CloudResponse& response, uint32_t now) {
  char previousScreenId[kMaxComponentId + 1] = {};
  if (hasUi && currentUi.screenCount > 0) {
    strlcpy(
        previousScreenId,
        currentUi.active().screenId,
        sizeof(previousScreenId));
  }
  const SduiParseResult parsed =
      SduiParser::parse(response.body, response.bodyLength, currentUi);
  if (parsed == SduiParseResult::kUnsupportedSchema) {
    screen.showUpdateRequired();
    nextUiPollAt = now + 30000;
    return;
  }
  if (parsed != SduiParseResult::kOk) {
    Serial.println("Rejected invalid SDUI document");
    nextUiPollAt = now + max<uint32_t>(currentUi.pollAfterMs, 5000);
    return;
  }

  syncControlCenterScreen();

  if (previousScreenId[0] != '\0') {
    for (uint8_t index = 0; index < currentUi.screenCount; ++index) {
      if (strcmp(currentUi.screens[index].screenId, previousScreenId) == 0) {
        currentUi.activeScreen = index;
        break;
      }
    }
  }
  if (strcmp(currentUi.active().screenId, "leopold-welcome") == 0) {
    hasUi = false;
    assistantReady = true;
    welcomeActive = true;
    welcomeStep = 0;
    welcomeStepsShown = 1;
    screen.showWelcome(ownerDisplayName, welcomeStep);
    nextWelcomeAt = now + kWelcomeScreenMs;
    nextUiPollAt = now + currentUi.pollAfterMs;
    return;
  }
  welcomeActive = false;
  assistantReady = false;
  hasUi = true;
  if (response.etag[0] != '\0') {
    strlcpy(uiEtag, response.etag, sizeof(uiEtag));
  } else {
    snprintf(
        uiEtag,
        sizeof(uiEtag),
        "\"rev-%lu\"",
        static_cast<unsigned long>(currentUi.revision));
  }
  if (!uiStore.save(response.body, response.bodyLength)) {
    Serial.println("SDUI cache write failed");
  }
  screen.renderSdui(currentUi);
  nextUiPollAt = now + currentUi.pollAfterMs;
}

void handleCloudResponse(CloudResponse& response, uint32_t now) {
  const bool successful =
      response.httpCode >= 200 && response.httpCode < 300;
  switch (response.operation) {
    case CloudOperation::kConnect:
      connectPending = false;
      if (response.httpCode == 401 || response.httpCode == 403) {
        cloudConnected = false;
        screen.showNotActivated();
        appState = AppState::kError;
        Serial.println("Leopold state: ERROR");
        nextCloudAttemptAt = now + retryDelayMs(++cloudFailures);
      } else if (successful && parseConnectResponse(response)) {
        cloudConnected = true;
        cloudFailures = 0;
        setState(AppState::kOnline);
        nextHeartbeatAt = now + heartbeatIntervalMs;
        if (onboardingPending && onboardingUrl[0] != '\0') {
          screen.showState(
              AppState::kOnline,
              "TERMINA EN",
              "TU TELEFONO");
          nextOnboardingPollAt = now + kOnboardingPollMs;
          Serial.println("Waiting for claim from captive onboarding");
        } else {
          wifi.completeSetup();
          startWelcome(ownerDisplayName, now);
          if (!welcomeActive) {
            nextUiPollAt = now;
          }
        }
      } else {
        scheduleCloudRetry(now);
      }
      break;

    case CloudOperation::kOnboardingStatus:
      onboardingStatusPending = false;
      if (successful && response.body != nullptr) {
        JsonDocument document;
        if (!deserializeJson(document, response.body, response.bodyLength) &&
            (document["claimed"] | false)) {
          onboardingPending = false;
          onboardingUrl[0] = '\0';
          wifi.completeSetup();
          const char* ownerName = document["owner_display_name"] | "";
          startWelcome(ownerName, now);
          if (!welcomeActive) {
            nextUiPollAt = now;
          }
          Serial.println("Device claimed successfully");
        } else {
          nextOnboardingPollAt = now + kOnboardingPollMs;
        }
      } else {
        nextOnboardingPollAt = now + kOnboardingPollMs;
      }
      break;

    case CloudOperation::kFetchUi:
      uiPending = false;
      if (response.httpCode == HTTP_CODE_NOT_MODIFIED) {
        cloudConnected = true;
        nextUiPollAt = now + currentUi.pollAfterMs;
      } else if (successful && response.body != nullptr) {
        cloudConnected = true;
        acceptUi(response, now);
      } else {
        scheduleCloudRetry(now);
      }
      break;

    case CloudOperation::kHeartbeat:
      heartbeatPending = false;
      if (successful) {
        cloudConnected = true;
        nextHeartbeatAt = now + heartbeatIntervalMs;
      } else {
        scheduleCloudRetry(now);
      }
      break;

    case CloudOperation::kEvent:
      eventPending = false;
      if (events[activeEventIndex].occupied) {
        if (successful) {
          events[activeEventIndex].occupied = false;
        } else if (++events[activeEventIndex].attempts >= kEventMaxAttempts) {
          events[activeEventIndex].occupied = false;
        } else {
          events[activeEventIndex].retryAt =
              now + retryDelayMs(events[activeEventIndex].attempts);
        }
      }
      if (!successful && response.httpCode < 0) {
        scheduleCloudRetry(now);
      }
      break;
  }
  response.release();
}

void formatOccurredAt(char output[25]) {
  const time_t now = time(nullptr);
  if (now < 1700000000) {
    strlcpy(output, "1970-01-01T00:00:00Z", 25);
    return;
  }
  struct tm utc = {};
  gmtime_r(&now, &utc);
  strftime(output, 25, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

void queueTap(const char* componentId, uint32_t revision, uint32_t now) {
  uint8_t slot = kEventQueueSize;
  for (uint8_t index = 0; index < kEventQueueSize; ++index) {
    if (!events[index].occupied) {
      slot = index;
      break;
    }
  }
  if (slot == kEventQueueSize) {
    Serial.println("Event queue full; tap discarded");
    return;
  }

  char occurredAt[25] = {};
  char eventId[64] = {};
  formatOccurredAt(occurredAt);
  snprintf(
      eventId,
      sizeof(eventId),
      "%s-%08lx-%04lx",
      LEOPOLD_DEVICE_ID,
      static_cast<unsigned long>(now),
      static_cast<unsigned long>(eventSequence++ & 0xFFFF));
  snprintf(
      events[slot].json,
      sizeof(events[slot].json),
      "{\"event_id\":\"%s\",\"revision\":%lu,\"type\":\"tap\","
      "\"component_id\":\"%s\",\"payload\":{},\"occurred_at\":\"%s\"}",
      eventId,
      static_cast<unsigned long>(revision),
      componentId,
      occurredAt);
  events[slot].attempts = 0;
  events[slot].retryAt = now;
  events[slot].occupied = true;
}

const SduiComponent* firstButton() {
  const SduiScreen& active = currentUi.active();
  for (uint8_t index = 0; index < active.componentCount; ++index) {
    if (active.components[index].type == ComponentType::kButton) {
      return &active.components[index];
    }
  }
  return nullptr;
}

bool openLocalScreen(const char* componentId) {
  if (componentId == nullptr || strncmp(componentId, "open-", 5) != 0) {
    return false;
  }
  const char* requested = componentId + 5;
  const char* screenId = nullptr;
  if (strcmp(requested, "home") == 0) screenId = "sky-home";
  else if (strcmp(requested, "clock") == 0) screenId = "sky-clock";
  else if (strcmp(requested, "voltage") == 0) screenId = "sky-voltage";
  else if (strcmp(requested, "whatsapp") == 0) screenId = "sky-whatsapp";
  else if (strcmp(requested, "usb") == 0) screenId = "computer-controls";
  else if (strcmp(requested, "settings") == 0) screenId = "device-control-center";
  else if (strcmp(requested, "rgb") == 0) screenId = "sky-rgb";
  else if (strcmp(requested, "weather") == 0) screenId = "sky-weather";
  else if (strcmp(requested, "navigation") == 0) screenId = "sky-navigation";
  else if (strcmp(requested, "spotify") == 0) screenId = "spotify-player";
  if (screenId == nullptr) return false;
  for (uint8_t index = 0; index < currentUi.screenCount; ++index) {
    if (strcmp(currentUi.screens[index].screenId, screenId) == 0) {
      currentUi.activeScreen = index;
      screen.renderSduiScreen(currentUi);
      lastIndicatorAt = 0;
      Serial.printf("Local screen opened: %s\n", screenId);
      return true;
    }
  }
  return false;
}

void processInput(uint32_t now) {
  TouchPoint point;
  TouchGesture gesture = TouchGesture::kNone;
  const bool hasGesture = touch.pollGesture(gesture, point, now);
  if (hasGesture && gesture == TouchGesture::kSwipeUp &&
      capturedSlider == nullptr && hasUi) {
    localPanel = LocalPanel::kNone;
    openLocalScreen("open-home");
    Serial.println("Global swipe up: home");
  } else if (hasGesture && gesture == TouchGesture::kTap &&
      localPanel == LocalPanel::kBluetoothDevices) {
    if (point.x <= 100 && point.y <= 92) {
      localPanel = LocalPanel::kBluetooth;
      showLocalConnectivityPanel(now);
    } else if (point.y >= 400) {
      beginBluetoothScan();
    } else if (!bluetooth.scanning() && point.y >= 105 && point.y < 400) {
      const uint8_t index = (point.y - 111) / 57;
      if (index < bluetooth.scanResultCount()) {
        renderBluetoothDevices("Conectando...");
        const bool connected = bluetooth.connectTo(index);
        localPanel = LocalPanel::kBluetooth;
        showLocalConnectivityPanel(now);
        Serial.println(connected ? "BLE peripheral connected" : "BLE connection failed");
      }
    }
  } else if (hasGesture && gesture == TouchGesture::kTap &&
      localPanel == LocalPanel::kWifiNetworks) {
    if (point.x <= 100 && point.y <= 92) {
      localPanel = LocalPanel::kWifi;
      showLocalConnectivityPanel(now);
    } else if (point.y >= 400) {
      beginWifiScan();
    } else if (point.y >= 105 && point.y < 400) {
      const uint8_t index = (point.y - 111) / 57;
      if (index < wifiNetworkCount) {
        selectedWifiName = wifiNetworks[index].name;
        selectedWifiPassword = String();
        wifiKeyGroup = -1;
        wifiUppercase = false;
        if (wifiNetworks[index].secured) {
          localPanel = LocalPanel::kWifiPassword;
          renderWifiPassword();
        } else {
          localPanel = LocalPanel::kWifiConnecting;
          wifiSelectionStartedAt = now;
          wifi.connectToNetwork(selectedWifiName.c_str(), "", now);
          screen.showWifiConnecting(
              selectedWifiName.c_str(), "Negociando conexion...");
        }
        Serial.printf("Wi-Fi selected on device: %s\n", selectedWifiName.c_str());
      }
    }
  } else if (hasGesture && gesture == TouchGesture::kTap &&
             localPanel == LocalPanel::kWifiPassword) {
    if (point.x <= 88 && point.y <= 76) {
      if (wifiKeyGroup >= 0) {
        wifiKeyGroup = -1;
        renderWifiPassword();
      } else {
        localPanel = LocalPanel::kWifiNetworks;
        renderWifiNetworks();
      }
    } else if (point.y >= 75 && point.y <= 151 && point.x >= 340) {
      if (!selectedWifiPassword.isEmpty()) {
        selectedWifiPassword.remove(selectedWifiPassword.length() - 1);
        renderWifiPassword();
      }
    } else if (wifiKeyGroup < 0 &&
               point.y >= 150 && point.y < 360 &&
               point.x >= 50 && point.x <= 416) {
      const uint8_t row = (point.y - 159) / 68;
      const uint8_t column = (point.x - 58) / 119;
      if (row < 3 && column < 3 &&
          (point.x - 58) % 119 < 111 &&
          (point.y - 159) % 68 < 60) {
        wifiKeyGroup = row * 3 + column;
        renderWifiPassword();
      }
    } else if (wifiKeyGroup < 0 && point.y >= 360 && point.y <= 432) {
      const uint8_t action = point.x < 177 ? 0 : (point.x < 296 ? 1 : 2);
      if (action == 0) {
        wifiUppercase = !wifiUppercase;
        renderWifiPassword();
      } else if (action == 1) {
        wifiKeyGroup = 9;
        renderWifiPassword();
      } else if (!selectedWifiPassword.isEmpty()) {
        localPanel = LocalPanel::kWifiConnecting;
        wifiSelectionStartedAt = now;
        wifi.connectToNetwork(
            selectedWifiName.c_str(), selectedWifiPassword.c_str(), now);
        selectedWifiPassword = String();
        screen.showWifiConnecting(
            selectedWifiName.c_str(), "Validando contrasena...");
      }
    } else if (wifiKeyGroup >= 0) {
      const char key = selectedWifiCharacterAt(point.x, point.y);
      if (key != '\0' && selectedWifiPassword.length() < 63) {
        selectedWifiPassword += key;
        wifiKeyGroup = -1;
        renderWifiPassword();
      }
    }
  } else if (hasGesture && gesture == TouchGesture::kTap &&
             localPanel == LocalPanel::kWifiConnecting) {
    if (point.x <= 100 && point.y <= 92) {
      WiFi.disconnect(false, false);
      localPanel = LocalPanel::kWifiNetworks;
      renderWifiNetworks("Conexion cancelada");
      Serial.println("On-device Wi-Fi connection cancelled");
    }
  } else if (hasGesture && gesture == TouchGesture::kTap &&
      localPanel != LocalPanel::kNone) {
    if (point.x <= 116 && point.y <= 108) {
      localPanel = LocalPanel::kNone;
      screen.renderSduiScreen(currentUi);
      lastIndicatorAt = 0;
      Serial.println("Connectivity details closed");
    } else if (point.y >= 356 && point.y <= 450) {
      if (localPanel == LocalPanel::kWifi) {
        if (point.x < 233) {
          wifi.reconnectNow(now);
        } else {
          beginWifiScan();
        }
      } else if (localPanel == LocalPanel::kCloud && wifi.connected()) {
        cloudConnected = false;
        cloudFailures = 0;
        nextCloudAttemptAt = now;
        Serial.println("Manual cloud reconnect requested");
      } else if (localPanel == LocalPanel::kBluetooth) {
        beginBluetoothScan();
      }
      showLocalConnectivityPanel(now);
    }
  } else if (hasGesture && gesture == TouchGesture::kTap && hasUi &&
             ((strcmp(currentUi.active().screenId, "device-control-center") == 0 &&
               point.x <= 100 && point.y <= 94) ||
              (strcmp(currentUi.active().screenId, "spotify-player") == 0 &&
               point.x <= 88 && point.y <= 94))) {
    openLocalScreen("open-home");
  } else if (hasGesture && gesture == TouchGesture::kTap && hasUi &&
             strcmp(currentUi.active().screenId, "device-control-center") == 0 &&
             ((point.x >= 72 && point.x <= 170 &&
               point.y >= 120 && point.y <= 218) ||
              (point.x >= 180 && point.x <= 278 &&
               point.y >= 120 && point.y <= 218) ||
              (point.x >= 124 && point.x <= 222 &&
               point.y >= 224 && point.y <= 322))) {
    localPanel = point.y >= 224
        ? LocalPanel::kCloud
        : (point.x >= 180 ? LocalPanel::kBluetooth : LocalPanel::kWifi);
    showLocalConnectivityPanel(now);
    Serial.println(
        localPanel == LocalPanel::kWifi
            ? "Wi-Fi details opened"
            : (localPanel == LocalPanel::kBluetooth
                   ? "Bluetooth details opened"
                   : "Cloud details opened"));
  }
  SduiComponent* slider = nullptr;
  if (hasGesture && hasUi && localPanel == LocalPanel::kNone) {
    if (gesture == TouchGesture::kDrag && capturedSlider == nullptr) {
      capturedSlider = findSliderAt(currentUi.active(), point.x, point.y);
    }
    slider = capturedSlider != nullptr
        ? capturedSlider
        : findSliderAt(currentUi.active(), point.x, point.y);
  }
  if (localPanel != LocalPanel::kNone) {
    // Local connectivity screens consume touch before SDUI navigation.
  } else if (slider != nullptr) {
    if (slider->height > slider->width) {
      const int16_t relativeY = min<int16_t>(
          max<int16_t>(slider->y + slider->height - point.y, 0),
          slider->height);
      slider->value = static_cast<uint32_t>(relativeY) * slider->maximum /
                      slider->height;
    } else {
      const int16_t relativeX = min<int16_t>(
          max<int16_t>(point.x - slider->x, 0),
          slider->width);
      slider->value = static_cast<uint32_t>(relativeX) * slider->maximum /
                      slider->width;
    }
    if (strcmp(slider->id, "display-brightness") == 0) {
      screen.setBrightnessPercent(slider->value);
      for (uint8_t index = 0;
           index < currentUi.active().componentCount;
           ++index) {
        SduiComponent& component = currentUi.active().components[index];
        if (component.type == ComponentType::kText &&
            strcmp(component.id, "brightness-value") == 0) {
          snprintf(
              component.text,
              sizeof(component.text),
              "%u%%",
              screen.brightnessPercent());
        }
      }
      if (strcmp(currentUi.active().screenId, "device-control-center") == 0) {
        screen.updateControlCenterBrightness();
      } else {
        screen.renderSduiScreen(currentUi);
      }
    } else if (gesture != TouchGesture::kDrag &&
               (strcmp(slider->id, "spotify-volume") == 0 ||
                strcmp(slider->id, "spotify-seek") == 0)) {
      char controlId[64] = {};
      snprintf(
          controlId,
          sizeof(controlId),
          "%s-%u",
          slider->id,
          min<uint16_t>(slider->value, 100));
      queueTap(controlId, currentUi.revision, now);
    }
    lastIndicatorAt = 0;
  } else if (hasGesture && welcomeActive &&
      (gesture == TouchGesture::kSwipeLeft ||
       gesture == TouchGesture::kSwipeRight)) {
    moveWelcome(gesture == TouchGesture::kSwipeLeft ? 1 : -1, now);
    Serial.println(
        gesture == TouchGesture::kSwipeLeft
            ? "Welcome swiped left"
            : "Welcome swiped right");
  } else if (hasUi &&
             (gesture == TouchGesture::kSwipeLeft ||
              gesture == TouchGesture::kSwipeRight) &&
             currentUi.screenCount > 1) {
    if (gesture == TouchGesture::kSwipeLeft) {
      currentUi.activeScreen =
          (currentUi.activeScreen + 1) % currentUi.screenCount;
    } else {
      currentUi.activeScreen =
          (currentUi.activeScreen + currentUi.screenCount - 1) %
          currentUi.screenCount;
    }
    screen.renderSduiScreen(currentUi);
    lastIndicatorAt = 0;
    Serial.printf(
        "SDUI screen: %s (%u/%u)\n",
        currentUi.active().screenId,
        currentUi.activeScreen + 1,
        currentUi.screenCount);
  } else if (gesture == TouchGesture::kTap && hasUi) {
    Serial.printf("Touch tap: x=%u y=%u\n", point.x, point.y);
    const SduiComponent* button =
        findButtonAt(currentUi.active(), point.x, point.y);
    if (button != nullptr) {
      if (!openLocalScreen(button->id)) {
        Serial.println("Touch matched SDUI button");
        queueTap(button->id, currentUi.revision, now);
      }
    } else {
      Serial.println("Touch did not match an SDUI button");
    }
  }

  if (hasGesture && gesture != TouchGesture::kDrag) {
    capturedSlider = nullptr;
  }

  const bool bootDown = digitalRead(board::kBootButton) == LOW;
  if (bootDown && !bootWasDown) {
    bootPressedAt = now;
    bootLongPressHandled = false;
  }
  if (bootDown && !bootLongPressHandled &&
      now - bootPressedAt >= kBootResetHoldMs) {
    bootLongPressHandled = true;
    screen.showState(
        AppState::kWifiPortal,
        "BORRANDO WIFI",
        "REINICIANDO");
    Serial.println("Clearing Wi-Fi settings");
    wifi.clearCredentialsAndRestart();
  }
  if (!bootDown && bootWasDown && !bootLongPressHandled) {
    const SduiComponent* button = hasUi ? firstButton() : nullptr;
    if (button != nullptr) {
      queueTap(button->id, currentUi.revision, now);
    } else {
      screen.cycleLocalExpression();
    }
  }
  bootWasDown = bootDown;
}

void processEvents(uint32_t now) {
  if (!cloudConnected || eventPending) {
    return;
  }
  for (uint8_t index = 0; index < kEventQueueSize; ++index) {
    if (events[index].occupied && now >= events[index].retryAt &&
        cloud.sendEvent(events[index].json)) {
      activeEventIndex = index;
      eventPending = true;
      break;
    }
  }
}

void processNetwork(uint32_t now) {
  bluetooth.loop();
  if (localPanel == LocalPanel::kBluetoothDevices) {
    if (bluetoothScanWasRunning && !bluetooth.scanning()) {
      bluetoothScanWasRunning = false;
      renderBluetoothDevices(
          bluetooth.scanResultCount() == 0 ? "No encontramos accesorios BLE" : nullptr);
    }
    return;
  }
  if (localPanel == LocalPanel::kWifiNetworks) {
    const int16_t scanResult = WiFi.scanComplete();
    if (wifiScanRunning && scanResult >= 0) {
      wifiScanRunning = false;
      wifiNetworkCount = min<uint8_t>(scanResult, kVisibleWifiNetworks);
      for (uint8_t index = 0; index < wifiNetworkCount; ++index) {
        wifiNetworks[index].name = WiFi.SSID(index);
        wifiNetworks[index].rssi = WiFi.RSSI(index);
        wifiNetworks[index].secured =
            WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
      renderWifiNetworks(
          wifiNetworkCount == 0 ? "No encontramos redes" : nullptr);
      Serial.printf("On-device Wi-Fi scan found %u networks\n", wifiNetworkCount);
    } else if (wifiScanRunning && scanResult == WIFI_SCAN_FAILED) {
      wifiScanRunning = false;
      renderWifiNetworks("La busqueda fallo. Intenta otra vez");
    }
    return;
  }
  if (localPanel == LocalPanel::kWifiPassword) {
    return;
  }
  if (localPanel == LocalPanel::kWifiConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiWasConnected = true;
      cloudConnected = false;
      cloudFailures = 0;
      nextCloudAttemptAt = now;
      localPanel = LocalPanel::kWifi;
      selectedWifiPassword = String();
      showLocalConnectivityPanel(now);
      Serial.printf("On-device Wi-Fi connected: %s\n", WiFi.SSID().c_str());
    } else if (now - wifiSelectionStartedAt >= kWifiSelectionTimeoutMs ||
               WiFi.status() == WL_CONNECT_FAILED) {
      localPanel = LocalPanel::kWifiNetworks;
      renderWifiNetworks("No se pudo conectar. Revisa la contrasena");
      Serial.println("On-device Wi-Fi connection failed");
    }
    return;
  }
  wifi.loop(now);
  if (wifi.portalActive()) {
    if (localPanel == LocalPanel::kWifi) {
      return;
    }
    if (appState != AppState::kWifiPortal) {
      appState = AppState::kWifiPortal;
      if (hasUi) {
        screen.renderSduiScreen(currentUi);
        lastIndicatorAt = 0;
      } else {
        screen.showQr(
            "CONFIGURA WIFI",
            wifi.setupUrl(),
            wifi.accessPointName());
      }
      Serial.printf("Leopold state: %s\n", appStateName(appState));
    }
    return;
  }
  if (!wifi.connected()) {
    cloudConnected = false;
    wifiWasConnected = false;
    const AppState disconnectedState =
        hasUi ? AppState::kOffline : AppState::kWifiConnecting;
    if (appState != disconnectedState) {
      setState(disconnectedState);
    }
    return;
  }

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    if (!timeSyncStarted) {
      configTzTime(
          "CST6",
          "pool.ntp.org",
          "time.google.com",
          "time.cloudflare.com");
      timeSyncStarted = true;
    }
    screen.showState(
        AppState::kOnline,
        "WIFI CONECTADO",
        "CONECTANDO NUBE");
    appState = AppState::kCloudConnecting;
    nextCloudAttemptAt = now + 1200;
    return;
  }

  if (!cloudConnected && !connectPending && now >= nextCloudAttemptAt) {
    setState(AppState::kCloudConnecting);
    connectPending = cloud.requestConnect(wifi.setupToken());
    if (!connectPending) {
      nextCloudAttemptAt = now + 1000;
    }
  }

  if (cloudConnected && onboardingPending &&
      !onboardingStatusPending && now >= nextOnboardingPollAt) {
    onboardingStatusPending = cloud.requestOnboardingStatus();
    if (!onboardingStatusPending) {
      nextOnboardingPollAt = now + 1000;
    }
  }
  if (cloudConnected && !onboardingPending &&
      !uiPending && now >= nextUiPollAt) {
    uiPending = cloud.requestUi(uiEtag);
  }
  if (cloudConnected && !heartbeatPending && now >= nextHeartbeatAt) {
    heartbeatPending = cloud.requestHeartbeat(
        WiFi.RSSI(),
        telemetry.batteryPercent(),
        now / 1000);
  }
}

void refreshIndicators(uint32_t now) {
  if (now - lastIndicatorAt < kIndicatorRefreshMs) {
    return;
  }
  char timeText[6] = {};
  telemetry.formattedTime(timeText);
  const uint16_t millivolts = telemetry.batteryMillivolts();
  const uint8_t batteryPercent = telemetry.batteryPercent();
  char voltageText[16] = {};
  snprintf(
      voltageText,
      sizeof(voltageText),
      "%u.%02u V",
      millivolts / 1000,
      (millivolts % 1000) / 10);
  for (uint8_t screenIndex = 0; screenIndex < currentUi.screenCount; ++screenIndex) {
    SduiScreen& liveScreen = currentUi.screens[screenIndex];
    for (uint8_t componentIndex = 0;
         componentIndex < liveScreen.componentCount;
         ++componentIndex) {
      SduiComponent& component = liveScreen.components[componentIndex];
      if (strcmp(component.id, "sky-time") == 0) {
        strlcpy(component.text, timeText, sizeof(component.text));
      } else if (strcmp(component.id, "sky-voltage-value") == 0) {
        strlcpy(component.text, voltageText, sizeof(component.text));
      }
    }
  }
  const bool wifiConnected = wifi.connected();
  const bool bluetoothEnabled = bluetooth.enabled();
  const bool bluetoothConnected = bluetooth.connected();
  const bool statusChanged =
      !indicatorsInitialized || wifiConnected != lastWifiIndicator ||
      cloudConnected != lastCloudIndicator ||
      bluetoothEnabled != lastBluetoothEnabledIndicator ||
      bluetoothConnected != lastBluetoothConnectedIndicator;
  if (localPanel != LocalPanel::kNone) {
    if (statusChanged || now - lastIndicatorAt >= kIndicatorRefreshMs) {
      showLocalConnectivityPanel(now);
    }
    indicatorsInitialized = true;
    lastWifiIndicator = wifiConnected;
    lastCloudIndicator = cloudConnected;
    lastBluetoothEnabledIndicator = bluetoothEnabled;
    lastBluetoothConnectedIndicator = bluetoothConnected;
    lastIndicatorAt = now;
    return;
  }
  screen.drawIndicators(
      wifiConnected,
      cloudConnected,
      bluetoothEnabled,
      bluetoothConnected,
      batteryPercent,
      timeText);
  const bool liveSkyScreen = hasUi && currentUi.screenCount > 0 &&
      (strcmp(currentUi.active().screenId, "sky-clock") == 0 ||
       strcmp(currentUi.active().screenId, "sky-voltage") == 0);
  if ((statusChanged && hasUi && currentUi.screenCount > 0 &&
       strcmp(currentUi.active().screenId, "device-control-center") == 0) ||
      liveSkyScreen) {
    screen.renderSduiScreen(currentUi);
    screen.drawIndicators(
        wifiConnected,
        cloudConnected,
        bluetoothEnabled,
        bluetoothConnected,
        batteryPercent,
        timeText);
  }
  indicatorsInitialized = true;
  lastWifiIndicator = wifiConnected;
  lastCloudIndicator = cloudConnected;
  lastBluetoothEnabledIndicator = bluetoothEnabled;
  lastBluetoothConnectedIndicator = bluetoothConnected;
  lastIndicatorAt = now;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.printf(
      "Leopold firmware %s starting\n",
      leopold::config::kFirmwareVersion);
  randomSeed(micros());

  pinMode(leopold::board::kBootButton, INPUT_PULLUP);
  if (!screen.begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(100);
    }
  }
  screen.showState(leopold::AppState::kBooting);

  touch.begin();
  telemetry.begin();
  uiStore.begin();
  hasUi = false;
  cachedRawUi = String();

  if (!cloud.begin()) {
    setState(leopold::AppState::kError, "SIN MEMORIA", "REINICIA LEOPOLD");
    fatalError = true;
    return;
  }
  wifi.begin(LEOPOLD_DEVICE_ID);
  bluetooth.begin(wifi.accessPointName(), LEOPOLD_DEVICE_ID);
  setState(leopold::AppState::kWifiConnecting);
  memset(&currentUi, 0, sizeof(currentUi));
  syncControlCenterScreen();
  hasUi = currentUi.screenCount > 0;
  if (hasUi) {
    screen.renderSduiScreen(currentUi);
  }
}

void loop() {
  const uint32_t now = millis();
  processInput(now);
  if (fatalError) {
    delay(5);
    return;
  }
  processNetwork(now);
  processWelcome(now);

  leopold::CloudResponse response;
  while (cloud.poll(response)) {
    handleCloudResponse(response, now);
  }

  processEvents(now);
  if (!welcomeActive && !assistantReady && !onboardingPending && !hasUi &&
      (appState == leopold::AppState::kOffline ||
                 appState == leopold::AppState::kOnline)) {
    screen.showLocalFace(now);
  }
  if (!welcomeActive && !assistantReady) {
    refreshIndicators(now);
  }
  screen.loop();
  delay(5);
}
