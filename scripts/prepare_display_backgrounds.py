"""Create deterministic 466x466 RGBA and RGB565A8 Display background assets."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "references" / "transparent-backgrounds"
OUTPUT = ROOT / "shared" / "assets"
NAMES = (
    ("background-flower", "01-blue-red-flower-transparent-1254.png"),
    ("background-chevron", "02-chevron-transparent-1254.png"),
    ("background-mechanical-flower", "03-mechanical-flower-transparent-1254.png"),
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
            if original.size != (1254, 1254) or original.mode != "RGBA":
                raise ValueError(f"unexpected source image: {source}")
            derivative = original.resize((466, 466), Image.Resampling.LANCZOS)
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
            "source_dimensions": [1254, 1254],
            "scaling": "Pillow Lanczos, full canvas, no crop, RGBA preserved",
            "png": f"shared/assets/{png.name}",
            "png_sha256": sha256(png),
            "png_dimensions": [466, 466],
            "format": "RGBA PNG",
            "device": f"shared/assets/{binary.name}",
            "device_sha256": sha256(binary),
            "device_bytes": len(encoded),
            "device_format": "RGB565A8",
        })
    with Image.open(HOME_SOURCE) as source:
        if source.size != (466, 466):
            raise ValueError(f"unexpected Home asset dimensions: {HOME_SOURCE}")
        overlay = source.convert("RGBA")
    pixels = overlay.load()
    for y in range(overlay.height):
        for x in range(overlay.width):
            red, green, blue, alpha = pixels[x, y]
            if red <= 2 and green <= 2 and blue <= 2:
                pixels[x, y] = (red, green, blue, 0)
            else:
                pixels[x, y] = (red, green, blue, alpha)
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
        "scaling": "no scaling; exact pure-black RGB pixels made transparent, all other RGBA samples preserved",
        "png": f"shared/assets/{overlay_png.name}",
        "png_sha256": sha256(overlay_png),
        "png_dimensions": [466, 466],
        "format": "RGBA PNG",
        "device": f"shared/assets/{overlay_bin.name}",
        "device_sha256": sha256(overlay_bin),
        "device_bytes": len(encoded),
        "device_format": "RGB565A8",
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
            "resampling": record["scaling"],
        }
    asset_manifest_path.write_text(json.dumps(asset_manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
