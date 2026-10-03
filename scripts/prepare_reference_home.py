"""Prepare the 466px reference home artwork for RGB565A8 conversion."""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def read_png(path: Path) -> tuple[int, int, bytearray]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("expected a PNG")
    offset = 8
    width = height = color_type = bit_depth = interlace = None
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += length + 12
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)
    if (bit_depth, color_type, interlace) != (8, 2, 0):
        raise ValueError("expected an 8-bit RGB PNG")
    raw = zlib.decompress(compressed)
    stride = width * 3
    rows: list[bytearray] = []
    cursor = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        encoded = raw[cursor : cursor + stride]
        cursor += stride
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - 3] if index >= 3 else 0
            above = previous[index]
            upper_left = previous[index - 3] if index >= 3 else 0
            if filter_type == 0:
                row[index] = value
            elif filter_type == 1:
                row[index] = (value + left) & 0xFF
            elif filter_type == 2:
                row[index] = (value + above) & 0xFF
            elif filter_type == 3:
                row[index] = (value + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                estimate = left + above - upper_left
                pa = abs(estimate - left)
                pb = abs(estimate - above)
                pc = abs(estimate - upper_left)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else upper_left
                row[index] = (value + predictor) & 0xFF
            else:
                raise ValueError(f"unsupported PNG filter: {filter_type}")
        rows.append(row)
        previous = row
    return width, height, bytearray(b"".join(rows))


def write_png(path: Path, width: int, height: int, rgba: bytes) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + rgba[row * width * 4 : (row + 1) * width * 4] for row in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main() -> None:
    source, destination = map(Path, sys.argv[1:3])
    face_radius = int(sys.argv[3]) if len(sys.argv) > 3 else 232
    keep_banner = len(sys.argv) > 4 and sys.argv[4] == "keep-banner"
    width, height, rgb = read_png(source)
    rgba = bytearray()
    cx = cy = (width - 1) / 2
    for y in range(height):
        for x in range(width):
            red, green, blue = rgb[(y * width + x) * 3 : (y * width + x + 1) * 3]
            inside_face = (x - cx) ** 2 + (y - cy) ** 2 <= face_radius**2
            # Match the live renderer's home status pill exactly so the
            # artwork mask cannot leave a dark halo around its edge.
            left, top, right, bottom, radius = 114, 202, 338, 258, 28
            in_banner = (left + radius <= x <= right - radius and top <= y <= bottom) or (left <= x <= right and top + radius <= y <= bottom - radius)
            if keep_banner:
                in_banner = False
            elif not in_banner:
                for corner_x, corner_y in ((left + radius, top + radius), (right - radius, top + radius), (left + radius, bottom - radius), (right - radius, bottom - radius)):
                    if (x - corner_x) ** 2 + (y - corner_y) ** 2 <= radius ** 2:
                        in_banner = True
                        break
            rgba.extend((0, 0, 0, 0 if not inside_face else 255) if in_banner or not inside_face else (red, green, blue, 255))
    write_png(destination, width, height, rgba)


if __name__ == "__main__":
    main()
