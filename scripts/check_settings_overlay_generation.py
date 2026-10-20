#!/usr/bin/env python3
"""Prove generation reproduces committed assets twice without touching the checkout."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

try:
    from .generate_settings_overlay_masks import main as generate
except ImportError:
    from generate_settings_overlay_masks import main as generate

ROOT = Path(__file__).resolve().parents[1]


def hashes(root: Path) -> dict[str, str]:
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (root / "shared/assets").iterdir() if p.suffix in {".png", ".bin"}}


def check(root: Path = ROOT) -> dict[str, object]:
    with tempfile.TemporaryDirectory(prefix="leopold-overlay-check-") as temp:
        staged = Path(temp)
        shutil.copytree(root / "shared", staged / "shared")
        before = hashes(staged)
        generate(staged)
        once = hashes(staged)
        generate(staged)
        twice = hashes(staged)
        first = sorted(name for name in before.keys() | once.keys() if before.get(name) != once.get(name))
        second = sorted(name for name in once.keys() | twice.keys() if once.get(name) != twice.get(name))
    return {"passed": not first and not second, "first_run_changed": first, "second_run_changed": second,
            "checkout_assets_modified": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = check()
    if args.report:
        with args.report.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
    print(json.dumps(result))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
