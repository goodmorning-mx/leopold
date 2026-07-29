#include "sdui.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace leopold {
namespace {

constexpr char kCachePath[] = "/last_ui.json";
constexpr uint32_t kMinimumPollMs = 2000;
constexpr uint32_t kMaximumPollMs = 300000;

template <size_t Size>
bool copyBounded(char (&target)[Size], const char* source) {
  if (source == nullptr) {
    target[0] = '\0';
    return true;
  }
  const size_t length = strnlen(source, Size);
  if (length >= Size) {
    return false;
  }
  memcpy(target, source, length + 1);
  return true;
}

bool readInt16(JsonVariantConst value, int16_t& output) {
  if (!value.is<int>()) {
    return false;
  }
  const int candidate = value.as<int>();
  if (candidate < -1024 || candidate > 2048) {
    return false;
  }
  output = static_cast<int16_t>(candidate);
  return true;
}

bool isSafeId(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  for (size_t index = 0; value[index] != '\0'; ++index) {
    const char character = value[index];
    if (!isalnum(static_cast<unsigned char>(character)) &&
        character != '-' && character != '_') {
      return false;
    }
  }
  return true;
}

bool parseComponent(JsonObjectConst source, SduiComponent& component) {
  const char* type = source["type"] | "";
  if (strcmp(type, "text") == 0) {
    component.type = ComponentType::kText;
    if (!copyBounded(component.text, source["text"] | "")) {
      return false;
    }
  } else if (strcmp(type, "indicator") == 0) {
    component.type = ComponentType::kIndicator;
    if (!copyBounded(component.text, source["label"] | "")) {
      return false;
    }
    component.connected = strcmp(source["state"] | "", "connected") == 0;
  } else if (strcmp(type, "button") == 0) {
    component.type = ComponentType::kButton;
    if (!copyBounded(component.text, source["label"] | "") ||
        !readInt16(source["width"], component.width) ||
        !readInt16(source["height"], component.height) ||
        component.width <= 0 || component.height <= 0) {
      return false;
    }
  } else if (strcmp(type, "progress") == 0) {
    component.type = ComponentType::kProgress;
    component.value = constrain(source["value"] | 0, 0, 65535);
    component.maximum = constrain(source["max"] | 100, 1, 65535);
    component.width = constrain(source["width"] | 200, 20, 466);
    component.height = constrain(source["height"] | 18, 6, 80);
  } else {
    return false;
  }

  if (!copyBounded(component.id, source["id"] | "") ||
      !isSafeId(component.id) ||
      !readInt16(source["x"], component.x) ||
      !readInt16(source["y"], component.y)) {
    return false;
  }

  component.size = constrain(source["size"] | 1, 1, 6);
  component.color = SduiParser::rgb565(source["color"] | "#FFFFFF");
  const char* align = source["align"] | "left";
  component.align = strcmp(align, "center") == 0
                        ? TextAlign::kCenter
                        : strcmp(align, "right") == 0 ? TextAlign::kRight
                                                       : TextAlign::kLeft;
  return true;
}

}  // namespace

SduiParseResult SduiParser::parse(
    const char* json,
    size_t length,
    SduiDocument& output) {
  if (json == nullptr || length == 0 || length > kMaxSduiBytes) {
    return length > kMaxSduiBytes ? SduiParseResult::kTooLarge
                                  : SduiParseResult::kInvalid;
  }

  JsonDocument parsed;
  const DeserializationError error = deserializeJson(parsed, json, length);
  if (error) {
    return SduiParseResult::kInvalid;
  }
  if ((parsed["schema_version"] | 0) != 1) {
    return SduiParseResult::kUnsupportedSchema;
  }

  JsonObjectConst screen = parsed["screen"].as<JsonObjectConst>();
  JsonArrayConst components = screen["components"].as<JsonArrayConst>();
  if (screen.isNull() || components.isNull() ||
      components.size() > kMaxSduiComponents) {
    return SduiParseResult::kInvalid;
  }

  SduiDocument candidate;
  candidate.revision = parsed["revision"] | 0;
  candidate.pollAfterMs = constrain(
      parsed["poll_after_ms"] | 5000,
      kMinimumPollMs,
      kMaximumPollMs);
  candidate.background = rgb565(screen["background"] | "#000000", 0);
  if (!copyBounded(candidate.screenId, screen["id"] | "")) {
    return SduiParseResult::kInvalid;
  }

  for (JsonObjectConst source : components) {
    SduiComponent component;
    const char* type = source["type"] | "";
    const bool known = strcmp(type, "text") == 0 ||
                       strcmp(type, "indicator") == 0 ||
                       strcmp(type, "button") == 0 ||
                       strcmp(type, "progress") == 0;
    if (!known) {
      continue;
    }
    if (!parseComponent(source, component)) {
      return SduiParseResult::kInvalid;
    }
    candidate.components[candidate.componentCount++] = component;
  }

  output = candidate;
  return SduiParseResult::kOk;
}

uint16_t SduiParser::rgb565(const char* color, uint16_t fallback) {
  if (color == nullptr || color[0] != '#' || strlen(color) != 7) {
    return fallback;
  }
  char* end = nullptr;
  const unsigned long rgb = strtoul(color + 1, &end, 16);
  if (end == nullptr || *end != '\0') {
    return fallback;
  }
  const uint8_t red = (rgb >> 16) & 0xFF;
  const uint8_t green = (rgb >> 8) & 0xFF;
  const uint8_t blue = rgb & 0xFF;
  return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

bool SduiStore::begin() {
  return LittleFS.begin(true);
}

bool SduiStore::load(SduiDocument& output, String& rawJson) {
  File file = LittleFS.open(kCachePath, "r");
  if (!file || file.size() == 0 || file.size() > kMaxSduiBytes) {
    return false;
  }
  rawJson = file.readString();
  file.close();
  return SduiParser::parse(rawJson.c_str(), rawJson.length(), output) ==
         SduiParseResult::kOk;
}

bool SduiStore::save(const char* json, size_t length) {
  if (json == nullptr || length == 0 || length > kMaxSduiBytes) {
    return false;
  }
  File file = LittleFS.open(kCachePath, "w");
  if (!file) {
    return false;
  }
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(json),
      length);
  file.close();
  return written == length;
}

const SduiComponent* findButtonAt(
    const SduiDocument& document,
    int16_t x,
    int16_t y) {
  for (uint8_t index = 0; index < document.componentCount; ++index) {
    const SduiComponent& component = document.components[index];
    if (component.type == ComponentType::kButton &&
        x >= component.x && x < component.x + component.width &&
        y >= component.y && y < component.y + component.height) {
      return &component;
    }
  }
  return nullptr;
}

}  // namespace leopold
