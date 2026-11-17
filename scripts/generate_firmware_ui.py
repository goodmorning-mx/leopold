"""Generate the firmware fallback-screen builder from shared/firmware-ui.json."""

from __future__ import annotations

import json
import math
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
from generate_home_runtime_assets import generate as generate_home_runtime_assets
from generate_device_asset_specs import generate as generate_device_asset_specs
from prepare_home_option_a_icons import generate as prepare_home_option_a_icons


def apply_home_button_ring(source_root: Path) -> None:
    rendering = json.loads((source_root / "shared/rendering.json").read_text(encoding="utf-8"))
    source = source_root / "shared/firmware-ui.json"
    document = json.loads(source.read_text(encoding="utf-8"))
    ring = rendering["home"]["button_ring"]
    screen = next((item for item in document["screens"] if item["id"] == "sky-home"), None)
    if screen is None:
        raise ValueError("Home screen is missing from shared firmware UI")
    components = {component["id"]: component for component in screen["components"]}
    targets = ring["angles_degrees"]
    if len(targets) != 8 or any(key not in components for key in targets):
        raise ValueError("Home ring must map all eight declared controls to Home components")
    center_x, center_y = ring["center_x"], ring["center_y"]
    radius, diameter = ring["radius"], ring["touch_diameter"]
    for component_id, degrees in targets.items():
        radians = math.radians(degrees)
        x = int(math.floor(center_x + math.cos(radians) * radius + 0.5))
        y = int(math.floor(center_y + math.sin(radians) * radius + 0.5))
        component = components[component_id]
        if component["type"] == "text":
            component.clear()
            component.update({
                "type": "button", "id": component_id, "label": "", "icon": "x",
                "x": x - diameter // 2, "y": y - diameter // 2,
                "width": diameter, "height": diameter, "color": "#FFFFFF",
            })
        else:
            component["x"], component["y"] = x - diameter // 2, y - diameter // 2
            component["width"] = component["height"] = diameter
    result = json.dumps(document, indent=2, ensure_ascii=False) + "\n"
    if source.read_text(encoding="utf-8") != result:
        source.write_text(result, encoding="utf-8")


apply_home_button_ring(root)
prepare_home_option_a_icons()
sync()
generate_home_runtime_assets(root)
generate_device_asset_specs(root)
source = root / "shared" / "firmware-ui.json"
target = root / "device" / "include" / "generated_firmware_ui.h"
home_asset_target = root / "device" / "include" / "generated_home_asset.h"
settings_asset_target = root / "device" / "include" / "generated_settings_asset.h"
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
shared_status_banner = status["shared_banner"]
shared_status_border_role = shared_status_banner.get("surface_border_role", "fixed")
if shared_status_border_role not in ("fixed", "appearance-accent"):
    raise ValueError(f"Unsupported shared status banner border role: {shared_status_border_role}")
shared_status_border_fallback = shared_status_banner["surface_border"]
if shared_status_border_role == "appearance-accent":
    shared_status_border_fallback = rendering["theme"]["accents"][rendering["theme"]["default_accent"]]["accentBright"]
shared_status_dividers = shared_status_banner["divider_x"]
if not isinstance(shared_status_dividers, list) or len(shared_status_dividers) != 2:
    raise ValueError("The shared banner must have exactly two dividers for three equal sections")
settings = rendering["settings"]
settings_title = settings["title"]
settings_slider = settings["slider"]
settings_glass_row = settings["glass_row"]
settings_cloud = settings["status_cloud"]
settings_bubble = settings["icon_bubble"]
settings_center = settings["icon_center"]
settings_selected_overlay = settings["selected_overlay"]
settings_chevron = settings["chevron"]
home_rendering = rendering["home"]
home_glass = home_rendering["glass_button"]
home_x_icon = home_rendering["x_icon"]
home_layout = status["home_layout"]
banner_battery = shared_status_banner["battery"]
battery_thresholds = status["battery_thresholds"]
weather_rendering = rendering["weather"]
weather_icon = weather_rendering["icon_bubble"]
weather_city = weather_rendering["city"]
weather_conditions = weather_rendering["conditions"]
weather_forecast = weather_rendering["forecast"]
weather_buttons = weather_rendering["duration_buttons"]
spotify_rendering = rendering["spotify"]
arc = spotify_rendering["arc"]
cover = spotify_rendering["cover"]
seek = spotify_rendering["seek"]
spotify_arc_uses_accent = arc.get("active_role") == "appearance-accent"
spotify_seek_uses_accent = seek.get("active_role") == "appearance-accent"
for label, role in (("Spotify arc", arc.get("active_role", "fixed")), ("Spotify seek", seek.get("active_role", "fixed"))):
    if role not in ("fixed", "appearance-accent"):
        raise ValueError(f"Unsupported {label} fill role: {role}")
controls = spotify_rendering["controls"]
actions = spotify_rendering["actions"]
usb = rendering["home"]["usb_icon"]
theme_accents = list(rendering["theme"]["accents"].values())


