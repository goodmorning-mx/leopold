#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace leopold::firmware_ota {

// Preserve the installed diagnostic layout verbatim. The decoder continues to
// recognize this 48-byte v1 record and never interprets it as the newer schema.
#pragma pack(push, 1)
struct BootHealthDiagnosticRecordV1 {
  uint32_t magic;
  uint16_t version;
  uint8_t phase;
  uint8_t initStage;
  uint32_t runningAddress;
  int32_t stateResult;
  int32_t imageState;
  uint32_t attemptedAddress;
  uint16_t identityFlags;
  uint8_t readinessFlags;
  uint8_t reserved;
  uint32_t elapsedMs;
  uint32_t loopCount;
  int32_t confirmationResult;
  int32_t rollbackApiResult;
  uint8_t rollbackReason;
  uint8_t padding[3];
};

// v2 is a bounded observation, not a new acceptance policy. Read masks make
// 0xffffffff a representable OTA state rather than an ambiguous missing value.
struct BootHealthDiagnosticRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t recordLength;
  uint8_t phase;
  uint8_t initStage;
  uint8_t failureStage;
  uint8_t rollbackReason;
  uint8_t stateValid;
  uint8_t attemptedAddressValid;
  uint8_t runningPartitionValid;
  uint8_t otaDataPartitionFound;
  uint8_t otaRecordReadMask;
  uint8_t otaRecordCrcValidMask;
  int8_t selectedRecordIndex;
  uint8_t selectionStatus;
  uint16_t identityFlags;
  uint8_t readinessFlags;
  uint8_t previousWriteResult;
  uint8_t identityMarkerStatus;
  uint8_t buildIdentityFlags;
  uint32_t runningAddress;
  uint32_t runningSubtype;
  int32_t stateResult;
  int32_t imageState;
  uint32_t attemptedAddress;
  uint32_t otaDataAddress;
  int32_t otaRecordReadResult;
  uint32_t otaSequence[2];
  uint32_t otaState[2];
  uint32_t otaCrc[2];
  uint32_t elapsedMs;
  uint32_t loopCount;
  int32_t confirmationResult;
  int32_t rollbackApiResult;
  uint8_t sourceCommitPrefix[8];
  uint8_t appElfShaPrefix[16];
  uint8_t selectedRecordLabel[20];
};
#pragma pack(pop)

static_assert(sizeof(BootHealthDiagnosticRecordV1) == 48, "v1 diagnostic layout changed");
static_assert(sizeof(BootHealthDiagnosticRecord) == 138, "v2 diagnostic layout changed");
static_assert(sizeof(BootHealthDiagnosticRecord) <= 160, "v2 diagnostic record must stay bounded");

struct BootHealthDiagnostics {
  enum class Phase : uint8_t {
    kBoot = 1,
    kIdentity = 2,
    kReadiness = 3,
    kProgress = 4,
    kConfirmation = 5,
    kRollbackIntent = 6,
    kConfirmed = 7,
    kRollbackApiReturned = 8,
  };
  enum class InitStage : uint8_t {
    kUnknown = 0,
    kDisplayFailed = 1,
    kIdentityFailed = 2,
    kLocalServicesReady = 3,
    kCloudFailed = 4,
    kSetupReady = 5,
  };
  enum class RollbackReason : uint8_t {
    kNone = 0,
    kPendingIdentityMismatch = 1,
    kLocalHealthTimeout = 2,
    kConfirmationApiFailed = 3,
  };
  enum class FailureStage : uint8_t {
    kNone = 0,
    kPublicStateLookup = 1,
    kAttemptPreferences = 2,
    kNormalAttemptIdentity = 3,
    kRunningPartition = 4,
    kOtaDataPartition = 5,
    kOtaDataRead = 6,
    kNoValidOtaRecord = 7,
    kAmbiguousOtaSequence = 8,
    kSelectedRecordState = 9,
    kRunningSlotMapping = 10,
    kMarkerLabel = 11,
    kMarkerSourcePrefix = 12,
    kIdentityMatched = 13,
    kLocalHealthTimeout = 14,
    kConfirmationApi = 15,
  };
  enum class SelectionStatus : uint8_t {
    kNotEvaluated = 0,
    kNoValidRecord = 1,
    kSequenceTie = 2,
    kHighestValidSequence = 3,
  };
  enum class MarkerStatus : uint8_t {
    kNotChecked = 0,
    kLabelMismatch = 1,
    kSourcePrefixMismatch = 2,
    kMatched = 3,
  };
  enum class PersistResult : uint8_t {
    kNotAttempted = 0,
    kWritten = 1,
    kNamespaceOpenFailed = 2,
    kShortWrite = 3,
  };

