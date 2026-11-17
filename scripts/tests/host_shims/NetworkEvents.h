#pragma once
#include <functional>
enum arduino_event_id_t {
  ARDUINO_EVENT_WIFI_STA_START,
  ARDUINO_EVENT_WIFI_STA_STOP,
  ARDUINO_EVENT_WIFI_STA_CONNECTED,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
  ARDUINO_EVENT_WIFI_STA_GOT_IP,
  ARDUINO_EVENT_WIFI_STA_LOST_IP
};
struct arduino_event_info_t {};
using NetworkEventFuncCb = std::function<void(arduino_event_id_t, arduino_event_info_t)>;
