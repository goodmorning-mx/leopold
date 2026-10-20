"""Derive the canonical settings gears wallpaper for Home and Settings panels."""
from pathlib import Path
import hashlib, json
from PIL import Image, ImageEnhance, ImageFilter
from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/settings-gears-2026-10-20/source.jpeg"
OUT = ROOT / "shared/assets/settings-gears-wallpaper-466.png"
BIN = OUT.with_suffix(".bin")

def compose():
    manifest = json.loads((ROOT / "shared/references/settings-glass-gears-2026-10-20/composition.json").read_text())
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != manifest["background_source"]["sha256"]:
        raise ValueError("Wallpaper source checksum mismatch")
    image = Image.open(SOURCE).convert("RGB").resize((466, 466), Image.Resampling.LANCZOS)
    image = ImageEnhance.Color(image).enhance(1.25)
    red, green, blue = image.split()
    image = Image.merge("RGB", (red.point(lambda value: round(value * .82)), green, blue.point(lambda value: min(255, round(value * 1.12)))))
    image = image.filter(ImageFilter.GaussianBlur(1))
    return image.convert("RGBA")

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = compose()
    if args.check:
        if Image.open(OUT).convert("RGBA").tobytes() != result.tobytes():
            raise SystemExit("Wallpaper derivation mismatch")
        _, _, payload = decode(OUT)
        if BIN.read_bytes() != payload:
            raise SystemExit("Wallpaper device bytes mismatch")
        print("Canonical gears wallpaper verified")
        return
    result.save(OUT)
    _, _, payload = decode(OUT)
    BIN.write_bytes(payload)

if __name__ == "__main__":
    main()
