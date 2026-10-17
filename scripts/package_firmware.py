#!/usr/bin/env python3
"""Package Arduino CLI ESP32 images and self-contained flashing tools."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "device" / "firmware-build.json"
HELPERS = (
    "flash_firmware.py",
    "verify_firmware.py",
    "capture_device_screenshot.py",
    "ppm_to_png.py",
)
UPDATE_NAMES = ("bootloader.bin", "partitions.bin", "firmware.bin")
FLASH_BYTES = 16 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_record(path: Path, offset: str) -> dict[str, object]:
    size = path.stat().st_size
    numeric_offset = int(offset, 0)
    return {
        "filename": path.name,
        "bytes": size,
        "sha256": sha256(path),
        "flash_offset": offset,
        "end_address": hex(numeric_offset + size),
    }


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: package_firmware.py BUILD_DIR PACKAGE_DIR")
    build_dir, package_dir = map(Path, sys.argv[1:])
    package_dir.mkdir(parents=True, exist_ok=True)
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    binaries = sorted(build_dir.glob("*.bin"))
    source_candidates = [
        path for path in binaries
        if not any(path.name.endswith(suffix) for suffix in (".bootloader.bin", ".partitions.bin", ".merged.bin"))
        and path.name != "boot_app0.bin"
    ]
    if len(source_candidates) != 1:
        raise SystemExit(f"expected one application binary, found: {source_candidates}")
    source = source_candidates[0]
    bootloader = next((path for path in binaries if path.name.endswith(".bootloader.bin")), None)
    partitions = next((path for path in binaries if path.name.endswith(".partitions.bin")), None)
    merged = next((path for path in binaries if path.name.endswith(".merged.bin")), None)
    image_sources = {
        "firmware.bin": source,
        "bootloader.bin": bootloader,
        "partitions.bin": partitions,
        "merged-flash.bin": merged,
    }
    copied_images: dict[str, dict[str, object]] = {}
    for name, path in image_sources.items():
        if path is None or not path.exists():
            raise SystemExit(f"Arduino CLI did not produce required image: {path}")
        target = package_dir / name
        shutil.copyfile(path, target)
        copied_images[name] = image_record(target, "0x0000" if name == "merged-flash.bin" else config["flash"]["images"][name])
    if copied_images["merged-flash.bin"]["bytes"] != FLASH_BYTES:
        raise SystemExit("merged image must be exactly 16 MiB")

    for helper in HELPERS:
        source_helper = ROOT / "scripts" / helper
        if not source_helper.is_file():
            raise SystemExit(f"missing package helper: {source_helper}")
        shutil.copyfile(source_helper, package_dir / helper)

    ui = json.loads((ROOT / "shared" / "firmware-ui.json").read_text(encoding="utf-8"))
    assets = json.loads((ROOT / "shared" / "asset-manifest.json").read_text(encoding="utf-8"))
    commit = os.environ.get("GITHUB_SHA", "local-uncommitted")
    preserved_regions = [
        {"name": "nvs", "start": "0x9000", "end": "0xE000", "preserved": True},
        {"name": "otadata", "start": "0xE000", "end": "0x10000", "preserved": True},
        {"name": "app1", "start": "0x650000", "end": "0xC90000", "preserved": True},
        {"name": "spiffs", "start": "0xC90000", "end": "0xFF0000", "preserved": True},
        {"name": "coredump", "start": "0xFF0000", "end": "0x1000000", "preserved": True},
    ]
    manifest = {
        "schema_version": 2,
        "product": "Leopold",
        "chip": config["flash"]["chip"],
        "flash_size": "16MB",
        "flash_size_bytes": FLASH_BYTES,
        "board_fqbn": config["fqbn"],
        "arduino_cli_profile": config["profile"],
        "arduino_cli_version": os.environ.get("ARDUINO_CLI_VERSION", "unknown"),
        "esp32_core": config["core"],
        "libraries": config["libraries"],
        "build_properties": config["build_properties"],
        "source_commit": commit,
        "ui_revision": ui.get("revision"),
        "ui_release": ui.get("firmware_release"),
        "asset_manifest_sha256": hashlib.sha256(json.dumps(assets, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
        "images": copied_images,
        "normal_update": {
            "images": [
                {"name": name, "flash_offset": config["flash"]["images"][name]}
                for name in UPDATE_NAMES
            ],
            "destructive": False,
            "verify_partition_table": True,
        },
        "factory_reset": {
            "image": "merged-flash.bin",
            "destructive": True,
            "requires_explicit_flag": True,
            "requires_verified_full_flash_backup": True,
        },
        "preserved_data_regions": preserved_regions,
        "helpers": list(HELPERS),
        "commands": {
            "update_command": "python3 flash_firmware.py --package . --port PORT",
            "backup_command": "python3 flash_firmware.py --backup-only --port PORT --backup BACKUP",
            "factory_reset_command": "python3 flash_firmware.py --package . --port PORT --factory-reset --backup BACKUP",
            "verify_command": "python3 verify_firmware.py --port PORT --commit " + commit,
            "calibration_commands": [
                "python3 capture_device_screenshot.py PORT calibration.ppm",
                "python3 ppm_to_png.py calibration.ppm calibration.png",
            ],
        },
    }
    (package_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksum_files = sorted(path for path in package_dir.iterdir() if path.name != "SHA256SUMS.txt")
    (package_dir / "SHA256SUMS.txt").write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in checksum_files if path.is_file()),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
