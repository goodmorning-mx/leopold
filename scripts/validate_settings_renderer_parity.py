#!/usr/bin/env python3
"""Guard the shared exact-reference Settings renderer contract."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMBEDDED_PRESSED = [
    ('kSettingsWifiPressedIconMaskData', 'settings-rainbow-wifi-pressed-icon-mask.bin'),
    ('kSettingsWifiPressedChevronMaskData', 'settings-rainbow-wifi-pressed-chevron-mask.bin'),
    ('kSettingsBluetoothPressedIconMaskData', 'settings-rainbow-bluetooth-pressed-icon-mask.bin'),
    ('kSettingsBluetoothPressedChevronMaskData', 'settings-rainbow-bluetooth-pressed-chevron-mask.bin'),
    ('kSettingsDisplayPressedIconMaskData', 'settings-rainbow-display-pressed-icon-mask.bin'),
    ('kSettingsDisplayPressedChevronMaskData', 'settings-rainbow-display-pressed-chevron-mask.bin'),
    ('kSettingsSoundPressedIconMaskData', 'settings-rainbow-sound-pressed-icon-mask.bin'),
    ('kSettingsSoundPressedChevronMaskData', 'settings-rainbow-sound-pressed-chevron-mask.bin'),
    ('kSettingsPressedBorderMaskData', 'settings-glass-pressed-border-mask-322x56-v1.bin'),
]


def fail(message: str) -> None:
    print(f"Settings renderer parity error: {message}", file=sys.stderr)
    raise SystemExit(1)


def contains(source: str, needle: str, label: str) -> None:
    if needle not in source:
        fail(f"{label} is missing")


def inside(bounds: tuple[int, int, int, int], mask: tuple[int, int, int, int]) -> bool:
    x, y, width, height = bounds
    mx, my, mw, mh = mask
    return mx <= x and my <= y and x + width <= mx + mw and y + height <= my + mh


def main() -> None:
    manifest = json.loads((ROOT / "shared/reference-manifest.json").read_text(encoding="utf-8"))
    settings = next(state for state in manifest["states"] if state["name"] == "settings")
    if settings["source"]["width"] != 466 or settings["source"]["height"] != 466:
        fail("Settings exact source is not 466x466")
    if not settings["source"].get("immutable") or settings["generated_asset"].get("format") != "lossless PNG":
        fail("Settings is not in exact-reference-asset mode")
    for entry in (settings["source"], settings["generated_asset"], settings["device_asset"]):
        path = ROOT / "shared" / entry["file"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            fail(f"Settings source or generated asset checksum mismatch: {path}")
    for entry in settings.get("active_status_assets", []):
        path = ROOT / "shared" / entry["asset"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            fail(f"active Settings status asset checksum mismatch: {path}")

    dynamic_masks = {
        item["name"]: (item["x"], item["y"], item["width"], item["height"])
        for item in settings["dynamic_masks"]
    }
    required_masks = {
        "status-shell": (137, 39, 195, 40),
        "live-clock": (216, 45, 49, 26),
        "wifi-status": (181, 49, 22, 22),
        "cloud-status": (264, 48, 24, 24),
        "bluetooth-status": (153, 49, 22, 22),
        "battery-status": (291, 52, 34, 20),
        "brightness-control": (63, 349, 342, 75),
    }
    if dynamic_masks != required_masks:
        fail(f"declared dynamic masks changed: {dynamic_masks}")

    renderer = (ROOT / "shared/lvgl_renderer.cpp").read_text(encoding="utf-8")
    banner_start = renderer.index("void renderSharedStatusBanner(")
    banner_end = renderer.index("void renderSettingsLiveBrightness(", banner_start)
    status_banner = renderer[banner_start:banner_end]
    contains(status_banner, "drawGlassCapsule(root, generated_ui::kSharedStatusBannerX", "shared translucent capsule geometry")
    contains(status_banner, "drawStatusContent(root, state, false, resolveImage);", "single shared status content renderer")
    contains(status_banner, "kSharedStatusBannerWidth", "generated shared banner width")
    contains(status_banner, "kSharedStatusBannerBorderWidth", "shared outline weight")
    render_start = renderer.index("void renderSettings(")
    render_end = renderer.index("const SduiComponent* findComponent", render_start)
    settings_render = renderer[render_start:render_end]
    for needle, label in (
        ('resolveImage("settings-neutral")', "legacy neutral fallback lookup"),
        ('resolveImage("settings-gears-base")', "shared liquid-glass backdrop lookup"),
        ('"SETTINGS ASSET ERROR"', "missing-asset diagnostic"),
        ("selectedRowId", "shared selected-row state"),
        ("drawSettingsGlassRows(root, screen, selectedRowId)", "shared live Settings glass rows and scroll positions"),
        ("renderSettingsLiveBrightness(root, state, gears != nullptr);", "live brightness composition"),
    ):
        contains(settings_render, needle, label)
    for forbidden, label in (
        ('"settings-wifi-pressed"', "Wi-Fi full-row pressed crop"),
        ('"settings-bluetooth-pressed"', "Bluetooth full-row pressed crop"),
        ('"settings-display-pressed"', "Display full-row pressed crop"),
        ('"settings-sound-pressed"', "Sound full-row pressed crop"),
    ):
        if forbidden in settings_render:
            fail(f"{label} remains in runtime Settings composition")
    contains(renderer, "generated_ui::kSettingsGlassRowBubbleDiameter", "generated glass row bubble size")
    contains(renderer, "generated_ui::kSettingsMenuHeight", "clipped Settings menu viewport")
    contains(renderer, "index < 5", "five Settings rows including Accounts and Sound")
    contains(renderer, "accentBright565()", "live Appearance accent for glass rows")

    status_start = renderer.index("void drawStatusContent(")
    status_end = renderer.index("void drawSettings", status_start)
    status = renderer[status_start:status_end]
    contains(status, "state.timeText", "shared banner uses the renderer clock state")
    generated = (ROOT / "device/include/generated_firmware_ui.h").read_text(encoding="utf-8")
    for token, label in (("kSharedStatusBluetoothY = 54", "Bluetooth overlay vertical position"),
                         ("kSharedStatusWifiY = 54", "Wi-Fi overlay vertical position"),
                         ("kSharedStatusCloudY = 54", "cloud overlay vertical position"),
                         ("kSharedStatusTimeY = 53", "clock overlay vertical position"),
                         ("kSharedStatusBatteryY = 53", "battery overlay vertical position")):
        contains(generated, token, label)
    shared_status = status_banner
    connectivity_start = renderer.index("void drawConnectivityGroup(")
    connectivity_end = renderer.index("void drawStatusContent(", connectivity_start)
    connectivity = renderer[connectivity_start:connectivity_end]
    contains(connectivity, "state.bluetoothConnected", "direct Bluetooth status color state")
    contains(connectivity, "state.wifiConnected", "direct Wi-Fi status color state")
    contains(connectivity, "state.cloudConnected", "direct cloud status color state")

    brightness = renderer[renderer.index("void renderSettingsLiveBrightness"):renderer.index("void renderSettings(")]
    contains(brightness, "if (!glassBackdrop)", "glass background is preserved below live brightness")
    contains(brightness, "makePanel(root, 63, 349, 342, 75", "legacy neutral fallback brightness mask clear")
    for bounds in ((98, 358, 272, 28), (153, 400, 160, 19)):
        if not inside(bounds, dynamic_masks["brightness-control"]):
            fail(f"brightness overlay bounds escape declared mask: {bounds}")

    simulator = (ROOT / "frontend/src/simulator/Simulator.tsx").read_text(encoding="utf-8")
    lvgl_canvas = (ROOT / "frontend/src/simulator/FirmwareLvglCanvas.tsx").read_text(encoding="utf-8")
    legacy_canvas = (ROOT / "frontend/src/simulator/FirmwareCanvas.tsx").read_text(encoding="utf-8")
    wasm = (ROOT / "frontend/wasm/lvgl_wasm.cpp").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build_lvgl_wasm.py").read_text(encoding="utf-8")
    contains(simulator, "localPanel === 'none' && !calibrationMode ? <FirmwareLvglCanvas", "normal and capture LVGL/WASM path")
    if "function drawSettings(" in legacy_canvas:
        fail("a second Settings renderer remains in the legacy browser canvas")
    contains(lvgl_canvas, "settings-neutral", "browser neutral fallback Settings asset mapping")
    contains(lvgl_canvas, "settings-gears-base", "browser shared liquid-glass backdrop mapping")
    for key in (
        "settings-pressed-border-mask",
        "settings-wifi-pressed-icon-mask", "settings-wifi-pressed-chevron-mask",
        "settings-bluetooth-pressed-icon-mask", "settings-bluetooth-pressed-chevron-mask",
        "settings-display-pressed-icon-mask", "settings-display-pressed-chevron-mask",
        "settings-sound-pressed-icon-mask", "settings-sound-pressed-chevron-mask",
    ):
        contains(renderer, key, f"shared bounded component asset {key}")
        contains(lvgl_canvas, key, f"browser bounded component asset {key}")
    contains(lvgl_canvas, "_lvgl_wasm_render", "browser invokes shared LVGL renderer")
    contains(build, "shared/lvgl_renderer.cpp", "WASM shared renderer source")

    embedded = ROOT / "device/include/generated_settings_asset.h"
    if not embedded.is_file():
        fail("embedded Settings exact asset is unavailable")
    embedded_source = embedded.read_text(encoding="utf-8")
    neutral_block = embedded_source.split("const uint8_t kSettingsAssetData[] PROGMEM = {", 1)[1].split("};", 1)[0]
    values = re.findall(r"0x([0-9A-Fa-f]{2})", neutral_block)
    embedded_bytes = bytes(int(value, 16) for value in values)
    canonical = ROOT / "shared/assets/settings-neutral-466.bin"
    if embedded_bytes != canonical.read_bytes():
        fail("embedded Settings exact asset differs from canonical RGB565A8 asset")
    expected_sha = settings["device_asset"]["sha256"]
    if hashlib.sha256(embedded_bytes).hexdigest() != expected_sha:
        fail("embedded Settings exact asset checksum does not match manifest")
    for entry, symbol in zip(settings.get("active_status_assets", []), (
        "kSettingsStatusBluetoothActiveData", "kSettingsStatusWifiActiveData", "kSettingsStatusCloudActiveData",
    )):
        block = embedded_source.split(f"const uint8_t {symbol}[] PROGMEM = {{", 1)[1].split("};", 1)[0]
        payload = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", block))
        if payload != (ROOT / "shared" / entry["asset"]).read_bytes():
            fail(f"embedded active Settings status asset differs from canonical: {symbol}")

    # Every Settings runtime overlay must resolve offline, not only in WASM/cloud.
    loader = (ROOT / "device/src/sky_assets.cpp").read_text()
    embedded_loader = loader.split("bool loadEmbeddedSettingsAsset() {", 1)[1].split("const lv_image_dsc_t* resolveAsset", 1)[0]
    for symbol, filename in EMBEDDED_PRESSED:
        block = embedded_source.split(f"const uint8_t {symbol}[] PROGMEM = {{", 1)[1].split("};", 1)[0]
        payload = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", block))
        if payload != (ROOT / "shared/assets" / filename).read_bytes():
            fail(f"embedded pressed asset differs from canonical: {filename}")
        contains(embedded_loader, f'loadCloudAsset("{filename}", {symbol}, {symbol}Size)', f"offline pressed asset {filename}")
    interaction = json.loads((ROOT / "shared/interaction.json").read_text())
    document = json.loads((ROOT / "shared/firmware-ui.json").read_text())
    controls = next(screen for screen in document["screens"] if screen["id"] == "device-control-center")["components"]
    for row in interaction["touch"]["settings_row_bounds"]:
        component = next(control for control in controls if control["id"] == row["id"])
        if any(component[key] != row[key] for key in ("x", "y", "width", "height")):
            fail(f"simulator/device Settings hit bounds disagree: {row['id']}")

    for relative in (
        "shared/assets/settings-neutral-466.png",
        "shared/assets/settings-neutral-466.bin",
        "device/assets/settings-neutral-466.bin",
        "frontend/public/firmware-assets/settings-neutral-466.bin",
    ):
        if not (ROOT / relative).is_file():
            fail(f"required exact asset is unavailable: {relative}")

    composition_entry = settings.get("background_composition")
    if composition_entry:
        composition_path = ROOT / "shared" / composition_entry["manifest"]
        if hashlib.sha256(composition_path.read_bytes()).hexdigest() != composition_entry["sha256"]:
            fail("background composition provenance differs")
        if composition_entry.get("runtime_asset_key") != "settings-gears-base":
            fail("Settings backing composition must retain the declared firmware asset identity")

    print("Settings renderer parity valid: exact asset, shared LVGL/WASM path, bounded live overlays, embedded asset")


if __name__ == "__main__":
    main()
