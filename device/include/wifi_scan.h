#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace leopold {
// Own the driver scan until its asynchronous stop/completion has drained.
// scanDelete() only clears Arduino bookkeeping; it does not stop the radio.
class WifiScan {
 public:
  static constexpr uint32_t kScanTimeoutMs = 30000;
  static constexpr uint32_t kDrainTimeoutMs = 5000;

  bool searching() const { return running_ || requested_; }
  bool busy() const { return running_ || draining_ || requested_; }
  const char* phase() const {
    if (draining_) return "draining";
    if (running_) return "running";
    if (requested_) return "requested";
    return "idle";
  }

  bool begin() {
    if (busy()) return false;
    requested_ = true;
    drainTimedOut_ = false;
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
      draining_ = true;
      drainStartedAt_ = millis();
      const int stopResult = esp_wifi_scan_stop();
      Serial.printf(
          "LEOPOLD_WIFI_SCAN phase=draining reason=preexisting_scan stop_result=%d\n",
          stopResult);
    }
    return true;
  }
  void cancel() {
    requested_ = false;
    if (running_) {
      running_ = false;
      draining_ = true;
      drainStartedAt_ = millis();
      drainTimedOut_ = false;
      const int stopResult = esp_wifi_scan_stop();
      Serial.printf(
          "LEOPOLD_WIFI_SCAN phase=draining reason=cancel stop_result=%d\n",
          stopResult);
    }
    if (!draining_) WiFi.scanDelete();
  }
  // -3 idle, -1 pending, -2 failure, >=0 completed result count.
  // Caller consumes completed records, then calls scanDelete().
  int16_t poll(void (*prepareStation)() = nullptr) {
    if (draining_) {
      const int16_t drainResult = WiFi.scanComplete();
      if (drainResult == WIFI_SCAN_RUNNING) {
        const uint32_t drainElapsed = millis() - drainStartedAt_;
        if (drainElapsed >= kDrainTimeoutMs && !drainTimedOut_) {
          drainTimedOut_ = true;
          const bool hadPendingRequest = requested_;
          requested_ = false;
          Serial.printf(
              "LEOPOLD_WIFI_SCAN phase=draining result=timeout elapsed_ms=%lu pending_request=%u\n",
              static_cast<unsigned long>(drainElapsed),
              hadPendingRequest ? 1u : 0u);
          if (hadPendingRequest) return WIFI_SCAN_FAILED;
        }
        return WIFI_SCAN_RUNNING;
      }
      Serial.printf(
          "LEOPOLD_WIFI_SCAN phase=drain_complete result=%d elapsed_ms=%lu\n",
          drainResult,
          static_cast<unsigned long>(millis() - drainStartedAt_));
      WiFi.scanDelete();
      draining_ = false;
      drainTimedOut_ = false;
    }
    if (requested_) {
      requested_ = false;
      // WiFiManager shutdown deletes scan bookkeeping. Drain its scan first.
      if (prepareStation) prepareStation();
      WiFi.scanDelete();
      const int16_t result = WiFi.scanNetworks(true, true);
      startedAt_ = millis();
      running_ = result == WIFI_SCAN_RUNNING;
      Serial.printf("LEOPOLD_WIFI_SCAN phase=start result=%d\n", result);
      if (!running_) return result;
    }
    if (!running_) return -3;
    const int16_t result = WiFi.scanComplete();
    // Read time after scan start/poll, never the loop's pre-input timestamp.
    const uint32_t elapsed = millis() - startedAt_;
    if (result >= 0) {
      running_ = false;
      Serial.printf(
          "LEOPOLD_WIFI_SCAN phase=complete count=%d elapsed_ms=%lu\n",
          result, static_cast<unsigned long>(elapsed));
      return result;
    }
    if (result == WIFI_SCAN_FAILED || elapsed >= kScanTimeoutMs) {
      const bool timedOut = elapsed >= kScanTimeoutMs;
      Serial.printf(
          "LEOPOLD_WIFI_SCAN phase=active result=%d reason=%s elapsed_ms=%lu\n",
          result, timedOut ? "timeout" : "driver_failure",
          static_cast<unsigned long>(elapsed));
      cancel();
      return WIFI_SCAN_FAILED;
    }
    return WIFI_SCAN_RUNNING;
  }
 private:
  bool running_ = false;
  bool draining_ = false;
  bool requested_ = false;
  bool drainTimedOut_ = false;
  uint32_t startedAt_ = 0;
  uint32_t drainStartedAt_ = 0;
};
} // namespace leopold
