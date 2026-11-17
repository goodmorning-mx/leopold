#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace leopold::memory_budget {

inline void log(const char* phase) {
  const uint32_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const uint32_t psramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  Serial.printf("LEOPOLD_MEMORY phase=%s internal_free=%lu internal_largest=%lu psram_free=%lu psram_largest=%lu\n",
                phase ? phase : "unknown", static_cast<unsigned long>(internalFree),
                static_cast<unsigned long>(internalLargest), static_cast<unsigned long>(psramFree),
                static_cast<unsigned long>(psramLargest));
}

}  // namespace leopold::memory_budget
