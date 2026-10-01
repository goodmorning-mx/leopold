"""Capture the ESP32 LVGL framebuffer over USB without third-party Python packages."""
from __future__ import annotations

import argparse
import os
import select
import struct
import termios
import time
from pathlib import Path


def read_until(fd: int, marker: bytes, timeout: float) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    while marker not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Timed out waiting for {marker!r}")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            data.extend(os.read(fd, 4096))
    return bytes(data)


def read_exact(fd: int, size: int, timeout: float) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    while len(data) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Timed out after {len(data)}/{size} bytes")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            data.extend(os.read(fd, min(16384, size - len(data))))
    return bytes(data)


def ppm_from_rgb565(raw: bytes, width: int, height: int, destination: Path) -> None:
    with destination.open("wb") as image:
        image.write(f"P6\n{width} {height}\n255\n".encode())
        for (pixel,) in struct.iter_unpack("<H", raw):
            image.write(bytes((
                ((pixel >> 11) & 0x1F) * 255 // 31,
                ((pixel >> 5) & 0x3F) * 255 // 63,
                (pixel & 0x1F) * 255 // 31,
            )))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY)
    try:
        attrs = termios.tcgetattr(fd)
        speed = getattr(termios, "B921600", termios.B115200)
        attrs[4] = speed
        attrs[5] = speed
        attrs[3] &= ~(termios.ICANON | termios.ECHO)
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        os.write(fd, b"LEOPOLD_SCREENSHOT\n")
        marker = b"LEOPOLD_SCREENSHOT_BEGIN "
        header = read_until(fd, marker, 10.0)
        header = header[header.rfind(marker):]
        line, _, payload = header.partition(b"\n")
        _, width, height, pixel_format, byte_count = line.decode().split()
        if pixel_format != "RGB565": raise RuntimeError(f"Unexpected format: {pixel_format}")
        raw = payload + read_exact(fd, int(byte_count) - len(payload), 30.0)
        ppm_from_rgb565(raw, int(width), int(height), args.output)
        print(f"Wrote {args.output} ({width}x{height})")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
