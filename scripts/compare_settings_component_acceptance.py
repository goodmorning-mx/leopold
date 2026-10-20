"""Component-level acceptance for reconstructed Settings held-row states."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

from scripts.compare_settings_acceptance import load_framebuffer_asset

ROOT = Path(__file__).resolve().parents[1]
STATES = {
    "settings-wifi": ("settings-wifi-pressed.png", 127),
    "settings-bluetooth": ("settings-bluetooth-pressed.png", 181),
    "settings-display": ("settings-display-pressed.png", 235),
    "settings-sound": ("settings-sound-pressed.png", 289),
}
BANNER_ASSETS = {
    "bluetooth": ("settings-status-bluetooth-active-22x22.bin", (153, 45, 22, 22)),
    "wifi": ("settings-status-wifi-active-22x22.bin", (181, 45, 22, 22)),
    "cloud": ("settings-status-cloud-active-24x24.bin", (264, 45, 24, 24)),
}


def compare(a: Image.Image, b: Image.Image, bounds: tuple[int, int, int, int]) -> dict[str, object]:
    left, top, width, height = bounds
    pa, pb = a.convert("RGB").load(), b.convert("RGB").load()
    changed = maximum = 0
    for y in range(top, top + height):
        for x in range(left, left + width):
            delta = max(abs(pa[x, y][channel] - pb[x, y][channel]) for channel in range(3))
            maximum = max(maximum, delta)
            changed += delta != 0
    return {"bounds": list(bounds), "compared_pixels": width * height, "pixels_over_delta": changed, "maximum_channel_delta": maximum, "passed": changed == 0}


def compare_local(reference: Image.Image, capture: Image.Image) -> dict[str, object]:
    pa, pb = reference.convert("RGB").load(), capture.convert("RGB").load()
    changed = maximum = 0
    for y in range(reference.height):
        for x in range(reference.width):
            delta = max(abs(pa[x, y][channel] - pb[x, y][channel]) for channel in range(3))
            maximum = max(maximum, delta)
            changed += delta != 0
    return {"bounds": [0, 0, reference.width, reference.height], "compared_pixels": reference.width * reference.height, "pixels_over_delta": changed, "maximum_channel_delta": maximum, "passed": changed == 0}


def compare_outside(reference: Image.Image, capture: Image.Image, ignored: list[tuple[int, int, int, int]]) -> dict[str, object]:
    pa, pb = reference.load(), capture.load()
    changed = maximum = 0
    for y in range(466):
        for x in range(466):
            if any(left <= x < left + width and top <= y < top + height for left, top, width, height in ignored):
                continue
            delta = max(abs(pa[x, y][channel] - pb[x, y][channel]) for channel in range(3))
            maximum = max(maximum, delta)
            changed += delta != 0
    return {"ignored_masks": [list(mask) for mask in ignored], "compared_pixels": 466 * 466 - sum(width * height for _, _, width, height in ignored), "pixels_over_delta": changed, "maximum_channel_delta": maximum, "passed": changed == 0}


def compare_component(mask: Image.Image, capture: Image.Image, neutral: Image.Image, bounds: tuple[int, int, int, int]) -> dict[str, object]:
    left, top, width, height = bounds
    pm, pc, pn = mask.convert("RGBA").load(), capture.convert("RGB").load(), neutral.convert("RGB").load()
    changed = maximum = 0
    for y in range(height):
        for x in range(width):
            alpha = pm[x, y][3]
            base = pn[left + x, top + y]
            if not alpha:
                continue
            expected = tuple(round((pm[x, y][channel] * alpha + base[channel] * (255 - alpha)) / 255) for channel in range(3)) if alpha else base
            delta = max(abs(expected[channel] - pc[left + x, top + y][channel]) for channel in range(3))
            maximum = max(maximum, delta)
            changed += delta != 0
    return {"bounds": list(bounds), "compared_pixels": sum(pm[x, y][3] != 0 for y in range(height) for x in range(width)), "pixels_over_delta": changed, "maximum_channel_delta": maximum, "passed": changed == 0}


def transparent_edge(mask: Image.Image) -> dict[str, object]:
    alpha = mask.convert("RGBA").getchannel("A")
    edge = [alpha.getpixel((x, y)) for x in range(alpha.width) for y in range(alpha.height) if x in (0, alpha.width - 1) or y in (0, alpha.height - 1)]
    opaque = sum(value != 0 for value in edge)
    return {"edge_pixels": len(edge), "opaque_edge_pixels": opaque, "passed": opaque == 0}


def compare_seams(reference: Image.Image, capture: Image.Image, regions: list[tuple[int, int, int, int]]) -> dict[str, object]:
    """Reject hard seams by bounding the local edge jump to the approved reference."""
    reference_pixels, capture_pixels = reference.load(), capture.load()
    measurements = []
    passed = True
    for left, top, width, height in regions:
        reference_peak = 0
        capture_peak = 0
        for y in range(top, top + height):
            for x in range(left - 1, left + width):
                reference_peak = max(reference_peak, sum(abs(reference_pixels[x + 1, y][channel] - reference_pixels[x, y][channel]) for channel in range(3)))
                capture_peak = max(capture_peak, sum(abs(capture_pixels[x + 1, y][channel] - capture_pixels[x, y][channel]) for channel in range(3)))
        # The outer capsule edges include LVGL/RGB565 rasterization. Keep the
        # bound relative to the approved edge profile, while still catching
        # the previous hard rectangular cyan edge (large edge jumps).
        allowed = reference_peak + (128 if left == 71 else 64)
        passed = passed and capture_peak <= allowed
        measurements.append({"region": [left, top, width, height], "reference_peak_edge_delta": reference_peak, "capture_peak_edge_delta": capture_peak, "allowed_peak_edge_delta": allowed, "passed": capture_peak <= allowed})
    return {"regions": measurements, "passed": passed}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_dir", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    neutral = load_framebuffer_asset(ROOT / "shared/assets/settings-neutral-466.bin", 466, 466).convert("RGB")
    profile = json.loads((ROOT / "shared/settings-acceptance-profile.json").read_text())
    manifest = json.loads((ROOT / "shared/reference-manifest.json").read_text())
    settings_state = next(state for state in manifest["states"] if state["name"] == "settings")
    shared_dynamic_masks = [(int(mask["x"]), int(mask["y"]), int(mask["width"]), int(mask["height"])) for mask in settings_state["dynamic_masks"]]
    rows: dict[str, object] = {}
    for row_id, (capture_name, y) in STATES.items():
        capture = Image.open(args.capture_dir / capture_name).convert("RGB")
        icon = load_framebuffer_asset(ROOT / "shared/assets" / f"{row_id}-pressed-icon-mask-61x56.bin", 61, 56)
        chevron = load_framebuffer_asset(ROOT / "shared/assets" / f"{row_id}-pressed-chevron-mask-39x56.bin", 39, 56)
        rows[row_id] = {
            "outside_row_mask": compare_outside(neutral, capture, shared_dynamic_masks + [(72, y, 322, 56)]),
            "row_interior_against_neutral": compare(neutral, capture, (133, y + 8, 222, 40)),
            "icon_against_derived_mask": compare_component(icon, capture, neutral, (72, y, 61, 56)),
            "chevron_against_derived_mask": compare_component(chevron, capture, neutral, (355, y, 39, 56)),
            "icon_mask_edge": transparent_edge(icon),
            "chevron_mask_edge": transparent_edge(chevron),
            "seam_boundaries": compare_seams(
                Image.open(ROOT / "shared/references/phase1-settings" / f"{row_id}-pressed-466.png").convert("RGB"),
                capture,
                [(71, y + 6, 2, 44), (390, y + 6, 3, 44)],
            ),
            "profile": profile["rows"][row_id],
        }
    connected = Image.open(args.capture_dir / "settings-connected.png").convert("RGB")
    banner: dict[str, object] = {}
    for name, (filename, bounds) in BANNER_ASSETS.items():
        asset = load_framebuffer_asset(ROOT / "shared/assets" / filename, bounds[2], bounds[3])
        banner[name] = compare_component(asset, connected, neutral, bounds)
        banner[name]["mask_edge"] = transparent_edge(asset)
        banner[name]["profile"] = profile["banner"][name]
    result = {"profile": str((ROOT / "shared/settings-acceptance-profile.json").relative_to(ROOT)), "captures": rows, "passed": all(
        item["outside_row_mask"]["passed"] and item["row_interior_against_neutral"]["passed"] and
        item["icon_against_derived_mask"]["passed"] and item["chevron_against_derived_mask"]["passed"]
        and item["icon_mask_edge"]["passed"] and item["chevron_mask_edge"]["passed"] and item["seam_boundaries"]["passed"]
        for item in rows.values()
    ) and all(item["passed"] and item["mask_edge"]["passed"] for item in banner.values()), "banner": banner}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"report": str(args.output), "passed": result["passed"]}))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
