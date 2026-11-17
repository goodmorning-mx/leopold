#pragma once
#include <stdint.h>
class IPAddress {
 public:
  IPAddress() = default;
  explicit IPAddress(uint32_t value) : value_(value) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : value_((uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | d) {}
  operator uint32_t() const { return value_; }
  bool operator==(const IPAddress& other) const { return value_ == other.value_; }
  bool operator!=(const IPAddress& other) const { return !(*this == other); }
 private:
  uint32_t value_ = 0;
};
