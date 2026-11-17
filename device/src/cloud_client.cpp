#include "cloud_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <time.h>

#include "config_guard.h"
#include "ota_test_connect_payload.h"
#include "sdui.h"
#include "tls_bundle.h"
#include "device_identity.h"
#include "firmware_ota.h"
#include "persistent_asset_cache.h"
#include "generated_selected_asset_specs.h"
#include "memory_budget.h"
#include "local_endpoint_transport.h"

namespace leopold {
namespace {

#ifdef LEOPOLD_OTA_TEST_MODE
bool buildOtaTestConnectPayload(char* payload, size_t payloadSize,
                               const char* setupToken) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running == nullptr) return false;
  return ota_test::buildConnectPayload(
      payload, payloadSize, config::kFirmwareVersion, setupToken, "C",
      config::kSourceCommit, config::kBuildId, running->address, running->label);
}
#endif

#ifndef LEOPOLD_ASSET_ORIGIN
#define LEOPOLD_ASSET_ORIGIN LEOPOLD_API_BASE_URL
#endif
#ifndef LEOPOLD_ASSET_TLS_ROOT_CA
#define LEOPOLD_ASSET_TLS_ROOT_CA LEOPOLD_TLS_ROOT_CA
#endif

constexpr uint32_t kHttpTimeoutMs = 5000;
constexpr int kResponseTooLarge = -1001;
constexpr uint32_t kMaxAssetTransferMs = 15000;

const char* pathFor(CloudOperation operation) {
  switch (operation) {
    case CloudOperation::kConnect:
      return "/v1/device/connect";
    case CloudOperation::kOnboardingStatus:
      return "/v1/device/onboarding/status";
    case CloudOperation::kFetchUi:
      return "/v1/device/ui";
    case CloudOperation::kFetchAsset:
      return "/v1/device/assets/";
    case CloudOperation::kHeartbeat:
      return "/v1/device/heartbeat";
    case CloudOperation::kEvent:
      return "/v1/device/events";
    case CloudOperation::kFirmwareUpdate:
      return "/v1/device/firmware/assignment";
  }
  return "/";
}

