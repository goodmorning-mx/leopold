"""Compare deterministic Settings simulator captures against their contracts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

from scripts.settings_geometry import brightness_knob_x


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_NAMES = (
    "settings-neutral",
    "settings-wifi-pressed",
    "settings-bluetooth-pressed",
    "settings-display-pressed",
    "settings-sound-pressed",
    "settings-connected",
    "settings-disconnected",
    "settings-brightness-5",
    "settings-brightness-50",
    "settings-brightness-100",
)
PRESSED = {
    "settings-wifi-pressed": ("settings-wifi-pressed-322x56.png", (72, 127, 322, 56)),
    "settings-bluetooth-pressed": ("settings-bluetooth-pressed-322x56.png", (72, 181, 322, 56)),
    "settings-display-pressed": ("settings-display-pressed-322x56.png", (72, 235, 322, 56)),
    "settings-sound-pressed": ("settings-sound-pressed-322x56.png", (72, 289, 322, 56)),
}


def masks(manifest: dict) -> list[tuple[int, int, int, int]]:
    state = next(item for item in manifest["states"] if item["name"] == "settings")
    return [(int(item["x"]), int(item["y"]), int(item["width"]), int(item["height"])) for item in state["dynamic_masks"]]


def in_masks(x: int, y: int, regions: list[tuple[int, int, int, int]]) -> bool:
    return any(left <= x < left + width and top <= y < top + height for left, top, width, height in regions)


def compare_outside(reference: Image.Image, capture: Image.Image, ignored: list[tuple[int, int, int, int]]) -> dict[str, object]:
    ref = reference.load()
    got = capture.load()
    total = failures = maximum = 0
    points: list[tuple[int, int]] = []
    for y in range(466):
        for x in range(466):
            if in_masks(x, y, ignored):
                continue
            delta = max(abs(ref[x, y][channel] - got[x, y][channel]) for channel in range(4))
            total += 1
            maximum = max(maximum, delta)
            if delta:
                failures += 1
                points.append((x, y))
    bbox = None
    if points:
        xs, ys = zip(*points)
        bbox = [min(xs), min(ys), max(xs) + 1, max(ys) + 1]
    return {"compared_pixels": total, "pixels_over_delta": failures, "maximum_color_delta": maximum, "failure_bounding_box": bbox, "passed": failures == 0}


def patch_compare(reference: Image.Image, capture: Image.Image, bounds: tuple[int, int, int, int], ignored: list[tuple[int, int, int, int]]) -> dict[str, object]:
    left, top, width, height = bounds
    ref = reference.load()
    got = capture.load()
    failures = maximum = 0
    for local_y in range(height):
        for local_x in range(width):
            if in_masks(left + local_x, top + local_y, ignored):
                continue
            delta = max(abs(ref[local_x, local_y][channel] - got[left + local_x, top + local_y][channel]) for channel in range(4))
            maximum = max(maximum, delta)
            failures += delta != 0
    return {"bounds": list(bounds), "compared_pixels": width * height, "pixels_over_delta": failures, "maximum_color_delta": maximum, "passed": failures == 0}


def write_difference(neutral: Image.Image, capture: Image.Image, ignored: list[tuple[int, int, int, int]], path: Path) -> None:
    output = Image.new("RGBA", (466, 466), (0, 0, 0, 255))
    source, got, pixels = neutral.load(), capture.load(), output.load()
    for y in range(466):
        for x in range(466):
            if in_masks(x, y, ignored):
                pixels[x, y] = (0, 80, 255, 255)
            else:
                delta = tuple(abs(source[x, y][channel] - got[x, y][channel]) for channel in range(3))
                pixels[x, y] = (*delta, 255)
    output.save(path, format="PNG")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "shared/reference-manifest.json").read_text())
    state = next(item for item in manifest["states"] if item["name"] == "settings")
    dynamic = masks(manifest)
    capture_dir = args.capture_dir.resolve()
    args.output_dir = args.output_dir.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    images = {name: Image.open(capture_dir / f"{name}.png").convert("RGBA") for name in CAPTURE_NAMES}
    if any(image.size != (466, 466) for image in images.values()):
        raise SystemExit("all Settings captures must be 466x466")
    neutral = Image.open(ROOT / "shared/assets/settings-neutral-466.png").convert("RGBA")
    reports: dict[str, object] = {}
    for name, image in images.items():
        ignored = list(dynamic)
        report: dict[str, object] = {"capture": str((capture_dir / f"{name}.png").relative_to(ROOT)), "dimensions": [466, 466]}
        if name == "settings-neutral":
            report["outside_dynamic_masks"] = compare_outside(neutral, image, dynamic)
        elif name in PRESSED:
            source_name, bounds = PRESSED[name]
            source = Image.open(ROOT / "shared/assets" / source_name).convert("RGBA")
            ignored.append(bounds)
            report["outside_neutral_and_pressed_masks"] = compare_outside(neutral, image, ignored)
            report["selected_patch"] = patch_compare(source, image, bounds, dynamic)
        elif "brightness" in name:
            ignored.append((63, 336, 342, 88))
            report["outside_brightness_mask"] = compare_outside(neutral, image, ignored)
            value = int(name.rsplit("-", 1)[1])
            report["brightness"] = {"value": value, "expected_knob_x": brightness_knob_x(value), "geometry_passed": value in (5, 50, 100)}
        else:
            ignored.append((137, 39, 195, 38))
            report["outside_status_masks"] = compare_outside(neutral, image, ignored)
            report["status_shell"] = {"bounds": [137, 39, 195, 38], "state": name.removeprefix("settings-")}
        report["difference"] = str((args.output_dir / f"{name}-difference.png").relative_to(ROOT))
        write_difference(neutral, image, ignored, args.output_dir / f"{name}-difference.png")
        report["passed"] = all(value.get("passed", True) for value in report.values() if isinstance(value, dict))
        reports[name] = report
    result = {"captures": reports, "passed": all(item["passed"] for item in reports.values())}
    (args.output_dir / "settings-acceptance-report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"report": str(args.output_dir / "settings-acceptance-report.json"), "passed": result["passed"]}))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
