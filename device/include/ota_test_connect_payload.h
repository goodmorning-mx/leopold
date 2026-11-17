#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

namespace leopold::ota_test {

inline bool appendJsonString(char* output, size_t capacity, size_t& used,
                             const char* value) {
  if (value == nullptr || used >= capacity) return false;
  auto append = [&](char character) {
    if (used + 1 >= capacity) return false;
    output[used++] = character;
    output[used] = '\0';
    return true;
  };
  if (!append('"')) return false;
  for (const unsigned char* cursor =
           reinterpret_cast<const unsigned char*>(value);
       *cursor != '\0'; ++cursor) {
    if (*cursor == '"' || *cursor == '\\') {
      if (!append('\\') || !append(static_cast<char>(*cursor))) return false;
    } else if (*cursor < 0x20) {
      if (used + 6 >= capacity) return false;
      const int written = std::snprintf(output + used, capacity - used,
                                        "\\u%04x", *cursor);
      if (written != 6) return false;
      used += static_cast<size_t>(written);
    } else if (!append(static_cast<char>(*cursor))) {
      return false;
    }
  }
  return append('"');
}

inline bool buildConnectPayload(char* output, size_t capacity,
                               const char* firmwareVersion,
                               const char* setupToken,
                               const char* stageMarker,
                               const char* sourceCommit,
                               const char* buildId,
                               uint32_t partitionAddress,
                               const char* partitionLabel) {
  if (output == nullptr || capacity == 0 || firmwareVersion == nullptr ||
      stageMarker == nullptr || sourceCommit == nullptr || buildId == nullptr ||
      partitionLabel == nullptr) return false;
  output[0] = '\0';
  size_t used = 0;
  auto appendLiteral = [&](const char* text) {
    const size_t length = std::strlen(text);
    if (length >= capacity - used) return false;
    std::memcpy(output + used, text, length);
    used += length;
    output[used] = '\0';
    return true;
  };
  auto appendNumber = [&](uint32_t value) {
    if (used >= capacity) return false;
    const int written = std::snprintf(output + used, capacity - used, "%lu",
                                      static_cast<unsigned long>(value));
    if (written < 0 || static_cast<size_t>(written) >= capacity - used)
      return false;
    used += static_cast<size_t>(written);
    return true;
  };
  if (!appendLiteral("{\"firmware_version\":")) return false;
  if (!appendJsonString(output, capacity, used, firmwareVersion)) return false;
  if (setupToken != nullptr && setupToken[0] != '\0') {
    if (!appendLiteral(",\"setup_token\":")) return false;
    if (!appendJsonString(output, capacity, used, setupToken)) return false;
  }
  if (!appendLiteral(",\"capabilities\":{\"display\":\"466x466\",\"touch\":true,\"audio\":false,\"battery\":true,\"ota_test_stage_marker\":")) return false;
  if (!appendJsonString(output, capacity, used, stageMarker)) return false;
  if (!appendLiteral(",\"ota_test_source_commit\":")) return false;
  if (!appendJsonString(output, capacity, used, sourceCommit)) return false;
  if (!appendLiteral(",\"ota_test_build_id\":")) return false;
  if (!appendJsonString(output, capacity, used, buildId)) return false;
  if (!appendLiteral(",\"ota_test_running_partition_address\":")) return false;
  if (!appendNumber(partitionAddress)) return false;
  if (!appendLiteral(",\"ota_test_running_partition_label\":")) return false;
  if (!appendJsonString(output, capacity, used, partitionLabel)) return false;
  return appendLiteral("}}");
}

}  // namespace leopold::ota_test
