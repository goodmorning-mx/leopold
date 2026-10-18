"""Generate the firmware fallback-screen builder from shared/firmware-ui.json."""

from __future__ import annotations

import json
from pathlib import Path

try:
    from SCons.Script import Import
except ModuleNotFoundError:
    Import = None

if Import is not None:
    Import("env")
    root = Path(env.subst("$PROJECT_DIR")).resolve().parent
else:
    root = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(root / "scripts"))
from sync_shared_assets import sync
sync()
source = root / "shared" / "firmware-ui.json"
target = root / "device" / "include" / "generated_firmware_ui.h"
home_asset_target = root / "device" / "include" / "generated_home_asset.h"
settings_asset_target = root / "device" / "include" / "generated_settings_asset.h"
display_asset_target = root / "device" / "include" / "generated_display_asset.h"
document = json.loads(source.read_text(encoding="utf-8"))
interaction = json.loads((root / "shared" / "interaction.json").read_text(encoding="utf-8"))
persistence = json.loads((root / "shared" / "persistence.json").read_text(encoding="utf-8"))
typography = json.loads((root / "shared" / "typography.json").read_text(encoding="utf-8"))
rendering = json.loads((root / "shared" / "rendering.json").read_text(encoding="utf-8"))
calibration = json.loads((root / "shared" / "calibration.json").read_text(encoding="utf-8"))


def q(value: object) -> str:
    return json.dumps(str(value))


def rgb565(value: str) -> str:
    red = int(value[1:3], 16)
    green = int(value[3:5], 16)
    blue = int(value[5:7], 16)
    packed = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return f"0x{packed:04X}"


status = rendering["status_bar"]
settings = rendering["settings"]
settings_title = settings["title"]
settings_slider = settings["slider"]
settings_cloud = settings["status_cloud"]
settings_bubble = settings["icon_bubble"]
settings_center = settings["icon_center"]
settings_chevron = settings["chevron"]
spotify_rendering = rendering["spotify"]
arc = spotify_rendering["arc"]
cover = spotify_rendering["cover"]
seek = spotify_rendering["seek"]
controls = spotify_rendering["controls"]
actions = spotify_rendering["actions"]
usb = rendering["home"]["usb_icon"]


