#pragma once
#include "FreeRTOS.h"
inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char*, uint32_t, void*,
                                          uint32_t, TaskHandle_t* handle, int) {
  if (handle) *handle = reinterpret_cast<void*>(1);
  return pdPASS;
}
inline void vTaskDelay(uint32_t) {}
