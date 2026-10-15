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


def main() -> int:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    subprocess.run([sys.executable, str(ROOT / "scripts" / "prepare_firmware_build.py")], check=True)
    output = Path(os.environ.get("LEOPOLD_BUILD_DIR", str(ROOT / "device" / "build")))
    output.mkdir(parents=True, exist_ok=True)
    properties = [
        item.replace("{root}", str(ROOT)) for item in config["build_properties"]
    ]
    commit = os.environ.get("GITHUB_SHA", "local-uncommitted")
    commit_flag = " -DLEOPOLD_SOURCE_COMMIT=\\\"" + commit + "\\\""
    for index, property_value in enumerate(properties):
        if property_value.startswith("compiler.cpp.extra_flags="):
            properties[index] += commit_flag
            break
    else:
        properties.append("compiler.cpp.extra_flags=" + commit_flag)
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
