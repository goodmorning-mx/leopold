#pragma once

#include <Arduino.h>

namespace leopold {

enum class CloudOperation : uint8_t {
  kConnect,
  kOnboardingStatus,
  kFetchUi,
  kHeartbeat,
  kEvent,
};

struct CloudResponse {
  CloudOperation operation = CloudOperation::kConnect;
  int httpCode = 0;
  char* body = nullptr;
  size_t bodyLength = 0;
  char etag[64] = {};
  char serverDate[40] = {};

  void release();
};

class CloudClient {
 public:
  bool begin();
  bool requestConnect(const char* setupToken);
  bool requestOnboardingStatus();
  bool requestUi(const char* etag);
  bool requestHeartbeat(
      int32_t wifiRssi,
      uint8_t batteryPercent,
      uint32_t uptimeSeconds);
  bool sendEvent(const char* eventJson);
  bool poll(CloudResponse& response);

 private:
  struct Request {
    CloudOperation operation = CloudOperation::kConnect;
    char payload[640] = {};
    char etag[64] = {};
  };

  static void taskEntry(void* context);
  void taskLoop();
  bool enqueue(const Request& request);
  CloudResponse perform(const Request& request);

  QueueHandle_t requestQueue_ = nullptr;
  QueueHandle_t responseQueue_ = nullptr;
  TaskHandle_t task_ = nullptr;
};

}  // namespace leopold
