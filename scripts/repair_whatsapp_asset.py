"""Recover alpha and crisp edges from the retained white-backed 72px WhatsApp mark."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
from PIL import Image
from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/icon_whatsapp-original-72-rgb565a8.bin"
PNG = ROOT / "shared/assets/icon_whatsapp.png"
BINARY = ROOT / "shared/assets/icon_whatsapp.bin"
MANIFEST = ROOT / "shared/asset-manifest.json"


def repair() -> tuple[str, str]:
    original = SOURCE.read_bytes()
    width = height = 72
    count = width * height
    if len(original) != count * 3:
        raise ValueError("retained WhatsApp source must be RGB565A8 72x72")
    rgb = Image.frombytes("RGB", (width, height), original[:count * 2], "raw", "BGR;16")
    source = rgb.load()
    output = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    target = output.load()
    # The source is a green mark antialiased against white. Recover coverage
    # from the green channel and un-composite the original brand color.
    for y in range(height):
        for x in range(width):
            r, g, b = source[x, y]
            coverage = max(0.0, min(1.0, (255.0 - g) / 45.0))
            if coverage <= 0.015:
                continue
            channels = [round(max(0, min(255, (value - 255.0 * (1.0 - coverage)) / coverage))) for value in (r, g, b)]
            target[x, y] = (*channels, round(coverage * 255))
    output.save(PNG, optimize=False)
    _, _, data = decode(PNG)
    BINARY.write_bytes(data)
    manifest = json.loads(MANIFEST.read_text())
    item = manifest["assets"]["open-whatsapp"]
    item.update({"file": BINARY.name, "width": width, "height": height, "format": "rgb565a8",
                 "derived_from": PNG.name, "source": "references/icon_whatsapp-original-72-rgb565a8.bin",
                 "source_sha256": hashlib.sha256(original).hexdigest(),
                 "resampling": "white-background alpha recovered from antialiased WhatsApp green; native 72x72, no upscaling",
                 "state": "whatsapp-transparent-brand-mark"})
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")
    return hashlib.sha256(PNG.read_bytes()).hexdigest(), hashlib.sha256(data).hexdigest()


if __name__ == "__main__":
    print(*repair())
