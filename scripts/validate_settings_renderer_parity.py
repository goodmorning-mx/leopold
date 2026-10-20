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
    ('kSettingsWifiPressedIconMaskData', 'settings-glass-wifi-pressed-icon-mask-61x56-v1.bin'),
    ('kSettingsWifiPressedChevronMaskData', 'settings-glass-wifi-pressed-chevron-mask-39x56-v1.bin'),
    ('kSettingsBluetoothPressedIconMaskData', 'settings-glass-bluetooth-pressed-icon-mask-61x56-v1.bin'),
    ('kSettingsBluetoothPressedChevronMaskData', 'settings-glass-bluetooth-pressed-chevron-mask-39x56-v1.bin'),
    ('kSettingsDisplayPressedIconMaskData', 'settings-glass-display-pressed-icon-mask-61x56-v1.bin'),
    ('kSettingsDisplayPressedChevronMaskData', 'settings-glass-display-pressed-chevron-mask-39x56-v1.bin'),
    ('kSettingsSoundPressedIconMaskData', 'settings-glass-sound-pressed-icon-mask-61x56-v1.bin'),
    ('kSettingsSoundPressedChevronMaskData', 'settings-glass-sound-pressed-chevron-mask-39x56-v1.bin'),
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
        "status-shell": (137, 39, 195, 38),
        "live-clock": (216, 45, 49, 26),
        "wifi-status": (181, 45, 22, 22),
        "cloud-status": (264, 45, 24, 24),
        "bluetooth-status": (153, 45, 22, 22),
        "battery-status": (294, 45, 31, 24),
        "brightness-control": (63, 349, 342, 75),
    }
    if dynamic_masks != required_masks:
        fail(f"declared dynamic masks changed: {dynamic_masks}")

    renderer = (ROOT / "shared/lvgl_renderer.cpp").read_text(encoding="utf-8")
    render_start = renderer.index("void renderSettings(")
    render_end = renderer.index("const SduiComponent* findComponent", render_start)
    settings_render = renderer[render_start:render_end]
    for needle, label in (
        ('resolveImage("settings-neutral")', "exact neutral asset lookup"),
        ('"SETTINGS ASSET ERROR"', "missing-asset diagnostic"),
        ("selectedRowId", "shared selected-row state"),
        ("drawSelectedSettingsRow(root, selectedIndex, resolveImage)", "bounded selected-row treatment"),
        ("renderSettingsLiveStatus(root, state, resolveImage);", "live status composition"),
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
    for forbidden, label in (
        ("drawGear(", "native Settings gear redraw"),
        ("kSettingsTitle", "native Settings title redraw"),
        ("kSettingsRow", "native Settings row redraw"),
        ("drawChevron(", "native Settings chevron redraw"),
    ):
        if forbidden in settings_render:
            fail(f"{label} remains in exact Settings composition")

    status = renderer[renderer.index("void renderSettingsLiveStatus"):renderer.index("void renderSettingsLiveBrightness")]
    contains(status, "state.timeText == nullptr ? \"--:--\"", "deterministic missing-clock fallback")
    for token, label in (("153, 45", "Bluetooth overlay position"), ("centerX = 192", "Wi-Fi overlay position"),
                         ("217, 50", "clock overlay position"), ("264, 45", "cloud overlay position"),
                         ("291, 52", "battery overlay position")):
        contains(status, token, label)
    for token, label in (("generated_ui::kSettingsIconActiveColor", "shared active status color"),
                         ("generated_ui::kSettingsIconActiveInnerGlow", "shared status inner glow")):
        contains(status, token, label)

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
    contains(simulator, "framebufferCaptureMode || (screen.id === 'device-control-center' && localPanel === 'none') ? <FirmwareLvglCanvas", "normal and capture Settings LVGL/WASM path")
    if "function drawSettings(" in legacy_canvas:
        fail("a second Settings renderer remains in the legacy browser canvas")
    contains(lvgl_canvas, "settings-neutral", "browser exact Settings asset mapping")
    for key in (
        "settings-pressed-border-mask",
        "settings-wifi-pressed-icon-mask", "settings-wifi-pressed-chevron-mask",
        "settings-bluetooth-pressed-icon-mask", "settings-bluetooth-pressed-chevron-mask",
        "settings-display-pressed-icon-mask", "settings-display-pressed-chevron-mask",
        "settings-sound-pressed-icon-mask", "settings-sound-pressed-chevron-mask",
    ):
        contains(renderer, key, f"shared bounded component asset {key}")
        contains(lvgl_canvas, key, f"browser bounded component asset {key}")
        contains(wasm, key, f"WASM bounded component asset {key}")
    for key in (
        "settings-status-bluetooth-active", "settings-status-wifi-active", "settings-status-cloud-active",
    ):
        contains(renderer, key, f"shared bounded status asset {key}")
        contains(lvgl_canvas, key, f"browser bounded status asset {key}")
        contains(wasm, key, f"WASM bounded status asset {key}")
    contains(wasm, '"settings-neutral"', "WASM exact Settings asset mapping")
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
    embedded_loader = loader.split("bool loadEmbeddedSettingsAsset() {", 1)[1].split("bool allAssetsLoaded()", 1)[0]
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
        composition = json.loads(composition_path.read_text())
        for entry, name, checksum in (
            (composition["source"], "file", "sha256"),
            (composition["output"], "png", "png_sha256"),
            (composition["output"], "device_asset", "device_asset_sha256"),
        ):
            asset = ROOT / "shared" / entry[name]
            if hashlib.sha256(asset.read_bytes()).hexdigest() != entry[checksum]:
                fail(f"background composition input/output differs: {asset}")
        for source, label in ((settings_render, "shared renderer"), (lvgl_canvas, "browser asset mapping"),
                              (wasm, "WASM mapping")):
            contains(source, "settings-gears-base", label + " background")
        block = embedded_source.split("const uint8_t kSettingsGearsAssetData[] PROGMEM = {", 1)[1].split("};", 1)[0]
        payload = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", block))
        if payload != (ROOT / "shared" / composition["output"]["device_asset"]).read_bytes():
            fail("embedded Settings background differs from shared device asset")

    print("Settings renderer parity valid: exact asset, shared LVGL/WASM path, bounded live overlays, embedded asset")


if __name__ == "__main__":
    main()
