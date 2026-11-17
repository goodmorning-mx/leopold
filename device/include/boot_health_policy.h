#pragma once

#include <stdint.h>
#include <string.h>

#include "boot_health_diagnostics.h"

namespace leopold::firmware_ota {

class BootHealthPolicy {
 public:
  enum class Action : uint8_t { kNone, kConfirm, kRollback };
  enum class PriorOutcome : uint8_t { kUnknown, kHealthy, kRolledBack };

  struct OtaRecordObservation {
    bool read = false;
    bool crcValid = false;
    uint32_t sequence = UINT32_MAX;
    uint32_t state = UINT32_MAX;
    uint32_t storedCrc = UINT32_MAX;
    uint8_t label[20] = {};
  };

  struct LocalUsbIdentityResult {
    uint16_t identityFlags = BootHealthDiagnostics::kLocalMarkerChecked;
    int8_t selectedRecordIndex = -1;
    BootHealthDiagnostics::SelectionStatus selectionStatus =
        BootHealthDiagnostics::SelectionStatus::kNotEvaluated;
    BootHealthDiagnostics::FailureStage failureStage =
        BootHealthDiagnostics::FailureStage::kNone;
    BootHealthDiagnostics::MarkerStatus markerStatus =
        BootHealthDiagnostics::MarkerStatus::kNotChecked;
    bool matched = false;
  };

  // This is the production fallback evaluator. The caller supplies snapshots
  // of the two raw records and the CRC result calculated with the pinned IDF
  // routine; tests exercise this same function with controlled observations.
  static LocalUsbIdentityResult evaluateLocalUsbIdentity(
      const OtaRecordObservation records[2], bool completeRead, bool runningSlotValid,
      uint32_t runningSlot, const char* sourceCommit) {
    LocalUsbIdentityResult result;
    if (!completeRead || records == nullptr || !records[0].read || !records[1].read) {
      result.failureStage = BootHealthDiagnostics::FailureStage::kOtaDataRead;
      return result;
    }
    result.identityFlags |= BootHealthDiagnostics::kLocalRecordRead;

    int selected = -1;
    uint32_t selectedSequence = 0;
    for (int index = 0; index < 2; ++index) {
      const OtaRecordObservation& record = records[index];
      if (!record.crcValid || record.sequence == UINT32_MAX) continue;
      if (selected < 0 || record.sequence > selectedSequence) {
        selected = index;
        selectedSequence = record.sequence;
      } else if (record.sequence == selectedSequence) {
        // Match the previous firmware scan: equal maximum sequences retain
        // the first array entry. Record the tie for diagnosis without changing
        // the existing decision policy.
        result.selectionStatus = BootHealthDiagnostics::SelectionStatus::kSequenceTie;
      }
    }
    if (selected < 0) {
      result.selectionStatus = BootHealthDiagnostics::SelectionStatus::kNoValidRecord;
      result.failureStage = BootHealthDiagnostics::FailureStage::kNoValidOtaRecord;
      return result;
    }
    result.selectedRecordIndex = static_cast<int8_t>(selected);
    if (result.selectionStatus != BootHealthDiagnostics::SelectionStatus::kSequenceTie)
      result.selectionStatus = BootHealthDiagnostics::SelectionStatus::kHighestValidSequence;
    result.identityFlags |= BootHealthDiagnostics::kLocalRecordSelected;
    const OtaRecordObservation& record = records[selected];
    if (record.state != 1U) {
      result.failureStage = BootHealthDiagnostics::FailureStage::kSelectedRecordState;
      return result;
    }
    result.identityFlags |= BootHealthDiagnostics::kLocalRecordPending;
    if (!runningSlotValid || runningSlot > 1 || (record.sequence - 1U) % 2U != runningSlot) {
      result.failureStage = BootHealthDiagnostics::FailureStage::kRunningSlotMapping;
      return result;
    }
    result.identityFlags |= BootHealthDiagnostics::kLocalRecordSlotMatches;
    if (memcmp(record.label, "LUSB", 4) != 0) {
      result.markerStatus = BootHealthDiagnostics::MarkerStatus::kLabelMismatch;
      result.failureStage = BootHealthDiagnostics::FailureStage::kMarkerLabel;
      return result;
    }
    if (!matchesLocalUsbIdentity(record.sequence, record.state, runningSlot,
                                 record.label, sourceCommit)) {
      result.markerStatus = BootHealthDiagnostics::MarkerStatus::kSourcePrefixMismatch;
      result.failureStage = BootHealthDiagnostics::FailureStage::kMarkerSourcePrefix;
      return result;
    }
    result.identityFlags |= BootHealthDiagnostics::kLocalMarkerPrefixMatches |
        BootHealthDiagnostics::kLocalMarkerMatched;
    result.markerStatus = BootHealthDiagnostics::MarkerStatus::kMatched;
    result.failureStage = BootHealthDiagnostics::FailureStage::kIdentityMatched;
    result.matched = true;
    return result;
  }