lines = [
    "#pragma once",
    "#ifndef LEOPOLD_WASM",
    "#include <Arduino.h>",
    "#endif",
    "#include \"sdui.h\"",
    "",
    "namespace leopold {",
    "namespace generated_ui {",
    f"constexpr int16_t kWifiPasswordFieldX = {interaction['local_panels']['wifi_password_field']['x']};",
    f"constexpr int16_t kWifiPasswordFieldY = {interaction['local_panels']['wifi_password_field']['y']};",
    f"constexpr int16_t kWifiPasswordFieldWidth = {interaction['local_panels']['wifi_password_field']['width']};",
    f"constexpr int16_t kWifiPasswordFieldHeight = {interaction['local_panels']['wifi_password_field']['height']};",
    f"constexpr int16_t kConnectivityBackX = {interaction['local_panels']['back']['x']};",
    f"constexpr int16_t kConnectivityBackY = {interaction['local_panels']['back']['y']};",
    f"constexpr int16_t kConnectivityBackWidth = {interaction['local_panels']['back']['width']};",
    f"constexpr int16_t kConnectivityBackHeight = {interaction['local_panels']['back']['height']};",
    f"constexpr int16_t kWifiScanX = {interaction['local_panels']['wifi_actions']['scan']['x']};",
    f"constexpr int16_t kWifiScanY = {interaction['local_panels']['wifi_actions']['scan']['y']};",
    f"constexpr int16_t kWifiScanWidth = {interaction['local_panels']['wifi_actions']['scan']['width']};",
    f"constexpr int16_t kWifiScanHeight = {interaction['local_panels']['wifi_actions']['scan']['height']};",
    f"constexpr int16_t kWifiChangeX = {interaction['local_panels']['wifi_actions']['change']['x']};",
    f"constexpr int16_t kWifiChangeY = {interaction['local_panels']['wifi_actions']['change']['y']};",
    f"constexpr int16_t kWifiChangeWidth = {interaction['local_panels']['wifi_actions']['change']['width']};",
    f"constexpr int16_t kWifiChangeHeight = {interaction['local_panels']['wifi_actions']['change']['height']};",
    f"constexpr int16_t kWifiRefreshX = {interaction['local_panels']['wifi_actions']['refresh']['x']};",
    f"constexpr int16_t kWifiRefreshY = {interaction['local_panels']['wifi_actions']['refresh']['y']};",
    f"constexpr int16_t kWifiRefreshWidth = {interaction['local_panels']['wifi_actions']['refresh']['width']};",
    f"constexpr int16_t kWifiRefreshHeight = {interaction['local_panels']['wifi_actions']['refresh']['height']};",
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
    "struct SettingsRowBounds { const char* id; int16_t x; int16_t y; int16_t width; int16_t height; };",
    f"constexpr bool kSettingsReleaseTap = {str('tap' in interaction['behaviour']['settings_release']['gestures']).lower()};",
    f"constexpr bool kSettingsReleaseLongPress = {str('long-press' in interaction['behaviour']['settings_release']['gestures']).lower()};",
    f"constexpr bool kSettingsReleaseNeutral = {str('release' in interaction['behaviour']['settings_release']['gestures']).lower()};",
    f"constexpr int16_t kSettingsMenuX = {interaction['touch']['settings_menu']['viewport']['x']};",
    f"constexpr int16_t kSettingsMenuY = {interaction['touch']['settings_menu']['viewport']['y']};",
    f"constexpr int16_t kSettingsMenuWidth = {interaction['touch']['settings_menu']['viewport']['width']};",
    f"constexpr int16_t kSettingsMenuHeight = {interaction['touch']['settings_menu']['viewport']['height']};",
    f"constexpr int16_t kSettingsMenuMaximumScroll = {interaction['touch']['settings_menu']['maximum_scroll']};",
    f"constexpr int16_t kSettingsMenuScrollStep = {interaction['touch']['settings_menu']['scroll_step']};",
    f"constexpr int16_t kSettingsGestureStartThreshold = {interaction['touch']['settings_menu']['gesture_start_threshold']};",
    "constexpr SettingsRowBounds kSettingsRowBounds[] = {",
]
for row in interaction["touch"]["settings_row_bounds"]:
    lines.append(
        f'    {{"{row["id"]}", {row["x"]}, {row["y"]}, {row["width"]}, {row["height"]}}},'
    )
