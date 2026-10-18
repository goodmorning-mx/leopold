"""Derive the live Settings base from the immutable reference artwork.

Only the baked clock and battery glyphs are cleared. The original reference
and all static banner, row, and slider artwork remain the visual authority.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image

from png_to_rgb565a8 import decode


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/phase1-settings/settings-neutral-466.png"
OUTPUT = ROOT / "shared/assets/settings-neutral-466.png"
DEVICE_OUTPUT = OUTPUT.with_suffix(".bin")
ACTIVE_ICONS = {
    "settings-status-bluetooth-active": (153, 45, 22, 22, (4, 2, 19, 21)),
    "settings-status-wifi-active": (181, 45, 22, 22, (2, 3, 21, 20)),
    "settings-status-cloud-active": (264, 45, 24, 24, (3, 2, 24, 24)),
}


def clear_glyph(image: Image.Image, bounds: tuple[int, int, int, int]) -> None:
    """Fill one live text slot from the artwork along all four edges."""
    left, top, right, bottom = bounds
    pixels = image.load()
    for y in range(top, bottom):
        left_edge, right_edge = pixels[left - 1, y], pixels[right, y]
        for x in range(left, right):
            fraction = (x - left + 1) / (right - left + 1)
            pixels[x, y] = tuple(round(a * (1 - fraction) + b * fraction) for a, b in zip(left_edge, right_edge))
    for _ in range(200):
        for y in range(top, bottom):
            for x in range(left, right):
                above, below = pixels[x, y - 1], pixels[x, y + 1]
                before, after = pixels[x - 1, y], pixels[x + 1, y]
                pixels[x, y] = tuple(round((above[i] + below[i] + before[i] + after[i]) / 4) for i in range(4))


def main() -> None:
    image = Image.open(SOURCE).convert("RGBA")
    if image.size != (466, 466):
        raise SystemExit("Settings source must be 466x466")
    for name, (x, y, width, height, glyph_bounds) in ACTIVE_ICONS.items():
        patch = image.crop((x, y, x + width, y + height))
        pixels = patch.load()
        for row in range(height):
            for column in range(width):
                red, green, blue, alpha = pixels[column, row]
                inside_glyph = glyph_bounds[0] <= column < glyph_bounds[2] and glyph_bounds[1] <= row < glyph_bounds[3]
                strength = max(0.0, min(0.65, (min(red, green, blue) - 70) / 160)) if inside_glyph else 0.0
                pixels[column, row] = (
                    round(red * (1 - strength) + 80 * strength),
                    round(green * (1 - strength) + 226 * strength),
                    round(blue * (1 - strength) + 255 * strength),
                    alpha,
                )
        patch_path = OUTPUT.with_name(f"{name}-{width}x{height}.png")
        patch.save(patch_path, format="PNG")
        _, _, patch_bytes = decode(patch_path)
        patch_path.with_suffix(".bin").write_bytes(patch_bytes)
    clear_glyph(image, (213, 50, 266, 69))
    clear_glyph(image, (291, 53, 318, 66))
    image.save(OUTPUT, format="PNG")
    _, _, device_bytes = decode(OUTPUT)
    DEVICE_OUTPUT.write_bytes(device_bytes)


if __name__ == "__main__":
    main()
