#!/usr/bin/env python3
"""Build Leopold with the repository-owned Arduino CLI profile."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "device" / "firmware-build.json"


def resolve_qrcode_paths() -> tuple[str, str]:
    """Install and explicitly select the pinned QRCode library sources."""
    subprocess.run(["arduino-cli", "lib", "install", "QRCode@0.0.1"], check=True)
    config = subprocess.run(
        ["arduino-cli", "config", "dump", "--format", "json"],
        check=True,
        capture_output=True,
        text=True,
    )
    directories = json.loads(config.stdout).get("directories", {})
    user_directory = Path(
        os.path.expandvars(
            os.path.expanduser(
                directories.get("user", str(Path.home() / "Arduino"))
            )
        )
    )
    roots = (
        user_directory / "libraries" / "QRCode",
        user_directory / "QRCode",
        Path.home() / "Arduino" / "libraries" / "QRCode",
        Path.home() / "arduino" / "libraries" / "QRCode",
    )
    for root in roots:
        include = root / "src"
        source = include / "qrcode.c"
        if (include / "qrcode.h").is_file() and source.is_file():
            return str(include), str(source)
    raise RuntimeError("Pinned QRCode library source directory was not found")


def main() -> int:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    subprocess.run([sys.executable, str(ROOT / "scripts" / "prepare_firmware_build.py")], check=True)
    output = Path(os.environ.get("LEOPOLD_BUILD_DIR", str(ROOT / "device" / "build")))
    output.mkdir(parents=True, exist_ok=True)
    properties = [
        item.replace("{root}", str(ROOT)) for item in config["build_properties"]
    ]
    commit = os.environ.get("GITHUB_SHA", "local-uncommitted")
    commit_flag = ' -DLEOPOLD_SOURCE_COMMIT="' + commit + '"'
    qrcode_include_path, qrcode_source_path = resolve_qrcode_paths()
    qrcode_include = " -I" + qrcode_include_path
    qrcode_source = ' -DLEOPOLD_QRCODE_SOURCE="' + qrcode_source_path + '"'
    for index, property_value in enumerate(properties):
        if property_value.startswith("compiler.cpp.extra_flags="):
            properties[index] += qrcode_include + qrcode_source + commit_flag
        elif property_value.startswith("compiler.c.extra_flags="):
            properties[index] += qrcode_include
    if not any(property_value.startswith("compiler.cpp.extra_flags=") for property_value in properties):
        properties.append("compiler.cpp.extra_flags=" + qrcode_include + qrcode_source + commit_flag)
    command = [
        "arduino-cli",
        "compile",
        "--profile",
        config["profile"],
        "--output-dir",
        str(output),
    ]
    for prop in properties:
        command.extend(["--build-property", prop])
    command.append(str(ROOT / config["sketch"]))
    print("$ " + " ".join(command))
    return subprocess.run(command, cwd=ROOT).returncode


if __name__ == "__main__":
    raise SystemExit(main())
