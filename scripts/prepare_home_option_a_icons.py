"""Derive Home and live status symbols from the approved Option A pixels."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageFilter, ImageOps
from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/home-option-a-20261028/home-source.png"
OUTPUT_PNG = ROOT / "shared/assets/home-settings-gear-64.png"
OUTPUT_BIN = ROOT / "shared/assets/home-settings-gear-64.bin"
REFERENCE_KEY = "owner_home_option_a_20261028"
SOURCE_SHA256 = "885321714b5448759898c5615ed9260516a7d3416981ef4ec5af29d41a867069"
# Source artwork places the gear over a black/white split glass circle. The left
# half has black behind it, so this crop captures actual gear pixels cleanly.
SOURCE_CROP = [968, 343, 1006, 419]
THRESHOLD = 168
SIZE = 64
STATUS_SOURCE = ROOT / "shared/references/home-option-a-20261028/shared-banner-source.png"
STATUS_SOURCE_SHA256 = "fe1433009981c5702a892097804bc6f0bb22d03c69ea5614480d80cbde98661e"
STATUS_GLYPHS = {
    "status-cloud-icon": ([48, 158, 96, 199], 14, 14),
    "status-bluetooth-icon": ([108, 158, 141, 199], 10, 14),
    "status-wifi-icon": ([151, 158, 201, 199], 14, 14),
}
CLOCK_CROP = [433, 150, 510, 235]
USB_CROP = [690, 385, 770, 465]
HOME_RIM_CROP = [207, 192, 346, 332]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def derive(source: Image.Image) -> Image.Image:
    crop = source.crop(tuple(SOURCE_CROP)).convert("RGB")
    # Bright neutral pixels are the white gear; reject dark glass and edge tint.
    mask = Image.new("L", crop.size)
    src = crop.load()
    dst = mask.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b = src[x, y]
            low, high = min(r, g, b), max(r, g, b)
            value = min(255, max(0, int((low - THRESHOLD) * 255 / (255 - THRESHOLD))))
            if high - low > 28:
                value = 0
            dst[x, y] = value
    # The reference draws only the left half against dark glass; mirror those
    # sampled pixels around the measured shaft center to recover the silhouette.
    mirrored = ImageOps.mirror(mask)
    full = Image.new("L", (mask.width * 2, mask.height))
    full.paste(mask, (0, 0))
    full.paste(mirrored, (mask.width, 0))
    full = full.filter(ImageFilter.GaussianBlur(0.25))
    fitted = ImageOps.contain(full, (SIZE - 8, SIZE - 8), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (SIZE, SIZE), 0)
    canvas.paste(fitted, ((SIZE - fitted.width) // 2, (SIZE - fitted.height) // 2))
    rgba = Image.new("RGBA", (SIZE, SIZE), (246, 250, 255, 0))
    rgba.putalpha(canvas)
    return rgba


def derive_status(source: Image.Image, crop_bounds: list[int], size: tuple[int, int]) -> Image.Image:
    crop = source.crop(tuple(crop_bounds)).convert("RGB")
    alpha = Image.new("L", crop.size)
    src, dst = crop.load(), alpha.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b = src[x, y]
            low = min(r, g, b)
            dst[x, y] = min(255, max(0, int((low - 160) * 255 / (255 - 160))))
    bbox = alpha.point(lambda value: 255 if value >= 96 else 0).getbbox()
    if bbox is None:
        raise ValueError(f"No white source glyph found in crop {crop_bounds}")
    alpha = alpha.crop(bbox)
    contained = ImageOps.contain(alpha, size, Image.Resampling.LANCZOS)
    canvas = Image.new("L", size, 0)
    canvas.paste(contained, ((size[0] - contained.width) // 2, (size[1] - contained.height) // 2))
    rgba = Image.new("RGBA", size, (246, 250, 255, 0))
    rgba.putalpha(canvas)
    return rgba


def fit_alpha(alpha: Image.Image, size: tuple[int, int]) -> Image.Image:
    bounds = alpha.point(lambda value: 255 if value >= 96 else 0).getbbox()
    if bounds is None:
        raise ValueError("No reference icon silhouette passed the source threshold")
    alpha = alpha.crop(bounds)
    scaled = ImageOps.contain(alpha, size, Image.Resampling.LANCZOS)
    canvas = Image.new("L", size, 0)
    canvas.paste(scaled, ((size[0] - scaled.width) // 2, (size[1] - scaled.height) // 2))
    return canvas


def derive_usb(source: Image.Image) -> Image.Image:
    crop = source.crop(tuple(USB_CROP)).convert("RGB")
    alpha = Image.new("L", crop.size)
    src, dst = crop.load(), alpha.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b = src[x, y]
            strength = min(b - r - 28, b - g - 20, b - 105)
            dst[x, y] = min(255, max(0, strength * 255 // 72))
    mask = fit_alpha(alpha, (32, 48))
    rgba = Image.new("RGBA", (32, 48), (246, 250, 255, 0))
    rgba.putalpha(mask)
    return rgba


def derive_clock(source: Image.Image) -> Image.Image:
    crop = source.crop(tuple(CLOCK_CROP)).convert("RGB")
    left = Image.new("L", crop.size)
    src, dst = crop.load(), left.load()
    center = 471 - CLOCK_CROP[0]
    for y in range(crop.height):
        for x in range(min(center, crop.width)):
            r, g, b = src[x, y]
            low, high = min(r, g, b), max(r, g, b)
            if high - low <= 64:
                dst[x, y] = min(255, max(0, int((low - 145) * 255 / (255 - 145))))
    mirrored = ImageOps.mirror(left.crop((0, 0, center, crop.height)))
    white = Image.new("L", crop.size)
    white.paste(left.crop((0, 0, center, crop.height)), (0, 0))
    white.paste(mirrored, (center, 0))
    blue = Image.new("L", crop.size)
    bdst = blue.load()
    for y in range(crop.height):
        for x in range(crop.width):
            # Keep only the central second hand and hub, excluding the blue rim.
            sx, sy = x + CLOCK_CROP[0], y + CLOCK_CROP[1]
            if not (464 <= sx <= 478 and 178 <= sy <= 234):
                continue
            r, g, b = src[x, y]
            strength = min(b - r - 24, g - r - 14, b - 100)
            bdst[x, y] = min(255, max(0, strength * 255 // 80))
    canvas_size = (80, 80)
    white = fit_alpha(white, canvas_size)
    blue = fit_alpha(blue, canvas_size)
    rgba = Image.new("RGBA", canvas_size, (246, 250, 255, 0))
    rgba.putalpha(white)
    blue_layer = Image.new("RGBA", canvas_size, (28, 184, 255, 0))
    blue_layer.putalpha(blue)
    rgba.alpha_composite(blue_layer)
    return rgba


def derive_home_rim(source: Image.Image) -> Image.Image:
    crop = source.crop(tuple(HOME_RIM_CROP)).convert("RGB")
    src = crop.load()
    alpha = Image.new("L", crop.size, 0)
    pixels = alpha.load()
    rgba = Image.new("RGBA", crop.size, (0, 0, 0, 0))
    output = rgba.load()
    cx, cy = 69.5, 70.0
    smooth = lambda lo, hi, value: max(0.0, min(1.0, (value - lo) / (hi - lo))) ** 2 * (3 - 2 * max(0.0, min(1.0, (value - lo) / (hi - lo))))
    for y in range(crop.height):
        for x in range(crop.width):
            dx, dy = x - cx, y - cy
            radius = (dx * dx + dy * dy) ** 0.5
            outer_band = (1.0 - smooth(66.0, 70.0, radius)) * smooth(53.0, 57.0, radius)
            r, g, b = src[x, y]
            intensity = max(r, g, b)
            a = int(intensity * outer_band)
            angle = math.degrees(math.atan2(dy, dx)) % 360.0
            spread = max(r, g, b) - min(r, g, b)
            if 49.0 <= radius <= 64.0 and 205.0 <= angle <= 270.0 and spread <= 72:
                a = max(a, int(intensity * smooth(45.0, 62.0, radius)))
            if intensity:
                output[x, y] = (min(255, r * 255 // intensity),
                                min(255, g * 255 // intensity),
                                min(255, b * 255 // intensity), a)
            pixels[x, y] = a
    fitted = rgba.resize((64, 64), Image.Resampling.LANCZOS)
    # Threshold interpolation residue back to zero; this keeps the entire
    # center and glyph fully transparent at the asset's final resolution.
    a = fitted.getchannel("A")
    a = a.point(lambda value: 0 if value < 5 else value)
    fitted.putalpha(a)
    return fitted


def write_asset(logical_id: str, image: Image.Image, source_path: Path,
                source_hash: str, source_dimensions: tuple[int, int],
                source_crop: list[int], role: str, method: str,
                output_png_name: str | None = None, output_bin_name: str | None = None) -> dict:
    output_png = ROOT / "shared/assets" / (output_png_name or f"{logical_id}.png")
    output_bin = ROOT / "shared/assets" / (output_bin_name or f"{logical_id}.bin")
    # Keep the checked-in encoding when only the PNG compressor would differ.
    same_pixels = False
    if output_png.is_file():
        with Image.open(output_png) as existing:
            same_pixels = (
                existing.size == image.size
                and existing.mode == image.mode
                and existing.tobytes() == image.tobytes()
            )
    if not same_pixels:
        image.save(output_png, optimize=False)
    decoded_width, decoded_height, payload = decode(output_png)
    if (decoded_width, decoded_height) != image.size:
        raise ValueError(f"Generated asset {logical_id} has unexpected dimensions")
    output_bin.write_bytes(payload)
    return {
        "logical_id": logical_id, "file": output_bin.name,
        "width": image.width, "height": image.height, "format": "rgb565a8",
        "derived_from": output_png.name, "source": str(source_path.relative_to(ROOT)),
        "source_sha256": source_hash, "source_dimensions": list(source_dimensions),
        "source_crop": source_crop, "source_crop_role": role,
        "output_png": str(output_png.relative_to(ROOT)),
        "output_rgb565a8": str(output_bin.relative_to(ROOT)),
        "resampling": method, "png_sha256": digest(output_png.read_bytes()),
        "output_png_sha256": digest(output_png.read_bytes()),
        "device_sha256": digest(payload), "bytes": len(payload),
        "sha256": digest(payload), "output_sha256": digest(payload),
        "delivery_class": "essential",
    }


def generate() -> None:
    original = SOURCE.read_bytes()
    if digest(original) != SOURCE_SHA256:
        raise ValueError("Option A Home source hash changed; refusing to derive an icon")
    source = Image.open(SOURCE).convert("RGB")
    width, height = source.size
    image = derive(source)
    gear = write_asset("home-settings-gear", image, SOURCE, SOURCE_SHA256,
        (width, height), SOURCE_CROP, "actual left half of Option A Settings gear on dark glass",
        "mirror sampled source half at shaft center; 0.25px blur; Lanczos fit to 56px in transparent 64px canvas; RGBA to RGB565A8",
        output_png_name="home-settings-gear-64.png", output_bin_name="home-settings-gear-64.bin")
    status_original = STATUS_SOURCE.read_bytes()
    if digest(status_original) != STATUS_SOURCE_SHA256:
        raise ValueError("Option A banner source hash changed; refusing to derive status icons")
    status_source = Image.open(STATUS_SOURCE).convert("RGB")
    status_width, status_height = status_source.size
    status_assets = []
    for key, (crop, icon_width, icon_height) in STATUS_GLYPHS.items():
        glyph = derive_status(status_source, crop, (icon_width, icon_height))
        status_assets.append(write_asset(key, glyph, STATUS_SOURCE, STATUS_SOURCE_SHA256,
            (status_width, status_height), crop, "bright connectivity glyph from the shared banner reference",
            "channel-min threshold begins at160; retain alpha holes; crop threshold bbox; Lanczos contain to declared pixels; RGBA to RGB565A8"))
    clock_icon = write_asset("open-clock", derive_clock(source), SOURCE, SOURCE_SHA256,
        (width, height), CLOCK_CROP, "analog clock hands and cyan second hand from the top Home bubble",
        "threshold white hour/minute hand from dark half and mirror about shaft; isolate source cyan hand/hub; Lanczos fit to 80x80; RGBA to RGB565A8",
        output_png_name="open-clock-reference.png", output_bin_name="icon_clock.bin")
    usb_icon = write_asset("home-usb-icon", derive_usb(source), SOURCE, SOURCE_SHA256,
        (width, height), USB_CROP, "blue USB symbol from the Home reference bubble",
        "isolate saturated blue source pixels to alpha; threshold-crop and Lanczos fit to 32x48; RGBA to RGB565A8")
    rim_icon = write_asset("home-glass-rim", derive_home_rim(source), SOURCE, SOURCE_SHA256,
        (width, height), HOME_RIM_CROP, "Navigation button annular glass rim and upper-left specular glint; interior and glyph excluded",
        "recover color/alpha against the black reference half; radial annulus r53-70 plus neutral upper-left glint r49-64; Lanczos to 64x64 transparent RGB565A8",
        output_png_name="home-glass-rim-64.png", output_bin_name="home-glass-rim-64.bin")

    asset_path = ROOT / "shared/asset-manifest.json"
    manifest = json.loads(asset_path.read_text(encoding="utf-8"))
    manifest["assets"]["home-settings-gear"] = gear
    for record in status_assets:
        manifest["assets"][record["logical_id"]] = record
    manifest["assets"]["open-clock"].update(clock_icon)
    manifest["assets"]["open-clock"]["file"] = "icon_clock.bin"
    manifest["assets"]["open-clock"].pop("logical_id", None)
    manifest["assets"]["home-usb-icon"] = usb_icon
    manifest["assets"]["home-glass-rim"] = rim_icon
    asset_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    reference_path = ROOT / "shared/reference-manifest.json"
    references = json.loads(reference_path.read_text(encoding="utf-8"))
    derived = [{
        "logical_id": "home-settings-gear", "source": str(SOURCE.relative_to(ROOT)),
        "source_sha256": SOURCE_SHA256, "source_dimensions": [width, height],
        "source_crop": SOURCE_CROP, "threshold": THRESHOLD,
        "output_png": str(OUTPUT_PNG.relative_to(ROOT)),
        "output_png_sha256": gear["png_sha256"],
        "output_rgb565a8": str(OUTPUT_BIN.relative_to(ROOT)), "output_sha256": gear["sha256"],
        "dimensions": [SIZE, SIZE],
        "method": "Threshold neutral white gear pixels from the source's dark-background half, mirror around its shaft center, antialias and resample; preserves source tooth contour and bore pixels.",
    }]
    for record in status_assets:
        derived.append({key: record[key] for key in ("logical_id", "source", "source_sha256", "source_dimensions", "source_crop", "output_png", "output_png_sha256", "output_rgb565a8", "output_sha256", "width", "height", "resampling")})
    derived.extend([
        {"logical_id":"open-clock","source":str(SOURCE.relative_to(ROOT)),"source_sha256":SOURCE_SHA256,"source_dimensions":[width,height],"source_crop":CLOCK_CROP,"output_png":"shared/assets/open-clock-reference.png","output_png_sha256":clock_icon["png_sha256"],"output_rgb565a8":"shared/assets/icon_clock.bin","output_sha256":clock_icon["sha256"],"dimensions":[80,80],"method":clock_icon["resampling"]},
        {"logical_id":"home-usb-icon","source":str(SOURCE.relative_to(ROOT)),"source_sha256":SOURCE_SHA256,"source_dimensions":[width,height],"source_crop":USB_CROP,"output_png":"shared/assets/home-usb-icon.png","output_png_sha256":usb_icon["png_sha256"],"output_rgb565a8":"shared/assets/home-usb-icon.bin","output_sha256":usb_icon["sha256"],"dimensions":[32,48],"method":usb_icon["resampling"]},
        {"logical_id":"home-glass-rim","source":str(SOURCE.relative_to(ROOT)),"source_sha256":SOURCE_SHA256,"source_dimensions":[width,height],"source_crop":HOME_RIM_CROP,"output_png":"shared/assets/home-glass-rim-64.png","output_png_sha256":rim_icon["png_sha256"],"output_rgb565a8":"shared/assets/home-glass-rim-64.bin","output_sha256":rim_icon["sha256"],"dimensions":[64,64],"method":rim_icon["resampling"]},
    ])
    references[REFERENCE_KEY]["derived_icons"] = derived
    reference_path.write_text(json.dumps(references, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    generate()