lines = [
    "#pragma once",
    "#ifndef LEOPOLD_WASM",
    "#include <Arduino.h>",
    "#endif",
    "#include \"sdui.h\"",
    "",
    "namespace leopold {",
    "namespace generated_ui {",
    f"constexpr uint32_t kUiRevision = {document['revision']};",
    f"constexpr int16_t kSwipeUpDistance = {interaction['touch']['swipe_up_distance']};",
    f"constexpr int16_t kSwipeHorizontalDistance = {interaction['touch']['swipe_horizontal_distance']};",
    f"constexpr int16_t kSwipeDirectionRatio = {int(interaction['touch']['swipe_direction_ratio'] * 10)};",
    f"constexpr uint32_t kSwipeUpMaxDurationMs = {interaction['touch']['swipe_up_max_duration_ms']};",
    f"constexpr uint32_t kSwipeHorizontalMaxDurationMs = {interaction['touch']['swipe_horizontal_max_duration_ms']};",
    f"constexpr uint32_t kLongPressDurationMs = {interaction['touch']['long_press_duration_ms']};",
    f"constexpr uint32_t kTapMaxDurationMs = {interaction['touch']['tap_max_duration_ms']};",
    f"constexpr int16_t kTapMaxMovement = {interaction['touch']['tap_max_movement']};",
    f"constexpr int16_t kLongPressMaxMovement = {interaction['touch']['long_press_max_movement']};",
    f"constexpr uint32_t kDragIntervalMs = {interaction['touch']['drag_interval_ms']};",
    f"constexpr uint8_t kBrightnessMinimum = {interaction['behaviour']['brightness_minimum']};",
    f"constexpr uint8_t kBrightnessMaximum = {interaction['behaviour']['brightness_maximum']};",
    f"constexpr uint8_t kDefaultBrightness = {persistence['settings']['brightness']['default']};",
    f"constexpr int16_t kCalibrationWidth = {calibration['width']};",
    f"constexpr int16_t kCalibrationHeight = {calibration['height']};",
    f"constexpr uint16_t kCalibrationBackground = {rgb565(calibration['background'])};",
    f"constexpr uint16_t kCalibrationBorderColor = {rgb565(calibration['border']['color'])};",
    f"constexpr uint8_t kCalibrationBorderWidth = {calibration['border']['width']};",
    f"constexpr uint16_t kCalibrationMarkerColor = {rgb565(calibration['corner_markers']['color'])};",
    f"constexpr uint8_t kCalibrationMarkerSize = {calibration['corner_markers']['size']};",
    f"constexpr uint16_t kCalibrationGridColor = {rgb565(calibration['grid']['color'])};",
    f"constexpr uint8_t kCalibrationGridSpacing = {calibration['grid']['spacing']};",
    f"constexpr uint8_t kCalibrationGridWidth = {calibration['grid']['width']};",
    f"constexpr uint16_t kCalibrationCrosshairColor = {rgb565(calibration['crosshair']['color'])};",
    f"constexpr uint8_t kCalibrationCrosshairWidth = {calibration['crosshair']['width']};",
    f"constexpr uint16_t kCalibrationCircleColor = {rgb565(calibration['circles']['color'])};",
    f"constexpr uint8_t kCalibrationCircleWidth = {calibration['circles']['width']};",
    f"constexpr uint16_t kCalibrationRadii[] = {{{', '.join(str(radius) for radius in calibration['circles']['radii'])}}};",
    f"constexpr uint8_t kCalibrationRadiusCount = {len(calibration['circles']['radii'])};",
    f"constexpr uint8_t kFontSize1 = {typography['sizes']['1']['lvgl']};",
    f"constexpr uint8_t kFontSize2 = {typography['sizes']['2']['lvgl']};",
    f"constexpr uint8_t kFontSize3 = {typography['sizes']['3']['lvgl']};",
    f"constexpr uint8_t kFontSize4 = {typography['sizes']['4']['lvgl']};",
    f"constexpr uint8_t kFontSize5 = {typography['sizes']['5']['lvgl']};",
    f"constexpr uint16_t kStatusTextColor = {rgb565(status['text_color'])};",
    f"constexpr int16_t kStatusTimeX = {status['time']['x']};",
    f"constexpr int16_t kStatusTimeY = {status['time']['y']};",
    f"constexpr int16_t kStatusBatteryX = {status['battery']['x']};",
    f"constexpr int16_t kStatusBatteryY = {status['battery']['y']};",
    f"constexpr int16_t kStatusBluetoothX = {status['bluetooth']['x']};",
    f"constexpr int16_t kStatusBluetoothY = {status['bluetooth']['y']};",
    f"constexpr int16_t kStatusWifiX = {status['wifi']['x']};",
    f"constexpr int16_t kStatusWifiY = {status['wifi']['y']};",
    f"constexpr int16_t kStatusCloudX = {status['cloud']['x']};",
    f"constexpr int16_t kStatusCloudY = {status['cloud']['y']};",
    f"constexpr int16_t kSettingsStatusX = {settings['status_pill']['x']};",
    f"constexpr int16_t kSettingsStatusY = {settings['status_pill']['y']};",
    f"constexpr int16_t kSettingsStatusWidth = {settings['status_pill']['width']};",
    f"constexpr int16_t kSettingsStatusHeight = {settings['status_pill']['height']};",
    f"constexpr int16_t kSettingsStatusRadius = {settings['status_pill']['radius']};",
    f"constexpr int16_t kSettingsCloudX = {settings_cloud['x']};",
    f"constexpr int16_t kSettingsCloudY = {settings_cloud['y']};",
    f"constexpr int16_t kSettingsCloudGlowBlur = {settings_cloud['glow_blur']};",
    f"constexpr int16_t kSettingsBubbleXOffset = {settings_bubble['x_offset']};",
    f"constexpr int16_t kSettingsBubbleYOffset = {settings_bubble['y_offset']};",
    f"constexpr int16_t kSettingsBubbleDiameter = {settings_bubble['diameter']};",
    f"constexpr int16_t kSettingsBubbleRadius = {settings_bubble['radius']};",
    f"constexpr uint32_t kSettingsBubbleInner = 0x{settings_bubble['gradient_inner'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleOuter = 0x{settings_bubble['gradient_outer'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleActiveInner = 0x{settings_bubble['active_gradient_inner'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleActiveOuter = 0x{settings_bubble['active_gradient_outer'].replace('#', '')};",
    f"constexpr int16_t kSettingsIconCenterXOffset = {settings_center['x_offset']};",
    f"constexpr int16_t kSettingsIconCenterYOffset = {int(settings_center['y_offset'] * 2)};",
    f"constexpr float kSettingsIconScale = {settings_center['scale']:.2f}f;",
    f"constexpr uint32_t kSettingsIconColor = 0x{settings_center['color'].replace('#', '')};",
    f"constexpr uint32_t kSettingsIconActiveColor = 0x{settings_center['active_color'].replace('#', '')};",
    f"constexpr int16_t kSettingsChevronXOffset = {settings_chevron['x_offset']};",
    f"constexpr int16_t kSettingsChevronYOffset = {settings_chevron['y_offset']};",
    f"constexpr int16_t kSettingsChevronMidX = {settings_chevron['points'][1][0]};",
    f"constexpr int16_t kSettingsChevronMidY = {settings_chevron['points'][1][1]};",
    f"constexpr int16_t kSettingsChevronEndY = {settings_chevron['points'][2][1]};",
    f"constexpr int16_t kSettingsChevronStroke = {int(settings_chevron['stroke'] + 0.5)};",
    f"constexpr int16_t kSettingsGearX = {settings_title['gear_x']};",
    f"constexpr int16_t kSettingsGearY = {settings_title['gear_y']};",
    f"constexpr int16_t kSettingsGearDiameter = {settings_title['gear_diameter']};",
    f"constexpr int16_t kSettingsTitleX = {settings_title['text_x']};",
    f"constexpr int16_t kSettingsTitleY = {settings_title['text_y']};",
    f"constexpr int16_t kSettingsTitleFontSize = {settings_title['font_size']};",
    f"constexpr int16_t kSettingsRowX[] = {{{', '.join(str(row['x']) for row in settings['rows'])}}};",
    f"constexpr int16_t kSettingsRowY[] = {{{', '.join(str(row['y']) for row in settings['rows'])}}};",
    f"constexpr int16_t kSettingsRowWidth[] = {{{', '.join(str(row['width']) for row in settings['rows'])}}};",
    f"constexpr int16_t kSettingsRowHeight[] = {{{', '.join(str(row['height']) for row in settings['rows'])}}};",
    f"constexpr uint8_t kSettingsRowCount = {len(settings['rows'])};",
    f"constexpr int16_t kSettingsSliderX = {settings_slider['x']};",
    f"constexpr int16_t kSettingsSliderY = {settings_slider['y']};",
    f"constexpr int16_t kSettingsSliderWidth = {settings_slider['width']};",
    f"constexpr int16_t kSettingsSliderHeight = {settings_slider['height']};",
    f"constexpr int16_t kSettingsSliderRadius = {settings_slider['radius']};",
    f"constexpr int16_t kSettingsSliderKnobRadius = {settings_slider['knob_radius']};",
    f"constexpr uint16_t kSettingsSliderTrack = {rgb565(settings_slider['track'])};",
    f"constexpr uint16_t kSettingsSliderFill = {rgb565(settings_slider['fill'])};",
    f"constexpr uint16_t kSettingsSliderFillStart = {rgb565(settings_slider['fill_start'])};",
    f"constexpr uint16_t kSettingsSliderFillEnd = {rgb565(settings_slider['fill_end'])};",
    f"constexpr uint16_t kSettingsSliderAccent = {rgb565(settings_slider['accent'])};",
    f"constexpr int16_t kSettingsBrightnessLabelX = {settings['brightness_label']['x']};",
    f"constexpr int16_t kSettingsBrightnessLabelY = {settings['brightness_label']['y']};",
    f"constexpr int16_t kSettingsBrightnessLabelFontSize = {settings['brightness_label']['font_size']};",
    "inline bool statusBarVisible(const char* screenId) {",
    "  return " + " || ".join(f'strcmp(screenId, {q(screen_id)}) == 0' for screen_id in status["visible_screens"]) + ";",
    "}",
    f"constexpr uint16_t kHomeSpotifyAssetScale = {rendering['home']['spotify_asset_scale']};",
    f"constexpr int16_t kHomeSpotifyAssetWidth = {rendering['home']['spotify_asset_width']};",
    f"constexpr int16_t kHomeSpotifyAssetHeight = {rendering['home']['spotify_asset_height']};",
    f"constexpr int16_t kUsbIconWidth = {usb['width']};",
    f"constexpr int16_t kUsbIconHeight = {usb['height']};",
    f"constexpr int16_t kUsbIconStroke = {usb['stroke']};",
    f"constexpr int16_t kUsbIconSegments[][4] = {{{', '.join('{' + ', '.join(str(item) for item in segment) + '}' for segment in usb['segments'])}}};",
    f"constexpr uint8_t kUsbIconSegmentCount = {len(usb['segments'])};",
    f"constexpr int16_t kSpotifyArcX = {arc['x']};",
    f"constexpr int16_t kSpotifyArcY = {arc['y']};",
    f"constexpr int16_t kSpotifyArcSize = {arc['size']};",
    f"constexpr int16_t kSpotifyArcRotation = {arc['rotation']};",
    f"constexpr int16_t kSpotifyArcBackgroundStart = {arc['background_start']};",
    f"constexpr int16_t kSpotifyArcBackgroundEnd = {arc['background_end']};",
    f"constexpr int16_t kSpotifyArcWidth = {arc['width']};",
    f"constexpr uint16_t kSpotifyArcBackground = {rgb565(arc['background'])};",
    f"constexpr uint16_t kSpotifyArcActive = {rgb565(arc['active'])};",
    f"constexpr int16_t kSpotifyCoverX = {cover['x']};",
    f"constexpr int16_t kSpotifyCoverY = {cover['y']};",
    f"constexpr int16_t kSpotifyCoverWidth = {cover['width']};",
    f"constexpr int16_t kSpotifyCoverHeight = {cover['height']};",
    f"constexpr int16_t kSpotifySeekX = {seek['x']};",
    f"constexpr int16_t kSpotifySeekY = {seek['y']};",
    f"constexpr int16_t kSpotifySeekWidth = {seek['width']};",
    f"constexpr int16_t kSpotifySeekHeight = {seek['height']};",
    f"constexpr int16_t kSpotifySeekRadius = {seek['radius']};",
    f"constexpr uint16_t kSpotifySeekBackground = {rgb565(seek['background'])};",
    f"constexpr uint16_t kSpotifySeekActive = {rgb565(seek['active'])};",
    f"constexpr int16_t kSpotifyActionX[] = {{{', '.join(str(item['x']) for item in actions)}}};",
    f"constexpr int16_t kSpotifyActionY[] = {{{', '.join(str(item['y']) for item in actions)}}};",
    f"constexpr int16_t kSpotifyControlX[] = {{{', '.join(str(item['x']) for item in controls)}}};",
    f"constexpr int16_t kSpotifyControlSize[] = {{{', '.join(str(item['size']) for item in controls)}}};",
    f"constexpr int16_t kSpotifyControlY[] = {{{', '.join(str(item['y']) for item in controls)}}};",
    f"constexpr uint8_t kSpotifyControlCount = {len(controls)};",
    "inline SduiScreen& append(SduiDocument& document, const char* id, const char* background) {",
    "  SduiScreen& screen = document.screens[document.screenCount++];",
    "  memset(&screen, 0, sizeof(screen));",
    "  strlcpy(screen.screenId, id, sizeof(screen.screenId));",
    "  screen.background = SduiParser::rgb565(background);",
    "  return screen;",
    "}",
    "inline void text(SduiScreen& screen, const char* id, const char* value, int16_t x, int16_t y, uint8_t size, const char* color, const char* align) {",
    "  SduiComponent& component = screen.components[screen.componentCount++];",
    "  component.type = ComponentType::kText; strlcpy(component.id, id, sizeof(component.id)); strlcpy(component.text, value, sizeof(component.text));",
    "  component.x = x; component.y = y; component.size = size; component.color = SduiParser::rgb565(color);",
    "  component.align = strcmp(align, \"right\") == 0 ? TextAlign::kRight : strcmp(align, \"left\") == 0 ? TextAlign::kLeft : TextAlign::kCenter;",
    "}",
    "inline void button(SduiScreen& screen, const char* id, const char* label, int16_t x, int16_t y, int16_t width, int16_t height, const char* color, const char* icon) {",
    "  SduiComponent& component = screen.components[screen.componentCount++];",
    "  component.type = ComponentType::kButton; strlcpy(component.id, id, sizeof(component.id)); strlcpy(component.text, label, sizeof(component.text)); strlcpy(component.icon, icon, sizeof(component.icon));",
    "  component.x = x; component.y = y; component.width = width; component.height = height; component.size = 1; component.color = SduiParser::rgb565(color);",
    "}",
    "inline void slider(SduiScreen& screen, const char* id, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t value, const char* color) {",
    "  SduiComponent& component = screen.components[screen.componentCount++]; component.type = ComponentType::kSlider; strlcpy(component.id, id, sizeof(component.id));",
    "  component.x = x; component.y = y; component.width = width; component.height = height; component.value = value; component.maximum = 100; component.color = SduiParser::rgb565(color);",
    "}",
    "inline void appendFallbackScreens(SduiDocument& document, const SduiScreen* spotifyOverride) {",
]

