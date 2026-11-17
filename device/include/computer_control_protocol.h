#pragma once

#include <cstdint>
#include <cstring>

namespace leopold {

constexpr uint16_t kConsumerVolumeIncrement = 0x00E9;
constexpr uint16_t kConsumerVolumeDecrement = 0x00EA;
constexpr uint16_t kConsumerMute = 0x00E2;

inline uint16_t computerControlUsage(const char* componentId) {
  if (componentId == nullptr) return 0;
  if (std::strcmp(componentId, "computer-volume-down") == 0)
    return kConsumerVolumeDecrement;
  if (std::strcmp(componentId, "computer-volume-mute") == 0)
    return kConsumerMute;
  if (std::strcmp(componentId, "computer-volume-up") == 0)
    return kConsumerVolumeIncrement;
  return 0;
}

class ConsumerControlSink {
 public:
  virtual ~ConsumerControlSink() = default;
  virtual bool connected() const = 0;
  virtual bool press(uint16_t usage) = 0;
  virtual bool release() = 0;
};

inline bool sendComputerControlTap(
    ConsumerControlSink& sink,
    const char* componentId) {
  const uint16_t usage = computerControlUsage(componentId);
  if (usage == 0 || !sink.connected()) return false;
  const bool pressed = sink.press(usage);
  // Always send a neutral report, even after a failed press attempt.
  const bool released = sink.release();
  return pressed && released;
}

}  // namespace leopold
