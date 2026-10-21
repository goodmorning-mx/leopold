import hashlib
import json
from pathlib import Path
import unittest
from PIL import Image
from scripts.prepare_settings_glass_asset import compose, derive_overlays, MANIFEST

ROOT = Path(__file__).resolve().parents[2]

class SettingsGlassTests(unittest.TestCase):
    def test_canonical_glass_asset_is_reproducible_and_full_canvas(self):
        manifest = json.loads(MANIFEST.read_text())
        image = compose()
        canonical = Image.open(ROOT / 'shared' / manifest['output']['png']).convert('RGB')
        self.assertEqual(image.size, (466, 466))
        self.assertEqual(image.tobytes(), canonical.tobytes())
        # Cardinal display edges and old black gaps must now contain gears.
        for point in [(233, 0), (0, 233), (465, 233), (233, 465), (220, 184), (220, 346), (233, 425)]:
            self.assertGreater(max(image.getpixel(point)), 10, point)
        original = Image.open(ROOT / 'shared/assets/settings-neutral-466.png').convert('RGB')
        # Fully white source glyphs retain their exact coordinates and color.
        white = [(x,y) for y in range(127,345) for x in range(72,394) if original.getpixel((x,y)) == (255,255,255)]
        self.assertGreater(len(white), 100)
        self.assertTrue(all(image.getpixel(p) == (255,255,255) for p in white))
        entry=json.loads((ROOT/'shared/asset-manifest.json').read_text())['assets']['settings-gears-base']
        self.assertEqual(entry['file'],'settings-glass-gears-v1-466.bin')
        for key in ('source','background_source','guidance_derivative'):
            data=manifest[key]
            self.assertEqual(hashlib.sha256((ROOT/'shared'/data['file']).read_bytes()).hexdigest(),data['sha256'])

    def test_bounded_state_masks_follow_glass_without_losing_alpha(self):
        manifest=json.loads(MANIFEST.read_text())
        changed=0
        for key,entry,image in derive_overlays(manifest):
            old=Image.open(ROOT/'shared'/entry['source_asset']).convert('RGBA')
            actual=Image.open(ROOT/'shared'/entry['png']).convert('RGBA')
            self.assertEqual(image.tobytes(),actual.tobytes(),key)
            self.assertEqual(image.getchannel('A').tobytes(),old.getchannel('A').tobytes(),key)
            if key == 'settings-pressed-border-mask':
                self.assertTrue(all(pixel[:3] == (0,210,255) for pixel in image.getdata() if pixel[3]))
            changed += sum(a!=b for a,b in zip(image.getdata(),old.getdata()))
            for a,b in zip(image.getdata(),old.getdata()):
                if key != 'settings-pressed-border-mask' and b[3] and max(b[:3])==255:
                    self.assertEqual(a,b) # Preserve pure cyan/white reference highlights.
        self.assertGreater(changed,100)