struct AssetStream {
  NetworkClient* stream;
  uint32_t started;
  size_t received;
  uint32_t internalMin;
  uint32_t psramMin;
  uint32_t psramLargestMin;
};
bool readAssetStream(void* opaque, uint8_t* target, size_t requested, size_t* received) {
  auto* context = static_cast<AssetStream*>(opaque);
  *received = 0;
  while (*received == 0) {
    if (millis() - context->started >= kMaxAssetTransferMs) return false;
    const int available = context->stream->available();
    if (available > 0) {
      *received = context->stream->readBytes(target, min(static_cast<size_t>(available), requested));
      context->received += *received;
      context->internalMin = min(context->internalMin, static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
      context->psramMin = min(context->psramMin, static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
      context->psramLargestMin = min(context->psramLargestMin, static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
      return *received > 0;
    }
    if (!context->stream->connected() && available == 0) return false;
    delay(1);
  }
  return true;
}

}  // namespace

void CloudResponse::release() {
  if (body != nullptr) heap_caps_free(body);
  body = nullptr;
  bodyLength = 0;
}

bool CloudClient::begin() {
  // The application task owns initialization and admission. The worker may
  // start immediately, so both queues must be assigned before task creation.
  if (task_ != nullptr) return true;
  requestQueue_ = xQueueCreate(6, sizeof(Request));
  if (requestQueue_ == nullptr) return false;

  responseQueue_ = xQueueCreate(4, sizeof(CloudResponse));
  if (responseQueue_ != nullptr) {
    if (xTaskCreatePinnedToCore(taskEntry, "leopold-cloud", 6144, this, 1, &task_, 0) == pdPASS) {
      return true;
    }
    vQueueDelete(responseQueue_);
    responseQueue_ = nullptr;
  }
  vQueueDelete(requestQueue_);
  requestQueue_ = nullptr;
  task_ = nullptr;
  return false;
}

bool CloudClient::requestConnect(const char* setupToken) {
  Request request;
  request.operation = CloudOperation::kConnect;
#ifdef LEOPOLD_OTA_TEST_MODE
  if (setupToken != nullptr && strlen(setupToken) >= 32) {
    if (!buildOtaTestConnectPayload(request.payload, sizeof(request.payload), setupToken))
      return false;
  } else {
    if (!buildOtaTestConnectPayload(request.payload, sizeof(request.payload), nullptr))
      return false;
  }
#else
  if (setupToken != nullptr && strlen(setupToken) >= 32) {
    snprintf(request.payload, sizeof(request.payload),
        "{\"firmware_version\":\"%s\",\"setup_token\":\"%s\",\"capabilities\":{\"display\":\"466x466\",\"touch\":true,\"audio\":false,\"battery\":true}}",
        config::kFirmwareVersion, setupToken);
  } else {
    snprintf(request.payload, sizeof(request.payload),
        "{\"firmware_version\":\"%s\",\"capabilities\":{\"display\":\"466x466\",\"touch\":true,\"audio\":false,\"battery\":true}}",
        config::kFirmwareVersion);
  }
#endif
  return enqueue(request);
}

bool CloudClient::requestOnboardingStatus() {
  Request request;
  request.operation = CloudOperation::kOnboardingStatus;
  strlcpy(request.payload, "{}", sizeof(request.payload));
  return enqueue(request);
}

bool CloudClient::requestUi(const char* etag) {
  Request request;
  request.operation = CloudOperation::kFetchUi;
  if (etag != nullptr) {
    strlcpy(request.etag, etag, sizeof(request.etag));
  }
  return enqueue(request);
}

bool CloudClient::requestAsset(const char* file) {
  if (file == nullptr || strlen(file) >= sizeof(Request::assetFile)) return false;
  Request request;
  request.operation = CloudOperation::kFetchAsset;
  strlcpy(request.assetFile, file, sizeof(request.assetFile));
  return enqueue(request);
}

bool CloudClient::requestHeartbeat(
    int32_t wifiRssi,
    uint8_t batteryPercent,
    uint32_t uptimeSeconds) {
  Request request;
  request.operation = CloudOperation::kHeartbeat;
  snprintf(
      request.payload,
      sizeof(request.payload),
      "{\"firmware_version\":\"%s\",\"wifi_rssi\":%ld,"
      "\"battery_percent\":%u,\"uptime_seconds\":%lu}",
      config::kFirmwareVersion,
      static_cast<long>(wifiRssi),
      batteryPercent,
      static_cast<unsigned long>(uptimeSeconds));
  return enqueue(request);
}

bool CloudClient::sendEvent(const char* eventJson) {
  if (eventJson == nullptr || strlen(eventJson) >= sizeof(Request::payload)) {
    return false;
  }
  Request request;
  request.operation = CloudOperation::kEvent;
  strlcpy(request.payload, eventJson, sizeof(request.payload));
  return enqueue(request);
}

bool CloudClient::requestFirmwareUpdate() {
  Request request;
  request.operation = CloudOperation::kFirmwareUpdate;
  return enqueue(request);
}

bool CloudClient::poll(CloudResponse& response) {
  return task_ != nullptr &&
         xQueueReceive(responseQueue_, &response, 0) == pdTRUE;
}

bool CloudClient::enqueue(const Request& request) {
  return task_ != nullptr &&
         xQueueSend(requestQueue_, &request, 0) == pdTRUE;
}

void CloudClient::taskEntry(void* context) {
  static_cast<CloudClient*>(context)->taskLoop();
}

void CloudClient::taskLoop() {
  Request request;
  bool stackReported = false;
  while (true) {
    if (xQueueReceive(requestQueue_, &request, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (!stackReported) {
      stackReported = true;
      Serial.printf("LEOPOLD_CLOUD_WORKER configured_stack=6144 free_stack=%lu\n",
                    static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    }
    firmware_ota::reportBootOutcomeWhenOnline();
    if (request.operation == CloudOperation::kFirmwareUpdate) {
      CloudResponse response;
      response.operation = request.operation;
      Serial.printf("LEOPOLD_CLOUD_WORKER ota_start free_stack=%lu\n",
                    static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
      response.httpCode = firmware_ota::checkAndInstall();
      Serial.printf("LEOPOLD_CLOUD_WORKER ota_end free_stack=%lu http_code=%d\n",
                    static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)),
                    response.httpCode);
      if (xQueueSend(responseQueue_, &response, 0) != pdTRUE) response.release();
      continue;
    }
    CloudResponse response = perform(request);
    if (xQueueSend(responseQueue_, &response, 0) != pdTRUE) {
      response.release();
    }
  }
}

CloudResponse CloudClient::perform(const Request& request) {
  CloudResponse response;
  response.operation = request.operation;
  strlcpy(response.assetFile, request.assetFile, sizeof(response.assetFile));
  if (WiFi.status() != WL_CONNECTED) {
    response.httpCode = HTTPC_ERROR_CONNECTION_REFUSED;
    return response;
  }

  const bool assetRequest = request.operation == CloudOperation::kFetchAsset;
  // The device credential is scoped to the API origin. Until the API issues
  // a separately scoped catalog capability, never forward it to a distinct
  // configured asset origin.
  if (assetRequest && strcmp(LEOPOLD_ASSET_ORIGIN, LEOPOLD_API_BASE_URL) != 0) {
    response.httpCode = HTTPC_ERROR_CONNECTION_REFUSED;
    return response;
  }
  local_endpoint::SecureClient secure;
  Serial.printf(
      "Cloud request %u: epoch=%lu heap=%u largest=%u\n",
      static_cast<unsigned>(request.operation),
      static_cast<unsigned long>(time(nullptr)),
      static_cast<unsigned>(ESP.getFreeHeap()),
      static_cast<unsigned>(heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* trustedOrigin = assetRequest ? LEOPOLD_ASSET_ORIGIN : LEOPOLD_API_BASE_URL;
  String url = String(trustedOrigin) + pathFor(request.operation);
  if (assetRequest) url += request.assetFile;
  const auto beginStatus = local_endpoint::beginRequest(
      http, secure, trustedOrigin, url,
      assetRequest ? LEOPOLD_ASSET_TLS_ROOT_CA : LEOPOLD_TLS_ROOT_CA, kHttpTimeoutMs);
  if (beginStatus != local_endpoint::BeginStatus::kReady) {
    response.httpCode = beginStatus == local_endpoint::BeginStatus::kTlsFailure
        ? HTTPC_ERROR_NOT_CONNECTED : HTTPC_ERROR_CONNECTION_REFUSED;
    return response;
  }

  const char* headers[] = {"ETag", "Date", "X-Leopold-Asset-SHA256", "X-Leopold-Catalog-Version", "X-Leopold-Renderer-Compatibility"};
  http.collectHeaders(headers, 5);
  http.addHeader("X-Leopold-Device-ID", identity::deviceId());
  http.addHeader("Authorization", String("Bearer ") + identity::deviceSecret());
  http.addHeader("Content-Type", "application/json");
  if (request.etag[0] != '\0') {
    http.addHeader("If-None-Match", request.etag);
  }

  response.httpCode = request.operation == CloudOperation::kFetchUi ||
                             request.operation == CloudOperation::kFetchAsset
                          ? http.GET()
                          : http.POST(String(request.payload));
  if (response.httpCode < 0) {
    char tlsError[128] = {};
    const int tlsCode = secure.lastError(tlsError, sizeof(tlsError));
    Serial.printf(
        "Cloud request %u failed: http=%d tls=%d %s\n",
        static_cast<unsigned>(request.operation),
        response.httpCode,
        tlsCode,
        tlsError);
  } else {
    Serial.printf(
        "Cloud request %u response: http=%d\n",
        static_cast<unsigned>(request.operation),
        response.httpCode);
  }
  if (assetRequest && response.httpCode == HTTP_CODE_OK) {
    memory_budget::log("asset-http-after-response");
    const asset_cache::AssetSpec* spec = asset_cache::findSpec(request.assetFile);
    const int contentLength = http.getSize();
    const String digest = http.header("X-Leopold-Asset-SHA256");
    const String catalog = http.header("X-Leopold-Catalog-Version");
    const String renderer = http.header("X-Leopold-Renderer-Compatibility");
    if (spec == nullptr || contentLength != static_cast<int>(spec->bytes) ||
        !digest.equalsIgnoreCase(spec->sha256) || catalog.toInt() != asset_cache::kCatalogVersion ||
        renderer.toInt() != asset_cache::kRendererCompatibility ||
        !asset_cache::mounted()) {
      response.httpCode = kResponseTooLarge;
    } else {
      AssetStream context{http.getStreamPtr(), millis(), 0,
          static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
          static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
          static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))};
      response.assetActivated = asset_cache::installStream(*spec, readAssetStream, &context,
                                                            asset_cache::Pin::kNone);
      Serial.printf("LEOPOLD_MEMORY phase=asset-http-min internal_free=%lu psram_free=%lu psram_largest=%lu received=%lu\n",
          static_cast<unsigned long>(context.internalMin), static_cast<unsigned long>(context.psramMin),
          static_cast<unsigned long>(context.psramLargestMin), static_cast<unsigned long>(context.received));
      if (!response.assetActivated || context.received != spec->bytes)
        response.httpCode = kResponseTooLarge;
    }
    strlcpy(response.etag, http.header("ETag").c_str(), sizeof(response.etag));
    strlcpy(response.serverDate, http.header("Date").c_str(), sizeof(response.serverDate));
    http.end();
    memory_budget::log("asset-http-closed");
    return response;
  }
  const bool expectsBody = request.operation == CloudOperation::kConnect ||
                           request.operation == CloudOperation::kOnboardingStatus ||
                           request.operation == CloudOperation::kFetchUi;
  if (expectsBody && response.httpCode > 0 &&
      response.httpCode != HTTP_CODE_NOT_MODIFIED) {
    const int contentLength = http.getSize();
    const int maxBody = static_cast<int>(kMaxSduiBytes);
    if (contentLength < 0 || contentLength > maxBody) {
      response.httpCode = kResponseTooLarge;
    } else {
      {
        String body = http.getString();
        if (body.length() <= kMaxSduiBytes) {
          response.body = static_cast<char*>(malloc(body.length() + 1));
          if (response.body != nullptr) {
            memcpy(response.body, body.c_str(), body.length() + 1);
            response.bodyLength = body.length();
          }
        } else {
          response.httpCode = kResponseTooLarge;
        }
      }
    }
  }
  strlcpy(response.etag, http.header("ETag").c_str(), sizeof(response.etag));
  strlcpy(response.serverDate, http.header("Date").c_str(), sizeof(response.serverDate));
  http.end();
  return response;
}

}  // namespace leopold
