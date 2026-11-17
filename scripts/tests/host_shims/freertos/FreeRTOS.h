#pragma once
#include <stdint.h>
using BaseType_t = int;
using TaskHandle_t = void*;
using TaskFunction_t = void (*)(void*);
constexpr BaseType_t pdPASS = 1;
inline uint32_t pdMS_TO_TICKS(uint32_t milliseconds) { return milliseconds; }
struct portMUX_TYPE { int unused; };
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
