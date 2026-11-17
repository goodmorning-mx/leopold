#!/usr/bin/env python3
"""Build the main-Settings Display glyph's tintable BHA material from its PNG."""

import hashlib
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/assets/settings-row-display.png"
HEADER = ROOT / "shared/settings-main-icons/settings_display_data.h"
PROVENANCE = ROOT / "shared/settings-main-icons/settings_display_data.json"


def build_material():
    image = Image.open(SOURCE).convert("RGBA")
    if image.size != (40, 40):
        raise ValueError(f"Expected the existing 40x40 Display glyph, got {image.size}")

    pixels = bytearray()
    for red, green, blue, alpha in image.get_flattened_data():
        # The supplied display artwork is neutral grayscale. Convert its
        # luminance into the same body/highlight material channels documented
        # by the prepared main-Settings glyph package, preserving alpha.
        luminance = (299 * red + 587 * green + 114 * blue + 500) // 1000
        highlight = round(max(0, luminance - 160) * 0.65 * 255 / 95)
        body = max(0, luminance - highlight)
        pixels.extend((body, min(255, highlight), alpha))

    if len(pixels) != 40 * 40 * 3:
        raise ValueError("Unexpected Display BHA material length")
    return image, pixels


def emit_header(pixels):
    rows = []
    for offset in range(0, len(pixels), 48):
        rows.append(",".join(str(value) for value in pixels[offset : offset + 48]))
    body = ",\n".join(rows)
    return (
        "#pragma once\n"
        "#include \"settings_icons.h\"\n"
        "namespace leopold_settings_icons {\n"
        "// Derived from the existing Display PNG; source identity is recorded in settings_display_data.json.\n"
        "alignas(4) static const uint8_t data_settings_display_40[] = {\n"
        f"{body}\n"
        "};\n"
        "static const Material kSettingsDisplayMaterial = {\"settings-main/display\",40,40,data_settings_display_40};\n"
        "}\n"
    )


def main():
    image, pixels = build_material()
    source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    HEADER.write_text(emit_header(pixels))
    PROVENANCE.write_text(
        json.dumps(
            {
                "id": "settings-main/display",
                "source": "shared/assets/settings-row-display.png",
                "source_sha256": source_hash,
                "dimensions": list(image.size),
                "source_color_mode": "RGBA",
                "material_format": "BHA8 row-major body/highlight/alpha triples",
                "transform": {
                    "luminance": "round((299*R + 587*G + 114*B) / 1000)",
                    "highlight": "round(max(0, luminance - 160) * 0.65 * 255 / 95)",
                    "body": "max(0, luminance - highlight)",
                    "alpha": "preserved from source PNG",
                    "tint": "existing leopold_settings_icons channel() formula",
                },
                "material_bytes": len(pixels),
                "material_sha256": hashlib.sha256(pixels).hexdigest(),
            },
            indent=2,
        )
        + "\n"
    )
    print(f"Generated {HEADER.relative_to(ROOT)} from {SOURCE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