lines.extend([
    "};",
    "constexpr uint8_t kSettingsInteractionRowCount = sizeof(kSettingsRowBounds) / sizeof(kSettingsRowBounds[0]);",
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
    f"constexpr int16_t kStatusTimePeriodY = {status['time']['period_y']};",
    f"constexpr int16_t kStatusBatteryX = {status['battery']['x']};",
    f"constexpr int16_t kStatusBatteryY = {status['battery']['y']};",
    f"constexpr int16_t kStatusBluetoothX = {status['bluetooth']['x']};",
    f"constexpr int16_t kStatusBluetoothY = {status['bluetooth']['y']};",
    f"constexpr int16_t kStatusWifiX = {status['wifi']['x']};",
    f"constexpr int16_t kStatusWifiY = {status['wifi']['y']};",
    f"constexpr int16_t kStatusCloudX = {status['cloud']['x']};",
    f"constexpr int16_t kStatusCloudY = {status['cloud']['y']};",
    f"constexpr int16_t kSharedStatusBannerX = {shared_status_banner['x']};",
    f"constexpr int16_t kSharedStatusBannerY = {shared_status_banner['y']};",
    f"constexpr int16_t kSharedStatusBannerWidth = {shared_status_banner['width']};",
    f"constexpr int16_t kSharedStatusBannerHeight = {shared_status_banner['height']};",
    f"constexpr uint16_t kSharedStatusBannerFill = {rgb565(shared_status_banner['surface_fill'])};",
    f"constexpr uint8_t kSharedStatusBannerFillOpacity = {shared_status_banner['surface_fill_opa']};",
    f"constexpr uint16_t kSharedStatusBannerBorder = {rgb565(shared_status_border_fallback)};",
    f"constexpr bool kSharedStatusBannerBorderUsesAccent = {str(shared_status_border_role == 'appearance-accent').lower()};",
    f"constexpr uint8_t kSharedStatusBannerBorderOpacity = {shared_status_banner['surface_border_opa']};",
    f"constexpr int16_t kSharedStatusBannerBorderWidth = {shared_status_banner['surface_border_width']};",
    f"constexpr int16_t kSharedStatusBannerRadius = {shared_status_banner['surface_radius']};",
    f"constexpr int16_t kSharedStatusBannerShadowWidth = {shared_status_banner['shadow_width']};",
    f"constexpr uint8_t kSharedStatusBannerShadowOpacity = {shared_status_banner['shadow_opa']};",
    f"constexpr uint16_t kSharedStatusBannerShadowColor = {rgb565(shared_status_banner['shadow_color'])};",
    f"constexpr uint16_t kSharedStatusBannerInnerRimColor = {rgb565(shared_status_banner['inner_rim_color'])};",
    f"constexpr uint8_t kSharedStatusBannerInnerRimOpacity = {shared_status_banner['inner_rim_opa']};",
    f"constexpr int16_t kSharedStatusBannerReflectionX = {shared_status_banner['reflection_x']};",
    f"constexpr int16_t kSharedStatusBannerReflectionY = {shared_status_banner['reflection_y']};",
    f"constexpr int16_t kSharedStatusBannerReflectionInset = {shared_status_banner['reflection_inset']};",
    f"constexpr int16_t kSharedStatusBannerReflectionHeight = {shared_status_banner['reflection_height']};",
    f"constexpr uint8_t kSharedStatusBannerReflectionOpacity = {shared_status_banner['reflection_opa']};",
    f"constexpr int16_t kSharedStatusDividerX[] = {{{', '.join(str(x) for x in shared_status_dividers)}}};",
    f"constexpr uint8_t kSharedStatusDividerCount = {len(shared_status_dividers)};",
    f"constexpr int16_t kSharedStatusEraseInsetX = {shared_status_banner['erase_inset_x']};",
    f"constexpr int16_t kSharedStatusEraseY = {shared_status_banner['erase_y']};",
    f"constexpr int16_t kSharedStatusEraseHeight = {shared_status_banner['erase_height']};",
    f"constexpr int16_t kSharedStatusDividerY = {shared_status_banner['divider_y']};",
    f"constexpr int16_t kSharedStatusDividerHeight = {shared_status_banner['divider_height']};",
    f"constexpr uint8_t kSharedStatusDividerOpacity = {shared_status_banner['divider_opa']};",
    f"constexpr uint16_t kSharedStatusDividerColor = {rgb565(shared_status_banner['divider_color'])};",
    f"constexpr int16_t kSharedStatusBluetoothX = {shared_status_banner['bluetooth']['x']};",
    f"constexpr int16_t kSharedStatusBluetoothY = {shared_status_banner['bluetooth']['y']};",
    f"constexpr int16_t kSharedStatusWifiX = {shared_status_banner['wifi']['x']};",
    f"constexpr int16_t kSharedStatusWifiY = {shared_status_banner['wifi']['y']};",
    f"constexpr int16_t kSharedStatusCloudX = {shared_status_banner['cloud']['x']};",
    f"constexpr int16_t kSharedStatusCloudY = {shared_status_banner['cloud']['y']};",
    f"constexpr int16_t kSharedStatusTimeX = {shared_status_banner['time']['x']};",
    f"constexpr int16_t kSharedStatusTimeY = {shared_status_banner['time']['y']};",
    f"constexpr uint16_t kSharedStatusTimeColor = {rgb565(shared_status_banner['time']['color'])};",
    f"constexpr int16_t kSharedStatusTimeWidth = {shared_status_banner['time']['width']};",
    f"constexpr int16_t kSharedStatusTimeFontSize = {shared_status_banner['time']['font_size']};",
    f"constexpr int16_t kSharedStatusBatteryX = {shared_status_banner['battery']['x']};",
    f"constexpr int16_t kSharedStatusBatteryY = {shared_status_banner['battery']['y']};",
    f"constexpr int16_t kSharedStatusBatteryIconX = {banner_battery['icon_x']};",
    f"constexpr int16_t kSharedStatusBatteryIconY = {banner_battery['icon_y']};",
    f"constexpr int16_t kSharedStatusBatteryIconWidth = {banner_battery['icon_width']};",
    f"constexpr int16_t kSharedStatusBatteryIconHeight = {banner_battery['icon_height']};",
    f"constexpr int16_t kSharedStatusBatteryWidth = {banner_battery['width']};",
    f"constexpr int16_t kSharedStatusBatteryFontSize = {banner_battery['font_size']};",
    f"constexpr int16_t kSharedStatusBatteryGap = {banner_battery['gap']};",
    f"constexpr uint16_t kSharedStatusBatteryColor = {rgb565(shared_status_banner['battery']['color'])};",
    f"constexpr uint8_t kBatteryGreenAbove = {battery_thresholds['green_above']};",
    f"constexpr uint8_t kBatteryYellowFrom = {battery_thresholds['yellow_from']};",
    f"constexpr uint8_t kBatteryRedAtOrBelow = {battery_thresholds['red_at_or_below']};",
    f"constexpr uint32_t kBatteryGreen = 0x{battery_thresholds['green'].replace('#', '')};",
    f"constexpr uint32_t kBatteryYellow = 0x{battery_thresholds['yellow'].replace('#', '')};",
    f"constexpr uint32_t kBatteryRed = 0x{battery_thresholds['red'].replace('#', '')};",
    f"constexpr int16_t kHomeStatusCloudX = {home_layout['cloud_x']};",
    f"constexpr int16_t kHomeStatusBluetoothX = {home_layout['bluetooth_x']};",
    f"constexpr int16_t kHomeStatusWifiX = {home_layout['wifi_x']};",
    f"constexpr int16_t kHomeStatusIconY = {home_layout['icon_y']};",
    f"constexpr int16_t kHomeStatusTimeX = {home_layout['time_x']};",
    f"constexpr int16_t kHomeStatusTimeY = {home_layout['time_y']};",
    f"constexpr int16_t kHomeStatusTimeWidth = {home_layout['time_width']};",
    f"constexpr int16_t kHomeStatusBatteryIconX = {home_layout['battery_icon_x']};",
    f"constexpr int16_t kHomeStatusBatteryIconY = {home_layout['battery_icon_y']};",
    f"constexpr int16_t kHomeStatusBatteryIconWidth = {home_layout['battery_icon_width']};",
    f"constexpr int16_t kHomeStatusBatteryIconHeight = {home_layout['battery_icon_height']};",
    f"constexpr int16_t kHomeStatusBatteryX = {home_layout['battery_x']};",
    f"constexpr int16_t kHomeStatusBatteryY = {home_layout['battery_y']};",
    f"constexpr int16_t kHomeStatusTimeFontSize = {home_layout['time_font_size']};",
    f"constexpr int16_t kHomeStatusBatteryWidth = {home_layout['battery_width']};",
    f"constexpr int16_t kHomeStatusBatteryFontSize = {home_layout['battery_font_size']};",
    f"constexpr int16_t kHomeStatusDividerX[] = {{{', '.join(str(item) for item in home_layout['divider_x'])}}};",
    f"constexpr uint8_t kHomeStatusDividerOpacity = {status['home_pill']['divider_opa']};",
    f"constexpr int16_t kHomeStatusDividerY = {status['home_pill']['divider_y']};",
    f"constexpr int16_t kHomeStatusDividerHeight = {status['home_pill']['divider_height']};",
    f"constexpr uint16_t kHomeStatusDividerColor = {rgb565(status['home_pill']['divider_color'])};",
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
    f"constexpr uint8_t kSettingsBubbleFillOpacity = {settings_bubble['fill_opacity']};",
    f"constexpr uint32_t kSettingsBubbleInner = 0x{settings_bubble['gradient_inner'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleOuter = 0x{settings_bubble['gradient_outer'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleActiveInner = 0x{settings_bubble['active_gradient_inner'].replace('#', '')};",
    f"constexpr uint32_t kSettingsBubbleActiveOuter = 0x{settings_bubble['active_gradient_outer'].replace('#', '')};",
    f"constexpr int16_t kSettingsIconCenterXOffset = {settings_center['x_offset']};",
    f"constexpr int16_t kSettingsIconCenterYOffset = {int(settings_center['y_offset'] * 2)};",
    f"constexpr float kSettingsIconScale = {settings_center['scale']:.2f}f;",
    f"constexpr uint32_t kSettingsIconColor = 0x{settings_center['color'].replace('#', '')};",
    f"constexpr uint32_t kSettingsIconActiveColor = 0x{settings_center['active_color'].replace('#', '')};",
    f"constexpr uint32_t kSettingsIconActiveInnerGlow = 0x{settings_center['active_inner_glow'].replace('#', '')};",
    f"constexpr uint8_t kSettingsSelectedBorderOpacity = {settings_selected_overlay['border_opacity']};",
    f"constexpr uint32_t kThemeAccentBright[] = {{{', '.join('0x' + color['accentBright'].replace('#', '') for color in theme_accents)}}};",
    f"constexpr uint32_t kThemeAccent[] = {{{', '.join('0x' + color['accent'].replace('#', '') for color in theme_accents)}}};",
    f"constexpr uint16_t kThemeAccent565[] = {{{', '.join(rgb565(color['accent']) for color in theme_accents)}}};",
    f"constexpr uint8_t kThemeAccentCount = {len(theme_accents)};",
    f"constexpr int16_t kDisplayMenuViewportX = {interaction['settings_submenu']['list_viewport']['x']};",
    f"constexpr int16_t kDisplayMenuViewportY = {interaction['settings_submenu']['list_viewport']['y']};",
    f"constexpr int16_t kDisplayMenuViewportWidth = {interaction['settings_submenu']['list_viewport']['width']};",
    f"constexpr int16_t kDisplayMenuViewportHeight = {interaction['settings_submenu']['list_viewport']['height']};",
    f"constexpr int16_t kDisplayMenuRowHeight = {rendering['display_submenu']['row_height']};",
    f"constexpr int16_t kDisplayMenuRowGap = {rendering['display_submenu']['row_gap']};",
    f"constexpr int16_t kDisplayMenuIconBubbleDiameter = {rendering['display_submenu']['icon_bubble_diameter']};",
    f"constexpr int16_t kWeatherDurationDays[] = {{1, 3, 5}};",
    f"constexpr int16_t kWeatherIconBubbleX = {weather_icon['x']};",
    f"constexpr int16_t kWeatherIconBubbleY = {weather_icon['y']};",
    f"constexpr int16_t kWeatherIconBubbleSize = {weather_icon['size']};",
    f"constexpr int16_t kWeatherCityX = {weather_city['x']};",
    f"constexpr int16_t kWeatherCityY = {weather_city['y']};",
    f"constexpr int16_t kWeatherCityWidth = {weather_city['width']};",
    f"constexpr int16_t kWeatherCityFontSize = {weather_city['font_size']};",
    f"constexpr int16_t kWeatherConditionsX = {weather_conditions['x']};",
    f"constexpr int16_t kWeatherConditionsY = {weather_conditions['y']};",
    f"constexpr int16_t kWeatherConditionsWidth = {weather_conditions['width']};",
    f"constexpr int16_t kWeatherConditionsHeight = {weather_conditions['height']};",
    f"constexpr int16_t kWeatherHumidityWidth = {weather_conditions['humidity_width']};",
    f"constexpr int16_t kWeatherTemperatureWidth = {weather_conditions['temperature_width']};",
    f"constexpr int16_t kWeatherRainWidth = {weather_conditions['rain_width']};",
    f"constexpr int16_t kWeatherLabelFontSize = {weather_conditions['label_font_size']};",
    f"constexpr int16_t kWeatherHumidityFontSize = {weather_conditions['humidity_font_size']};",
    f"constexpr int16_t kWeatherTemperatureFontSize = {weather_conditions['temperature_font_size']};",
    f"constexpr int16_t kWeatherRainFontSize = {weather_conditions['rain_font_size']};",
    f"constexpr int16_t kWeatherForecastX = {weather_forecast['x']};",
    f"constexpr int16_t kWeatherForecastY = {weather_forecast['y']};",
    f"constexpr int16_t kWeatherForecastWidth = {weather_forecast['width']};",
    f"constexpr int16_t kWeatherForecastHeight = {weather_forecast['height']};",
    f"constexpr int16_t kWeatherForecastDays = {weather_forecast['days']};",
    f"constexpr int16_t kWeatherDurationButtonY = {weather_buttons['y']};",
    f"constexpr int16_t kWeatherDurationButtonSize = {weather_buttons['size']};",
    f"constexpr int16_t kWeatherDurationButtonCentersX[] = {{{', '.join(str(value) for value in weather_buttons['centers_x'])}}};",
    f"constexpr uint32_t kDisplayMenuText[] = {{0x{rendering['theme']['schemes']['dark']['text'][1:]}, 0x{rendering['theme']['schemes']['light']['text'][1:]}}};",
    f"constexpr uint32_t kDisplayMenuDisabled[] = {{0x{rendering['theme']['schemes']['dark']['disabled'][1:]}, 0x{rendering['theme']['schemes']['light']['disabled'][1:]}}};",
    f"constexpr uint32_t kDisplayMenuMuted[] = {{0x{rendering['theme']['schemes']['dark']['mutedText'][1:]}, 0x{rendering['theme']['schemes']['light']['mutedText'][1:]}}};",
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
    f"constexpr const char* kSettingsRowLabels[] = {{{', '.join(q(row['label']) for row in settings['rows'])}}};",
    f"constexpr uint8_t kSettingsRowCount = {len(settings['rows'])};",
    f"constexpr uint16_t kSettingsGlassRowDarkFill = {rgb565(settings_glass_row['dark_fill'])};",
    f"constexpr uint16_t kSettingsGlassRowDarkGradient = {rgb565(settings_glass_row['dark_gradient'])};",
    f"constexpr uint16_t kSettingsGlassRowLightFill = {rgb565(settings_glass_row['light_fill'])};",
    f"constexpr uint16_t kSettingsGlassRowLightGradient = {rgb565(settings_glass_row['light_gradient'])};",
    f"constexpr uint8_t kSettingsGlassRowFillOpacity = {settings_glass_row['fill_opacity']};",
    f"constexpr uint16_t kSettingsGlassRowDarkShadow = {rgb565(settings_glass_row['dark_shadow'])};",
    f"constexpr uint16_t kSettingsGlassRowLightShadow = {rgb565(settings_glass_row['light_shadow'])};",
    f"constexpr uint8_t kSettingsGlassRowBorderOpacity = {settings_glass_row['border_opacity']};",
    f"constexpr uint8_t kSettingsGlassRowBorderWidth = {settings_glass_row['border_width']};",
    f"constexpr uint8_t kSettingsGlassRowRestShadowWidth = {settings_glass_row['rest_shadow_width']};",
    f"constexpr uint8_t kSettingsGlassRowRestShadowOpacity = {settings_glass_row['rest_shadow_opacity']};",
    f"constexpr uint8_t kSettingsGlassRowActiveShadowWidth = {settings_glass_row['active_shadow_width']};",
    f"constexpr uint8_t kSettingsGlassRowActiveShadowOpacity = {settings_glass_row['active_shadow_opacity']};",
    f"constexpr uint8_t kSettingsGlassRowInnerRimOpacity = {settings_glass_row['inner_rim_opacity']};",
    f"constexpr int16_t kSettingsGlassRowReflectionInset = {settings_glass_row['reflection_inset']};",
    f"constexpr int16_t kSettingsGlassRowReflectionHeight = {settings_glass_row['reflection_height']};",
    f"constexpr uint8_t kSettingsGlassRowReflectionOpacity = {settings_glass_row['reflection_opacity']};",
    f"constexpr int16_t kSettingsGlassRowBubbleDiameter = {settings_glass_row['bubble_diameter']};",
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
    f"constexpr uint8_t kSettingsSliderTrackOpacity = {settings_slider['track_opacity']};",
    f"constexpr uint16_t kSettingsSliderKnobFill = {rgb565(settings_slider['knob_fill'])};",
    f"constexpr uint16_t kSettingsSliderKnobGradient = {rgb565(settings_slider['knob_gradient'])};",
    f"constexpr uint8_t kSettingsSliderKnobBorderWidth = {settings_slider['knob_border_width']};",
    f"constexpr uint8_t kSettingsSliderKnobShadowWidth = {settings_slider['knob_shadow_width']};",
    f"constexpr uint8_t kSettingsSliderKnobShadowOpacity = {settings_slider['knob_shadow_opacity']};",
    f"constexpr int16_t kSettingsBrightnessLabelX = {settings['brightness_label']['x']};",
    f"constexpr int16_t kSettingsBrightnessLabelY = {settings['brightness_label']['y']};",
    f"constexpr int16_t kSettingsBrightnessLabelFontSize = {settings['brightness_label']['font_size']};",
    "inline bool statusBarVisible(const char* screenId) {",
    "  return " + " || ".join(f'strcmp(screenId, {q(screen_id)}) == 0' for screen_id in status["visible_screens"]) + ";",
    "}",
    f"constexpr uint8_t kHomeSymbolGlintOpacity = {home_rendering['symbol_glint_opacity']};",
    f"constexpr uint8_t kHomeSymbolGlintWidth = {home_rendering['symbol_glint_width']};",
    f"constexpr uint16_t kHomeSpotifyAssetScale = {home_rendering['spotify_asset_scale']};",
    "inline uint16_t homeIconScale(const char* id) {",
    "  " + " ".join(f'if (strcmp(id, {q(key)}) == 0) return {scale};' for key, scale in home_rendering['icon_scales'].items()),
    "  return 100;",
    "}",
    f"constexpr int16_t kHomeSpotifyAssetWidth = {home_rendering['spotify_asset_width']};",
    f"constexpr int16_t kHomeSpotifyAssetHeight = {home_rendering['spotify_asset_height']};",
    f"constexpr uint16_t kHomeGlassFill = {rgb565(home_glass['fill'])};",
    f"constexpr uint8_t kHomeGlassFillOpacity = {home_glass['fill_opa']};",
    f"constexpr int16_t kHomeIconVisibleDiameter = {home_glass['icon_visible_diameter']};",
    f"constexpr uint8_t kHomeRestTintPercent = {home_glass['rest_tint_percent']};",
    f"constexpr uint8_t kHomePressedTintPercent = {home_glass['pressed_tint_percent']};",
    f"constexpr uint16_t kHomePressedScalePercent = {home_glass['pressed_scale_percent']};",
    f"constexpr int16_t kHomePillX = {rendering['status_bar']['home_pill']['x']};",
    f"constexpr int16_t kHomePillY = {rendering['status_bar']['home_pill']['y']};",
    f"constexpr int16_t kHomePillWidth = {status['home_pill']['width']};",
    f"constexpr int16_t kHomePillHeight = {status['home_pill']['height']};",
    f"constexpr uint16_t kHomePillFillColor = {rgb565(status['home_pill']['fill'])};",
    f"constexpr uint8_t kHomePillFillOpacity = {status['home_pill']['fill_opa'] * 255 // 100};",
    f"constexpr uint16_t kHomePillGradientColor = {rgb565(status['home_pill']['gradient_color'])};",
    f"constexpr int16_t kHomePillShadowWidth = {status['home_pill']['shadow_width']};",
    f"constexpr uint8_t kHomePillShadowOpacity = {status['home_pill']['shadow_opa']};",
    f"constexpr uint16_t kHomePillShadowColor = {rgb565(status['home_pill']['shadow_color'])};",
    f"constexpr uint16_t kHomePillBorder = {rgb565(status['home_pill']['border'])};",
    f"constexpr uint8_t kHomePillBorderOpacity = {status['home_pill']['border_opa']};",
    f"constexpr uint8_t kHomePillBorderWidth = {status['home_pill']['border_width']};",
    f"constexpr uint16_t kHomePillInnerRimColor = {rgb565(status['home_pill']['inner_rim_color'])};",
    f"constexpr uint8_t kHomePillInnerRimOpacity = {status['home_pill']['inner_rim_opa']};",
    f"constexpr int16_t kHomePillRadius = {status['home_pill']['border_radius']};",
    f"constexpr uint16_t kHomePillReflectionColor = 0xFFFF;",
    f"constexpr int16_t kHomePillReflectionX = {status['home_pill']['reflection_x']};",
    f"constexpr int16_t kHomePillReflectionY = {status['home_pill']['reflection_y']};",
    f"constexpr int16_t kHomePillReflectionInset = {status['home_pill']['reflection_inset']};",
    f"constexpr int16_t kHomePillReflectionHeight = {status['home_pill']['reflection_height']};",
    f"constexpr uint8_t kHomePillReflectionOpacity = {status['home_pill']['reflection_opa']};",
    f"constexpr int16_t kHomeStatusTouchY = {interaction['touch']['home_status']['y']};",
    f"constexpr int16_t kHomeStatusTouchHeight = {interaction['touch']['home_status']['height']};",
    f"constexpr int16_t kHomeStatusCloudTouchX = {interaction['touch']['home_status']['cloud']['x']};",
    f"constexpr int16_t kHomeStatusCloudTouchWidth = {interaction['touch']['home_status']['cloud']['width']};",
    f"constexpr int16_t kHomeStatusBluetoothTouchX = {interaction['touch']['home_status']['bluetooth']['x']};",
    f"constexpr int16_t kHomeStatusBluetoothTouchWidth = {interaction['touch']['home_status']['bluetooth']['width']};",
    f"constexpr int16_t kHomeStatusWifiTouchX = {interaction['touch']['home_status']['wifi']['x']};",
    f"constexpr int16_t kHomeStatusWifiTouchWidth = {interaction['touch']['home_status']['wifi']['width']};",
    f"constexpr uint16_t kHomeGlassBorder = {rgb565(home_glass['border'])};",
    f"constexpr uint8_t kHomeGlassBorderOpacity = {home_glass['border_opa']};",
    f"constexpr uint16_t kHomeGlassLightFill = {rgb565(home_glass['light_fill'])};",
    f"constexpr uint8_t kHomeGlassLightFillOpacity = {home_glass['light_fill_opa']};",
    f"constexpr uint16_t kHomeGlassLightBorder = {rgb565(home_glass['light_border'])};",
    f"constexpr uint8_t kHomeGlassLightBorderOpacity = {home_glass['light_border_opa']};",
    f"constexpr uint16_t kHomeGlassLightInset = {rgb565(home_glass['light_inset'])};",
    f"constexpr uint8_t kHomeGlassLightInsetOpacity = {home_glass['light_inset_opa']};",
    f"constexpr uint16_t kHomeLightNeutralIcon = {rgb565(home_glass['light_neutral_icon'])};",
    f"constexpr uint8_t kHomeGlassBorderWidth = {home_glass['border_width']};",
    f"constexpr int16_t kHomeBubbleDiameter = {home_glass['rest_diameter']};",
    f"constexpr uint8_t kHomeGlassRestShadowWidth = {home_glass['rest_shadow_width']};",
    f"constexpr uint8_t kHomeGlassRestShadowOpacity = {home_glass['rest_shadow_opa']};",
    f"constexpr uint16_t kHomeGlassShadowColor = {rgb565(home_glass['shadow_color'])};",
    f"constexpr int16_t kHomeXIconWidth = {home_x_icon['width']};",
    f"constexpr int16_t kHomeXIconHeight = {home_x_icon['height']};",
    f"constexpr int16_t kHomeXIconStroke = {home_x_icon['stroke']};",
    f"constexpr int16_t kHomeXIconSegments[][4] = {{{', '.join('{' + ', '.join(str(item) for item in segment) + '}' for segment in home_x_icon['segments'])}}};",
    f"constexpr uint8_t kHomeXIconSegmentCount = {len(home_x_icon['segments'])};",
    f"constexpr uint8_t kHomeGlassInsetOpacity = {home_glass['inset_opa']};",
    f"constexpr uint8_t kHomeGlassPressedShadowWidth = {home_glass['pressed_shadow_width']};",
    f"constexpr uint8_t kHomeGlassPressedShadowOpacity = {home_glass['pressed_shadow_opa']};",
    f"constexpr uint16_t kHomeGlassReflectionColor = {rgb565(home_glass['reflection_color'])};",
    f"constexpr uint8_t kHomeGlassReflectionWidth = {home_glass['reflection_width']};",
    f"constexpr uint8_t kHomeGlassReflectionOpacity = {home_glass['reflection_opa']};",
    f"constexpr uint8_t kHomeInnerHighlightOpacity = {home_glass['inner_highlight_opa']};",
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
    f"constexpr bool kSpotifyArcActiveUsesAccent = {str(spotify_arc_uses_accent).lower()};",
    f"constexpr uint8_t kSpotifyArcTrackOpacity = {arc['track_opacity']};",
    f"constexpr int16_t kSpotifyArcKnobDiameter = {arc['knob_diameter']};",
    f"constexpr uint16_t kSpotifyArcKnobFill = {rgb565(arc['knob_fill'])};",
    f"constexpr uint16_t kSpotifyArcKnobGradient = {rgb565(arc['knob_gradient'])};",
    f"constexpr uint8_t kSpotifyArcKnobBorderWidth = {arc['knob_border_width']};",
    f"constexpr uint8_t kSpotifyArcKnobShadowWidth = {arc['knob_shadow_width']};",
    f"constexpr uint8_t kSpotifyArcKnobShadowOpacity = {arc['knob_shadow_opacity']};",
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
    f"constexpr bool kSpotifySeekActiveUsesAccent = {str(spotify_seek_uses_accent).lower()};",
    f"constexpr uint8_t kSpotifySeekTrackOpacity = {seek['track_opacity']};",
    f"constexpr int16_t kSpotifySeekKnobDiameter = {seek['knob_diameter']};",
    f"constexpr uint16_t kSpotifySeekKnobFill = {rgb565(seek['knob_fill'])};",
    f"constexpr uint16_t kSpotifySeekKnobGradient = {rgb565(seek['knob_gradient'])};",
    f"constexpr uint8_t kSpotifySeekKnobBorderWidth = {seek['knob_border_width']};",
    f"constexpr uint8_t kSpotifySeekKnobShadowWidth = {seek['knob_shadow_width']};",
    f"constexpr uint8_t kSpotifySeekKnobShadowOpacity = {seek['knob_shadow_opacity']};",
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
])

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