for screen in document["screens"]:
    if screen["id"] == "spotify-player":
        continue
    name = screen["id"].replace("-", "_")
    lines.append(f"  SduiScreen& screen_{name} = append(document, {q(screen['id'])}, {q(screen['background'])});")
    for component in screen["components"]:
        kind = component["type"]
        if kind == "text":
            lines.append(f"  text(screen_{name}, {q(component['id'])}, {q(component['text'])}, {component['x']}, {component['y']}, {component.get('size', 1)}, {q(component.get('color', '#FFFFFF'))}, {q(component.get('align', 'left'))});")
        elif kind == "button":
            lines.append(f"  button(screen_{name}, {q(component['id'])}, {q(component['label'])}, {component['x']}, {component['y']}, {component['width']}, {component['height']}, {q(component.get('color', '#FFFFFF'))}, {q(component.get('icon', ''))});")
        elif kind == "slider":
            lines.append(f"  slider(screen_{name}, {q(component['id'])}, {component['x']}, {component['y']}, {component['width']}, {component['height']}, {component.get('value', 0)}, {q(component.get('color', '#FFFFFF'))});")

lines.append("  if (spotifyOverride != nullptr) { document.screens[document.screenCount++] = *spotifyOverride; } else {")
spotify = next(screen for screen in document["screens"] if screen["id"] == "spotify-player")
lines.append(f"    SduiScreen& screen_spotify_player = append(document, {q(spotify['id'])}, {q(spotify['background'])});")
for component in spotify["components"]:
    kind = component["type"]
    if kind == "text":
        lines.append(f"    text(screen_spotify_player, {q(component['id'])}, {q(component['text'])}, {component['x']}, {component['y']}, {component.get('size', 1)}, {q(component.get('color', '#FFFFFF'))}, {q(component.get('align', 'left'))});")
    elif kind == "button":
        lines.append(f"    button(screen_spotify_player, {q(component['id'])}, {q(component['label'])}, {component['x']}, {component['y']}, {component['width']}, {component['height']}, {q(component.get('color', '#FFFFFF'))}, {q(component.get('icon', ''))});")
