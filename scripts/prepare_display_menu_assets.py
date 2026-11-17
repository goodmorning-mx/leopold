"""Register supplied Display menu icon art and native-size previews."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw

from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
REFERENCES = ROOT / "shared/references/display-assets"
ASSETS = ROOT / "shared/assets"
ICON_FILES = {
    "display-icon-background": "icon_background.png",
    "display-icon-appearance": "icon_appearance.png",
    "display-icon-clock": "icon_clock.png",
    "display-icon-units": "icon_units.png",
    "display-icon-language": "icon_language.png",
    "display-icon-weather-location": "icon_weather_location.png",
    "display-icon-display": "icon_display.png",
}

BACKGROUND_SOURCES = {
    "background-preview-flower": "background-flower-466.png",
    "background-preview-chevron": "background-chevron-466.png",
    "background-preview-mechanical": "background-mechanical-flower-466.png",
    "background-preview-black-white-circle": "background-black-white-circle-466.png",
}
CLOCK_BACKGROUND_SOURCES = {
    "clock-background-preview-flower": "background-flower-466.png",
    "clock-background-preview-chevron": "background-chevron-466.png",
    "clock-background-preview-mechanical": "background-mechanical-flower-466.png",
}
CLOCK_REFERENCE = ROOT / "shared/references/display-submenus/Sub Menu/Display_Clock_Submenu.png"
CLOCK_FACE_CROPS = [(54, 177, 94, 217), (101, 177, 141, 217), (148, 177, 188, 217)]


def make_background_icon(path: Path) -> tuple[int, int]:
    """Create a compact photo/landscape glyph so Background has a clear icon."""
    scale = 4
    image = Image.new("RGBA", (30 * scale, 30 * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    white = (238, 246, 255, 255)
    width = 2 * scale
    draw.rounded_rectangle((3 * scale, 4 * scale, 27 * scale, 26 * scale), radius=3 * scale, outline=white, width=width)
    draw.ellipse((18 * scale, 8 * scale, 22 * scale, 12 * scale), outline=white, width=width)
    draw.line([(5 * scale, 22 * scale), (12 * scale, 14 * scale), (16 * scale, 18 * scale), (19 * scale, 15 * scale), (25 * scale, 22 * scale)], fill=white, width=width, joint="curve")
    image.resize((30, 30), Image.Resampling.LANCZOS).save(path)
    return 30, 30


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def register_asset(key: str, source: Path, output_name: str, width: int, height: int,
                   *, crop: tuple[int, int, int, int] | None = None,
                   rounded_alpha: int | None = None) -> dict[str, object]:
    output_png = ASSETS / output_name
    with Image.open(source) as opened:
        image = opened.convert("RGBA")
        if crop is not None:
            image = image.crop(crop)
        if image.size != (width, height):
            raise ValueError(f"{key} dimensions differ: {image.size} != {(width, height)}")
    if rounded_alpha is not None:
        scale = 4
        mask = Image.new("L", (width * scale, height * scale), 0)
        ImageDraw.Draw(mask).rounded_rectangle(
            (0, 0, (width - 1) * scale, (height - 1) * scale),
            radius=rounded_alpha * scale,
            fill=255,
        )
        image.putalpha(mask.resize((width, height), Image.Resampling.LANCZOS))
    image.save(output_png, format="PNG", optimize=False)
    out_width, out_height, payload = decode(output_png)
    if (out_width, out_height) != (width, height):
        raise ValueError(f"encoded dimensions differ for {key}")
    output_bin = output_png.with_suffix(".bin")
    output_bin.write_bytes(payload)
    return {
        "key": key,
        "source": str(source.relative_to(ROOT)),
        "source_sha256": digest(source),
        "source_dimensions": list(Image.open(source).size),
        "crop": list(crop) if crop is not None else None,
        "output_png": str(output_png.relative_to(ROOT)),
        "output_png_sha256": digest(output_png),
        "output_bin": str(output_bin.relative_to(ROOT)),
        "output_bin_sha256": digest(output_bin),
        "dimensions": [width, height],
        "format": "RGBA PNG / RGB565A8 runtime",
        "alpha_mask": f"rounded rectangle radius {rounded_alpha}px" if rounded_alpha is not None else "preserved exactly",
    }


def main() -> None:
    records = []
    manifest_path = ROOT / "shared/asset-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    delivery_classes = {key: entry.get("delivery_class") for key, entry in manifest["assets"].items()}
    manifest["assets"].pop("settings-status-banner", None)
    for key, filename in ICON_FILES.items():
        if key == "display-icon-background":
            source = ASSETS / "display-icon-background.svg"
            source.write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">'
                '<rect x="3.75" y="4.75" width="22.5" height="20.5" rx="3" fill="none" stroke="#EEF6FF" stroke-width="1.8"/>'
                '<circle cx="20" cy="10" r="1.6" fill="none" stroke="#EEF6FF" stroke-width="1.8"/>'
                '<path d="M5.5 22 12 14.5 16 18.5 19 15.5 24.5 22" fill="none" stroke="#EEF6FF" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>'
                '</svg>\n', encoding="utf-8",
            )
            output_png = ASSETS / f"{key}.png"
            make_background_icon(output_png)
            _, _, payload = decode(output_png)
            output_bin = output_png.with_suffix(".bin")
            output_bin.write_bytes(payload)
            record = {
                "key": key, "source": str(source.relative_to(ROOT)), "source_sha256": digest(source),
                "source_dimensions": [30, 30], "crop": None,
                "output_png": str(output_png.relative_to(ROOT)), "output_png_sha256": digest(output_png),
                "output_bin": str(output_bin.relative_to(ROOT)), "output_bin_sha256": digest(output_bin),
                "dimensions": [30, 30], "format": "RGBA PNG / RGB565A8 runtime",
                "alpha_mask": "preserved exact white landscape/photo vector",
            }
            records.append(record)
            manifest["assets"][key] = {
                "file": output_bin.name, "width": 30, "height": 30, "format": "rgb565a8",
                "derived_from": source.name, "source": record["source"],
                "source_sha256": record["source_sha256"], "png_sha256": record["output_png_sha256"],
                "device_sha256": record["output_bin_sha256"],
                "resampling": "4x antialiased vector rasterization at native 30x30",
            }
            continue
        else:
            source = REFERENCES / filename
            with Image.open(source) as image:
                dimensions = image.size
        width, height = dimensions
        record = register_asset(key, source, f"{key}.png", width, height)
        records.append(record)
        manifest["assets"][key] = {
            "file": Path(record["output_bin"]).name,
            "width": width,
            "height": height,
            "format": "rgb565a8",
            "derived_from": Path(record["output_png"]).name,
            "source": record["source"],
            "source_sha256": record["source_sha256"],
            "png_sha256": record["output_png_sha256"],
            "device_sha256": record["output_bin_sha256"],
            "resampling": "generated 4x antialiased landscape/photo glyph" if key == "display-icon-background" else "no scaling; source pixels preserved",
        }

    # LVGL image transforms are disabled in the browser/device build. Ship
    # small native-size composite previews instead of shrinking 466px artwork
    # at runtime, which otherwise produces blank gallery cells.
    for key, filename in BACKGROUND_SOURCES.items():
        source = ASSETS / filename
        with Image.open(source) as opened:
            art = opened.convert("RGBA")
        tile = Image.new("RGBA", (88, 88), (8, 16, 27, 255))
        art.thumbnail((88, 88), Image.Resampling.LANCZOS)
        tile.alpha_composite(art, ((88 - art.width) // 2, (88 - art.height) // 2))
        preview_path = ASSETS / f"{key}.png"
        tile.save(preview_path, format="PNG", optimize=False)
        _, _, payload = decode(preview_path)
        bin_path = preview_path.with_suffix(".bin")
        bin_path.write_bytes(payload)
        manifest["assets"][key] = {
            "file": bin_path.name, "width": 88, "height": 88, "format": "rgb565a8",
            "derived_from": filename, "source": f"shared/assets/{filename}",
            "source_sha256": digest(source), "png_sha256": digest(preview_path),
            "device_sha256": digest(bin_path),
            "resampling": "Lanczos thumbnail composited over #08101B at native 88x88",
        }
        records.append({
            "key": key, "source": f"shared/assets/{filename}", "source_sha256": digest(source),
            "source_dimensions": [466, 466], "crop": None,
            "output_png": f"shared/assets/{preview_path.name}", "output_png_sha256": digest(preview_path),
            "output_bin": f"shared/assets/{bin_path.name}", "output_bin_sha256": digest(bin_path),
            "dimensions": [88, 88], "format": "RGBA PNG / RGB565A8 runtime",
            "alpha_mask": "opaque preview tile with background alpha composited",
        })

    for key, filename in CLOCK_BACKGROUND_SOURCES.items():
        source = ASSETS / filename
        with Image.open(source) as opened:
            art = opened.convert("RGBA")
        tile = Image.new("RGBA", (88, 32), (8, 16, 27, 255))
        art.thumbnail((88, 32), Image.Resampling.LANCZOS)
        tile.alpha_composite(art, ((88 - art.width) // 2, (32 - art.height) // 2))
        preview_path = ASSETS / f"{key}.png"
        tile.save(preview_path, format="PNG", optimize=False)
        _, _, payload = decode(preview_path)
        bin_path = preview_path.with_suffix(".bin")
        bin_path.write_bytes(payload)
        manifest["assets"][key] = {
            "file": bin_path.name, "width": 88, "height": 32, "format": "rgb565a8",
            "derived_from": filename, "source": f"shared/assets/{filename}",
            "source_sha256": digest(source), "png_sha256": digest(preview_path),
            "device_sha256": digest(bin_path),
            "resampling": "Lanczos thumbnail composited over #08101B at native 88x32",
        }
        records.append({
            "key": key, "source": f"shared/assets/{filename}", "source_sha256": digest(source),
            "source_dimensions": [466, 466], "crop": None,
            "output_png": f"shared/assets/{preview_path.name}", "output_png_sha256": digest(preview_path),
            "output_bin": f"shared/assets/{bin_path.name}", "output_bin_sha256": digest(bin_path),
            "dimensions": [88, 32], "format": "RGBA PNG / RGB565A8 runtime",
            "alpha_mask": "opaque preview tile with background alpha composited",
        })

    with Image.open(CLOCK_REFERENCE) as opened:
        clock_reference = opened.convert("RGBA")
    for index, bounds in enumerate(CLOCK_FACE_CROPS):
        face = clock_reference.crop(bounds).resize((46, 46), Image.Resampling.LANCZOS)
        mask = Image.new("L", face.size, 0)
        ImageDraw.Draw(mask).ellipse((1, 1, 44, 44), fill=255)
        face.putalpha(mask)
        key = f"clock-face-preview-{index}"
        preview_path = ASSETS / f"{key}.png"
        face.save(preview_path, format="PNG", optimize=False)
        _, _, payload = decode(preview_path)
        bin_path = preview_path.with_suffix(".bin")
        bin_path.write_bytes(payload)
        manifest["assets"][key] = {
            "file": bin_path.name, "width": 46, "height": 46, "format": "rgb565a8",
            "derived_from": str(CLOCK_REFERENCE.relative_to(ROOT)),
            "source": str(CLOCK_REFERENCE.relative_to(ROOT)), "source_sha256": digest(CLOCK_REFERENCE),
            "crop": list(bounds), "png_sha256": digest(preview_path), "device_sha256": digest(bin_path),
            "resampling": "reference crop resized Lanczos to 46x46 with circular alpha mask",
        }
        records.append({
            "key": key, "source": str(CLOCK_REFERENCE.relative_to(ROOT)), "source_sha256": digest(CLOCK_REFERENCE),
            "source_dimensions": list(clock_reference.size), "crop": list(bounds),
            "output_png": f"shared/assets/{preview_path.name}", "output_png_sha256": digest(preview_path),
            "output_bin": f"shared/assets/{bin_path.name}", "output_bin_sha256": digest(bin_path),
            "dimensions": [46, 46], "format": "RGBA PNG / RGB565A8 runtime",
            "alpha_mask": "circular face crop",
        })

    for record in records:
        key = record["key"]
        entry = manifest["assets"][key]
        encoded = (ASSETS / entry["file"]).read_bytes()
        encoded_sha256 = hashlib.sha256(encoded).hexdigest()
        entry.update({
            "bytes": len(encoded),
            "sha256": encoded_sha256,
            "logical_id": key,
            "delivery_class": delivery_classes.get(key) or ("preview" if "preview" in key else "optional"),
        })
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    references = []
    for filename in ("banner_status.png", "banner_selected_row.png"):
        source = REFERENCES / filename
        with Image.open(source) as image:
            references.append({"file": str(source.relative_to(ROOT)), "sha256": digest(source), "dimensions": list(image.size), "role": "visual reference; preserved without alteration"})
    out = {
        "schema_version": 1,
        "purpose": "Source identity and runtime conversions for Display menu icons and native-size background/clock previews",
        "records": records,
        "additional_references": references,
    }
    (ROOT / "shared/display-menu-asset-manifest.json").write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
