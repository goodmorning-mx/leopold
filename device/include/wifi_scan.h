#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace leopold {
// Own the driver scan until its asynchronous stop/completion has drained.
// scanDelete() only clears Arduino bookkeeping; it does not stop the radio.
class WifiScan {
 public:
  bool searching() const { return running_ || requested_; }
  bool busy() const { return running_ || draining_ || requested_; }
  void begin() {
    if (busy()) return;
    requested_ = true;
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
      draining_ = true;
      esp_wifi_scan_stop();
    }
  }
  void cancel() {
    requested_ = false;
    if (running_) {
      running_ = false;
      draining_ = true;
      esp_wifi_scan_stop();
    }
    if (!draining_) WiFi.scanDelete();
  }
  // -3 idle, -1 pending, -2 failure, >=0 completed result count.
  // Caller consumes completed records, then calls scanDelete().
  int16_t poll(void (*prepareStation)() = nullptr) {
    if (draining_) {
      if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return WIFI_SCAN_RUNNING;
      WiFi.scanDelete();
      draining_ = false;
    }
    if (requested_) {
      requested_ = false;
      // WiFiManager shutdown deletes scan bookkeeping. Drain its scan first.
      if (prepareStation) prepareStation();
      WiFi.scanDelete();
      const int16_t result = WiFi.scanNetworks(true, true);
      startedAt_ = millis();
      running_ = result == WIFI_SCAN_RUNNING;
      Serial.printf("Wi-Fi scan start result=%d\n", result);
      if (!running_) return result;
    }
    if (!running_) return -3;
    const int16_t result = WiFi.scanComplete();
    // Read time after scan start/poll, never the loop's pre-input timestamp.
    const uint32_t elapsed = millis() - startedAt_;
    if (result >= 0) {
      running_ = false;
      Serial.printf("Wi-Fi scan complete count=%d elapsed_ms=%lu\n", result, static_cast<unsigned long>(elapsed));
      return result;
    }
    if (result == WIFI_SCAN_FAILED || elapsed >= 30000) {
      Serial.printf("Wi-Fi scan failure result=%d elapsed_ms=%lu\n", result, static_cast<unsigned long>(elapsed));
      cancel();
      return WIFI_SCAN_FAILED;
    }
    return WIFI_SCAN_RUNNING;
  }
 private:
  bool running_ = false;
  bool draining_ = false;
  bool requested_ = false;
  uint32_t startedAt_ = 0;
};
} // namespace leopold
