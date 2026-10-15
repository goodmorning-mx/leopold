#!/usr/bin/env python3
"""Read the revision banner emitted by a flashed Leopold over serial."""

from __future__ import annotations

import argparse
import re
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("install pyserial for revision verification") from exc
    with serial.Serial(args.port, 115200, timeout=2) as port:
        deadline = time.time() + 15
        while time.time() < deadline:
            line = port.readline().decode(errors="replace").strip()
            if line.startswith("LEOPOLD_FIRMWARE_REVISION "):
                match = re.search(r"commit=([^ ]+)", line)
                if match and match.group(1) == args.commit:
                    print(line)
                    return 0
                print(line, file=sys.stderr)
                return 1
    raise SystemExit("firmware revision banner was not observed")


if __name__ == "__main__":
    raise SystemExit(main())
