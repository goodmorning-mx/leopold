#include "cloud_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config_guard.h"
#include "sdui.h"

namespace leopold {
namespace {

constexpr uint32_t kHttpTimeoutMs = 5000;
constexpr int kResponseTooLarge = -1001;

const char* pathFor(CloudOperation operation) {
  switch (operation) {
    case CloudOperation::kConnect:
      return "/v1/device/connect";
    case CloudOperation::kFetchUi:
      return "/v1/device/ui";
    case CloudOperation::kHeartbeat:
      return "/v1/device/heartbeat";
    case CloudOperation::kEvent:
      return "/v1/device/events";
  }
  return "/";
}

}  // namespace

void CloudResponse::release() {
  free(body);
  body = nullptr;
  bodyLength = 0;
}

bool CloudClient::begin() {
  if (requestQueue_ != nullptr) {
    return true;
  }
  requestQueue_ = xQueueCreate(6, sizeof(Request));
  responseQueue_ = xQueueCreate(4, sizeof(CloudResponse));
  if (requestQueue_ == nullptr || responseQueue_ == nullptr) {
    return false;
  }
  return xTaskCreatePinnedToCore(
             taskEntry,
             "leopold-cloud",
             8192,
             this,
             1,
             &task_,
             0) == pdPASS;
}

bool CloudClient::requestConnect() {
  Request request;
  request.operation = CloudOperation::kConnect;
  snprintf(
      request.payload,
      sizeof(request.payload),
      "{\"firmware_version\":\"%s\",\"capabilities\":{\"display\":"
      "\"466x466\",\"touch\":true,\"audio\":false,\"battery\":true}}",
      config::kFirmwareVersion);
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

bool CloudClient::poll(CloudResponse& response) {
  return responseQueue_ != nullptr &&
         xQueueReceive(responseQueue_, &response, 0) == pdTRUE;
}

bool CloudClient::enqueue(const Request& request) {
  return requestQueue_ != nullptr &&
         xQueueSend(requestQueue_, &request, 0) == pdTRUE;
}

void CloudClient::taskEntry(void* context) {
  static_cast<CloudClient*>(context)->taskLoop();
}

void CloudClient::taskLoop() {
  Request request;
  while (true) {
    if (xQueueReceive(requestQueue_, &request, portMAX_DELAY) != pdTRUE) {
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
  if (WiFi.status() != WL_CONNECTED) {
    response.httpCode = HTTPC_ERROR_CONNECTION_REFUSED;
    return response;
  }

  WiFiClientSecure secure;
  secure.setCACert(LEOPOLD_TLS_ROOT_CA);
  secure.setHandshakeTimeout(5);

  HTTPClient http;
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setTimeout(kHttpTimeoutMs);
  const String url = String(LEOPOLD_API_BASE_URL) + pathFor(request.operation);
  if (!http.begin(secure, url)) {
    response.httpCode = HTTPC_ERROR_CONNECTION_REFUSED;
    return response;
  }

  const char* responseHeaders[] = {"ETag"};
  http.collectHeaders(responseHeaders, 1);
  http.addHeader("X-Leopold-Device-ID", LEOPOLD_DEVICE_ID);
  http.addHeader("Authorization", String("Bearer ") + LEOPOLD_DEVICE_SECRET);
  http.addHeader("Content-Type", "application/json");
  if (request.etag[0] != '\0') {
    http.addHeader("If-None-Match", request.etag);
  }

  response.httpCode = request.operation == CloudOperation::kFetchUi
                          ? http.GET()
                          : http.POST(String(request.payload));
  const bool expectsBody = request.operation == CloudOperation::kConnect ||
                           request.operation == CloudOperation::kFetchUi;
  if (expectsBody && response.httpCode > 0 &&
      response.httpCode != HTTP_CODE_NOT_MODIFIED) {
    const int contentLength = http.getSize();
    if (contentLength < 0 ||
        contentLength > static_cast<int>(kMaxSduiBytes)) {
      response.httpCode = kResponseTooLarge;
    } else {
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
  strlcpy(response.etag, http.header("ETag").c_str(), sizeof(response.etag));
  http.end();
  return response;
}

}  // namespace leopold
