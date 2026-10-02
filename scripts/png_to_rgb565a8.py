"""Convert a non-interlaced 8-bit RGBA PNG to Leopold's RGB565A8 format."""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def paeth(a: int, b: int, c: int) -> int:
    estimate = a + b - c
    pa, pb, pc = abs(estimate - a), abs(estimate - b), abs(estimate - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def decode(path: Path, remove_white_background: bool = False) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("input is not a PNG")
    offset = 8
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        chunk = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            break
    if bit_depth != 8 or color_type not in (2, 6) or interlace != 0:
        raise ValueError("expected an 8-bit RGB/RGBA, non-interlaced PNG")

    raw = zlib.decompress(compressed)
    bytes_per_pixel = 4 if color_type == 6 else 3
    stride = width * bytes_per_pixel
    rows: list[bytes] = []
    cursor = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        encoded = raw[cursor : cursor + stride]
        cursor += stride
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                row[index] = value
            elif filter_type == 1:
                row[index] = (value + left) & 0xFF
            elif filter_type == 2:
                row[index] = (value + above) & 0xFF
            elif filter_type == 3:
                row[index] = (value + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                row[index] = (value + paeth(left, above, upper_left)) & 0xFF
            else:
                raise ValueError(f"unsupported PNG filter: {filter_type}")
        rows.append(bytes(row))
        previous = row

    rgb565 = bytearray()
    alpha = bytearray()
    for row in rows:
        pixels = struct.iter_unpack("BBBB", row) if color_type == 6 else ((red, green, blue, 255) for red, green, blue in struct.iter_unpack("BBB", row))
        for red, green, blue, opacity in pixels:
            if remove_white_background and red >= 248 and green >= 248 and blue >= 248:
                opacity = 0
            packed = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            rgb565.extend(struct.pack("<H", packed))
            alpha.append(opacity)
    return width, height, bytes(rgb565 + alpha)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: png_to_rgb565a8.py INPUT.png OUTPUT.bin [--remove-white-background]")
    _, _, output = decode(Path(sys.argv[1]), len(sys.argv) == 4 and sys.argv[3] == "--remove-white-background")
    Path(sys.argv[2]).write_bytes(output)
