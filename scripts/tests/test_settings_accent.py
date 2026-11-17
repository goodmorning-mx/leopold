"""Exercise the pixel tint used by the shared renderer, without LVGL or hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class SettingsAccentTests(unittest.TestCase):
    def test_main_settings_glyph_stays_white_and_gets_accent_glow_while_pressed(self):
        renderer = (ROOT / 'shared/lvgl_renderer.cpp').read_text()
        glyph_start = renderer.index('const lv_image_dsc_t* settingsMainGlyph(')
        glyph_end = renderer.index('const lv_image_dsc_t* tintHomeGlassRim(', glyph_start)
        glyph = renderer[glyph_start:glyph_end]
        self.assertIn('useAppearanceAccent', glyph)
        self.assertIn(': 0xFFFFFF', glyph)
        self.assertIn('settingsMainGlyph(glyphKeys[index], false)', renderer)
        self.assertIn('rowPressed = selectedRowId != nullptr && strcmp(selectedRowId, rowIds[index]) == 0', renderer)
        self.assertIn('const bool active = index == 0;', renderer)
        self.assertIn('const lv_image_dsc_t* glowGlyph = settingsMainGlyph(glyphKeys[index], true)', renderer)
        self.assertIn('lv_obj_set_style_transform_scale(glow, 282, 0)', renderer)
        self.assertIn('lv_obj_set_style_image_opa(glow, static_cast<lv_opa_t>(255 * 80 / 100), 0)', renderer)
        self.assertIn('lv_obj_move_to_index(glow, lv_obj_get_index(icon))', renderer)
        self.assertNotIn('set_style_shadow_color(icon,', renderer)
        self.assertIn('cache[index].appearanceTint == useAppearanceAccent', glyph)
        self.assertIn('lv_image_cache_drop(&item.descriptor)', glyph)
        # The title gear keeps the previous appearance-accent mapping.
        self.assertIn('settingsMainGlyph("settings-main/gear")', renderer)

    def test_settings_circle_uses_fixed_home_glass_and_home_rim(self):
        renderer = (ROOT / 'shared/lvgl_renderer.cpp').read_text()
        start = renderer.index('void drawSettingsGlassRows(')
        end = renderer.index('const lv_font_t* statusFont(', start)
        settings_rows = renderer[start:end]
        self.assertIn('makeHomeButton(viewport, bubble, nullptr, false, true, true)', settings_rows)
        self.assertIn('drawHomeGlassRim(iconBubble, resolveImage, generated_ui::kSettingsGlassRowBubbleDiameter, false)', settings_rows)
        self.assertIn('neutralGlass ? LV_OPA_COVER', renderer)
        self.assertIn('bubble.color = rowFill', settings_rows)
        self.assertIn('appearanceTint ? tintHomeGlassRim(source) : neutralHomeGlassRim(source)', renderer)
        neutral_start = renderer.index('const lv_image_dsc_t* neutralHomeGlassRim(')
        neutral_end = renderer.index('const lv_image_dsc_t* contrastHomeIcon(', neutral_start)
        neutral_rim = renderer[neutral_start:neutral_end]
        self.assertIn('const uint8_t luminance', neutral_rim)
        self.assertIn('memcpy(cache.bytes, source->data, source->data_size)', neutral_rim)
        self.assertIn('rowPressed = selectedRowId != nullptr', settings_rows)
        self.assertNotIn('makeHomeButton(viewport, bubble, rowPressed', settings_rows)
        self.assertIn('kHomeRestTintPercent', renderer)
        self.assertIn('drawHomeGlassRim(iconBubble, resolveImage, generated_ui::kSettingsGlassRowBubbleDiameter, false)', settings_rows)
        self.assertIn('kSettingsGlassRowRestShadowWidth', settings_rows)
        self.assertNotIn('active ? generated_ui::kSettingsGlassRowActiveShadowWidth', settings_rows)
        self.assertNotIn('set_style_shadow_width(iconBubble', settings_rows)
        self.assertIn('rowPressed', settings_rows)

    def test_main_settings_rows_use_neutral_mode_surfaces_with_accent_outline(self):
        renderer = (ROOT / 'shared/lvgl_renderer.cpp').read_text()
        start = renderer.index('void drawSettingsGlassRows(')
        end = renderer.index('const lv_font_t* statusFont(', start)
        settings_rows = renderer[start:end]
        self.assertIn('gDarkAppearance ? generated_ui::kSettingsGlassRowDarkFill : generated_ui::kSettingsGlassRowLightFill', settings_rows)
        self.assertIn('gDarkAppearance ? generated_ui::kSettingsGlassRowDarkGradient : generated_ui::kSettingsGlassRowLightGradient', settings_rows)
        self.assertIn('gDarkAppearance ? lv_color_white() : lv_color_black()', settings_rows)
        self.assertIn('rowGradient, 0xFFFF, generated_ui::kSettingsGlassRowBorderOpacity', settings_rows)
        self.assertIn('rowShadow, 0xFFFF, generated_ui::kSettingsGlassRowInnerRimOpacity', settings_rows)
        self.assertIn('generated_ui::kSettingsGlassRowReflectionOpacity, true);', settings_rows)
        self.assertNotIn('accent, generated_ui::kSettingsGlassRowFillOpacity', settings_rows)
        self.assertNotIn('rowPressed ? generated_ui::kSettingsGlassRowActiveShadowWidth', settings_rows)

    def test_neutral_pixels_survive_all_palette_tints(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        self.assertIsNotNone(compiler)
        source = r'''
#include "settings_accent.h"
#include <cassert>
int main() {
  const uint16_t accents[] = {0xF800,0xFC00,0xFFE0,0x07E0,0x06BF,0x7BDE,0xC31F};
  for (uint16_t accent : accents) {
    // Neutral backing and white glyph highlights must never become blocks.
    for (int level = 0; level < 32; ++level) {
      uint16_t grey = (level << 11) | ((level * 63 / 31) << 5) | level;
      assert(leopold::tintSettingsAccentPixel(grey,accent) == grey);
    }
    assert(leopold::tintSettingsAccentPixel(0,accent) == 0);
    assert(leopold::tintSettingsAccentPixel(0xFFFF,accent) == 0xFFFF);
    // The current wallpaper is capped at 148/255; colored backing must
    // remain unchanged too, not just greys and black.
    for (int r=0;r<=18;r+=3) for(int g=0;g<=36;g+=6) for(int b=0;b<=18;b+=3) {
      uint16_t wallpaper=(r<<11)|(g<<5)|b;
      assert(leopold::tintSettingsAccentPixel(wallpaper,accent)==wallpaper);
    }
    // A pure cyan highlight follows the palette instead of staying cyan.
    assert(leopold::tintSettingsAccentPixel(0x069F,accent) == accent);
    // Specular highlights retain their lightness rather than flattening.
    uint16_t highlight=leopold::tintSettingsAccentPixel(0xCF5F,accent);
    assert(((highlight >> 11) & 31) >= 25);
  }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'test.cpp'; exe=Path(directory)/'test'
            cpp.write_text(source)
            subprocess.run([compiler,'-std=c++11','-I',str(ROOT/'shared'),str(cpp),'-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe)],check=True,capture_output=True)
