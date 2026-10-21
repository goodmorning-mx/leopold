#!/usr/bin/env python3
"""Read the revision banner emitted by a flashed Leopold over serial."""

from __future__ import annotations

import argparse
import re
import sys
import time
import json
from pathlib import Path


def parse_revision(line: str) -> dict[str, object]:
    match = re.fullmatch(r"LEOPOLD_FIRMWARE_REVISION commit=([0-9a-f]{40}) ui=(\d+)", line.strip())
    if not match:
        raise ValueError("malformed firmware revision banner")
    return {"commit": match.group(1), "ui_revision": int(match.group(2))}


def verify_stream(port, commit: str, ui_revision: int, timeout: float = 15) -> dict[str, object]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode(errors="replace").strip()
        if line.startswith("LEOPOLD_FIRMWARE_REVISION "):
            revision = parse_revision(line)
            if revision != {"commit": commit, "ui_revision": ui_revision}:
                raise ValueError(f"firmware revision mismatch: {revision}")
            return revision
    raise ValueError("firmware revision banner was not observed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--ui-revision", type=int, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[0-9a-f]{40}", args.commit) or args.ui_revision < 0:
        parser.error("an exact commit and non-negative UI revision are required")
    if args.report and args.report.exists():
        parser.error("revision evidence already exists; use a new report path")
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("install pyserial for revision verification") from exc
    with serial.Serial(args.port, 115200, timeout=2) as port:
        try:
            revision = verify_stream(port, args.commit, args.ui_revision)
        except ValueError as exc:
            print(str(exc), file=sys.stderr)
            return 1
    # Persist only the revision fields, never arbitrary serial/network output.
    if args.report:
        with args.report.open("x", encoding="utf-8") as stream:
            json.dump({"passed": True, "port": args.port, **revision}, stream, indent=2)
            stream.write("\n")
    print(json.dumps({"passed": True, **revision}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
