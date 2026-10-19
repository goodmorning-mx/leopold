"""Generate transparent live Settings overlays from neutral and approved states."""

from __future__ import annotations

from pathlib import Path

from PIL import Image

from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
NEUTRAL = Image.open(ROOT / "shared/references/phase1-settings/settings-neutral-466.png").convert("RGB")

ROWS = {
    "wifi": (127, "settings-wifi-pressed-466.png"),
    "bluetooth": (181, "settings-bluetooth-pressed-466.png"),
    "display": (235, "settings-display-pressed-466.png"),
    "sound": (289, "settings-sound-pressed-466.png"),
}
COMPONENTS = {
    "icon": (72, 61, 56),
    "chevron": (355, 39, 56),
}
STATUS = {
    "bluetooth-active": (153, 45, 22, 22),
    "wifi-active": (181, 45, 22, 22),
    "cloud-active": (264, 45, 24, 24),
}


def diff_overlay(target: Image.Image, base: Image.Image) -> Image.Image:
    """Keep only pixels that differ from the baked base; leave edges transparent."""
    output = Image.new("RGBA", target.size)
    source = target.load()
    baked = base.load()
    pixels = output.load()
    for y in range(target.height):
        for x in range(target.width):
            rgb = source[x, y]
            changed = rgb != baked[x, y]
            # The approved pressed crop contains the whole row's dark plate.
            # Keep only bright glyph/glow pixels so that plate pixels cannot
            # become opaque rectangles when composited over the neutral base.
            edge = x < 3 or y < 3 or x >= target.width - 3 or y >= target.height - 3
            pixels[x, y] = (*rgb, 255 if changed and not edge and max(rgb) >= 100 else 0)
    return output


def write_overlay(image: Image.Image, name: str) -> None:
    png = ROOT / "shared/assets" / f"{name}.png"
    binary = png.with_suffix(".bin")
    image.save(png, format="PNG")
    _, _, payload = decode(png)
    binary.write_bytes(payload)


def main() -> None:
    for row, (y, reference_name) in ROWS.items():
        reference = Image.open(ROOT / "shared/references/phase1-settings" / reference_name).convert("RGB")
        for component, (x, width, height) in COMPONENTS.items():
            target = reference.crop((x, y, x + width, y + height))
            base = NEUTRAL.crop((x, y, x + width, y + height))
            write_overlay(diff_overlay(target, base), f"settings-{row}-pressed-{component}-mask-{width}x{height}")

    for name, (x, y, width, height) in STATUS.items():
        source = Image.open(ROOT / "shared/assets" / f"settings-status-{name}-{width}x{height}.png").convert("RGB")
        base = NEUTRAL.crop((x, y, x + width, y + height))
        write_overlay(diff_overlay(source, base), f"settings-status-{name}-{width}x{height}")


if __name__ == "__main__":
    main()