  static constexpr uint32_t kMagic = 0x4C424844;  // LBHD
  static constexpr uint16_t kVersion = 2;
  static constexpr uint8_t kApiStatePending = 1U << 0;
  static constexpr uint16_t kOtaStatePending = 1U << 0;
  static constexpr uint16_t kPreferencesOpened = 1U << 1;
  static constexpr uint16_t kNvsIdentityMatched = 1U << 2;
  static constexpr uint16_t kPendingIdentityMatched = 1U << 3;
  static constexpr uint16_t kNvsReleasePresent = 1U << 4;
  static constexpr uint16_t kNvsPartitionMatches = 1U << 5;
  static constexpr uint16_t kNvsCommitMatches = 1U << 6;
  static constexpr uint16_t kLocalMarkerChecked = 1U << 7;
  static constexpr uint16_t kLocalRecordRead = 1U << 8;
  static constexpr uint16_t kLocalRecordSelected = 1U << 9;
  static constexpr uint16_t kLocalRecordPending = 1U << 10;
  static constexpr uint16_t kLocalRecordSlotMatches = 1U << 11;
  static constexpr uint16_t kLocalMarkerPrefixMatches = 1U << 12;
  static constexpr uint16_t kLocalMarkerMatched = 1U << 13;
  static constexpr uint8_t kDisplayReady = 1U << 0;
  static constexpr uint8_t kTouchReady = 1U << 1;
  static constexpr uint8_t kSettingsReady = 1U << 2;
  static constexpr uint8_t kSetupReady = 1U << 3;
  static constexpr uint8_t kCommitPrefixAvailable = 1U << 0;
  static constexpr uint8_t kAppElfShaAvailable = 1U << 1;

  static void initialize(BootHealthDiagnosticRecord& record) {
    memset(&record, 0, sizeof(record));
    record.magic = kMagic;
    record.version = kVersion;
    record.recordLength = sizeof(record);
    record.phase = static_cast<uint8_t>(Phase::kBoot);
    record.runningAddress = UINT32_MAX;
    record.runningSubtype = UINT32_MAX;
    record.stateResult = INT32_MIN;
    record.imageState = INT32_MIN;
    record.attemptedAddress = UINT32_MAX;
    record.otaDataAddress = UINT32_MAX;
    record.otaRecordReadResult = INT32_MIN;
    record.selectedRecordIndex = -1;
    record.selectionStatus = static_cast<uint8_t>(SelectionStatus::kNotEvaluated);
    record.previousWriteResult = static_cast<uint8_t>(PersistResult::kNotAttempted);
    record.identityMarkerStatus = static_cast<uint8_t>(MarkerStatus::kNotChecked);
    record.confirmationResult = INT32_MIN;
    record.rollbackApiResult = INT32_MIN;
    for (uint8_t index = 0; index < 2; ++index) {
      record.otaSequence[index] = UINT32_MAX;
      record.otaState[index] = UINT32_MAX;
      record.otaCrc[index] = UINT32_MAX;
    }
  }

  static void setBuildIdentity(BootHealthDiagnosticRecord& record, const char* sourceCommit,
                               const uint8_t* appElfSha) {
    if (sourceCommit != nullptr && strlen(sourceCommit) == 40) {
      bool valid = true;
      for (uint8_t index = 0; index < 8; ++index) {
        const int high = nibble(sourceCommit[index * 2]);
        const int low = nibble(sourceCommit[index * 2 + 1]);
        if (high < 0 || low < 0) { valid = false; break; }
        record.sourceCommitPrefix[index] = static_cast<uint8_t>((high << 4) | low);
      }
      if (valid) record.buildIdentityFlags |= kCommitPrefixAvailable;
    }
    if (appElfSha != nullptr) {
      memcpy(record.appElfShaPrefix, appElfSha, sizeof(record.appElfShaPrefix));
      record.buildIdentityFlags |= kAppElfShaAvailable;
    }
  }

  static uint8_t readiness(bool display, bool touch, bool settings, bool setup) {
    return (display ? kDisplayReady : 0) | (touch ? kTouchReady : 0) |
        (settings ? kSettingsReady : 0) | (setup ? kSetupReady : 0);
  }

  template <typename PreferencesLike>
  static PersistResult persist(PreferencesLike& preferences,
                               const BootHealthDiagnosticRecord& record,
                               size_t* written = nullptr) {
    if (written != nullptr) *written = 0;
    if (!preferences.begin("boot-health", false)) return PersistResult::kNamespaceOpenFailed;
    const size_t count = preferences.putBytes("last", &record, sizeof(record));
    preferences.end();
    if (written != nullptr) *written = count;
    return count == sizeof(record) ? PersistResult::kWritten : PersistResult::kShortWrite;
  }

  static bool valid(const BootHealthDiagnosticRecord& record) {
    return record.magic == kMagic && record.version == kVersion &&
        record.recordLength == sizeof(record) &&
        record.phase >= static_cast<uint8_t>(Phase::kBoot) &&
        record.phase <= static_cast<uint8_t>(Phase::kRollbackApiReturned) &&
        record.initStage <= static_cast<uint8_t>(InitStage::kSetupReady) &&
        record.rollbackReason <= static_cast<uint8_t>(RollbackReason::kConfirmationApiFailed) &&
        record.failureStage <= static_cast<uint8_t>(FailureStage::kConfirmationApi) &&
        record.selectionStatus <= static_cast<uint8_t>(SelectionStatus::kHighestValidSequence) &&
        record.identityMarkerStatus <= static_cast<uint8_t>(MarkerStatus::kMatched) &&
        record.previousWriteResult <= static_cast<uint8_t>(PersistResult::kShortWrite) &&
        record.stateValid <= 1 && record.attemptedAddressValid <= 1 &&
        record.runningPartitionValid <= 1 && record.otaDataPartitionFound <= 1 &&
        record.selectedRecordIndex >= -1 && record.selectedRecordIndex <= 1 &&
        (record.otaRecordReadMask & ~0x03U) == 0 &&
        (record.otaRecordCrcValidMask & ~record.otaRecordReadMask) == 0;
  }

 private:
  static int nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  }
};

}  // namespace leopold::firmware_ota
