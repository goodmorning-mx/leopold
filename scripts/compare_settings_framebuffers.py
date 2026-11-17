#!/usr/bin/env python3
"""Compare two raw 466x466 Settings framebuffer captures by named regions."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageChops


REGIONS = {
    "circular-display-bounds": (0, 0, 466, 466),
    "status-banner": (137, 39, 195, 38),
    "title-gear": (160, 103, 40, 33),
    "title-text": (196, 103, 114, 33),
    "wifi-row": (106, 148, 258, 40),
    "bluetooth-row": (106, 191, 258, 41),
    "display-row": (106, 234, 258, 41),
    "sound-row": (106, 277, 258, 41),
    "brightness-control": (63, 336, 342, 88),
}


def metrics(first: Image.Image, second: Image.Image, bounds: tuple[int, int, int, int]) -> dict[str, object]:
    left, top, width, height = bounds
    a = first.crop((left, top, left + width, top + height))
    b = second.crop((left, top, left + width, top + height))
    difference = ImageChops.difference(a, b)
    extrema = difference.getextrema()
    max_delta = max(channel[1] for channel in extrema)
    nonzero = sum(1 for pixel in difference.getdata() if any(pixel))
    return {
        "bounds": list(bounds),
        "compared_pixels": width * height,
        "pixels_different": nonzero,
        "pixels_different_percent": nonzero * 100 / (width * height),
        "maximum_color_delta": max_delta,
        "exact_match": nonzero == 0,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("corrected_capture", type=Path)
    parser.add_argument("historical_device_capture", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    corrected = Image.open(args.corrected_capture).convert("RGB")
    device = Image.open(args.historical_device_capture).convert("RGB")
    if corrected.size != (466, 466) or device.size != (466, 466):
        raise SystemExit(f"expected two 466x466 captures, got {corrected.size} and {device.size}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    difference = ImageChops.difference(corrected, device).convert("RGB")
    difference_path = args.output_dir / "lvgl-vs-historical-device-difference.png"
    difference.save(difference_path, format="PNG")
    report = {
        "corrected_capture": str(args.corrected_capture),
        "historical_device_capture": str(args.historical_device_capture),
        "dimensions": [466, 466],
        "comparison": "direct raw framebuffer comparison; historical device capture is diagnostic evidence and is not an acceptance gate",
        "regions": {name: metrics(corrected, device, bounds) for name, bounds in REGIONS.items()},
        "difference": str(difference_path),
    }
    report["exact_match"] = all(region["exact_match"] for region in report["regions"].values())
    report_path = args.output_dir / "lvgl-vs-historical-device-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(report_path), "difference": str(difference_path), "exact_match": report["exact_match"]}))


if __name__ == "__main__":
    main()
