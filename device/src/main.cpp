#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include "app_state.h"
#include "board_config.h"
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

struct PendingEvent {
  char json[512] = {};
  uint8_t attempts = 0;
  uint32_t retryAt = 0;
  bool occupied = false;
};

DisplayManager screen;
WifiPortal wifi;
CloudClient cloud;
TouchInput touch;
Telemetry telemetry;
SduiStore uiStore;
SduiDocument currentUi;
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
bool fatalError = false;
bool onboardingPending = false;
bool welcomeActive = false;
bool welcomeShownThisBoot = false;
bool assistantReady = false;
uint8_t welcomeStep = 0;
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

void startWelcome(const char* ownerName, uint32_t now) {
  if (welcomeShownThisBoot || ownerName == nullptr || ownerName[0] == '\0') {
    return;
  }
  strlcpy(ownerDisplayName, ownerName, sizeof(ownerDisplayName));
  welcomeShownThisBoot = true;
  welcomeActive = true;
  assistantReady = true;
  welcomeStep = 0;
  screen.showWelcome(ownerDisplayName, welcomeStep);
  nextWelcomeAt = now + kWelcomeScreenMs;
  Serial.printf("Starting welcome for %s\n", ownerDisplayName);
}

void processWelcome(uint32_t now) {
  if (!welcomeActive) {
    return;
  }
  if (now >= nextWelcomeAt) {
    welcomeStep = (welcomeStep + 1) % 2;
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

void acceptUi(const CloudResponse& response, uint32_t now) {
  SduiDocument candidate;
  const SduiParseResult parsed =
      SduiParser::parse(response.body, response.bodyLength, candidate);
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

  currentUi = candidate;
  hasUi = true;
  if (response.etag[0] != '\0') {
    strlcpy(uiEtag, response.etag, sizeof(uiEtag));
  } else {
    snprintf(
        uiEtag,
        sizeof(uiEtag),
        "\"rev-%lu\"",
        static_cast<unsigned long>(candidate.revision));
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
  for (uint8_t index = 0; index < currentUi.componentCount; ++index) {
    if (currentUi.components[index].type == ComponentType::kButton) {
      return &currentUi.components[index];
    }
  }
  return nullptr;
}

const SduiComponent* onlyButton() {
  const SduiComponent* match = nullptr;
  for (uint8_t index = 0; index < currentUi.componentCount; ++index) {
    if (currentUi.components[index].type != ComponentType::kButton) {
      continue;
    }
    if (match != nullptr) {
      return nullptr;
    }
    match = &currentUi.components[index];
  }
  return match;
}

void processInput(uint32_t now) {
  TouchPoint point;
  TouchGesture gesture = TouchGesture::kNone;
  if (touch.pollGesture(gesture, point, now) && welcomeActive &&
      (gesture == TouchGesture::kSwipeLeft ||
       gesture == TouchGesture::kSwipeRight)) {
    moveWelcome(gesture == TouchGesture::kSwipeLeft ? 1 : -1, now);
    Serial.println(
        gesture == TouchGesture::kSwipeLeft
            ? "Welcome swiped left"
            : "Welcome swiped right");
  } else if (gesture == TouchGesture::kTap && hasUi) {
    Serial.printf("Touch tap: x=%u y=%u\n", point.x, point.y);
    const SduiComponent* button = findButtonAt(currentUi, point.x, point.y);
    if (button == nullptr) {
      button = onlyButton();
    }
    if (button != nullptr) {
      Serial.println("Touch matched SDUI button");
      queueTap(button->id, currentUi.revision, now);
    } else {
      Serial.println("Touch did not match an SDUI button");
    }
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
  wifi.loop(now);
  if (wifi.portalActive()) {
    if (appState != AppState::kWifiPortal) {
      appState = AppState::kWifiPortal;
      screen.showQr(
          "AGREGA LEOPOLD",
          wifi.setupUrl(),
          "ESCANEA DESDE TU PERFIL");
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
  if (cloudConnected && !onboardingPending && !welcomeActive &&
      !assistantReady &&
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
  screen.drawIndicators(
      wifi.connected(),
      cloudConnected,
      telemetry.batteryPercent(),
      timeText);
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
  configTzTime(
      "CST6",
      "pool.ntp.org",
      "time.google.com",
      "time.cloudflare.com");
  setState(leopold::AppState::kWifiConnecting);
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
  delay(5);
}
