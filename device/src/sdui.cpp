#include "sdui.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdspi_host.h>
#include <driver/spi_master.h>

#include "board_config.h"

namespace leopold {
namespace {

constexpr char kCachePath[] = "/last_ui.json";
constexpr char kSdCachePath[] = "/sd/last_ui.json";
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

sdmmc_card_t* sdCard = nullptr;
bool sdMounted = false;

void mountSdCard() {
  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = board::kSdMosi;
  busConfig.miso_io_num = board::kSdMiso;
  busConfig.sclk_io_num = board::kSdSclk;
  busConfig.quadwp_io_num = -1;
  busConfig.quadhd_io_num = -1;
  busConfig.max_transfer_sz = 4000;
  const esp_err_t busResult =
      spi_bus_initialize(SPI3_HOST, &busConfig, SDSPI_DEFAULT_DMA);
  if (busResult != ESP_OK && busResult != ESP_ERR_INVALID_STATE) {
    Serial.printf("SD SPI initialization failed: %s\n", esp_err_to_name(busResult));
    return;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;
  sdspi_device_config_t slotConfig = SDSPI_DEVICE_CONFIG_DEFAULT();
  slotConfig.gpio_cs = static_cast<gpio_num_t>(board::kSdCs);
  slotConfig.host_id = SPI3_HOST;
  esp_vfs_fat_mount_config_t mountConfig = {};
  mountConfig.format_if_mount_failed = true;
  mountConfig.max_files = 4;
  mountConfig.allocation_unit_size = 32 * 1024;
  const esp_err_t result = esp_vfs_fat_sdspi_mount(
      "/sd", &host, &slotConfig, &mountConfig, &sdCard);
  if (result == ESP_OK) {
    sdMounted = true;
    Serial.printf("SD card mounted: %.2f GB\n",
                  static_cast<double>(sdCard->csd.capacity) / 2048.0 / 1024.0);
  } else {
    Serial.printf("SD card unavailable: %s\n", esp_err_to_name(result));
  }
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
        !copyBounded(component.icon, source["icon"] | "") ||
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
  } else if (strcmp(type, "slider") == 0) {
    component.type = ComponentType::kSlider;
    component.value = constrain(source["value"] | 70, 0, 100);
    component.maximum = 100;
    component.width = constrain(source["width"] | 300, 20, 420);
    component.height = constrain(source["height"] | 32, 20, 420);
  } else if (strcmp(type, "icon") == 0) {
    component.type = ComponentType::kIcon;
    if (!copyBounded(component.icon, source["name"] | "")) {
      return false;
    }
    component.width = constrain(source["size"] | 64, 24, 160);
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

  JsonArrayConst screens = parsed["screens"].as<JsonArrayConst>();
  if (screens.isNull() || screens.size() == 0 ||
      screens.size() > kMaxSduiScreens) {
    return SduiParseResult::kInvalid;
  }

  memset(&output, 0, sizeof(output));
  output.revision = parsed["revision"] | 0;
  output.pollAfterMs = constrain(
      parsed["poll_after_ms"] | 5000,
      kMinimumPollMs,
      kMaximumPollMs);
  for (JsonObjectConst sourceScreen : screens) {
    JsonArrayConst components =
        sourceScreen["components"].as<JsonArrayConst>();
    if (components.isNull() || components.size() > kMaxSduiComponents) {
      return SduiParseResult::kInvalid;
    }
    SduiScreen& target = output.screens[output.screenCount];
    target.background =
        rgb565(sourceScreen["background"] | "#000000", 0);
    if (!copyBounded(target.screenId, sourceScreen["id"] | "") ||
        !isSafeId(target.screenId)) {
      return SduiParseResult::kInvalid;
    }
    for (JsonObjectConst source : components) {
      SduiComponent component;
      const char* type = source["type"] | "";
      const bool known = strcmp(type, "text") == 0 ||
                         strcmp(type, "indicator") == 0 ||
                         strcmp(type, "button") == 0 ||
                         strcmp(type, "progress") == 0 ||
                         strcmp(type, "slider") == 0 ||
                         strcmp(type, "icon") == 0;
      if (!known) {
        continue;
      }
      if (!parseComponent(source, component)) {
        return SduiParseResult::kInvalid;
      }
      target.components[target.componentCount++] = component;
    }
    ++output.screenCount;
  }
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
  const bool littleFsReady = LittleFS.begin(true);
  mountSdCard();
  return littleFsReady;
}

bool SduiStore::load(SduiDocument& output, String& rawJson) {
  if (sdMounted) {
    FILE* file = fopen(kSdCachePath, "rb");
    if (file != nullptr) {
      fseek(file, 0, SEEK_END);
      const long size = ftell(file);
      rewind(file);
      if (size > 0 && size <= static_cast<long>(kMaxSduiBytes)) {
        rawJson.reserve(static_cast<unsigned int>(size));
        while (rawJson.length() < static_cast<unsigned int>(size)) {
          char buffer[256];
          const size_t read = fread(buffer, 1, sizeof(buffer), file);
          if (read == 0) break;
          rawJson.concat(buffer, read);
        }
      }
      fclose(file);
      if (SduiParser::parse(rawJson.c_str(), rawJson.length(), output) ==
          SduiParseResult::kOk) {
        return true;
      }
      rawJson = String();
    }
  }
  File file = LittleFS.open(kCachePath, "r");
  if (!file || file.size() == 0 || file.size() > kMaxSduiBytes) return false;
  rawJson = file.readString();
  file.close();
  return SduiParser::parse(rawJson.c_str(), rawJson.length(), output) ==
         SduiParseResult::kOk;
}

bool SduiStore::save(const char* json, size_t length) {
  if (json == nullptr || length == 0 || length > kMaxSduiBytes) {
    return false;
  }
  if (sdMounted) {
    FILE* file = fopen(kSdCachePath, "wb");
    if (file != nullptr) {
      const size_t written = fwrite(json, 1, length, file);
      fclose(file);
      if (written == length) return true;
    }
  }
  File file = LittleFS.open(kCachePath, "w");
  if (!file) return false;
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(json),
      length);
  file.close();
  return written == length;
}

const SduiComponent* findButtonAt(
    const SduiScreen& screen,
    int16_t x,
    int16_t y) {
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    const SduiComponent& component = screen.components[index];
    if (component.type == ComponentType::kButton &&
        x >= component.x && x < component.x + component.width &&
        y >= component.y && y < component.y + component.height) {
      return &component;
    }
  }
  return nullptr;
}

SduiComponent* findSliderAt(
    SduiScreen& screen,
    int16_t x,
    int16_t y) {
  for (uint8_t index = 0; index < screen.componentCount; ++index) {
    SduiComponent& component = screen.components[index];
    constexpr int16_t kTouchPadding = 16;
    if (component.type == ComponentType::kSlider &&
        x >= component.x - kTouchPadding &&
        x <= component.x + component.width + kTouchPadding &&
        y >= component.y - kTouchPadding &&
        y <= component.y + component.height + kTouchPadding) {
      return &component;
    }
  }
  return nullptr;
}

}  // namespace leopold
