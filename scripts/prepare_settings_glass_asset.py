"""Compose the approved continuous gears/glass treatment at native 466x466.

The new supplied composition is styling guidance. Immutable original artwork
still supplies geometry and white glyphs. Retired opaque assets are preserved.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter
try:
    from .png_to_rgb565a8 import decode
except ImportError:
    from png_to_rgb565a8 import decode

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'shared/references/settings-glass-gears-2026-10-20/composition.json'

def background_and_scrim(manifest: dict, root: Path):
    background = Image.open(root / 'shared' / manifest['background_source']['file']).convert('RGB').resize((466, 466), Image.Resampling.LANCZOS)
    background = ImageEnhance.Color(background).enhance(1.25)
    background = Image.merge('RGB', (background.getchannel('R').point(lambda v: round(v * .82)), background.getchannel('G'), background.getchannel('B').point(lambda v: min(255, round(v * 1.12)))))
    background = background.filter(ImageFilter.GaussianBlur(1))
    scrim = Image.new('L', (466, 466))
    draw = ImageDraw.Draw(scrim)
    for box in manifest['composition']['rows']:
        draw.rounded_rectangle(box, radius=24, fill=190)
    draw.rounded_rectangle((137, 39, 332, 77), radius=19, fill=65)
    scrim = scrim.filter(ImageFilter.GaussianBlur(1))
    return background, scrim

def derive_overlays(manifest: dict, root: Path = ROOT):
    background, scrim = background_and_scrim(manifest, root)
    for key, entry in manifest['state_assets'].items():
        source = root / 'shared' / entry['source_asset']
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry['source_sha256']:
            raise ValueError(f'Approved mask source differs: {key}')
        image = Image.open(source).convert('RGBA')
        pixels = image.load()
        left, top, width, height = entry['bounds']
        if image.size != (width, height):
            raise ValueError(f'Mask dimensions differ: {key}')
        for y in range(height):
            for x in range(width):
                color = pixels[x,y]
                if color[3] and key == 'settings-pressed-border-mask':
                    pixels[x,y] = (0, 210, 255, color[3])
                elif color[3]:
                    back = background.getpixel((left+x, top+y))
                    shade = scrim.getpixel((left+x, top+y)) / 255
                    opacity = max(color[:3]) / 255
                    pixels[x,y] = tuple(min(255, round(color[c] + back[c] * (1-opacity) * (1-shade))) for c in range(3)) + (color[3],)
        yield key, entry, image

def compose(root: Path = ROOT) -> Image.Image:
    manifest = json.loads((root / MANIFEST.relative_to(ROOT)).read_text())
    for key in ('source', 'background_source', 'guidance_derivative'):
        entry = manifest[key]
        path = root / 'shared' / entry['file']
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Approved glass input differs: {key}')
    base = Image.open(root / 'shared/assets/settings-neutral-466.png').convert('RGB')
    background, scrim = background_and_scrim(manifest, root)
    original, gears, shade = base.load(), background.load(), scrim.load()
    result = Image.new('RGB', (466, 466))
    pixels = result.load()
    left, top, right, bottom = manifest['composition']['brightness_clear']
    for y in range(466):
        for x in range(466):
            color = (0, 0, 0) if left <= x < right and top <= y < bottom else original[x, y]
            opacity = max(color) / 255
            pixels[x, y] = tuple(min(255, round(color[c] + gears[x, y][c] * (1 - opacity) * (1 - shade[x, y] / 255))) for c in range(3))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    result = compose()
    png = ROOT / 'shared' / manifest['output']['png']
    binary = ROOT / 'shared' / manifest['output']['device_asset']
    if args.check:
        if Image.open(png).convert('RGB').tobytes() != result.tobytes() or decode(png)[2] != binary.read_bytes():
            raise SystemExit('Glass composition or device bytes differ')
        for key, entry, overlay in derive_overlays(manifest):
            target = ROOT / 'shared' / entry['png']
            if Image.open(target).convert('RGBA').tobytes() != overlay.tobytes() or decode(target)[2] != (ROOT / 'shared' / entry['device_asset']).read_bytes():
                raise SystemExit(f'Glass overlay differs: {key}')
        print('Glass composition, bounded overlays and device bytes verified without rewriting')
        return
    result.save(png)
    binary.write_bytes(decode(png)[2])
    manifest['output'].update(png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(), device_asset_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
    for key, entry, overlay in derive_overlays(manifest):
        target = ROOT / 'shared' / entry['png']
        overlay.save(target)
        data = decode(target)[2]
        (ROOT / 'shared' / entry['device_asset']).write_bytes(data)
        entry.update(png_sha256=hashlib.sha256(target.read_bytes()).hexdigest(), device_asset_sha256=hashlib.sha256(data).hexdigest())
    MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n')
    references = ROOT / 'shared/reference-manifest.json'
    document = json.loads(references.read_text())
    state = next(item for item in document['states'] if item['name'] == 'settings')
    state['background_composition'] = {'manifest':str(MANIFEST.relative_to(ROOT / 'shared')), 'sha256':hashlib.sha256(MANIFEST.read_bytes()).hexdigest(), 'runtime_asset_key':'settings-gears-base', 'original_reference_role':'Immutable geometry and white artwork; latest user request supersedes opaque background/card fill only. New supplied composition is treatment guidance.'}
    # Preserve compact formatting and all unrelated reference entries.
    replacement = '"background_composition": ' + json.dumps(state['background_composition'], indent=2).replace('\n', '\n      ')
    text, count = re.subn(r'"background_composition": \{.*?\n      \}', lambda _: replacement, references.read_text(), count=1, flags=re.S)
    if count != 1:
        raise ValueError('Expected one existing Settings composition record')
    # Preserve each original reference-derived status asset as provenance.
    for entry in state['active_status_assets']:
        key = 'settings-status-' + entry['state'].removesuffix('-connected') + '-active'
        overlay = manifest['state_assets'][key]
        entry.update(asset=overlay['device_asset'], sha256=overlay['device_asset_sha256'], reference_asset=overlay['source_asset'].replace('.png', '.bin'))
        pattern = r'^.*"state": "' + re.escape(entry['state']) + r'", "asset":.*$'
        text, count = re.subn(pattern, lambda match: '        ' + json.dumps(entry) + (',' if match[0].rstrip().endswith(',') else ''), text, count=1, flags=re.M)
        if count != 1:
            raise ValueError('Expected one compact active status asset entry')
    json.loads(text)  # Validate formatting-preserving edits before replacing the record.
    references.write_text(text)

if __name__ == '__main__':
    main()
