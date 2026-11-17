#!/usr/bin/env python3
"""Extract only Leopold's bounded boot-health record from an NVS readback."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

try:
    from .vendor.esp_idf_5_4_1_nvs import nvs_parser
except ImportError:  # direct invocation: python scripts/decode_boot_health_diag.py
    from vendor.esp_idf_5_4_1_nvs import nvs_parser


RECORD_V1 = struct.Struct("<IHBBIiiI HBBIIiiB3s")
RECORD = RECORD_V1  # Preserve callers and fixtures that refer to the installed v1 schema.
RECORD_V2 = struct.Struct("<IHH10BbBH4BIIiiIIiIIIIIIIIii8s16s20s")
MAGIC = 0x4C424844
VERSION_V1 = 1
VERSION_V2 = 2
PHASES = {
    1: "boot",
    2: "identity",
    3: "readiness",
    4: "progress",
    5: "confirmation",
    6: "rollback",
    7: "confirmed",
}
INIT_STAGES = {
    0: "unknown",
    1: "display_failed",
    2: "identity_failed",
    3: "local_services_ready",
    4: "cloud_failed",
    5: "setup_ready",
}
ROLLBACK_REASONS = {
    0: "none",
    1: "pending_identity_mismatch",
    2: "local_health_timeout",
    3: "confirmation_api_failed",
}
FAILURE_STAGES = {
    0: "none",
    1: "public_state_lookup",
    2: "attempt_preferences",
    3: "normal_attempt_identity",
    4: "running_partition",
    5: "ota_data_partition",
    6: "ota_data_read",
    7: "no_valid_ota_record",
    8: "ambiguous_ota_sequence",
    9: "selected_record_state",
    10: "running_slot_mapping",
    11: "marker_label",
    12: "marker_source_prefix",
    13: "identity_matched",
    14: "local_health_timeout",
    15: "confirmation_api",
}
SELECTION_STATUSES = {
    0: "not_evaluated",
    1: "no_valid_record",
    2: "sequence_tie",
    3: "highest_valid_sequence",
}
MARKER_STATUSES = {
    0: "not_checked",
    1: "label_mismatch",
    2: "source_prefix_mismatch",
    3: "matched",
}
PERSIST_RESULTS = {
    0: "not_attempted",
    1: "written",
    2: "namespace_open_failed",
    3: "short_write",
}
OTA_STATES = {
    -1: "undefined",
    0: "new",
    1: "pending_verify",
    2: "valid",
    3: "invalid",
    4: "aborted",
    0xFFFFFFFF: "undefined",
}


class DiagnosticError(ValueError):
    pass


def _all_entries(partition: nvs_parser.NVS_Partition):
    for page in partition.pages:
        for entry in page.entries:
            if entry.state == "Written":
                yield entry


def _check_entry_crc(entry) -> None:
    if entry.metadata["crc"]["original"] != entry.metadata["crc"]["computed"]:
        raise DiagnosticError("NVS entry CRC mismatch")


def _get_blob(partition: nvs_parser.NVS_Partition, namespace: int, key: str) -> bytes:
    entries = list(_all_entries(partition))
    indexes = [entry for entry in entries if entry.metadata["namespace"] == namespace
               and entry.key == key and entry.metadata["type"] == "blob_index"]
    if len(indexes) != 1:
        raise DiagnosticError("expected one current diagnostic blob index")
    index = indexes[0]
    _check_entry_crc(index)
    size = index.data["size"]
    count = index.data["chunk_count"]
    start = index.data["chunk_start"]
    chunks = []
    for wanted in ((start + offset) & 0xFF for offset in range(count)):
        matches = [entry for entry in entries if entry.metadata["namespace"] == namespace
                   and entry.key == key and entry.metadata["type"] == "blob_data"
                   and entry.metadata["chunk_index"] == wanted]
        if len(matches) != 1:
            raise DiagnosticError("diagnostic blob chunk is missing or ambiguous")
        entry = matches[0]
        _check_entry_crc(entry)
        if entry.metadata["crc"]["data_original"] != entry.metadata["crc"]["data_computed"]:
            raise DiagnosticError("diagnostic blob data CRC mismatch")
        chunk = b"".join(bytes(child.raw) for child in entry.children)
        chunks.append(chunk[:entry.data["size"]])
    result = b"".join(chunks)
    if len(result) != size:
        raise DiagnosticError("diagnostic blob size mismatch")
    return result


def decode_record_blob(blob: bytes) -> dict[str, object]:
    if len(blob) < 8:
        raise DiagnosticError("diagnostic record header is truncated")
    magic = struct.unpack_from("<I", blob)[0]
    version = struct.unpack_from("<H", blob, 4)[0]
    if magic != MAGIC:
        raise DiagnosticError("diagnostic record magic is invalid")
    if version == VERSION_V1:
        return _decode_v1(blob)
    if version == VERSION_V2:
        return _decode_v2(blob)
    raise DiagnosticError("diagnostic record schema version is unsupported")


def _decode_v1(blob: bytes) -> dict[str, object]:
    if len(blob) != RECORD_V1.size:
        raise DiagnosticError("v1 diagnostic record has an unsupported size")
    (magic, version, phase, init_stage, running_address, state_result, image_state,
     attempted_address, identity_flags, readiness_flags, _reserved, elapsed_ms,
     loop_count, confirmation_result, rollback_api_result, rollback_reason,
     _padding) = RECORD_V1.unpack(blob)
    if magic != MAGIC or version != VERSION_V1 or phase not in PHASES:
        raise DiagnosticError("v1 diagnostic record fields are invalid")
    if init_stage not in INIT_STAGES or rollback_reason not in ROLLBACK_REASONS:
        raise DiagnosticError("v1 diagnostic record enum value is invalid")
    return {
        "schema_version": VERSION_V1,
        "phase": PHASES[phase],
        "init_stage": INIT_STAGES[init_stage],
        "running_address": running_address,
        "state_result": state_result,
        "image_state": image_state,
        "attempted_address": attempted_address,
        "identity_flags": identity_flags,
        "readiness_flags": readiness_flags,
        "elapsed_ms": elapsed_ms,
        "loop_count": loop_count,
        "confirmation_result": _optional_result(confirmation_result),
        "rollback_api_result": _optional_result(rollback_api_result),
        "rollback_reason": ROLLBACK_REASONS[rollback_reason],
        "raw_ota_records": None,
        "freshness": "unbound_v1_record",
    }


def _optional_result(value: int) -> int | None:
    return None if value == -0x80000000 else value


def _decode_v2(blob: bytes) -> dict[str, object]:
    if len(blob) != RECORD_V2.size:
        raise DiagnosticError("v2 diagnostic record is truncated or has an unsupported size")
    values = RECORD_V2.unpack(blob)
    (magic, version, record_length, phase, init_stage, failure_stage, rollback_reason,
     state_valid, attempted_address_valid, running_valid, ota_found, read_mask,
     crc_valid_mask, selected_index, selection_status, identity_flags, readiness_flags,
     previous_write_result, marker_status, build_identity_flags, running_address,
     running_subtype, state_result, image_state, attempted_address, ota_data_address,
     ota_read_result, seq0, seq1, state0, state1, crc0, crc1, elapsed_ms, loop_count,
     confirmation_result, rollback_api_result, commit_prefix, elf_prefix,
     selected_label) = values
    if magic != MAGIC or version != VERSION_V2 or record_length != len(blob):
        raise DiagnosticError("v2 diagnostic record header or declared length is invalid")
    if phase not in (1, 2, 3, 4, 5, 6, 7, 8) or init_stage not in INIT_STAGES:
        raise DiagnosticError("v2 diagnostic record phase or init stage is invalid")
    if failure_stage not in FAILURE_STAGES or rollback_reason not in ROLLBACK_REASONS:
        raise DiagnosticError("v2 diagnostic record reason enum is invalid")
    if selection_status not in SELECTION_STATUSES or marker_status not in MARKER_STATUSES:
        raise DiagnosticError("v2 diagnostic record selection or marker enum is invalid")
    if previous_write_result not in PERSIST_RESULTS:
        raise DiagnosticError("v2 diagnostic record persistence enum is invalid")
    if any(value not in (0, 1) for value in
           (state_valid, attempted_address_valid, running_valid, ota_found)):
        raise DiagnosticError("v2 diagnostic record validity flag is invalid")
    if selected_index not in (-1, 0, 1) or read_mask & ~0x03 or crc_valid_mask & ~read_mask:
        raise DiagnosticError("v2 diagnostic record record masks or selection index are invalid")
    records = []
    for index, (sequence, state, crc) in enumerate(((seq0, state0, crc0), (seq1, state1, crc1))):
        read = bool(read_mask & (1 << index))
        records.append({
            "read": read,
            "sequence": sequence if read else None,
            "state_raw": state if read else None,
            "state": OTA_STATES.get(state, "unknown") if read else "unavailable",
            "crc": crc if read else None,
            "crc_valid": bool(crc_valid_mask & (1 << index)) if read else None,
        })
    selected = records[selected_index] if selected_index >= 0 else None
    return {
        "schema_version": VERSION_V2,
        "record_length": record_length,
        "phase": {1: "boot", 2: "identity", 3: "readiness", 4: "progress", 5: "confirmation",
                  6: "rollback_intent", 7: "confirmed", 8: "rollback_api_returned"}[phase],
        "init_stage": INIT_STAGES[init_stage],
        "failure_stage": FAILURE_STAGES[failure_stage],
        "rollback_reason": ROLLBACK_REASONS[rollback_reason],
        "running_partition": {
            "available": bool(running_valid),
            "address": running_address if running_valid else None,
            "subtype": running_subtype if running_valid else None,
        },
        "public_ota_state": {
            "available": bool(state_valid),
            "result": state_result,
            "state_raw": image_state if state_valid else None,
            "state": OTA_STATES.get(image_state, "unknown") if state_valid else "unavailable",
        },
        "attempted_address": attempted_address if attempted_address_valid else None,
        "identity_flags": identity_flags,
        "readiness_flags": readiness_flags,
        "ota_data_partition": {
            "found": bool(ota_found),
            "address": ota_data_address if ota_found else None,
            "read_result": _optional_result(ota_read_result),
            "records_read_mask": read_mask,
            "crc_valid_mask": crc_valid_mask,
        },
        "selection": {
            "status": SELECTION_STATUSES[selection_status],
            "record_index": selected_index if selected_index >= 0 else None,
            "record": selected,
        },
        "ota_records": records,
        "identity_marker": {
            "status": MARKER_STATUSES[marker_status],
            "selected_label_hex": selected_label.hex() if selected_index >= 0 else None,
        },
        "build_identity": {
            "source_commit_prefix": commit_prefix.hex() if build_identity_flags & 0x01 else None,
            "app_elf_sha256_prefix": elf_prefix.hex() if build_identity_flags & 0x02 else None,
        },
        "previous_persistence_result": PERSIST_RESULTS[previous_write_result],
        "elapsed_ms": elapsed_ms,
        "loop_count": loop_count,
        "confirmation_result": _optional_result(confirmation_result),
        "rollback_api_result": _optional_result(rollback_api_result),
        "rollback_completion": "unknown_until_later_boot_observation",
    }


def extract_record(raw_nvs: bytes) -> dict[str, object]:
    try:
        partition = nvs_parser.NVS_Partition("nvs-readback", bytearray(raw_nvs))
    except (nvs_parser.NotAlignedError, IndexError, ValueError) as error:
        raise DiagnosticError("NVS partition read is incomplete or malformed") from error
    namespace_entries = [entry for entry in _all_entries(partition)
                         if entry.metadata["namespace"] == 0 and entry.key == "boot-health"]
    if len(namespace_entries) != 1:
        raise DiagnosticError("boot-health namespace is missing or ambiguous")
    namespace = namespace_entries[0]
    _check_entry_crc(namespace)
    namespace_index = namespace.data["value"]
    blob = _get_blob(partition, namespace_index, "last")
    return decode_record_blob(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nvs_image", type=Path, help="private read-only NVS partition capture")
    args = parser.parse_args()
    try:
        record = extract_record(args.nvs_image.read_bytes())
    except (OSError, DiagnosticError, nvs_parser.NotAlignedError) as error:
        print(f"boot-health diagnostic unavailable: {error}", file=sys.stderr)
        return 2
    print(json.dumps(record, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
