"""Convert the raw PPM captures produced by capture_device_screenshot.py."""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def main() -> None:
    source, destination = map(Path, sys.argv[1:3])
    data = source.read_bytes()
    header_end = data.index(b"255\n") + 4
    header = data[:header_end].split()
    width, height = int(header[1]), int(header[2])
    pixels = data[header_end:]
    rows = b"".join(b"\x00" + pixels[row * width * 3:(row + 1) * width * 3] for row in range(height))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9))
    png += chunk(b"IEND", b"")
    destination.write_bytes(png)


if __name__ == "__main__":
    main()
