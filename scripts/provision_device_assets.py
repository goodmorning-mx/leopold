"""Provision the ESP32's internal SD card over USB serial."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import select
import termios
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "shared" / "asset-manifest.json"
ASSETS = ROOT / "shared" / "assets"


def configure(fd: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
    attrs[1] &= ~termios.OPOST
    attrs[2] &= ~(termios.CSIZE | termios.PARENB)
    attrs[2] |= termios.CS8
    attrs[3] &= ~(termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def read_until(fd: int, marker: bytes, timeout: float) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    while marker not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"timed out waiting for {marker!r}; received {data[-240:]!r}")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            data.extend(os.read(fd, 4096))
    return bytes(data)


def write_all(fd: int, data: bytes) -> None:
    view = memoryview(data)
    while view:
        view = view[os.write(fd, view):]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="USB serial device, for example /dev/cu.usbmodem...")
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    files = [ASSETS / asset["file"] for asset in manifest["assets"].values()]

    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY)
    try:
        configure(fd)
        for path in files:
            payload = path.read_bytes()
            digest = hashlib.sha256(payload).hexdigest()
            write_all(fd, f"LEOPOLD_ASSET_BEGIN {path.name} {len(payload)} {digest}\n".encode())
            read_until(fd, b"LEOPOLD_ASSET_READY\r\n", args.timeout)
            write_all(fd, payload)
            response = read_until(fd, b"\n", args.timeout).decode(errors="replace").strip()
            expected = f"LEOPOLD_ASSET_OK {path.name}"
            if response != expected:
                raise RuntimeError(f"device rejected {path.name}: {response}")
            print(f"Provisioned {path.name} ({len(payload)} bytes)")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
