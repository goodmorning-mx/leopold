#!/usr/bin/env python3
"""Safely flash a self-contained Leopold firmware artifact."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
from datetime import datetime, timezone
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 3
PRODUCT = "Leopold"
CHIP = "esp32s3"
FLASH_BYTES = 16 * 1024 * 1024
FLASH_SIZE = "0x1000000"
UPDATE_NAMES = ("bootloader.bin", "partitions.bin", "firmware.bin")
APPROVED_OFFSETS = {"bootloader.bin": 0x0000, "partitions.bin": 0x8000, "firmware.bin": 0x10000}
IMAGE_NAMES = frozenset((*UPDATE_NAMES, "merged-flash.bin"))
PACKAGED_HELPERS = {"flash_firmware.py", "verify_firmware.py", "capture_device_screenshot.py",
                   "ppm_to_png.py", "verify_firmware_package.py", "detect_esp32.py"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, (int, str)):
        raise SystemExit(f"{label} must be a numeric offset")
    try:
        result = int(value, 0) if isinstance(value, str) else value
    except ValueError as exc:
        raise SystemExit(f"{label} is not a valid numeric offset: {value!r}") from exc
    if result < 0:
        raise SystemExit(f"{label} must not be negative")
    return result


def safe_filename(name: Any, label: str) -> str:
    if not isinstance(name, str) or not name or name != PurePosixPath(name).name:
        raise SystemExit(f"unsafe {label} filename: {name!r}")
    if "\\" in name or name in {".", ".."} or ".." in PurePosixPath(name).parts:
        raise SystemExit(f"unsafe {label} filename: {name!r}")
    return name


def validate_ranges(entries: list[dict[str, Any]], label: str) -> None:
    ranges: list[tuple[int, int, str]] = []
    for entry in entries:
        start = entry["offset"]
        end = entry["end_address"]
        ranges.append((start, end, entry["name"]))
    for index, (start, end, name) in enumerate(ranges):
        for other_start, other_end, other_name in ranges[index + 1 :]:
            if start < other_end and other_start < end:
                raise SystemExit(f"overlapping {label} images: {name} and {other_name}")


def validate_manifest(manifest: dict[str, Any], package: Path, checksums: dict[str, str]) -> None:
    if not isinstance(manifest, dict):
        raise SystemExit("manifest must be an object")
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise SystemExit(f"unsupported manifest schema_version: {manifest.get('schema_version')!r}")
    if manifest.get("product") != PRODUCT:
        raise SystemExit(f"unexpected manifest product: {manifest.get('product')!r}")
    if manifest.get("chip") != CHIP:
        raise SystemExit(f"unexpected chip: {manifest.get('chip')!r}")
    if manifest.get("flash_size_bytes") != FLASH_BYTES or manifest.get("flash_size") != "16MB":
        raise SystemExit("manifest must declare a 16 MiB flash")
    if not re.fullmatch(r"[0-9a-f]{40}", str(manifest.get("source_commit", ""))):
        raise SystemExit("manifest requires an exact source commit")
    if type(manifest.get("ui_revision")) is not int or manifest["ui_revision"] < 0:
        raise SystemExit("manifest requires a non-negative UI revision")
    for section in ("normal_update", "factory_reset", "preserved_data_regions", "commands", "helpers"):
        if section not in manifest:
            raise SystemExit(f"manifest is missing required section: {section}")
    helpers = manifest["helpers"]
    if not isinstance(helpers, list) or len(helpers) != len(PACKAGED_HELPERS) or set(helpers) != PACKAGED_HELPERS:
        raise SystemExit("manifest must contain the complete schema-3 helper set")
    for helper in helpers:
        name = safe_filename(helper, "helper")
        if name != helper or not (package / name).is_file():
            raise SystemExit(f"missing packaged helper: {helper}")
    commands = manifest["commands"]
    required_commands = {"update_command", "backup_command", "factory_reset_command", "verify_command", "calibration_commands"}
    if not isinstance(commands, dict) or not required_commands.issubset(commands):
        raise SystemExit("manifest commands are incomplete")
    if not isinstance(commands["calibration_commands"], list) or not commands["calibration_commands"]:
        raise SystemExit("manifest calibration_commands must be a non-empty list")

    images = manifest.get("images")
    if not isinstance(images, dict) or set(images) != IMAGE_NAMES:
        raise SystemExit("manifest images must contain exactly the four approved image names")
    entries: dict[str, dict[str, Any]] = {}
    for expected_name, image in images.items():
        name = safe_filename(expected_name, "image")
        if name != expected_name or not isinstance(image, dict):
            raise SystemExit(f"invalid image entry: {expected_name!r}")
        if image.get("filename") != name:
            raise SystemExit(f"image filename mismatch: {name}")
        path = package / name
        if not path.is_file():
            raise SystemExit(f"missing image: {name}")
        declared_bytes = image.get("bytes")
        if type(declared_bytes) is not int or declared_bytes <= 0 or declared_bytes != path.stat().st_size:
            raise SystemExit(f"image size mismatch: {name}")
        digest = image.get("sha256")
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise SystemExit(f"invalid image SHA-256: {name}")
        if digest != sha256(path) or checksums.get(name) != digest:
            raise SystemExit(f"image checksum mismatch: {name}")
        offset = parse_int(image.get("flash_offset"), f"{name} flash_offset")
        end = parse_int(image.get("end_address"), f"{name} end_address")
        if end != offset + declared_bytes or end > FLASH_BYTES:
            raise SystemExit(f"image range is invalid or out of flash: {name}")
        entries[name] = {"name": name, "offset": offset, "end_address": end, "bytes": declared_bytes}

    update = manifest["normal_update"]
    if not isinstance(update, dict) or update.get("destructive") is not False:
        raise SystemExit("normal_update must be explicitly non-destructive")
    update_images = update.get("images")
    if not isinstance(update_images, list) or [item.get("name") for item in update_images if isinstance(item, dict)] != list(UPDATE_NAMES):
        raise SystemExit("normal_update images must be bootloader, partitions, firmware")
    update_entries: list[dict[str, Any]] = []
    for item, expected_name in zip(update_images, UPDATE_NAMES):
        if not isinstance(item, dict) or item.get("name") != expected_name:
            raise SystemExit("normal_update contains an invalid image")
        declared_offset = parse_int(item.get("flash_offset"), f"{expected_name} update flash_offset")
        if declared_offset != entries[expected_name]["offset"] or declared_offset != APPROVED_OFFSETS[expected_name]:
            raise SystemExit(f"unapproved normal-update offset for {expected_name}")
        update_entries.append(entries[expected_name])
    if update.get("verify_partition_table") is not True:
        raise SystemExit("normal_update must require partition-table verification")
    if update.get("requires_verified_full_flash_backup") is not True:
        raise SystemExit("normal_update must require a fresh verified full-flash backup")
    validate_ranges(update_entries, "normal update")

    factory = manifest["factory_reset"]
    if not isinstance(factory, dict) or factory.get("destructive") is not True or factory.get("requires_explicit_flag") is not True:
        raise SystemExit("factory_reset must be explicitly destructive and flag-gated")
    if factory.get("image") != "merged-flash.bin" or entries["merged-flash.bin"]["offset"] != 0:
        raise SystemExit("factory_reset must use merged-flash.bin at 0x0000")
    if factory.get("requires_verified_full_flash_backup") is not True:
        raise SystemExit("factory_reset must require a verified full-flash backup")
    if entries["merged-flash.bin"]["bytes"] != FLASH_BYTES:
        raise SystemExit("merged-flash.bin must be exactly 16 MiB")
    regions = manifest["preserved_data_regions"]
    if not isinstance(regions, list) or not regions:
        raise SystemExit("preserved_data_regions must be a non-empty list")
    for region in regions:
        if not isinstance(region, dict) or not isinstance(region.get("name"), str):
            raise SystemExit("invalid preserved data region")
        start = parse_int(region.get("start"), f"{region['name']} start")
        end = parse_int(region.get("end"), f"{region['name']} end")
        if start >= end or end > FLASH_BYTES or region.get("preserved") is not True:
            raise SystemExit(f"invalid preserved data region: {region.get('name')}")
        for entry in update_entries:
            # esptool erases complete 4 KiB sectors before writing.
            erase_start = entry["offset"] // 4096 * 4096
            erase_end = (entry["end_address"] + 4095) // 4096 * 4096
            if erase_start < end and start < erase_end:
                raise SystemExit(f"normal update overlaps preserved region: {region['name']}")


def load_package(package: Path) -> dict[str, Any]:
    manifest_path = package / "manifest.json"
    sums_path = package / "SHA256SUMS.txt"
    if not manifest_path.is_file() or not sums_path.is_file():
        raise SystemExit("package must contain manifest.json and SHA256SUMS.txt")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"invalid manifest.json: {exc}") from exc
    if not isinstance(manifest, dict):
        raise SystemExit("manifest must be an object")
    checksums: dict[str, str] = {}
    for raw_line in sums_path.read_text(encoding="utf-8").splitlines():
        if not raw_line.strip():
            continue
        try:
            digest, raw_name = raw_line.split("  ", 1)
        except ValueError as exc:
            raise SystemExit(f"invalid checksum line: {raw_line!r}") from exc
        name = safe_filename(raw_name, "checksum")
        if name in checksums:
            raise SystemExit(f"duplicate checksum entry: {name}")
        if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise SystemExit(f"invalid checksum digest: {name}")
        path = package / name
        if path.is_symlink() or not path.is_file() or sha256(path) != digest:
            raise SystemExit(f"checksum mismatch: {name}")
        checksums[name] = digest
    if "manifest.json" not in checksums:
        raise SystemExit("missing checksum entry: manifest.json")
    image_names = set(manifest.get("images", {})) if isinstance(manifest.get("images"), dict) else set()
    required = image_names | {"manifest.json"} | set(manifest.get("helpers", []))
    if not required.issubset(checksums):
        raise SystemExit(f"checksum coverage is incomplete: {sorted(required - set(checksums))}")
    listed_bins = {name for name in checksums if name.endswith(".bin")}
    if listed_bins != image_names:
        raise SystemExit("checksum file must cover exactly every packaged binary image")
    validate_manifest(manifest, package, checksums)
    return manifest


def command_for(manifest: dict[str, Any], port: str, *parts: str) -> list[str]:
    return [sys.executable, "-m", "esptool", "--chip", manifest["chip"], "--port", port, *parts]


def command_text(command: list[str]) -> str:
    return " ".join(command)


def image_command(manifest: dict[str, Any], package: Path, port: str) -> list[str]:
    parts: list[str] = ["write-flash"]
    for item in manifest["normal_update"]["images"]:
        image = manifest["images"][item["name"]]
        parts.extend((item["flash_offset"], str(package / image["filename"])))
    return command_for(manifest, port, *parts)


def backup_command(manifest: dict[str, Any], port: str, destination: Path) -> list[str]:
    return command_for(manifest, port, "read-flash", "0x000000", FLASH_SIZE, str(destination))


def partition_read_command(manifest: dict[str, Any], port: str, destination: Path) -> list[str]:
    table = manifest["images"]["partitions.bin"]
    offset = next(item["flash_offset"] for item in manifest["normal_update"]["images"] if item["name"] == "partitions.bin")
    return command_for(manifest, port, "read-flash", offset, hex(table["bytes"]), str(destination))


def verify_backup(path: Path) -> str:
    if not path.is_file() or path.stat().st_size != FLASH_BYTES:
        raise SystemExit(f"backup must be exactly {FLASH_BYTES} bytes: {path}")
    digest = sha256(path)
    write_private(path.with_name(path.name + ".sha256"), f"{digest}  {path.name}\n")
    return digest


def write_private(path: Path, text: str = "") -> None:
    """Exclusive files: never overwrite an earlier backup or its evidence."""
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as exc:
        raise SystemExit(f"refusing to overwrite existing backup/evidence: {path}") from exc
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        stream.write(text)


def reserve_backup(path: Path) -> None:
    for candidate in (path, path.with_name(path.name + ".sha256"), path.with_name(path.name + ".json")):
        if candidate.exists() or candidate.is_symlink():
            raise SystemExit(f"refusing to overwrite existing backup/evidence: {candidate}")
    path.parent.mkdir(parents=True, exist_ok=True)
    write_private(path)


def probe_device(manifest: dict[str, Any], port: str) -> dict[str, Any]:
    result = subprocess.run(command_for(manifest, port, "flash-id"), capture_output=True, text=True)
    if result.returncode:
        raise SystemExit("device identification failed; no write attempted")
    size = re.search(r"Detected flash size:\s*(\S+)", result.stdout)
    mac = re.search(r"\bMAC:\s*((?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})\b", result.stdout)
    if not size or size.group(1) != "16MB" or not mac:
        raise SystemExit("expected an identifiable ESP32-S3 with 16 MiB flash; no write attempted")
    return {"chip": manifest["chip"], "flash_size_bytes": FLASH_BYTES, "mac": mac.group(1).lower()}


def create_backup(manifest: dict[str, Any], port: str, path: Path, device: dict[str, Any]) -> int:
    reserve_backup(path)
    result = subprocess.run(backup_command(manifest, port, path))
    if result.returncode:
        return result.returncode
    digest = verify_backup(path)
    write_private(path.with_name(path.name + ".json"), json.dumps({
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "port": port, "device": device, "bytes": FLASH_BYTES, "sha256": digest,
        "package_source_commit": manifest.get("source_commit"),
        "package_ui_revision": manifest.get("ui_revision"),
    }, indent=2) + "\n")
    print(f"verified fresh backup: {path} sha256={digest}")
    return 0


def require_unchanged_backup(path: Path) -> None:
    receipt = json.loads(path.with_name(path.name + ".json").read_text())
    sidecar = path.with_name(path.name + ".sha256").read_text()
    if (path.is_symlink() or path.stat().st_size != FLASH_BYTES or sha256(path) != receipt["sha256"]
            or sidecar != f"{receipt['sha256']}  {path.name}\n"):
        raise SystemExit("backup changed after verification; refusing to write")


def print_update_plan(manifest: dict[str, Any], package: Path, port: str) -> None:
    print("mode: non-destructive firmware update")
    print("writes:")
    for item in manifest["normal_update"]["images"]:
        image = manifest["images"][item["name"]]
        print(f"  {item['name']}: {item['flash_offset']}–{hex(parse_int(image['end_address'], item['name'] + ' end_address'))}")
    print("partition check: the installed partition table will be read and compared before any write")
    print("backup check: a fresh private 16 MiB backup and SHA-256 evidence are mandatory before any write")
    print("preserves: declared data regions only, after partition-table equality is verified")
    print("$ " + command_text(image_command(manifest, package, port)))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--port", required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--factory-reset", action="store_true")
    parser.add_argument("--backup-only", action="store_true")
    parser.add_argument("--backup", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.backup_only and args.factory_reset:
        raise SystemExit("--backup-only and --factory-reset cannot be combined")
    if args.backup_only and args.package is not None:
        raise SystemExit("--package is not valid with --backup-only")
    if args.backup_only:
        manifest = {"chip": CHIP}
    else:
        if args.package is None:
            raise SystemExit("--package is required for firmware update modes")
        manifest = load_package(args.package)
    if args.backup is None:
        raise SystemExit("--backup is required; every update creates a fresh verified full-flash backup")
    for candidate in (args.backup, args.backup.with_name(args.backup.name + ".sha256"), args.backup.with_name(args.backup.name + ".json")):
        if candidate.exists() or candidate.is_symlink():
            raise SystemExit(f"refusing to overwrite existing backup/evidence: {candidate}")

    if args.backup_only:
        command = backup_command(manifest, args.port, args.backup)
        print("mode: full-flash backup only")
        print(f"writes: none; reads 0x000000–0x1000000 ({FLASH_BYTES} bytes)")
        print("preserves: device contents (read-only operation)")
        print("$ " + command_text(command))
        if args.dry_run:
            return 0
        return create_backup(manifest, args.port, args.backup, probe_device(manifest, args.port))

    if args.factory_reset:
        image = manifest["images"]["merged-flash.bin"]
        read_command = backup_command(manifest, args.port, args.backup)
        flash_command = command_for(manifest, args.port, "write-flash", "0x000000", str(args.package / image["filename"]))
        print("mode: explicit factory reset")
        print(f"backup reads: 0x000000–0x1000000 ({FLASH_BYTES} bytes)")
        print("writes: 0x000000–0x1000000 merged-flash.bin")
        print("preserves: no; NVS and all data partitions may be replaced")
        print("$ " + command_text(read_command))
        print("$ " + command_text(flash_command))
        if args.dry_run:
            return 0
        device = probe_device(manifest, args.port)
        result = create_backup(manifest, args.port, args.backup, device)
        if result:
            return result
        if probe_device(manifest, args.port) != device:
            raise SystemExit("device identity changed after backup; refusing to write")
        require_unchanged_backup(args.backup)
        return subprocess.run(flash_command).returncode

    print_update_plan(manifest, args.package, args.port)
    if args.dry_run:
        return 0
    device = probe_device(manifest, args.port)
    result = create_backup(manifest, args.port, args.backup, device)
    if result:
        return result
    with tempfile.NamedTemporaryFile(prefix="leopold-partitions-", suffix=".bin", delete=False) as temporary:
        installed_path = Path(temporary.name)
    try:
        read_command = partition_read_command(manifest, args.port, installed_path)
        print("$ " + command_text(read_command))
        result = subprocess.run(read_command)
        if result.returncode:
            return result.returncode
        packaged_path = args.package / manifest["images"]["partitions.bin"]["filename"]
        if installed_path.read_bytes() != packaged_path.read_bytes():
            print("partition-table mismatch: refusing to write; authorized partition migration or factory reset is required", file=sys.stderr)
            return 2
        if probe_device(manifest, args.port) != device:
            raise SystemExit("device identity changed after backup; refusing to write")
        require_unchanged_backup(args.backup)
        return subprocess.run(image_command(manifest, args.package, args.port)).returncode
    finally:
        installed_path.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
