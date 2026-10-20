"""Generate transparent live Settings overlays from neutral and approved states."""

from __future__ import annotations

from pathlib import Path
import hashlib
import json

from PIL import Image

try:
    from .png_to_rgb565a8 import decode
except ImportError:
    from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]

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


def diff_overlay(target: Image.Image, base: Image.Image, *, edge_margin: int = 3) -> Image.Image:
    """Keep approved changed pixels, including dark cover pixels over baked white art."""
    output = Image.new("RGBA", target.size)
    source = target.load()
    baked = base.load()
    pixels = output.load()
    for y in range(target.height):
        for x in range(target.width):
            rgb = source[x, y]
            changed = rgb != baked[x, y]
            edge = x < edge_margin or y < edge_margin or x >= target.width - edge_margin or y >= target.height - edge_margin
            # Component masks are explicit artwork, not a translucent filter:
            # changed pixels must fully replace baked white glyphs. The border
            # mask below is the only place where the measured glow ramp uses
            # partial alpha.
            alpha = 255 if changed and not (edge and edge_margin > 0) else 0
            pixels[x, y] = (*rgb, alpha)
    return output


def border_overlay(target: Image.Image, base: Image.Image) -> Image.Image:
    """Extract only the approved rounded perimeter, never the row interior."""
    output = Image.new("RGBA", target.size)
    source, baked, pixels = target.load(), base.load(), output.load()
    for y in range(target.height):
        for x in range(target.width):
            distance = min(x, y, target.width - 1 - x, target.height - 1 - y)
            rgb, neutral = source[x, y], baked[x, y]
            delta = max(abs(rgb[channel] - neutral[channel]) for channel in range(3))
            # Keep the glow through the outer seven pixels; the declared
            # interior acceptance region must remain the neutral base.
            changed = rgb != neutral and distance < 8
            pixels[x, y] = (*rgb, min(255, delta * 3) if changed else 0)
    return output


def write_overlay(image: Image.Image, name: str, root: Path) -> None:
    png = root / "shared/assets" / f"{name}.png"
    binary = png.with_suffix(".bin")
    image.save(png, format="PNG")
    _, _, payload = decode(png)
    binary.write_bytes(payload)


def main(root: Path = ROOT) -> None:
    neutral = Image.open(root / "shared/references/phase1-settings/settings-neutral-466.png").convert("RGB")
    sources = json.loads((root / "shared/settings-overlay-sources.json").read_text())["status_components"]
    if set(sources) != {"bluetooth", "wifi", "cloud"}:
        raise ValueError("exactly three independent status sources are required")
    # Validate every independent input before touching any generated output.
    status_inputs = []
    for entry in sources.values():
        source = root / "shared" / entry["file"]
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"status source checksum mismatch: {source}")
        with Image.open(source) as image:
            if image.mode != "RGBA" or image.size != (entry["width"], entry["height"]):
                raise ValueError(f"status source must retain RGBA dimensions: {source}")
        _, _, payload = decode(source)
        status_inputs.append((entry["output"], source.read_bytes(), payload))
    for row, (y, reference_name) in ROWS.items():
        reference = Image.open(root / "shared/references/phase1-settings" / reference_name).convert("RGB")
        for component, (x, width, height) in COMPONENTS.items():
            target = reference.crop((x, y, x + width, y + height))
            base = neutral.crop((x, y, x + width, y + height))
            write_overlay(diff_overlay(target, base), f"settings-{row}-pressed-{component}-mask-{width}x{height}", root)

    reference = Image.open(root / "shared/references/phase1-settings/settings-wifi-pressed-466.png").convert("RGB")
    write_overlay(
        border_overlay(reference.crop((72, 127, 394, 183)), neutral.crop((72, 127, 394, 183))),
        "settings-pressed-border-mask-322x56",
        root,
    )

    # Preserve intentional alpha and dark cover pixels. Outputs are never inputs.
    for filename, png_bytes, payload in status_inputs:
        target = root / "shared/assets" / filename
        target.write_bytes(png_bytes)
        target.with_suffix(".bin").write_bytes(payload)


if __name__ == "__main__":
    main()
