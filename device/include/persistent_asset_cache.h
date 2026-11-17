#pragma once

#include <cstddef>
#include <cstdint>

namespace leopold::asset_cache {

constexpr size_t kCacheLimitBytes = 1400000;
constexpr size_t kMaxAssetBytes = 651468;
// Keep cache I/O off the task stack. The main loop task is only 8 KiB and
// filesystem/crypto callers consume additional stack below this buffer.
constexpr size_t kIoChunkBytes = 1024;

struct AssetSpec {
  const char* id;
  const char* file;
  uint32_t bytes;
  const char* sha256;
  uint16_t width;
  uint16_t height;
};

enum class Pin : uint8_t { kNone = 0, kGlobalWallpaper = 1, kClockWallpaper = 2 };

// Filesystem must already be mounted without auto-format. The cache never
// formats storage and only activates a file after exact length/hash checks.
bool begin();
const AssetSpec* findSpec(const char* file);
const AssetSpec* selectedWallpaper(uint8_t index);
bool contains(const AssetSpec& spec);
bool setPin(const AssetSpec& spec, Pin pin);
bool install(const AssetSpec& spec, const uint8_t* bytes, size_t length, Pin pin);
bool installStream(const AssetSpec& spec, bool (*read)(void*, uint8_t*, size_t, size_t*), void* context, Pin pin);
bool select(const AssetSpec& spec, Pin pin, uint8_t** data, size_t* length);
size_t persistentBytes();
bool mounted();

}  // namespace leopold::asset_cache
