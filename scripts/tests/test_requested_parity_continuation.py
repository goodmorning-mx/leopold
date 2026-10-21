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

    def test_home_composition_keeps_banner_and_clears_only_approved_live_time_mask(self):
        from PIL import Image
        from scripts.prepare_home_outward_controls import OUT, SOURCE, BANNER, LIVE_TIME
        source = Image.open(SOURCE).convert('RGBA')
        output = Image.open(OUT).convert('RGBA')
        wallpaper = Image.open(ROOT / 'shared/assets/settings-gears-wallpaper-466.png').convert('RGBA')
        self.assertEqual(source.getchannel('A').getextrema(), (255, 255))
        self.assertEqual(wallpaper.getchannel('A').getextrema(), (255, 255))
        bx, by, bw, bh = BANNER
        mask = Image.new('L', (bw, bh), 0)
        from PIL import ImageDraw
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, bw - 1, bh - 1), radius=28, fill=255)
        tx, ty, tw, th = LIVE_TIME
        for y in range(by, by + bh):
            for x in range(bx, bx + bw):
                matte = source.getpixel((x, y))
                key_alpha = min(224, max(0, max(matte[:3]) - 24) * 4)
                alpha = key_alpha * mask.getpixel((x - bx, y - by)) // 255
                if tx <= x < tx + tw and ty <= y < ty + th:
                    continue
                # Keep this independent pointwise check inside the central
                # shell, away from the translated weather/nav/USB glows.
                if not 190 <= x < 285:
                    continue
                if alpha:
                    foreground = tuple(min(255, (matte[c] * 255 + alpha // 2) // alpha) for c in range(3))
                    base = wallpaper.getpixel((x, y))
                    expected = tuple((base[c] * (255 - alpha) + foreground[c] * alpha + 127) // 255 for c in range(3)) + (255,)
                else:
                    expected = wallpaper.getpixel((x, y))
                self.assertEqual(output.getpixel((x, y)), expected, f'banner composite pixel ({x},{y})')
        # The rounded upper-left corner remains the gears pixels; opaque black
        # from the source matte must not form a background shape.
        self.assertEqual(output.getpixel((bx, by)), wallpaper.getpixel((bx, by)))
        # Reconstruct the clear with an independent harmonic interpolation
        # over the exact approved live-time bounds. Its four-edge boundary
        # keeps the pill continuous without painting a rectangular backing.
        for channel in range(3):
            grid = [[0.0] * (tw + 2) for _ in range(th + 2)]
            for y in range(th + 2):
                for x in range(tw + 2):
                    if x in (0, tw + 1) or y in (0, th + 1):
                        grid[y][x] = output.getpixel((tx + x - 1, ty + y - 1))[channel]
            for y in range(1, th + 1):
                vertical = y / (th + 1)
                for x in range(1, tw + 1):
                    horizontal = x / (tw + 1)
                    top, bottom = grid[0][x], grid[th + 1][x]
                    left, right = grid[y][0], grid[y][tw + 1]
                    grid[y][x] = ((1 - vertical) * top + vertical * bottom + (1 - horizontal) * left + horizontal * right) / 2
            for _ in range(600):
                for y in range(1, th + 1):
                    for x in range(1, tw + 1):
                        grid[y][x] = (grid[y][x - 1] + grid[y][x + 1] + grid[y - 1][x] + grid[y + 1][x]) / 4
            for y in range(th):
                for x in range(tw):
                    pixel = list(output.getpixel((tx + x, ty + y)))
                    pixel[channel] = round(grid[y + 1][x + 1])
                    self.assertEqual(output.getpixel((tx + x, ty + y)), tuple(pixel[:3]) + (255,))
        # Check the complete 68x68 weather artwork, including its sun/cloud,
        # glass bubble, edge glow and transparent black-matte area. Every
        # source pixel must move left exactly ten pixels and composite over
        # the canonical gears wallpaper with the renderer's screen blend.
        for sy in range(194, 263):
            for sx in range(75, 144):
                src_pixel = source.getpixel((sx, sy))
                wall_pixel = wallpaper.getpixel((sx - 10, sy))
                peak = max(src_pixel[:3])
                if peak <= 8:
                    expected_weather = wall_pixel
                else:
                    screen_weather = tuple(255 - (255 - wall_pixel[c]) * (255 - src_pixel[c]) // 255 for c in range(3))
                    coverage = min(255, peak)
                    expected_weather = tuple(wall_pixel[c] + (screen_weather[c] - wall_pixel[c]) * coverage // 255 for c in range(3)) + (255,)
                self.assertEqual(output.getpixel((sx - 10, sy)), expected_weather,
                                 f'complete weather composite pixel ({sx},{sy})')
        self.assertNotEqual(output.crop((313, 139, 328, 157)).tobytes(), wallpaper.crop((313,139,328,157)).tobytes())

    def test_home_live_time_uses_asset_mask_without_a_backing_panel(self):
        renderer = (ROOT / 'shared/lvgl_renderer.cpp').read_text()
        home_status = renderer[renderer.index('if (strcmp(screen.screenId, "sky-home") == 0)'):renderer.index('const bool top =', renderer.index('if (strcmp(screen.screenId, "sky-home") == 0)'))]
        self.assertNotIn('makePanel(', home_status)
        self.assertIn('lv_obj_set_pos(time, 218, 214)', home_status)
        browser = (ROOT / 'frontend/src/simulator/FirmwareCanvas.tsx').read_text()
        self.assertNotIn("ctx.fillStyle = '#555C6A'; ctx.fillRect(211, 208, 68, 31)", browser)
        mask = read_json('shared/reference-manifest.json')['states'][0]['dynamic_masks'][0]
        self.assertEqual((mask['x'], mask['y'], mask['width'], mask['height']), (211, 219, 56, 22))

if __name__ == '__main__':
    unittest.main()
