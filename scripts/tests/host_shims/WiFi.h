#pragma once
#include "IPAddress.h"
#include "NetworkEvents.h"

constexpr int WL_CONNECTED = 3;
class FakeWiFi {
 public:
  int status() const { return status_; }
  IPAddress localIP() const { return local_; }
  void onEvent(NetworkEventFuncCb cb) { callback_ = cb; }
  void emit(arduino_event_id_t event) { if (callback_) callback_(event, {}); }
  int status_ = WL_CONNECTED;
  IPAddress local_{0x0a000002};
 private:
  NetworkEventFuncCb callback_;
};
extern FakeWiFi WiFi;
