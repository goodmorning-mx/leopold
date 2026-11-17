#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <cstring>

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  String(const String&) = default;
  String& operator=(const char* value) { value_ = value ? value : ""; return *this; }
  String& operator=(const String&) = default;
  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.length(); }
  String substring(size_t start, size_t count) const { return value_.substr(start, count); }
  bool endsWith(const char* suffix) const {
    const size_t n = strlen(suffix);
    return value_.size() >= n && value_.compare(value_.size() - n, n, suffix) == 0;
  }
 private:
  std::string value_;
};

extern uint32_t fake_millis;
inline uint32_t millis() { return fake_millis; }
inline size_t strlcpy(char* dst, const char* src, size_t size) {
  if (!size) return strlen(src);
  const size_t n = strlen(src);
  const size_t copied = n < size - 1 ? n : size - 1;
  memcpy(dst, src, copied);
  dst[copied] = '\0';
  return n;
}
struct FakeSerial {
  template <typename... Args> int printf(const char* format, Args... args) {
    return std::printf(format, args...);
  }
};
extern FakeSerial Serial;
