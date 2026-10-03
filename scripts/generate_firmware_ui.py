"""Generate the firmware fallback-screen builder from shared/firmware-ui.json."""

from __future__ import annotations

import json
from pathlib import Path

from SCons.Script import Import

Import("env")
root = Path(env.subst("$PROJECT_DIR")).resolve().parent
import sys
sys.path.insert(0, str(root / "scripts"))
from sync_shared_assets import sync
sync()
source = root / "shared" / "firmware-ui.json"
target = root / "device" / "include" / "generated_firmware_ui.h"
home_asset_target = root / "device" / "include" / "generated_home_asset.h"
document = json.loads(source.read_text(encoding="utf-8"))
interaction = json.loads((root / "shared" / "interaction.json").read_text(encoding="utf-8"))
persistence = json.loads((root / "shared" / "persistence.json").read_text(encoding="utf-8"))
typography = json.loads((root / "shared" / "typography.json").read_text(encoding="utf-8"))
rendering = json.loads((root / "shared" / "rendering.json").read_text(encoding="utf-8"))


def q(value: object) -> str:
    return json.dumps(str(value))


def rgb565(value: str) -> str:
    red = int(value[1:3], 16)
    green = int(value[3:5], 16)
    blue = int(value[5:7], 16)
    packed = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return f"0x{packed:04X}"


status = rendering["status_bar"]
spotify_rendering = rendering["spotify"]
arc = spotify_rendering["arc"]
cover = spotify_rendering["cover"]
seek = spotify_rendering["seek"]
controls = spotify_rendering["controls"]
actions = spotify_rendering["actions"]
usb = rendering["home"]["usb_icon"]


lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "#include \"sdui.h\"",
    "",
    "namespace leopold {",
    "namespace generated_ui {",
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

home_asset = (root / "shared" / "assets" / "sky_home.bin").read_bytes()
home_asset_lines = [
    "#pragma once",
    "#include <Arduino.h>",
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