lines.extend(["  }", "}", "}  // namespace generated_ui", "}  // namespace leopold", ""])
generated = "\n".join(lines)
if not target.exists() or target.read_text(encoding="utf-8") != generated:
    target.write_text(generated, encoding="utf-8")

home_asset = (root / "shared" / "assets" / "home-reference-466.bin").read_bytes()
home_asset_lines = [
    "#pragma once",
    "#ifndef LEOPOLD_WASM",
    "#include <Arduino.h>",
    "#endif",
    "",
    "namespace leopold::sky {",
    "const uint8_t kHomeAssetData[] PROGMEM = {",
]
for offset in range(0, len(home_asset), 16):
    home_asset_lines.append("  " + ", ".join(f"0x{value:02X}" for value in home_asset[offset : offset + 16]) + ",")
home_asset_lines.extend([
    "};",
    "constexpr size_t kHomeAssetDataSize = sizeof(kHomeAssetData);",
    "}  // namespace leopold::sky",
    "",
])
home_asset_generated = "\n".join(home_asset_lines)
if not home_asset_target.exists() or home_asset_target.read_text(encoding="utf-8") != home_asset_generated:
    home_asset_target.write_text(home_asset_generated, encoding="utf-8")

settings_asset = (root / "shared" / "assets" / "settings-neutral-466.bin").read_bytes()
settings_asset_lines = [
    "#pragma once",
    "#ifndef LEOPOLD_WASM",
    "#include <Arduino.h>",
    "#endif",
    "",
    "namespace leopold::sky {",
    "const uint8_t kSettingsAssetData[] PROGMEM = {",
]
for offset in range(0, len(settings_asset), 16):
    settings_asset_lines.append("  " + ", ".join(f"0x{value:02X}" for value in settings_asset[offset : offset + 16]) + ",")