home_asset = (root / "shared" / "assets" / "home-controls-outward-10-466.bin").read_bytes()
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
])
for name, file in (
    ("kSettingsWallpaperAssetData", "settings-gears-wallpaper-466.bin"),
    ("kSettingsGearsAssetData", "settings-glass-gears-v1-466.bin"),
    ("kSettingsGearsRainbowAssetData", "settings-gears-rainbow-466.bin"),
    ("kSettingsStatusBluetoothActiveData", "settings-glass-status-bluetooth-active-22x22-v1.bin"),
    ("kSettingsStatusWifiActiveData", "settings-glass-status-wifi-active-22x22-v1.bin"),
    ("kSettingsStatusCloudActiveData", "settings-glass-status-cloud-active-24x24-v1.bin"),
    ("kSettingsWifiPressedIconMaskData", "settings-rainbow-wifi-pressed-icon-mask.bin"),
    ("kSettingsWifiPressedChevronMaskData", "settings-rainbow-wifi-pressed-chevron-mask.bin"),
    ("kSettingsBluetoothPressedIconMaskData", "settings-rainbow-bluetooth-pressed-icon-mask.bin"),
    ("kSettingsBluetoothPressedChevronMaskData", "settings-rainbow-bluetooth-pressed-chevron-mask.bin"),
    ("kSettingsDisplayPressedIconMaskData", "settings-rainbow-display-pressed-icon-mask.bin"),
    ("kSettingsDisplayPressedChevronMaskData", "settings-rainbow-display-pressed-chevron-mask.bin"),
    ("kSettingsSoundPressedIconMaskData", "settings-rainbow-sound-pressed-icon-mask.bin"),
    ("kSettingsSoundPressedChevronMaskData", "settings-rainbow-sound-pressed-chevron-mask.bin"),
    ("kSettingsPressedBorderMaskData", "settings-glass-pressed-border-mask-322x56-v1.bin"),
    ("kSettingsTitleGearData", "settings-title-gear.bin"),
    ("kSettingsRowWifiData", "settings-row-wifi.bin"),
    ("kSettingsRowBluetoothData", "settings-row-bluetooth.bin"),
    ("kSettingsRowDisplayData", "settings-row-display.bin"),
    ("kSettingsRowSoundData", "settings-row-sound.bin"),
    ("kDisplayBackgroundIconData", "display-icon-background.bin"),
    ("kDisplayAppearanceIconData", "display-icon-appearance.bin"),
    ("kDisplayClockIconData", "display-icon-clock.bin"),
    ("kDisplayUnitsIconData", "display-icon-units.bin"),
    ("kDisplayLanguageIconData", "display-icon-language.bin"),
    ("kDisplayWeatherLocationIconData", "display-icon-weather-location.bin"),
    ("kDisplayIconData", "display-icon-display.bin"),
    ("kBackgroundPreviewFlowerData", "background-preview-flower.bin"),
    ("kBackgroundPreviewChevronData", "background-preview-chevron.bin"),
    ("kBackgroundPreviewMechanicalData", "background-preview-mechanical.bin"),
    ("kBackgroundPreviewBlackWhiteCircleData", "background-preview-black-white-circle.bin"),
    ("kClockBackgroundPreviewFlowerData", "clock-background-preview-flower.bin"),
    ("kClockBackgroundPreviewChevronData", "clock-background-preview-chevron.bin"),
    ("kClockBackgroundPreviewMechanicalData", "clock-background-preview-mechanical.bin"),
    ("kClockFacePreview0Data", "clock-face-preview-0.bin"),
    ("kClockFacePreview1Data", "clock-face-preview-1.bin"),
    ("kClockFacePreview2Data", "clock-face-preview-2.bin"),
):
    payload = (root / "shared" / "assets" / file).read_bytes()
    settings_asset_lines.append(f"const uint8_t {name}[] PROGMEM = {{")
    for offset in range(0, len(payload), 16):
        settings_asset_lines.append("  " + ", ".join(f"0x{value:02X}" for value in payload[offset : offset + 16]) + ",")
    settings_asset_lines.extend(["};", f"constexpr size_t {name}Size = sizeof({name});"])
