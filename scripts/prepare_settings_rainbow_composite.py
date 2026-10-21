"""Compose the shared Settings controls over the approved Rainbow Gears art."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageEnhance

from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "shared" / "assets"
OUTPUT_PNG = ASSETS / "settings-gears-rainbow-466.png"
OUTPUT_BIN = ASSETS / "settings-gears-rainbow-466.bin"


def compose() -> Image.Image:
    wallpaper = Image.open(ASSETS / "settings-gears-wallpaper-466.png").convert("RGBA")
    neutral = Image.open(ASSETS / "settings-neutral-466.png").convert("RGBA")
    if wallpaper.size != (466, 466) or neutral.size != (466, 466):
        raise ValueError("Settings source art must remain 466x466")

    # Darken the vivid photo enough to retain legibility under the glass rows.
    background = ImageEnhance.Brightness(wallpaper).enhance(0.58)
    pixels = neutral.load()
    # Brightness is rendered live by the shared settings renderer, so remove
    # the static reference caption to avoid displaying the label twice.
    for y in range(397, 418):
        for x in range(186, 282):
            pixels[x, y] = (0, 0, 0, 0)
    for y in range(neutral.height):
        for x in range(neutral.width):
            red, green, blue, _ = pixels[x, y]
            # The neutral reference uses true black as its empty canvas. Keep
            # all visible artwork opaque while letting the Rainbow Gears show
            # through those empty pixels.
            pixels[x, y] = (red, green, blue, 0 if max(red, green, blue) <= 15 else 255)
    return Image.alpha_composite(background, neutral).convert("RGBA")


def main() -> None:
    result = compose()
    result.save(OUTPUT_PNG, format="PNG", optimize=False)
    width, height, encoded = decode(OUTPUT_PNG)
    if (width, height) != (466, 466):
        raise ValueError("invalid Rainbow Gears Settings derivative")
    OUTPUT_BIN.write_bytes(encoded)

    manifest_path = ROOT / "shared" / "asset-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["assets"]["settings-gears-rainbow"] = {
        "file": OUTPUT_BIN.name,
        "width": 466,
        "height": 466,
        "format": "rgb565a8",
        "derived_from": OUTPUT_PNG.name,
        "source": "settings-neutral-466.png composited over settings-gears-wallpaper-466.png",
        "source_sha256": hashlib.sha256((ASSETS / "settings-neutral-466.png").read_bytes()).hexdigest(),
        "wallpaper_sha256": hashlib.sha256((ASSETS / "settings-gears-wallpaper-466.png").read_bytes()).hexdigest(),
        "device_sha256": hashlib.sha256(OUTPUT_BIN.read_bytes()).hexdigest(),
        "resampling": "466x466; RGB565A8; canvas pixels <= RGB 15 transparent; Settings artwork retained over 58% brightness Rainbow Gears, static brightness caption cleared for live rendering",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
