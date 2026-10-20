import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read_json(path):
    return json.loads((ROOT / path).read_text())

class RequestedParityContinuationTests(unittest.TestCase):
    def test_live_values_reach_shared_renderer_before_screen_paint(self):
        source = (ROOT / 'device/src/main.cpp').read_text()
        sync_start = source.index('void syncDisplayIndicators()')
        sync = source[sync_start:source.index('\n}\n', sync_start) + 3]
        self.assertIn('telemetry.formattedTime(timeText)', sync)
        self.assertIn('screen.drawIndicators(', sync)
        for expression in ['wifi.connected()', 'cloudConnected', 'bluetooth.enabled()', 'bluetooth.connected()', 'telemetry.batteryPercent()', 'timeText']:
            self.assertIn(expression, sync)
        self.assertLess(sync.index('screen.drawIndicators('), sync.index('currentUi.screenCount'))
        start = source.index('bool openLocalScreen(const char* componentId) {')
        opener = source[start:source.index('\n}\n', start) + 3]
        self.assertIn('syncDisplayIndicators();', opener)
        self.assertLess(opener.index('syncDisplayIndicators();'), opener.index('screen.renderSduiScreen(currentUi);'))
        back = source[source.index('if (isConnectivityBack(point)) {', source.index('localPanel != LocalPanel::kNone)')):]
        self.assertLess(back.index('syncDisplayIndicators();'), back.index('screen.renderSduiScreen(currentUi);'))
        telemetry = (ROOT / 'device/src/telemetry.cpp').read_text()
        self.assertIn('if (!timeSynchronized())', telemetry)
        self.assertIn('memcpy(output, "--:--", 6)', telemetry)

    def test_default_brightness_and_persisted_value_contract(self):
        persistence = read_json('shared/persistence.json')['settings']['brightness']
        self.assertEqual(persistence['default'], 100)
        self.assertEqual((persistence['minimum'], persistence['maximum']), (5, 100))
        ui = read_json('shared/firmware-ui.json')
        slider = next(c for s in ui['screens'] if s['id'] == 'device-control-center' for c in s['components'] if c['id'] == 'display-brightness')
        self.assertEqual(slider['value'], 100)
        display = (ROOT / 'device/src/display_manager.cpp').read_text()
        self.assertIn('displayPreferences.getUChar("brightness", generated_ui::kDefaultBrightness)', display)
        self.assertIn('displayPreferences.putUChar("brightness", brightnessPercent_)', display)
        simulator = (ROOT / 'frontend/src/simulator/Simulator.tsx').read_text()
        self.assertIn("stored('brightness', Number(persistence.settings.brightness.default))", simulator)

    def test_display_row_opens_existing_additional_screen_and_back_returns(self):
        behavior = read_json('shared/interaction.json')['behaviour']['settings_row_actions']
        self.assertEqual(behavior['settings-display'], 'open-additional-options')
        ui = read_json('shared/firmware-ui.json')
        settings = next(s for s in ui['screens'] if s['id'] == 'device-control-center')
        self.assertFalse(any(c['id'] == 'open-additional-options' for c in settings['components']))
        additional = next(s for s in ui['screens'] if s['id'] == 'device-additional-options')
        self.assertTrue(any(c['id'] == 'open-settings' and c['label'] == 'BACK' for c in additional['components']))
        main = (ROOT / 'device/src/main.cpp').read_text()
        self.assertIn('openLocalScreen("open-additional-options")', main)
        self.assertIn('strcmp(button->id, "open-settings") == 0', main)
        self.assertIn("localTargets[component.id]", (ROOT / 'frontend/src/simulator/Simulator.tsx').read_text())

    def test_home_groups_and_hit_bounds_move_outward_exactly_ten_pixels(self):
        ui = read_json('shared/firmware-ui.json')
        home = next(s for s in ui['screens'] if s['id'] == 'sky-home')
        components = {c['id']: c for c in home['components'] if c['type'] == 'button'}
        bounds = read_json('shared/interaction.json')['behaviour']['home_button_bounds']
        self.assertEqual(len(bounds), 7)
        for entry in bounds:
            c = components[entry['id']]
            self.assertEqual((c['x'], c['y'], c['width'], c['height']), (entry['x'], entry['y'], entry['width'], entry['height']))
            cx, cy = entry['center']; ox, oy = entry['move_from_center']
            self.assertEqual((cx - ox) ** 2 + (cy - oy) ** 2, entry['radial_delta'] ** 2)
            self.assertEqual(entry['radial_delta'], 10)
            self.assertGreater((cx-233)**2 + (cy-233)**2, (ox-233)**2 + (oy-233)**2)
            for x in (c['x'], c['x'] + c['width']):
                for y in (c['y'], c['y'] + c['height']):
                    self.assertLessEqual((x-233)**2+(y-233)**2, 233**2)

    def test_canonical_wallpaper_reaches_settings_home_and_nested_panels(self):
        manifest = read_json('shared/asset-manifest.json')['assets']['settings-gears-wallpaper']
        self.assertEqual((manifest['width'], manifest['height']), (466, 466))
        for path in ['frontend/src/simulator/FirmwareCanvas.tsx', 'shared/lvgl_renderer.cpp', 'device/src/display_manager.cpp']:
            self.assertIn('settings-gears-wallpaper', (ROOT / path).read_text())
        renderer = (ROOT / 'shared/lvgl_renderer.cpp').read_text()
        self.assertIn('resolveImage("home")', renderer)
        self.assertNotIn('resolveImage(state != nullptr && state->backgroundMode == 2 ? "chevron" : "home")', renderer)
        simulator = (ROOT / 'frontend/src/simulator/Simulator.tsx').read_text()
        self.assertIn("framebufferCaptureMode && localPanel === 'none'", simulator)
        manager = (ROOT / 'device/src/display_manager.cpp').read_text()
        for screen in ['buildLvglConnectivityDetails', 'showBluetoothDetails', 'showBluetoothDevices', 'showWifiNetworks', 'showWifiPassword', 'showWifiConnecting']:
            section = manager[manager.index('DisplayManager::' + screen):]
            self.assertIn('drawSettingsWallpaper(root)', section[:1200])
        self.assertIn('"home-controls-outward-10-466.bin"', (ROOT / 'device/src/sky_assets.cpp').read_text())

    def test_home_composition_keeps_banner_and_fixed_placeholder(self):
        from PIL import Image
        from scripts.prepare_home_outward_controls import OUT, SOURCE, PILL
        source = Image.open(SOURCE).convert('RGBA')
        output = Image.open(OUT).convert('RGBA')
        mask = Image.new('L', source.size, 0)
        from PIL import ImageDraw
        ImageDraw.Draw(mask).rounded_rectangle(PILL, radius=28, fill=255)
        # The central banner area is unchanged; rounded corners blend into the new background.
        self.assertEqual(output.crop((142, 220, 310, 240)).tobytes(), source.crop((142, 220, 310, 240)).tobytes())
        self.assertNotEqual(output.crop((313, 139, 328, 157)).tobytes(), Image.open(ROOT / 'shared/assets/settings-gears-wallpaper-466.png').convert('RGBA').crop((313,139,328,157)).tobytes())

if __name__ == '__main__':
    unittest.main()
