from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _nvs_entry(namespace: int, item_type: int, span: int, chunk: int,
               key: str, data: bytes) -> bytes:
    entry = bytearray(b"\xff" * 32)
    entry[0:4] = bytes((namespace, item_type, span, chunk))
    entry[8:24] = key.encode("ascii").ljust(16, b"\0")
    entry[24:32] = data[:8].ljust(8, b"\xff")
    checksum = zlib.crc32(entry[:4] + entry[8:], 0xFFFFFFFF)
    entry[4:8] = checksum.to_bytes(4, "little")
    return bytes(entry)


def _synthetic_nvs(record: bytes) -> bytes:
    page = bytearray(b"\xff" * 4096)
    page[0:4] = (0xFFFFFFFE).to_bytes(4, "little")
    page[4:8] = (1).to_bytes(4, "little")
    page[8] = 0xFE
    page[28:32] = zlib.crc32(page[4:28], 0xFFFFFFFF).to_bytes(4, "little")
    entries: list[bytes] = []
    entries.append(_nvs_entry(0, 0x01, 1, 0, "boot-health", b"\x01"))
    payload_crc = zlib.crc32(record, 0xFFFFFFFF)
    chunk_span = 1 + (len(record) + 31) // 32
    entries.append(_nvs_entry(1, 0x48, 1, 0xFF, "last",
                               len(record).to_bytes(4, "little") + bytes((1, 0))))
    entries.append(_nvs_entry(1, 0x42, chunk_span, 0, "last",
                               len(record).to_bytes(2, "little") + b"\xff\xff" +
                               payload_crc.to_bytes(4, "little")))
    padded = record.ljust((chunk_span - 1) * 32, b"\xff")
    entries.extend(padded[offset:offset + 32] for offset in range(0, len(padded), 32))
    bitmap = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    for index in range(len(entries)):
        shift = index * 2
        bitmap &= ~(3 << shift)
        bitmap |= 2 << shift
    page[32:64] = bitmap.to_bytes(32, "little")
    for index, entry in enumerate(entries):
        start = 64 + index * 32
        page[start:start + 32] = entry
    return bytes(page) + b"\xff" * (4096 * 4)


