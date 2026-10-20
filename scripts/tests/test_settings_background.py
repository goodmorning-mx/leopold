from pathlib import Path
import hashlib
import json
import unittest
from PIL import Image
from scripts.prepare_settings_gears_asset import compose, PROTECTED

ROOT = Path(__file__).resolve().parents[2]

class SettingsBackgroundTests(unittest.TestCase):
    def test_background_derives_from_approved_source_and_keeps_controls(self):
        result, mask = compose()
        original = Image.open(ROOT / 'shared/assets/settings-neutral-466.png').convert('RGBA')
        rendered = Image.open(ROOT / 'shared/assets/settings-gears-base-466.png').convert('RGBA')
        self.assertEqual(result.size, (466,466))
        self.assertEqual(result.tobytes(), rendered.tobytes())
        for left,top,right,bottom in PROTECTED:
            box = (left,top,right+1,bottom+1)
            self.assertEqual(original.crop(box).tobytes(),result.crop(box).tobytes())
        self.assertNotEqual(original.getpixel((0,233)), result.getpixel((0,233)))
        manifest=json.loads((ROOT/'shared/references/settings-gears-2026-10-20/composition.json').read_text())
        for file,checksum in [('shared/'+manifest['source']['file'],manifest['source']['sha256']),
                              ('shared/'+manifest['output']['png'],manifest['output']['png_sha256']),
                              ('shared/'+manifest['output']['device_asset'],manifest['output']['device_asset_sha256'])]:
            self.assertEqual(hashlib.sha256((ROOT/file).read_bytes()).hexdigest(),checksum)

    def test_shared_back_button_fits_inside_round_panel(self):
        bounds=json.loads((ROOT/'shared/interaction.json').read_text())['local_panels']['back']
        for x in [bounds['x'],bounds['x']+bounds['width']]:
            for y in [bounds['y'],bounds['y']+bounds['height']]:
                self.assertLessEqual((x-233)**2+(y-233)**2,233**2)
        field=json.loads((ROOT/'shared/interaction.json').read_text())['local_panels']['wifi_password_field']
        self.assertLessEqual(bounds['y']+bounds['height'],field['y'])
        self.assertLess(field['y']+field['height'],159)  # first keyboard row
