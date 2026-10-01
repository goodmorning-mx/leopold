"""Provision the Git-controlled image pack onto a mounted Leopold SD card."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "shared" / "asset-manifest.json"
ASSETS = ROOT / "shared" / "assets"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount_point", type=Path, help="Mounted SD-card directory")
    args = parser.parse_args()
    destination = args.mount_point / "assets"
    destination.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    checksums: dict[str, str] = {}
    for asset in manifest["assets"].values():
        source = ASSETS / asset["file"]
        if not source.is_file():
            raise SystemExit(f"Missing canonical asset: {source}")
        target = destination / asset["file"]
        shutil.copyfile(source, target)
        checksums[asset["file"]] = hashlib.sha256(source.read_bytes()).hexdigest()
    (destination / "manifest.json").write_text(
        json.dumps({"schema_version": 1, "assets": checksums}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Provisioned {len(checksums)} assets to {destination}")


if __name__ == "__main__":
    main()
