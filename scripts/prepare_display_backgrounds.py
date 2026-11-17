"""Create deterministic 466x466 RGBA and RGB565A8 Display background assets."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "references" / "transparent-backgrounds"
OUTPUT = ROOT / "shared" / "assets"
NAMES = (
    ("background-flower", "01-blue-red-flower-transparent-1254.png"),
    ("background-chevron", "02-chevron-transparent-1254.png"),
    ("background-mechanical-flower", "03-mechanical-flower-transparent-1254.png"),
    ("background-black-white-circle", "black-white-circle-466.png"),
)
HOME_SOURCE = OUTPUT / "home-controls-outward-10-466.png"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    records = []
    for key, source_name in NAMES:
        source = SOURCE / source_name
        with Image.open(source) as original:
            if original.mode != "RGBA":
                raise ValueError(f"unexpected source image: {source}")
            if original.size == (466, 466):
                derivative = original.copy()
            elif original.size == (1254, 1254):
                derivative = original.resize((466, 466), Image.Resampling.LANCZOS)
            else:
                raise ValueError(f"unexpected source dimensions: {source} ({original.size})")
        png = OUTPUT / f"{key}-466.png"
        binary = OUTPUT / f"{key}-466.bin"
        derivative.save(png, format="PNG", optimize=False)
        width, height, encoded = decode(png)
        if (width, height) != (466, 466) or len(encoded) != 466 * 466 * 3:
            raise ValueError(f"invalid encoded derivative: {png}")
        binary.write_bytes(encoded)
        records.append({
            "key": key,
            "source": f"shared/references/transparent-backgrounds/{source_name}",
            "source_sha256": sha256(source),
            "source_dimensions": list(original.size),
            "scaling": "no scaling; source RGBA pixels preserved" if original.size == (466, 466) else "Pillow Lanczos, full canvas, no crop, RGBA preserved",
            "png": f"shared/assets/{png.name}",
            "png_sha256": sha256(png),
            "png_dimensions": [466, 466],
            "format": "RGBA PNG",
            "device": f"shared/assets/{binary.name}",
            "device_sha256": sha256(binary),
            "device_bytes": len(encoded),
            "device_format": "RGB565A8",
            "bytes": len(encoded),
            "sha256": sha256(binary),
            "logical_id": key,
            "delivery_class": "optional",
        })
    with Image.open(HOME_SOURCE) as source:
        if source.size != (466, 466):
            raise ValueError(f"unexpected Home asset dimensions: {HOME_SOURCE}")
        overlay = source.convert("RGBA")
    # Retain only the seven glass controls and the central status pill. The
    # previous derivative made black transparent but left the full old gear
    # wallpaper opaque, hiding the selected Display background on Home.
    mask = Image.new("L", (466, 466), 0)
    draw = ImageDraw.Draw(mask)
    for center_x, center_y in ((240, 94), (151, 139), (99, 231), (373, 231), (147, 318), (327, 318), (236, 372)):
        draw.ellipse((center_x - 33, center_y - 33, center_x + 33, center_y + 33), fill=255)
    draw.rounded_rectangle((143, 205, 330, 255), radius=25, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.2))
    alpha = ImageChops.multiply(overlay.getchannel("A"), mask)
    overlay.putalpha(alpha)
    # The source artwork spells out Spotify. Clear just the word; the source
    # ring remains, and each renderer supplies the Spotify brand glyph.
    for x in range(302, 353):
        for y in range(311, 328):
            overlay.putpixel((x, y), (0, 0, 0, 0))
    overlay_png = OUTPUT / "home-controls-outward-10-466-transparent.png"
    overlay_bin = OUTPUT / "home-controls-outward-10-466-transparent.bin"
    overlay.save(overlay_png, format="PNG", optimize=False)
    width, height, encoded = decode(overlay_png)
    overlay_bin.write_bytes(encoded)
    records.append({
        "key": "home-controls-transparent",
        "source": "shared/assets/home-controls-outward-10-466.png",
        "source_sha256": sha256(HOME_SOURCE),
        "source_dimensions": [466, 466],
        "scaling": "no scaling; alpha mask retains glass control silhouettes and status pill, with baked Spotify wordmark cleared for dynamic logo overlay",
        "png": f"shared/assets/{overlay_png.name}",
        "png_sha256": sha256(overlay_png),
        "png_dimensions": [466, 466],
        "format": "RGBA PNG",
        "device": f"shared/assets/{overlay_bin.name}",
        "device_sha256": sha256(overlay_bin),
        "device_bytes": len(encoded),
        "device_format": "RGB565A8",
        "bytes": len(encoded),
        "sha256": sha256(overlay_bin),
        "logical_id": "home-controls-transparent",
        "delivery_class": "essential",
    })
    manifest = {
        "schema_version": 1,
        "purpose": "Canonical transparent background source-to-runtime identity",
        "derivative_size": [466, 466],
        "records": records,
    }
    (ROOT / "shared" / "display-background-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    asset_manifest_path = ROOT / "shared" / "asset-manifest.json"
    asset_manifest = json.loads(asset_manifest_path.read_text(encoding="utf-8"))
    for record in records:
        png_name = Path(record["png"]).name
        bin_name = Path(record["device"]).name
        asset_manifest["assets"][record["key"]] = {
            "file": bin_name,
            "width": 466,
            "height": 466,
            "format": "rgb565a8",
            "derived_from": png_name,
            "source": record["source"],
            "source_sha256": record["source_sha256"],
            "png_sha256": record["png_sha256"],
            "device_sha256": record["device_sha256"],
            "bytes": record["device_bytes"],
            "sha256": record["device_sha256"],
            "logical_id": record["key"],
            "delivery_class": record["delivery_class"],
            "resampling": record["scaling"],
        }
    asset_manifest_path.write_text(json.dumps(asset_manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
