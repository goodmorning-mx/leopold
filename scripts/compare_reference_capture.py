"""Compare a raw simulator framebuffer PNG with an exact-reference asset."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


def inside_mask(x: int, y: int, masks: list[dict[str, object]]) -> bool:
    return any(
        int(mask["x"]) <= x < int(mask["x"]) + int(mask["width"])
        and int(mask["y"]) <= y < int(mask["y"]) + int(mask["height"])
        for mask in masks
    )


def metrics(reference: Image.Image, capture: Image.Image, bounds: tuple[int, int, int, int], masks: list[dict[str, object]]) -> dict[str, object]:
    left, top, width, height = bounds
    total_delta = maximum_delta = over_delta = compared = 0
    failure_points: list[tuple[int, int]] = []
    reference_pixels = reference.load()
    capture_pixels = capture.load()
    for y in range(top, top + height):
        for x in range(left, left + width):
            if inside_mask(x, y, masks):
                continue
            delta = max(abs(reference_pixels[x, y][channel] - capture_pixels[x, y][channel]) for channel in range(4))
            compared += 1
            total_delta += delta
            maximum_delta = max(maximum_delta, delta)
            if delta > 0:
                over_delta += 1
                failure_points.append((x, y))
    failure_box = None
    if failure_points:
        xs, ys = zip(*failure_points)
        failure_box = [min(xs), min(ys), max(xs) + 1, max(ys) + 1]
    return {
        "compared_pixels": compared,
        "mean_error": total_delta / compared if compared else 0,
        "maximum_color_delta": maximum_delta,
        "pixels_over_delta": over_delta,
        "pixels_over_delta_percent": over_delta * 100 / compared if compared else 0,
        "failure_bounding_box": failure_box,
        "passed": maximum_delta == 0 and over_delta == 0,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("state", choices=("home", "settings"))
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "shared/reference-manifest.json").read_text())
    state = next(item for item in manifest["states"] if item["name"] == args.state)
    reference_path = ROOT / "shared" / state["generated_asset"]["file"]
    with Image.open(reference_path) as opened:
        reference = opened.convert("RGBA")
    with Image.open(args.capture) as opened:
        capture = opened.convert("RGBA")
    if reference.size != (466, 466) or capture.size != (466, 466):
        raise SystemExit(f"expected identical 466x466 images, got reference={reference.size} capture={capture.size}")
    masks = state["dynamic_masks"]
    overall = metrics(reference, capture, (0, 0, 466, 466), masks)
    regions = {name: metrics(reference, capture, tuple(bounds), masks) for name, bounds in state["regions"].items()}
    difference = Image.new("RGBA", (466, 466), (0, 0, 0, 255))
    difference_pixels = difference.load()
    reference_pixels = reference.load()
    capture_pixels = capture.load()
    for y in range(466):
        for x in range(466):
            if inside_mask(x, y, masks):
                difference_pixels[x, y] = (0, 80, 255, 255)
            else:
                delta = tuple(abs(reference_pixels[x, y][channel] - capture_pixels[x, y][channel]) for channel in range(3))
                difference_pixels[x, y] = (*delta, 255)
    args.output.mkdir(parents=True, exist_ok=True)
    difference_path = args.output / f"{args.state}-difference.png"
    difference.save(difference_path, format="PNG")
    report = {
        "state": args.state,
        "reference": str(reference_path.relative_to(ROOT)),
        "capture": str(args.capture),
        "dimensions": [466, 466],
        "profile": manifest["comparison_profile"],
        "dynamic_masks": masks,
        "overall": overall,
        "regions": regions,
        "difference": str(difference_path),
        "passed": overall["passed"] and all(region["passed"] for region in regions.values()),
    }
    report_path = args.output / f"{args.state}-pixel-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"report": str(report_path), "passed": report["passed"], "overall": overall}))
    if not report["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
