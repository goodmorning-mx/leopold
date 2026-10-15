#!/usr/bin/env python3
"""Flash an already-built Leopold GitHub package; never compile firmware."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--port", required=True)
    args = parser.parse_args()
    manifest = json.loads((args.package / "manifest.json").read_text(encoding="utf-8"))
    sums = (args.package / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
    for line in sums:
        digest, name = line.split("  ", 1)
        actual = hashlib.sha256((args.package / name).read_bytes()).hexdigest()
        if actual != digest:
            raise SystemExit(f"checksum mismatch: {name}")
    command = ["python", "-m", "esptool", "--chip", manifest["chip"], "--port", args.port, "write-flash"]
    merged = manifest.get("merged_flash_image")
    if merged:
        command.extend([manifest["merged_flash_offset"], str(args.package / merged)])
    else:
        for name, image in manifest["flash_images"].items():
            if image["flash_offset"] is not None:
                command.extend([image["flash_offset"], str(args.package / name)])
    print("$ " + " ".join(command))
    return subprocess.run(command).returncode


if __name__ == "__main__":
    raise SystemExit(main())
