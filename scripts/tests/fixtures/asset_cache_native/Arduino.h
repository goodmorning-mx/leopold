#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
using std::min;
struct FixtureSerial { template<class... T> int printf(const char*, T...) { return 0; } };
inline FixtureSerial Serial;
inline uint32_t millis() { return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
inline size_t strlcpy(char* dst, const char* src, size_t cap) {
  const size_t n = src ? std::strlen(src) : 0;
  if (cap) { const size_t copied = min(n, cap - 1); if (copied) std::memcpy(dst, src, copied); dst[copied] = '\0'; }
  return n;
}
