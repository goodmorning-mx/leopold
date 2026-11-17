"""Generate deterministic 466px Home and Settings exact-reference assets."""

from __future__ import annotations

import argparse
import hashlib
import json
from io import BytesIO
from pathlib import Path

from PIL import Image

from png_to_rgb565a8 import decode as decode_rgb565a8


ROOT = Path(__file__).resolve().parents[1]
TARGET_SIZE = (466, 466)
PILLOW_VERSION = "12.3.0"
SOURCES = (
    {
        "name": "home",
        "screen": "sky-home",
        "source": ROOT / "shared/references/home-source.png",
        "source_sha256": "5a1d2f08b32d38eb22ad13ef683ddec753c92a65ecbb15fc32a890cad5c58abe",
        "source_size": (1310, 1200),
        "overlays": (
            {
                "name": "home-status-banner",
                "source": ROOT / "shared/references/home-status-banner-source.png",
                "source_sha256": "879555f5ee1d81444dff31fead0ca25ad327ae1b61bd2d5ecd67db21e84c7bb2",
                "source_size": (660, 206),
                "bounds": (145, 203, 184, 57),
            },
        ),
        "asset": ROOT / "shared/assets/home-reference-466.png",
        "device_asset": ROOT / "shared/assets/home-reference-466.bin",
        "dynamic_masks": [{"name": "live-clock", "reason": "runtime clock", "x": 211, "y": 219, "width": 56, "height": 22}],
        "regions": {
            "full-static-frame": [0, 0, 466, 466],
            "status-strip": [145, 203, 184, 57],
            "primary-controls": [70, 55, 326, 344],
            "settings-button": [206, 324, 55, 55],
        },
    },
    {
        "name": "settings",
        "screen": "device-control-center",
        "source": ROOT / "shared/references/settings-source.png",
        "source_sha256": "dca1b4ca563f70626733b4c64ce0ca1442f80bd9ca3e7ccd4d3afbdb65ed02ac",
        "source_size": (1310, 1200),
        "asset": ROOT / "shared/assets/settings-reference-466.png",
        "device_asset": ROOT / "shared/assets/settings-reference-466.bin",
        "dynamic_masks": [
            {"name": "live-clock", "reason": "runtime clock", "x": 218, "y": 72, "width": 49, "height": 26},
            {"name": "wifi-status", "reason": "Wi-Fi connected/disconnected state", "x": 190, "y": 73, "width": 22, "height": 22},
            {"name": "cloud-status", "reason": "cloud connected/disconnected state", "x": 264, "y": 72, "width": 24, "height": 24},
            {"name": "device-connection-status", "reason": "simulator/ESP32 connection state", "x": 163, "y": 73, "width": 20, "height": 22},
            {"name": "brightness-control", "reason": "live brightness fill, knob, and percentage", "x": 123, "y": 336, "width": 220, "height": 61},
        ],
        "regions": {
            "full-static-frame": [0, 0, 466, 466],
            "status-strip": [160, 67, 150, 34],
            "title": [160, 103, 150, 33],
            "wifi-row": [106, 148, 258, 40],
            "bluetooth-row": [106, 191, 258, 41],
            "display-row": [106, 234, 258, 41],
            "sound-row": [106, 277, 258, 41],
            "brightness-slider": [123, 336, 220, 61],
        },
    },
    {
        "name": "display",
        "screen": "settings-display",
        "source": ROOT / "shared/references/display-source.png",
        "source_sha256": "e3bd058ddc0c3470bb0eb64111365de8613bcf3eda5fe536208179d76f528b4c",
        "source_size": (1254, 1254),
        "asset": ROOT / "shared/assets/display-reference-466.png",
        "device_asset": ROOT / "shared/assets/display-reference-466.bin",
        "dynamic_masks": [
            {"name": "live-clock", "reason": "runtime clock", "x": 211, "y": 39, "width": 57, "height": 29},
            {"name": "wifi-status", "reason": "Wi-Fi connected/disconnected state", "x": 181, "y": 40, "width": 22, "height": 22},
            {"name": "cloud-status", "reason": "cloud connected/disconnected state", "x": 267, "y": 39, "width": 24, "height": 24},
            {"name": "device-connection-status", "reason": "simulator/ESP32 connection state", "x": 154, "y": 40, "width": 20, "height": 22},
        ],
        "regions": {
            "full-static-frame": [0, 0, 466, 466],
            "status-strip": [141, 37, 185, 34],
            "title": [140, 70, 188, 46],
            "language-row": [75, 183, 315, 50],
            "background-row": [75, 242, 315, 50],
            "clock-row": [75, 301, 315, 50],
            "units-row": [75, 359, 315, 50],
        },
    },
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def render(source: Path, overlays: tuple[dict[str, object], ...] = ()) -> bytes:
    with Image.open(source) as opened:
        image = opened.convert("RGBA")
    image.thumbnail(TARGET_SIZE, Image.Resampling.BILINEAR, reducing_gap=None)
    canvas = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 255))
    offset = ((TARGET_SIZE[0] - image.width) // 2, (TARGET_SIZE[1] - image.height) // 2)
    canvas.alpha_composite(image, offset)
    for overlay in overlays:
        overlay_source = overlay["source"]
        bounds = overlay["bounds"]
        assert isinstance(overlay_source, Path) and isinstance(bounds, tuple)
        with Image.open(overlay_source) as opened:
            overlay_image = opened.convert("RGBA")
        overlay_image = overlay_image.resize((bounds[2], bounds[3]), Image.Resampling.BILINEAR)
        canvas.alpha_composite(overlay_image, (bounds[0], bounds[1]))
    output = BytesIO()
    canvas.save(output, format="PNG", compress_level=9, optimize=False)
    return output.getvalue()


def make_manifest(outputs: list[dict[str, object]]) -> dict[str, object]:
    return {
        "schema_version": 1,
        "mode": "exact-reference-asset",
        "viewport": {"width": 466, "height": 466},
        "generator": {
            "script": "scripts/generate_exact_reference_asset.py",
            "pillow_version": PILLOW_VERSION,
            "scaling": "RGBA uniform contain, Pillow bilinear, centered black letterbox, no crop",
        },
        "comparison_profile": {
            "capture": "raw canvas PNG from canvas.toDataURL('image/png')",
            "allowed_color_delta": 0,
            "maximum_mean_error": 0,
            "maximum_pixels_over_delta_percent": 0,
        },
        "states": outputs,
    }


def build() -> tuple[dict[Path, bytes], bytes]:
    files: dict[Path, bytes] = {}
    states: list[dict[str, object]] = []
    for definition in SOURCES:
        source = definition["source"]
        assert isinstance(source, Path)
        source_data = source.read_bytes()
        if sha256(source_data) != definition["source_sha256"]:
            raise ValueError(f"immutable {definition['name']} source checksum changed")
        with Image.open(BytesIO(source_data)) as opened:
            if opened.size != definition["source_size"]:
                raise ValueError(f"immutable {definition['name']} source dimensions changed")
        overlays = definition.get("overlays", ())
        assert isinstance(overlays, tuple)
        overlay_manifest = []
        for overlay in overlays:
            overlay_source = overlay["source"]
            overlay_size = overlay["source_size"]
            bounds = overlay["bounds"]
            assert isinstance(overlay_source, Path) and isinstance(overlay_size, tuple) and isinstance(bounds, tuple)
            overlay_data = overlay_source.read_bytes()
            if sha256(overlay_data) != overlay["source_sha256"]:
                raise ValueError(f"immutable {overlay['name']} source checksum changed")
            with Image.open(BytesIO(overlay_data)) as opened:
                if opened.size != overlay_size:
                    raise ValueError(f"immutable {overlay['name']} source dimensions changed")
            overlay_manifest.append(
                {
                    "name": overlay["name"],
                    "file": str(overlay_source.relative_to(ROOT / "shared")),
                    "sha256": sha256(overlay_data),
                    "width": overlay_size[0],
                    "height": overlay_size[1],
                    "immutable": True,
                    "generated_bounds": {"x": bounds[0], "y": bounds[1], "width": bounds[2], "height": bounds[3]},
                    "scaling": "Pillow bilinear exact bounds",
                }
            )
        png = render(source, overlays)
        asset = definition["asset"]
        device_asset = definition["device_asset"]
        assert isinstance(asset, Path) and isinstance(device_asset, Path)
        temporary = ROOT / "work-reference-output.png"
        temporary.write_bytes(png)
        try:
            _, _, device_bytes = decode_rgb565a8(temporary)
        finally:
            temporary.unlink(missing_ok=True)
        files[asset] = png
        files[device_asset] = device_bytes
        states.append(
            {
                "name": definition["name"],
                "screen": definition["screen"],
                "source": {
                    "file": str(source.relative_to(ROOT / "shared")),
                    "sha256": sha256(source_data),
                    "width": definition["source_size"][0],
                    "height": definition["source_size"][1],
                    "immutable": True,
                },
                "overlays": overlay_manifest,
                "generated_asset": {
                    "file": str(asset.relative_to(ROOT / "shared")),
                    "sha256": sha256(png),
                    "width": 466,
                    "height": 466,
                    "format": "lossless PNG",
                },
                "device_asset": {
                    "file": str(device_asset.relative_to(ROOT / "shared")),
                    "sha256": sha256(device_bytes),
                    "format": "RGB565A8 derived from generated_asset",
                },
                "dynamic_masks": definition["dynamic_masks"],
                "regions": definition["regions"],
                "consumers": {
                    "browser": f"frontend/public/firmware-assets/{asset.name}",
                    "device": f"device/assets/{device_asset.name}",
                },
            }
        )
    manifest = (json.dumps(make_manifest(states), indent=2) + "\n").encode()
    return files, manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated outputs without writing")
    args = parser.parse_args()
    files, manifest = build()
    manifest_path = ROOT / "shared/reference-manifest.json"
    files[manifest_path] = manifest
    if args.check:
        stale = [str(path.relative_to(ROOT)) for path, data in files.items() if not path.is_file() or path.read_bytes() != data]
        if stale:
            raise SystemExit("stale exact-reference outputs: " + ", ".join(stale))
        print("Exact-reference outputs valid: home, settings, and display submenu")
        return
    for path, data in files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    print("Generated exact-reference assets: home and settings (466x466)")


if __name__ == "__main__":
    main()
