#pragma once

#include <stddef.h>

#if __has_include("leopold_config.h")
#include "leopold_config.h"
#else
#error "Missing include/leopold_config.h. Copy leopold_config.example.h and configure the device identity."
#endif

namespace leopold::config {

#if __has_include(<leopold_build_identity.h>)
#include <leopold_build_identity.h>
#endif
#ifndef LEOPOLD_SOURCE_COMMIT
#define LEOPOLD_SOURCE_COMMIT "local-uncommitted"
#endif
#ifndef LEOPOLD_BUILD_ID
#define LEOPOLD_BUILD_ID "unidentified"
#endif

constexpr char kSourceCommit[] = LEOPOLD_SOURCE_COMMIT;
constexpr char kBuildId[] = LEOPOLD_BUILD_ID;

constexpr bool contains(const char* value, const char* needle) {
  for (size_t start = 0; value[start] != '\0'; ++start) {
    size_t offset = 0;
    while (needle[offset] != '\0' && value[start + offset] == needle[offset]) {
      ++offset;
    }
    if (needle[offset] == '\0') {
      return true;
    }
  }
  return false;
}

static_assert(
    !contains(LEOPOLD_DEVICE_ID, "CHANGE-ME"),
    "LEOPOLD_DEVICE_ID must be configured in include/leopold_config.h");
static_assert(
    !contains(LEOPOLD_DEVICE_SECRET, "CHANGE-ME"),
    "LEOPOLD_DEVICE_SECRET must be configured in include/leopold_config.h");
static_assert(
    sizeof(LEOPOLD_TLS_ROOT_CA) > 100,
    "LEOPOLD_TLS_ROOT_CA must contain a trusted public root certificate");

constexpr char kFirmwareVersion[] = "0.8.2";

}  // namespace leopold::config
