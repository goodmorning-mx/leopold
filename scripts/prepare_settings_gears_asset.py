"""Compose the user-approved gears backdrop without changing Settings controls.

The original reference and live plate are immutable inputs. Full opacity in the
protected component regions retains their exact bytes; a 12px soft black scrim
outside those regions keeps the busy background from reducing readability.
"""
from pathlib import Path
import hashlib, json
from PIL import Image, ImageDraw, ImageFilter, ImageChops
try:
    from .png_to_rgb565a8 import decode
except ImportError:
    from png_to_rgb565a8 import decode
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared/references/settings-gears-2026-10-20/source.jpeg"
PROTECTED = [(67, 121, 400, 351), (132, 34, 337, 82), (141, 81, 319, 124), (63, 349, 405, 424)]
def compose():
    manifest = json.loads((SOURCE.parent / "composition.json").read_text())
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != manifest["source"]["sha256"]:
        raise ValueError("Background source checksum differs from approved input")
    image = Image.open(SOURCE)
    if image.size != (1024, 1024):
        raise ValueError("Expected original square 1024x1024 background")
    background = image.convert("RGBA").resize((466,466), Image.Resampling.LANCZOS)
    base = Image.open(ROOT / "shared/assets/settings-neutral-466.png").convert("RGBA")
    mask = Image.new("L", (466,466)); draw = ImageDraw.Draw(mask)
    for bounds in PROTECTED: draw.rectangle(bounds, fill=255)
    mask = ImageChops.lighter(mask, mask.filter(ImageFilter.GaussianBlur(12)).point(lambda value: min(255, value * 2)))
    # Composite the existing opaque artwork, with a feathered scrim outside.
    result = Image.composite(base, background, mask)
    return result, mask

def main():
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true', help='Verify derivation without rewriting assets')
    args=parser.parse_args()
    result,mask=compose()
    out=ROOT / "shared/assets/settings-gears-base-466.png"
    if args.check:
        if Image.open(out).convert("RGBA").tobytes() != result.tobytes():
            raise SystemExit("Background composition pixels differ")
        _,_,data=decode(out)
        if out.with_suffix(".bin").read_bytes()!=data:
            raise SystemExit("Background device bytes differ")
        print("Settings gears composition verified; no outputs rewritten")
        return
    result.save(out)
    _,_,data=decode(out);out.with_suffix(".bin").write_bytes(data)
    manifest_path = SOURCE.parent / "composition.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["output"].update(
        png_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),
        device_asset_sha256=hashlib.sha256(out.with_suffix(".bin").read_bytes()).hexdigest(),
    )
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    return result
if __name__ == "__main__": main()
