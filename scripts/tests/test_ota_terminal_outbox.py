from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class TerminalOutboxTests(unittest.TestCase):
    def test_production_outbox_lifecycle_survives_reboot_and_storage_failures(self) -> None:
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("a local C++ compiler is required for the terminal outbox host harness")
        source = r'''#include <cassert>
#include <cstring>
#include <vector>
#include "ota_terminal_outbox.h"
using namespace leopold::firmware_ota;
class MemoryStore final : public TerminalOutcomeStore {
 public:
  std::vector<uint8_t> bytes;
  bool failWrite = false;
  bool failClear = false;
  bool corruptRead = false;
  bool corruptAfterWrite = false;
  size_t read(void* output, size_t capacity) override {
    if (corruptRead) return SIZE_MAX;
    if (bytes.empty()) return 0;
    if (output == nullptr && capacity == 0) return bytes.size();
    if (bytes.size() > capacity) return SIZE_MAX;
    memcpy(output, bytes.data(), bytes.size());
    return bytes.size();
  }
  bool write(const void* input, size_t length) override {
    if (failWrite) return false;
    const auto* first = static_cast<const uint8_t*>(input);
    bytes.assign(first, first + length);
    if (corruptAfterWrite && !bytes.empty()) bytes[0] ^= 0xff;
    return true;
  }
  bool clear() override {
    if (failClear) return false;
    bytes.clear();
    return true;
  }
};
TerminalOutcomeRecord event(TerminalState state, const char* release, const char* id,
                            const char* detail = "") {
  TerminalOutcomeRecord result{};
  assert(makeTerminalOutcomeRecord(&result, state, release, id, detail));
  return result;
}
int main() {
  MemoryStore nvs;
  auto healthy = event(TerminalState::kHealthy, "release-A", "stable-event-1");
  assert(enqueueTerminalOutcome(nvs, healthy) == OutboxResult::kQueued);
  TerminalOutcomeRecord afterRestart{};
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kQueued);
  assert(memcmp(&healthy, &afterRestart, sizeof(healthy)) == 0);
  // Reconstructing the same terminal result after a reboot preserves its ID.
  auto reconstructed = event(TerminalState::kHealthy, "release-A", "new-random-id");
  assert(enqueueTerminalOutcome(nvs, reconstructed) == OutboxResult::kSameOutcomeAlreadyQueued);
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kQueued);
  assert(strcmp(afterRestart.eventId, "stable-event-1") == 0);
  // A different outcome cannot overwrite the single pending record.
  auto rollback = event(TerminalState::kRolledBack, "release-B", "event-2");
  assert(enqueueTerminalOutcome(nvs, rollback) == OutboxResult::kPendingDifferentOutcome);
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kQueued);
  assert(strcmp(afterRestart.releaseId, "release-A") == 0);
  // Lost response: retrying is the same event; only confirmed ack clears it.
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kQueued);
  assert(strcmp(afterRestart.eventId, healthy.eventId) == 0);
  nvs.failClear = true;
  assert(clearAcknowledgedOutcome(nvs, healthy) == OutboxResult::kStorageError);
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kQueued);
  nvs.failClear = false;
  assert(clearAcknowledgedOutcome(nvs, healthy) == OutboxResult::kEmpty);
  assert(loadTerminalOutcome(nvs, &afterRestart) == OutboxResult::kEmpty);
  // An acknowledged event is not replayed after another restart.
  assert(loadTerminalOutcome(nvs, nullptr) == OutboxResult::kEmpty);
  // Failed persistence and corrupt/full state fail closed without overwrite.
  nvs.failWrite = true;
  assert(enqueueTerminalOutcome(nvs, rollback) == OutboxResult::kStorageError);
  assert(nvs.bytes.empty());
  nvs.failWrite = false;
  nvs.corruptAfterWrite = true;
  assert(enqueueTerminalOutcome(nvs, rollback) == OutboxResult::kStorageError);
  nvs.bytes.clear();
  nvs.corruptAfterWrite = false;
  assert(enqueueTerminalOutcome(nvs, rollback) == OutboxResult::kQueued);
  nvs.corruptRead = true;
  assert(enqueueTerminalOutcome(nvs, healthy) == OutboxResult::kStorageError);
  nvs.corruptRead = false;
  nvs.bytes[0] ^= 0xff;
  assert(enqueueTerminalOutcome(nvs, healthy) == OutboxResult::kCorruptRecord);
  assert(!nvs.bytes.empty());
  auto failedA = event(TerminalState::kFailed, "release-C", "event-3", "http-error");
  nvs.bytes.clear();
  assert(enqueueTerminalOutcome(nvs, failedA) == OutboxResult::kQueued);
  auto failedB = event(TerminalState::kFailed, "release-C", "event-4", "bad-signature");
  assert(enqueueTerminalOutcome(nvs, failedB) == OutboxResult::kPendingDifferentOutcome);
  TerminalOutcomeRecord invalid{};
  assert(!makeTerminalOutcomeRecord(&invalid, TerminalState::kHealthy, "", "event"));
  assert(!makeTerminalOutcomeRecord(&invalid, TerminalState::kHealthy, "release", ""));
  char tooLongRelease[66];
  memset(tooLongRelease, 'R', sizeof(tooLongRelease));
  tooLongRelease[sizeof(tooLongRelease) - 1] = '\0';
  assert(!makeTerminalOutcomeRecord(&invalid, TerminalState::kHealthy, tooLongRelease, "event"));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / "outbox_test.cpp"
            binary_path = Path(directory) / "outbox_test"
            source_path.write_text(source)
            subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "device/include"), str(source_path), "-o", str(binary_path)],
                check=True,
            )
            subprocess.run([str(binary_path)], check=True)

if __name__ == "__main__":
    unittest.main()
