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
    # The live renderer owns the complete slider, glow and caption region.
    # Clear all baked slider artwork before compositing; clearing only the
    # caption leaves a second track visible underneath the live control.
    # Start below the Sound row's glow and restore the unbroken wallpaper.
    for y in range(349, 424):
        for x in range(63, 405):
            pixels[x, y] = (0, 0, 0, 0)
    for y in range(neutral.height):
        for x in range(neutral.width):
            red, green, blue, _ = pixels[x, y]
            # The neutral reference uses true black as its empty canvas. Keep
            # all visible artwork opaque while letting the Rainbow Gears show
            # through those empty pixels.
            pixels[x, y] = (red, green, blue, 0 if max(red, green, blue) <= 15 else 255)
    return Image.alpha_composite(background, neutral).convert("RGBA")


def selection_overlays():
    """Use the same black-canvas composition for bounded pressed artwork."""
    source_manifest = json.loads((ROOT / "shared/references/settings-glass-gears-2026-10-20/composition.json").read_text())
    background = ImageEnhance.Brightness(Image.open(ASSETS / "settings-gears-wallpaper-466.png").convert("RGBA")).enhance(0.58)
    for key, entry in source_manifest["state_assets"].items():
        if "pressed-icon-mask" not in key and "pressed-chevron-mask" not in key:
            continue
        source = ROOT / "shared" / entry["source_asset"]
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry["source_sha256"]:
            raise ValueError(f"Selection source differs: {key}")
        image = Image.open(source).convert("RGBA")
        left, top, width, height = entry["bounds"]
        for y in range(height):
            for x in range(width):
                pixel = image.getpixel((x, y))
                # Opaque reference repair pixels extend to rectangular crop
                # edges. Keep the selected artwork within its actual icon or
                # chevron footprint so that backing never becomes a patch.
                if "icon-mask" in key:
                    distance = ((x - 28) ** 2 + (y - 28) ** 2) ** 0.5
                    coverage = max(0.0, min(1.0, 28.0 - distance))
                else:
                    coverage = 1.0 if 7 <= x <= 25 and 13 <= y <= 42 else 0.0
                alpha = round(pixel[3] * coverage)
                color = background.getpixel((left + x, top + y))[:3] if max(pixel[:3]) <= 15 else pixel[:3]
                image.putpixel((x, y), color + (alpha,))
        yield key, entry, image


def main() -> None:
    result = compose()
    result.save(OUTPUT_PNG, format="PNG", optimize=False)
    width, height, encoded = decode(OUTPUT_PNG)
    if (width, height) != (466, 466):
        raise ValueError("invalid Rainbow Gears Settings derivative")
    OUTPUT_BIN.write_bytes(encoded)

    manifest_path = ROOT / "shared" / "asset-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    rainbow_bytes = OUTPUT_BIN.read_bytes()
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
        "bytes": len(rainbow_bytes),
        "sha256": hashlib.sha256(rainbow_bytes).hexdigest(),
        "logical_id": "settings-gears-rainbow",
        "delivery_class": "optional",
        "resampling": "466x466; RGB565A8; canvas pixels <= RGB 15 transparent; Settings artwork retained over 58% brightness Rainbow Gears, complete brightness slider/glow/caption region (63,349)-(405,424) cleared for live rendering",
    }
    for key, entry, overlay in selection_overlays():
        name = "settings-rainbow-" + key.removeprefix("settings-")
        png = ASSETS / (name + ".png")
        binary = ASSETS / (name + ".bin")
        overlay.save(png, format="PNG", optimize=False)
        binary.write_bytes(decode(png)[2])
        overlay_bytes = binary.read_bytes()
        manifest["assets"][key] = {
            "file": binary.name, "width": overlay.width, "height": overlay.height,
            "format": "rgb565a8", "derived_from": png.name,
            "source": entry["source_asset"], "source_sha256": entry["source_sha256"],
            "bounds": entry["bounds"],
            "device_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "bytes": len(overlay_bytes),
            "sha256": hashlib.sha256(overlay_bytes).hexdigest(),
            "logical_id": key,
            "delivery_class": "optional",
            "composition": "Same 58% Rainbow background and black-canvas threshold as neutral base; reference artwork with circular icon and bounded chevron coverage; exclude opaque rectangular repair margins",
        }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