  static bool matchesLocalUsbIdentity(uint32_t sequence, uint32_t state, uint32_t runningSlot,
                                      const uint8_t* label, const char* sourceCommit) {
    if (sequence == 0 || sequence == UINT32_MAX || state != 1 || runningSlot > 1 ||
        label == nullptr || sourceCommit == nullptr || strlen(sourceCommit) != 40 ||
        (sequence - 1U) % 2U != runningSlot || memcmp(label, "LUSB", 4) != 0) return false;
    for (uint32_t index = 0; index < 16; ++index) {
      const char high = sourceCommit[index * 2];
      const char low = sourceCommit[index * 2 + 1];
      const auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
      };
      const int highValue = nibble(high);
      const int lowValue = nibble(low);
      if (highValue < 0 || lowValue < 0 ||
          label[4 + index] != static_cast<uint8_t>((highValue << 4) | lowValue)) return false;
    }
    return true;
  }

  static bool matchesPendingImage(bool stateIsPending, bool releasePresent,
                                  uint32_t runningAddress, uint32_t attemptedAddress,
                                  bool sourceCommitMatches) {
    return stateIsPending && releasePresent && attemptedAddress != UINT32_MAX &&
        runningAddress == attemptedAddress && sourceCommitMatches;
  }

  static PriorOutcome classifyPriorImage(bool stateReadable, bool stateIsValid,
                                         uint32_t runningAddress, uint32_t attemptedAddress,
                                         bool sourceCommitMatches) {
    if (!stateReadable || !stateIsValid || attemptedAddress == UINT32_MAX) return PriorOutcome::kUnknown;
    if (runningAddress != attemptedAddress) return PriorOutcome::kRolledBack;
    return sourceCommitMatches ? PriorOutcome::kHealthy : PriorOutcome::kUnknown;
  }

  static constexpr uint32_t kMinimumMs = 30000;
  static constexpr uint32_t kDeadlineMs = 90000;
  static constexpr uint32_t kMinimumLoops = 100;

  static Action afterConfirmationApi(bool apiSucceeded) {
    return apiSucceeded ? Action::kNone : Action::kRollback;
  }

  void reset(uint32_t now) {
    startedAt_ = now;
    loops_ = 0;
    displayReady_ = touchReady_ = settingsReady_ = setupReady_ = false;
    finished_ = false;
  }

  void setReadiness(bool display, bool touch, bool settings, bool setup) {
    displayReady_ = display;
    touchReady_ = touch;
    settingsReady_ = settings;
    setupReady_ = setup;
  }

  Action tick(uint32_t now) {
    if (finished_) return Action::kNone;
    ++loops_;
    const uint32_t elapsed = now - startedAt_;
    if (elapsed >= kDeadlineMs && (!allReady() || loops_ < kMinimumLoops)) {
      finished_ = true;
      return Action::kRollback;
    }
    if (elapsed >= kMinimumMs && allReady() && loops_ >= kMinimumLoops) {
      finished_ = true;
      return Action::kConfirm;
    }
    return Action::kNone;
  }

  uint32_t elapsed(uint32_t now) const { return now - startedAt_; }
  uint32_t loops() const { return loops_; }
  bool displayReady() const { return displayReady_; }
  bool touchReady() const { return touchReady_; }
  bool settingsReady() const { return settingsReady_; }
  bool setupReady() const { return setupReady_; }

 private:
  bool allReady() const { return displayReady_ && touchReady_ && settingsReady_ && setupReady_; }
  uint32_t startedAt_ = 0;
  uint32_t loops_ = 0;
  bool displayReady_ = false;
  bool touchReady_ = false;
  bool settingsReady_ = false;
  bool setupReady_ = false;
  bool finished_ = false;
};

}  // namespace leopold::firmware_ota
