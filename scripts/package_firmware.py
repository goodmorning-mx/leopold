#!/usr/bin/env python3
"""Package Arduino CLI ESP32 images with a reproducible flash manifest."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "device" / "firmware-build.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
        "boot_app0.bin": build_dir / "boot_app0.bin",
        "merged-flash.bin": merged,
    }
    files = {}
    for name, path in image_sources.items():
        if path is None or not path.exists():
            if name == "boot_app0.bin":
                continue
            raise SystemExit(f"Arduino CLI did not produce required image: {path}")
        target = package_dir / name
        shutil.copyfile(path, target)
        files[name] = {"bytes": target.stat().st_size, "sha256": sha256(target)}

    ui = json.loads((ROOT / "shared" / "firmware-ui.json").read_text(encoding="utf-8"))
    assets = json.loads((ROOT / "shared" / "asset-manifest.json").read_text(encoding="utf-8"))
    commit = os.environ.get("GITHUB_SHA", "local-uncommitted")
    manifest = {
        "schema_version": 1,
        "product": "Leopold",
        "chip": config["flash"]["chip"],
        "board_fqbn": config["fqbn"],
        "arduino_cli_profile": config["profile"],
        "arduino_cli_version": os.environ.get("ARDUINO_CLI_VERSION", "unknown"),
        "esp32_core": config["core"],
        "libraries": config["libraries"],
        "build_properties": config["build_properties"],
        "source_commit": commit,
        "ui_revision": ui.get("revision"),
        "ui_release": ui.get("firmware_release"),
        "asset_manifest_sha256": hashlib.sha256(
            json.dumps(assets, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest(),
        "flash_images": {
            name: {**details, "flash_offset": config["flash"]["images"].get(name)}
            for name, details in files.items()
            if name != "merged-flash.bin"
        },
        "merged_flash_image": "merged-flash.bin" if "merged-flash.bin" in files else None,
        "merged_flash_offset": "0x0000" if "merged-flash.bin" in files else None,
        "flash_command": "python scripts/flash_firmware.py --package . --port PORT",
        "verify_command": "python scripts/verify_firmware.py --port PORT --commit " + commit,
    }
    (package_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksum_files = sorted(
        path for path in package_dir.iterdir() if path.name != "SHA256SUMS.txt"
    )
    (package_dir / "SHA256SUMS.txt").write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in checksum_files if path.is_file()),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