settings_asset_lines.extend([
    "};",
    "constexpr size_t kSettingsAssetDataSize = sizeof(kSettingsAssetData);",
    "}  // namespace leopold::sky",
    "",
])
settings_asset_generated = "\n".join(settings_asset_lines)
if not settings_asset_target.exists() or settings_asset_target.read_text(encoding="utf-8") != settings_asset_generated:
    settings_asset_target.write_text(settings_asset_generated, encoding="utf-8")

display_asset = (root / "shared" / "assets" / "display-reference-466.bin").read_bytes()
display_asset_lines = [
    "#pragma once",
    "#ifndef LEOPOLD_WASM",
    "#include <Arduino.h>",
    "#endif",
    "",
    "namespace leopold::sky {",
    "const uint8_t kDisplayAssetData[] PROGMEM = {",
]
for offset in range(0, len(display_asset), 16):
    display_asset_lines.append("  " + ", ".join(f"0x{value:02X}" for value in display_asset[offset : offset + 16]) + ",")
display_asset_lines.extend([
    "};",
    "constexpr size_t kDisplayAssetDataSize = sizeof(kDisplayAssetData);",
    "}  // namespace leopold::sky",
    "",
])
display_asset_generated = "\n".join(display_asset_lines)
if not display_asset_target.exists() or display_asset_target.read_text(encoding="utf-8") != display_asset_generated:
    display_asset_target.write_text(display_asset_generated, encoding="utf-8")