class BootHealthPolicyTests(unittest.TestCase):
    @staticmethod
    def _v2_record(**overrides: object) -> bytes:
        from scripts.decode_boot_health_diag import RECORD_V2

        values: dict[str, object] = {
            "magic": 0x4C424844, "version": 2, "length": RECORD_V2.size,
            "phase": 2, "init": 2, "failure": 9, "rollback": 1,
            "state_valid": 1, "attempt_valid": 0, "running_valid": 1,
            "ota_found": 1, "read_mask": 3, "crc_mask": 3,
            "selected": 1, "selection": 3, "flags": 899,
            "readiness": 0, "previous_write": 1, "marker": 0, "build_flags": 3,
            "running_address": 0x650000, "running_subtype": 17,
            "state_result": 0, "image_state": 1, "attempted": 0xFFFFFFFF,
            "ota_address": 0xE000, "ota_read_result": 0,
            "seq0": 1, "seq1": 2, "state0": 2, "state1": 0,
            "crc0": 0x11111111, "crc1": 0x22222222,
            "elapsed": 27, "loops": 0, "confirm": -0x80000000,
            "rollback_api": -0x80000000,
            "commit": bytes.fromhex("0123456789abcdef"),
            "elf": bytes(range(16)), "label": b"LUSB" + bytes(range(16)),
        }
        values.update(overrides)
        ordered = [values[k] for k in (
            "magic", "version", "length", "phase", "init", "failure", "rollback",
            "state_valid", "attempt_valid", "running_valid", "ota_found", "read_mask",
            "crc_mask", "selected", "selection", "flags", "readiness", "previous_write",
            "marker", "build_flags", "running_address", "running_subtype", "state_result",
            "image_state", "attempted", "ota_address", "ota_read_result", "seq0", "seq1",
            "state0", "state1", "crc0", "crc1", "elapsed", "loops", "confirm",
            "rollback_api", "commit", "elf", "label",
        )]
        return RECORD_V2.pack(*ordered)

    def test_private_nvs_extractor_returns_only_diagnostic_record(self) -> None:
        from scripts.decode_boot_health_diag import RECORD, extract_record

        record = RECORD.pack(
            0x4C424844, 1, 6, 4, 0x650000, 0, 1, 0xFFFFFFFF,
            0x008F, 0x0007, 0, 90000, 201, -1, 0, 2, b"\0\0\0",
        )
        result = extract_record(_synthetic_nvs(record))
        self.assertEqual(result["phase"], "rollback")
        self.assertEqual(result["init_stage"], "cloud_failed")
        self.assertEqual(result["rollback_reason"], "local_health_timeout")
        self.assertEqual(result["elapsed_ms"], 90000)
        self.assertEqual(result["loop_count"], 201)
        self.assertNotIn("device_id", result)
        self.assertNotIn("credentials", result)

    def test_private_nvs_extractor_rejects_bad_record_crc(self) -> None:
        from scripts.decode_boot_health_diag import RECORD, DiagnosticError, extract_record

        record = RECORD.pack(
            0x4C424844, 1, 6, 4, 0x650000, 0, 1, 0xFFFFFFFF,
            0x008F, 0x0007, 0, 90000, 201, -1, 0, 2, b"\0\0\0",
        )
        raw = bytearray(_synthetic_nvs(record))
        raw[64 + 2 * 32 + 8] ^= 0x01
        with self.assertRaises(DiagnosticError):
            extract_record(bytes(raw))

    def test_v1_capture_pattern_stays_decodable_without_inventing_raw_state(self) -> None:
        from scripts.decode_boot_health_diag import RECORD_V1, decode_record_blob

        blob = RECORD_V1.pack(
            0x4C424844, 1, 6, 0, 0x650000, 0, 1, 0xFFFFFFFF,
            899, 0, 0, 27, 0, -0x80000000, -0x80000000, 1, b"\0\0\0",
        )
        result = decode_record_blob(blob)
        self.assertEqual(result["identity_flags"], 899)
        self.assertIsNone(result["raw_ota_records"])
        self.assertEqual(result["freshness"], "unbound_v1_record")

    def test_v2_record_decodes_observations_and_keeps_unknowns_explicit(self) -> None:
        from scripts.decode_boot_health_diag import decode_record_blob

        result = decode_record_blob(self._v2_record(state1=0xDEADBEEF))
        self.assertEqual(result["schema_version"], 2)
        self.assertEqual(result["failure_stage"], "selected_record_state")
        self.assertEqual(result["selection"]["record_index"], 1)
        self.assertEqual(result["ota_records"][1]["state"], "unknown")
        self.assertEqual(result["ota_records"][1]["state_raw"], 0xDEADBEEF)
        self.assertIsNone(result["rollback_api_result"])
        self.assertEqual(result["rollback_completion"], "unknown_until_later_boot_observation")

        undefined = decode_record_blob(self._v2_record(state1=0xFFFFFFFF))
        self.assertEqual(undefined["ota_records"][1]["state"], "undefined")

        successful = decode_record_blob(self._v2_record(state1=1, marker=3, failure=13))
        self.assertEqual(successful["selection"]["record"]["state"], "pending_verify")
        self.assertEqual(successful["identity_marker"]["status"], "matched")
        self.assertEqual(successful["failure_stage"], "identity_matched")

        unavailable = decode_record_blob(self._v2_record(
            state_valid=0, image_state=1, read_mask=1, crc_mask=1,
            state1=1, selected=-1, selection=1, failure=6,
        ))
        self.assertEqual(unavailable["public_ota_state"]["state"], "unavailable")
        self.assertIsNone(unavailable["public_ota_state"]["state_raw"])
        self.assertEqual(unavailable["ota_records"][1]["state"], "unavailable")

    def test_v2_rejects_truncated_corrupt_unsupported_and_incomplete_headers(self) -> None:
        from scripts.decode_boot_health_diag import DiagnosticError, decode_record_blob

        valid = self._v2_record()
        for bad in (valid[:-1], valid + b"\0", b"BAD!" + valid[4:],
                    self._v2_record(version=99), self._v2_record(length=137),
                    self._v2_record(read_mask=4), self._v2_record(crc_mask=4),
                    self._v2_record(state_valid=2)):
            with self.subTest(blob_length=len(bad)):
                with self.assertRaises(DiagnosticError):
                    decode_record_blob(bad)

    def test_nvs_extractor_supports_v2_record(self) -> None:
        from scripts.decode_boot_health_diag import extract_record

        result = extract_record(_synthetic_nvs(self._v2_record()))
        self.assertEqual(result["schema_version"], 2)
        self.assertEqual(result["build_identity"]["source_commit_prefix"], "0123456789abcdef")

    def test_firmware_policy_host_harness(self) -> None:
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("a local C++ compiler is required for the firmware policy host harness")
        source = r'''#include <cassert>
#include "boot_health_policy.h"
#include "boot_health_diagnostics.h"
using leopold::firmware_ota::BootHealthPolicy;
using leopold::firmware_ota::BootHealthDiagnosticRecord;
using leopold::firmware_ota::BootHealthDiagnosticRecordV1;
using leopold::firmware_ota::BootHealthDiagnostics;
struct FakePreferences {
  bool open = true;
  size_t returned = static_cast<size_t>(-1);
  bool ended = false;
  bool begin(const char*, bool) { return open; }
  size_t putBytes(const char*, const void*, size_t size) {
    return returned == static_cast<size_t>(-1) ? size : returned;
  }
  void end() { ended = true; }
};
int main() {
  BootHealthDiagnosticRecord diagnostic = {};
  BootHealthDiagnostics::initialize(diagnostic);
  diagnostic.phase = static_cast<uint8_t>(BootHealthDiagnostics::Phase::kRollbackIntent);
  diagnostic.rollbackReason = static_cast<uint8_t>(BootHealthDiagnostics::RollbackReason::kLocalHealthTimeout);
  assert(BootHealthDiagnostics::valid(diagnostic));
  diagnostic.rollbackReason = 99;
  assert(!BootHealthDiagnostics::valid(diagnostic));
  diagnostic.rollbackReason = static_cast<uint8_t>(BootHealthDiagnostics::RollbackReason::kLocalHealthTimeout);
  diagnostic.version++;
  assert(!BootHealthDiagnostics::valid(diagnostic));
  assert(BootHealthDiagnostics::readiness(true, false, true, false) ==
      (BootHealthDiagnostics::kDisplayReady | BootHealthDiagnostics::kSettingsReady));
  assert(sizeof(BootHealthDiagnosticRecordV1) == 48);
  assert(sizeof(BootHealthDiagnosticRecord) <= 160);
  // The retrieved v1 pattern records API-level PENDING_VERIFY and a selected
  // local OTA entry, but not a successful local-record pending-state check.
  // Its raw ota_state value is not stored, so exercise the fail-closed gate
  // without claiming which non-pending state the device actually contained.
  const uint16_t observedFlags = BootHealthDiagnostics::kOtaStatePending |
      BootHealthDiagnostics::kPreferencesOpened | BootHealthDiagnostics::kLocalMarkerChecked |
      BootHealthDiagnostics::kLocalRecordRead | BootHealthDiagnostics::kLocalRecordSelected;
  assert(observedFlags == 899);
  assert(observedFlags & BootHealthDiagnostics::kOtaStatePending);
  assert(observedFlags & BootHealthDiagnostics::kLocalRecordSelected);
  assert(!(observedFlags & BootHealthDiagnostics::kLocalRecordPending));
  BootHealthPolicy p;
  using Outcome = BootHealthPolicy::PriorOutcome;
  assert(BootHealthPolicy::afterConfirmationApi(true) == BootHealthPolicy::Action::kNone);
  assert(BootHealthPolicy::afterConfirmationApi(false) == BootHealthPolicy::Action::kRollback);
  assert(BootHealthPolicy::matchesPendingImage(true, true, 0x650000, 0x650000, true));
  assert(!BootHealthPolicy::matchesPendingImage(true, true, 0x650000, 0x10000, true));
  assert(!BootHealthPolicy::matchesPendingImage(true, true, 0x650000, 0x650000, false));
  assert(!BootHealthPolicy::matchesPendingImage(true, false, 0x650000, 0x650000, true));
  const char commit[] = "0123456789abcdef0123456789abcdef01234567";
  const uint8_t label[] = {'L','U','S','B',0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
                           0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
  assert(BootHealthPolicy::matchesLocalUsbIdentity(2, 1, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 0, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 2, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 3, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, UINT32_MAX, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 1, 0, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(3, 1, 1, label, commit));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 1, 1, label, "1123456789abcdef0123456789abcdef01234567"));
  assert(!BootHealthPolicy::matchesLocalUsbIdentity(2, 1, 1, label, "bad"));

  BootHealthPolicy::OtaRecordObservation records[2] = {};
  records[0].read = records[0].crcValid = true;
  records[0].sequence = 1; records[0].state = 2;
  records[1].read = records[1].crcValid = true;
  records[1].sequence = 2; records[1].state = 1;
  memcpy(records[1].label, label, sizeof(label));
  auto decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(decision.matched && decision.selectedRecordIndex == 1);
  assert(decision.selectionStatus == BootHealthDiagnostics::SelectionStatus::kHighestValidSequence);
  assert(decision.identityFlags & BootHealthDiagnostics::kLocalRecordPending);
  assert(decision.markerStatus == BootHealthDiagnostics::MarkerStatus::kMatched);

  records[1].state = 0;  // Representative non-pending state; device raw value is unknown.
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched && decision.selectedRecordIndex == 1);
  assert(decision.failureStage == BootHealthDiagnostics::FailureStage::kSelectedRecordState);
  assert(!(decision.identityFlags & BootHealthDiagnostics::kLocalRecordPending));
  records[1].state = 1;
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 0, commit);
  assert(!decision.matched);
  assert(decision.failureStage == BootHealthDiagnostics::FailureStage::kRunningSlotMapping);
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, false, 1, commit);
  assert(!decision.matched);
  assert(decision.failureStage == BootHealthDiagnostics::FailureStage::kRunningSlotMapping);

  records[1].label[0] = 'X';
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched);
  assert(decision.markerStatus == BootHealthDiagnostics::MarkerStatus::kLabelMismatch);
  records[1].label[0] = 'L';
  records[1].label[19] ^= 1;
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched);
  assert(decision.markerStatus == BootHealthDiagnostics::MarkerStatus::kSourcePrefixMismatch);
  memcpy(records[1].label, label, sizeof(label));

  records[1].crcValid = false;
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched && decision.selectedRecordIndex == 0);
  records[0].crcValid = false;
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched);
  assert(decision.selectionStatus == BootHealthDiagnostics::SelectionStatus::kNoValidRecord);
  records[0].crcValid = records[1].crcValid = true;
  records[0].sequence = records[1].sequence = 2;
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(!decision.matched);
  assert(decision.selectionStatus == BootHealthDiagnostics::SelectionStatus::kSequenceTie);
  // Existing scan behavior keeps entry 0 when equal maximum sequences tie;
  // diagnostics report the tie but must not introduce a new rejection.
  records[0].state = 1;
  memcpy(records[0].label, label, sizeof(label));
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, true, true, 1, commit);
  assert(decision.matched && decision.selectedRecordIndex == 0);
  assert(decision.selectionStatus == BootHealthDiagnostics::SelectionStatus::kSequenceTie);
  decision = BootHealthPolicy::evaluateLocalUsbIdentity(records, false, true, 1, commit);
  assert(!decision.matched);
  assert(decision.failureStage == BootHealthDiagnostics::FailureStage::kOtaDataRead);

  BootHealthDiagnosticRecord diagnosticV2;
  BootHealthDiagnostics::initialize(diagnosticV2);
  assert(BootHealthDiagnostics::valid(diagnosticV2));
  const uint8_t elfSha[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  BootHealthDiagnostics::setBuildIdentity(diagnosticV2, commit, elfSha);
  assert(diagnosticV2.buildIdentityFlags ==
      (BootHealthDiagnostics::kCommitPrefixAvailable | BootHealthDiagnostics::kAppElfShaAvailable));
  diagnosticV2.recordLength--;
  assert(!BootHealthDiagnostics::valid(diagnosticV2));
  diagnosticV2.recordLength++;

  FakePreferences preferences;
  preferences.open = false;
  assert(BootHealthDiagnostics::persist(preferences, diagnosticV2) ==
      BootHealthDiagnostics::PersistResult::kNamespaceOpenFailed);
  assert(!preferences.ended);
  preferences.open = true;
  preferences.returned = sizeof(diagnosticV2) - 1;
  assert(BootHealthDiagnostics::persist(preferences, diagnosticV2) ==
      BootHealthDiagnostics::PersistResult::kShortWrite);
  assert(preferences.ended);
  preferences.returned = static_cast<size_t>(-1);
  assert(BootHealthDiagnostics::persist(preferences, diagnosticV2) ==
      BootHealthDiagnostics::PersistResult::kWritten);
  assert(BootHealthPolicy::classifyPriorImage(true, true, 0x650000, 0x650000, true) == Outcome::kHealthy);
  assert(BootHealthPolicy::classifyPriorImage(true, true, 0x650000, 0x650000, false) == Outcome::kUnknown);
  assert(BootHealthPolicy::classifyPriorImage(true, true, 0x10000, 0x650000, false) == Outcome::kRolledBack);
  assert(BootHealthPolicy::classifyPriorImage(true, false, 0x650000, 0x650000, true) == Outcome::kUnknown);
  assert(BootHealthPolicy::classifyPriorImage(false, false, 0x650000, 0x650000, true) == Outcome::kUnknown);
  assert(BootHealthPolicy::classifyPriorImage(true, true, 0x650000, UINT32_MAX, true) == Outcome::kUnknown);

  p.reset(1000);
  p.setReadiness(true, true, true, true);
  for (int i = 0; i < 99; ++i) assert(p.tick(31000) == BootHealthPolicy::Action::kNone);
  assert(p.tick(31000) == BootHealthPolicy::Action::kConfirm);
  assert(p.tick(31001) == BootHealthPolicy::Action::kNone);

  p.reset(0);
  p.setReadiness(true, false, true, true);
  for (int i = 0; i < 100; ++i) assert(p.tick(89999) == BootHealthPolicy::Action::kNone);
  p.setReadiness(true, true, true, true);
  assert(p.tick(90000) == BootHealthPolicy::Action::kConfirm);

  p.reset(0);
  p.setReadiness(true, false, true, true);
  for (int i = 0; i < 99; ++i) assert(p.tick(89999) == BootHealthPolicy::Action::kNone);
  assert(p.tick(90000) == BootHealthPolicy::Action::kRollback);
  assert(p.loops() == 100);

  p.reset(0);
  p.setReadiness(true, true, true, true);
  for (int i = 0; i < 99; ++i) assert(p.tick(89999) == BootHealthPolicy::Action::kNone);
  assert(p.tick(90000) == BootHealthPolicy::Action::kConfirm);

  p.reset(0);
  p.setReadiness(true, true, true, false);  // e.g. fatal cloud initialization
  for (int i = 0; i < 100; ++i) assert(p.tick(30000) == BootHealthPolicy::Action::kNone);
  assert(p.tick(90000) == BootHealthPolicy::Action::kRollback);

  p.reset(0xFFFFFF00u);
  p.setReadiness(true, true, true, true);
  for (int i = 0; i < 99; ++i) assert(p.tick(0x00007530u) == BootHealthPolicy::Action::kNone);
  assert(p.tick(0x00007530u) == BootHealthPolicy::Action::kConfirm);

  p.reset(0);
  p.setReadiness(false, false, false, false);
  assert(p.tick(90000) == BootHealthPolicy::Action::kRollback);
  assert(p.tick(90005) == BootHealthPolicy::Action::kNone);
}
'''
        with tempfile.TemporaryDirectory(prefix="leopold-health-test-") as temporary:
            source_path = Path(temporary) / "health_test.cpp"
            binary_path = Path(temporary) / "health_test"
            source_path.write_text(source)
            subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "device/include"), str(source_path), "-o", str(binary_path)],
                check=True,
            )
            subprocess.run([str(binary_path)], check=True)


if __name__ == "__main__":
    unittest.main()
