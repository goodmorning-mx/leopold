"""Derive the live Settings base from the immutable reference artwork.

The complete baked legacy status strip is cleared. The shared LVGL renderer
owns the only live banner; Settings rows and slider artwork remain sourced
from the immutable reference.
"""

from __future__ import annotations

from pathlib import Path
import hashlib
import json

from PIL import Image

from png_to_rgb565a8 import decode


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/phase1-settings/settings-neutral-466.png"
OUTPUT = ROOT / "shared/assets/settings-neutral-466.png"
DEVICE_OUTPUT = OUTPUT.with_suffix(".bin")
LEGACY_STATUS_STRIP = (143, 43, 324, 78)


def main() -> None:
    image = Image.open(SOURCE).convert("RGBA")
    if image.size != (466, 466):
        raise SystemExit("Settings source must be 466x466")
    # Keep the live capsule's translucent fill showing the selected wallpaper.
    # The old reference capsule contains its own icons and rim, which would
    # otherwise ghost through the live shared capsule.
    pixels = image.load()
    left, top, right, bottom = LEGACY_STATUS_STRIP
    for y in range(top, bottom):
        for x in range(left, right):
            pixels[x, y] = (0, 0, 0, 255)
    image.save(OUTPUT, format="PNG")
    _, _, device_bytes = decode(OUTPUT)
    DEVICE_OUTPUT.write_bytes(device_bytes)

    device_sha256 = hashlib.sha256(device_bytes).hexdigest()
    manifest_path = ROOT / "shared/asset-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    asset = manifest["assets"]["settings-neutral"]
    asset.update({
        "bytes": len(device_bytes),
        "sha256": device_sha256,
        "device_sha256": device_sha256,
        "png_sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
    })
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    reference_path = ROOT / "shared/reference-manifest.json"
    references = json.loads(reference_path.read_text(encoding="utf-8"))
    settings = next(state for state in references["states"] if state.get("name") == "settings")
    settings["generated_asset"].update({
        "sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
        "derivation": "scripts/prepare_settings_live_asset.py clears the complete legacy status strip at (143,43)-(324,78), plus baked clock and battery glyph slots, from the immutable source",
    })
    settings["device_asset"]["sha256"] = device_sha256
    reference_path.write_text(json.dumps(references, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
