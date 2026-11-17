#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

namespace leopold::firmware_ota {

// One bounded, versioned record is enough because OTA installation is paused
// while an earlier terminal result is awaiting acknowledgment.
struct TerminalOutcomeRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t bytes;
  uint8_t state;
  uint8_t reserved[3];
  char releaseId[65];
  char eventId[129];
  char detail[257];
};

enum class TerminalState : uint8_t { kHealthy = 1, kRolledBack = 2, kFailed = 3 };
enum class OutboxResult : uint8_t {
  kQueued,
  kSameOutcomeAlreadyQueued,
  kEmpty,
  kPendingDifferentOutcome,
  kStorageError,
  kCorruptRecord,
};

constexpr uint32_t kTerminalOutcomeMagic = 0x4c4f5442;  // LOTB
constexpr uint16_t kTerminalOutcomeVersion = 1;
inline bool validTerminalOutcome(const TerminalOutcomeRecord& record);

inline bool makeTerminalOutcomeRecord(TerminalOutcomeRecord* output, TerminalState state,
                                      const char* releaseId, const char* eventId,
                                      const char* detail = nullptr) {
  if (output == nullptr || releaseId == nullptr || eventId == nullptr ||
      releaseId[0] == '\0' || eventId[0] == '\0') return false;
  TerminalOutcomeRecord record{};
  record.magic = kTerminalOutcomeMagic;
  record.version = kTerminalOutcomeVersion;
  record.bytes = sizeof(record);
  record.state = static_cast<uint8_t>(state);
  const int releaseLength = snprintf(record.releaseId, sizeof(record.releaseId), "%s", releaseId);
  const int eventLength = snprintf(record.eventId, sizeof(record.eventId), "%s", eventId);
  const int detailLength = detail == nullptr ? 0
      : snprintf(record.detail, sizeof(record.detail), "%s", detail);
  if (releaseLength <= 0 || static_cast<size_t>(releaseLength) >= sizeof(record.releaseId) ||
      eventLength <= 0 || static_cast<size_t>(eventLength) >= sizeof(record.eventId) ||
      detailLength < 0 || static_cast<size_t>(detailLength) >= sizeof(record.detail)) return false;
  if (!validTerminalOutcome(record)) return false;
  *output = record;
  return true;
}

class TerminalOutcomeStore {
 public:
  virtual ~TerminalOutcomeStore() = default;
  // Returns byte count, 0 when absent, or SIZE_MAX on storage error.
  virtual size_t read(void* output, size_t capacity) = 0;
  virtual bool write(const void* input, size_t length) = 0;
  virtual bool clear() = 0;
};

inline bool validTerminalOutcome(const TerminalOutcomeRecord& record) {
  return record.magic == kTerminalOutcomeMagic &&
         record.version == kTerminalOutcomeVersion && record.bytes == sizeof(record) &&
         record.state >= static_cast<uint8_t>(TerminalState::kHealthy) &&
         record.state <= static_cast<uint8_t>(TerminalState::kFailed) &&
         record.releaseId[0] != '\0' && memchr(record.releaseId, '\0', sizeof(record.releaseId)) &&
         record.eventId[0] != '\0' && memchr(record.eventId, '\0', sizeof(record.eventId)) &&
         memchr(record.detail, '\0', sizeof(record.detail));
}

inline OutboxResult loadTerminalOutcome(TerminalOutcomeStore& store,
                                        TerminalOutcomeRecord* output) {
  TerminalOutcomeRecord record{};
  const size_t length = store.read(&record, sizeof(record));
  if (length == SIZE_MAX) return OutboxResult::kStorageError;
  if (length == 0) return OutboxResult::kEmpty;
  if (length != sizeof(record) || !validTerminalOutcome(record)) return OutboxResult::kCorruptRecord;
  if (output != nullptr) *output = record;
  return OutboxResult::kQueued;
}

inline bool sameTerminalOutcome(const TerminalOutcomeRecord& left,
                                const TerminalOutcomeRecord& right) {
  return left.state == right.state && strcmp(left.releaseId, right.releaseId) == 0 &&
         strcmp(left.detail, right.detail) == 0;
}

inline OutboxResult enqueueTerminalOutcome(TerminalOutcomeStore& store,
                                           const TerminalOutcomeRecord& record) {
  if (!validTerminalOutcome(record)) return OutboxResult::kCorruptRecord;
  TerminalOutcomeRecord current{};
  const OutboxResult loaded = loadTerminalOutcome(store, &current);
  if (loaded == OutboxResult::kQueued) {
    return sameTerminalOutcome(current, record) ? OutboxResult::kSameOutcomeAlreadyQueued
                                                : OutboxResult::kPendingDifferentOutcome;
  }
  if (loaded != OutboxResult::kEmpty) return loaded;
  if (!store.write(&record, sizeof(record))) return OutboxResult::kStorageError;
  TerminalOutcomeRecord verified{};
  const OutboxResult reread = loadTerminalOutcome(store, &verified);
  if (reread != OutboxResult::kQueued || memcmp(&record, &verified, sizeof(record)) != 0)
    return OutboxResult::kStorageError;
  return OutboxResult::kQueued;
}

inline OutboxResult clearAcknowledgedOutcome(TerminalOutcomeStore& store,
                                              const TerminalOutcomeRecord& acknowledged) {
  TerminalOutcomeRecord current{};
  const OutboxResult loaded = loadTerminalOutcome(store, &current);
  if (loaded != OutboxResult::kQueued) return loaded;
  if (memcmp(&current, &acknowledged, sizeof(current)) != 0)
    return OutboxResult::kPendingDifferentOutcome;
  if (!store.clear()) return OutboxResult::kStorageError;
  const size_t after = store.read(nullptr, 0);
  return after == 0 ? OutboxResult::kEmpty : OutboxResult::kStorageError;
}

}  // namespace leopold::firmware_ota