settings_asset_lines.extend(["}  // namespace leopold::sky", ""])
settings_asset_generated = "\n".join(settings_asset_lines)
if not settings_asset_target.exists() or settings_asset_target.read_text(encoding="utf-8") != settings_asset_generated:
    settings_asset_target.write_text(settings_asset_generated, encoding="utf-8")

# Keep browser and C++ WASM asset slots on one ordered contract. Asset indices
# are ABI values passed through the JS module, so insertion/deletion must reach
# both consumers together.
wasm_index_source = root / "shared" / "wasm-asset-index.json"
wasm_index = json.loads(wasm_index_source.read_text(encoding="utf-8"))
wasm_keys = wasm_index["keys"]
asset_keys = set(json.loads((root / "shared" / "asset-manifest.json").read_text(encoding="utf-8"))["assets"])
if len(wasm_keys) != len(set(wasm_keys)) or not set(wasm_keys).issubset(asset_keys):
    raise ValueError("WASM asset index contains duplicate or unknown logical IDs")
wasm_index_text = json.dumps(wasm_index, indent=2, ensure_ascii=False) + "\n"
if wasm_index_source.read_text(encoding="utf-8") != wasm_index_text:
    wasm_index_source.write_text(wasm_index_text, encoding="utf-8")
wasm_ts_target = root / "frontend" / "src" / "shared" / "wasm-asset-index.json"
if not wasm_ts_target.exists() or wasm_ts_target.read_text(encoding="utf-8") != wasm_index_text:
    wasm_ts_target.write_text(wasm_index_text, encoding="utf-8")
wasm_header_target = root / "shared" / "wasm_asset_index.h"
wasm_header_lines = ["#pragma once", "namespace leopold::wasm {", "inline constexpr const char* kAssetKeys[] = {"]
wasm_header_lines.extend(f'  "{key}",' for key in wasm_keys)
wasm_header_lines.extend(["};", "inline constexpr int kAssetCount = sizeof(kAssetKeys) / sizeof(kAssetKeys[0]);", "}", ""])
wasm_header_text = "\n".join(wasm_header_lines)
if not wasm_header_target.exists() or wasm_header_target.read_text(encoding="utf-8") != wasm_header_text:
    wasm_header_target.write_text(wasm_header_text, encoding="utf-8")
