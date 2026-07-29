#pragma once

#include <Arduino.h>

namespace leopold {

constexpr size_t kMaxSduiBytes = 12288;
constexpr size_t kMaxSduiComponents = 12;
constexpr size_t kMaxComponentText = 96;
constexpr size_t kMaxComponentId = 32;

enum class ComponentType : uint8_t {
  kText,
  kIndicator,
  kButton,
  kProgress,
};

enum class TextAlign : uint8_t {
  kLeft,
  kCenter,
  kRight,
};

struct SduiComponent {
  ComponentType type = ComponentType::kText;
  char id[kMaxComponentId + 1] = {};
  char text[kMaxComponentText + 1] = {};
  int16_t x = 0;
  int16_t y = 0;
  int16_t width = 0;
  int16_t height = 0;
  uint16_t color = 0xFFFF;
  uint8_t size = 1;
  TextAlign align = TextAlign::kLeft;
  bool connected = false;
  uint16_t value = 0;
  uint16_t maximum = 100;
};

struct SduiDocument {
  uint32_t revision = 0;
  uint32_t pollAfterMs = 5000;
  uint16_t background = 0;
  char screenId[kMaxComponentId + 1] = {};
  SduiComponent components[kMaxSduiComponents] = {};
  uint8_t componentCount = 0;
};

enum class SduiParseResult : uint8_t {
  kOk,
  kTooLarge,
  kInvalid,
  kUnsupportedSchema,
};

class SduiParser {
 public:
  static SduiParseResult parse(
      const char* json,
      size_t length,
      SduiDocument& output);
  static uint16_t rgb565(const char* color, uint16_t fallback = 0xFFFF);
};

class SduiStore {
 public:
  bool begin();
  bool load(SduiDocument& output, String& rawJson);
  bool save(const char* json, size_t length);
};

const SduiComponent* findButtonAt(
    const SduiDocument& document,
    int16_t x,
    int16_t y);

}  // namespace leopold
