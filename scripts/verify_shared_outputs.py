"""Verify that generated consumer assets still match shared/assets."""

from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "assets"
DESTINATIONS = (
    ROOT / "backend" / "assets",
    ROOT / "device" / "assets",
    ROOT / "frontend" / "public" / "firmware-assets",
)
CONTRACT_SOURCE = ROOT / "shared"
CONTRACT_DESTINATION = ROOT / "frontend" / "src" / "shared"
CONTRACTS = ("firmware-ui.json", "interaction.json", "typography.json", "persistence.json", "asset-manifest.json", "rendering.json", "reference-manifest.json", "reference-states.json", "wasm-asset-index.json")

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

errors = []
assets = [source for source in sorted(SOURCE.iterdir()) if source.is_file() and source.suffix in {".bin", ".png"}]
for source in assets:
    for destination in DESTINATIONS:
        candidate = destination / source.name
        if not candidate.is_file() or digest(source) != digest(candidate):
            errors.append(f"{candidate} does not match {source}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
for name in CONTRACTS:
    source = CONTRACT_SOURCE / name
    candidate = CONTRACT_DESTINATION / name
    if not candidate.is_file() or digest(source) != digest(candidate):
        errors.append(f"{candidate} does not match {source}")
asset_manifest = json.loads((CONTRACT_SOURCE / "asset-manifest.json").read_text())["assets"]
native_slots = (ROOT / "device" / "src" / "sky_assets.cpp").read_text()
wasm_index = json.loads((CONTRACT_SOURCE / "wasm-asset-index.json").read_text())
wasm_header = (CONTRACT_SOURCE / "wasm_asset_index.h").read_text()
wasm_cpp = (ROOT / "frontend" / "wasm" / "lvgl_wasm.cpp").read_text()
wasm_ts = (ROOT / "frontend" / "src" / "simulator" / "FirmwareLvglCanvas.tsx").read_text()
header_match = re.search(r"kAssetKeys\[\]\s*=\s*\{(.*?)\};", wasm_header, re.S)
header_keys = re.findall(r'"([^"]+)"', header_match.group(1)) if header_match else []
if (header_keys != wasm_index["keys"] or
        re.search(r"assets\[\d+\]|assetReady\[\d+\]|index\s*>=\s*46", wasm_cpp) or
        "wasm_asset_index.h" not in wasm_cpp or
        "wasmAssetIndex.keys" not in wasm_ts):
    errors.append("WASM C++ and browser asset slots do not match the ordered shared index contract")
for logical_id in ("home-settings-gear", "home-usb-icon", "home-glass-rim", "status-cloud-icon", "status-bluetooth-icon", "status-wifi-icon", "open-clock"):
    asset = asset_manifest[logical_id]
    native_slot = re.search(rf'\{{"{re.escape(logical_id)}", "{re.escape(asset["file"])}", (\d+), (\d+),', native_slots)
    if logical_id not in wasm_index["keys"] or not native_slot or (int(native_slot.group(1)), int(native_slot.group(2))) != (asset["width"], asset["height"]):
        errors.append(f"reference-derived Home/status asset is missing an aligned native/WASM slot: {logical_id}")
backend_manifest = ROOT / "backend" / "assets" / "asset-manifest.json"
if not backend_manifest.is_file() or digest(CONTRACT_SOURCE / "asset-manifest.json") != digest(backend_manifest):
    errors.append(f"{backend_manifest} does not match {CONTRACT_SOURCE / 'asset-manifest.json'}")
for destination in DESTINATIONS:
    candidate = destination / "reference-manifest.json"
    if not candidate.is_file() or digest(CONTRACT_SOURCE / "reference-manifest.json") != digest(candidate):
        errors.append(f"{candidate} does not match {CONTRACT_SOURCE / 'reference-manifest.json'}")
embedded = (ROOT / "device" / "include" / "generated_settings_asset.h").read_text()
browser_loader = (ROOT / "frontend" / "src" / "simulator" / "FirmwareLvglCanvas.tsx").read_text()
fallback_assets = {
    "display-icon-background": "kDisplayBackgroundIconData",
    "display-icon-appearance": "kDisplayAppearanceIconData",
    "display-icon-clock": "kDisplayClockIconData",
    "display-icon-units": "kDisplayUnitsIconData",
    "display-icon-language": "kDisplayLanguageIconData",
    "display-icon-weather-location": "kDisplayWeatherLocationIconData",
    "display-icon-display": "kDisplayIconData",
    "background-preview-flower": "kBackgroundPreviewFlowerData",
    "background-preview-chevron": "kBackgroundPreviewChevronData",
    "background-preview-mechanical": "kBackgroundPreviewMechanicalData",
    "background-preview-black-white-circle": "kBackgroundPreviewBlackWhiteCircleData",
    "clock-background-preview-flower": "kClockBackgroundPreviewFlowerData",
    "clock-background-preview-chevron": "kClockBackgroundPreviewChevronData",
    "clock-background-preview-mechanical": "kClockBackgroundPreviewMechanicalData",
    "clock-face-preview-0": "kClockFacePreview0Data",
    "clock-face-preview-1": "kClockFacePreview1Data",
    "clock-face-preview-2": "kClockFacePreview2Data",
}
for logical_id, symbol in fallback_assets.items():
    asset = asset_manifest[logical_id]
    block = re.search(rf"const uint8_t {symbol}\[\] PROGMEM = \{{(.*?)\n\}};", embedded, re.S)
    if not block:
        errors.append(f"firmware embedded fallback missing {symbol}")
        continue
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", block.group(1)))
    filename = asset["file"]
    if len(data) != asset["bytes"] or hashlib.sha256(data).hexdigest() != asset["sha256"]:
        errors.append(f"firmware fallback bytes/hash mismatch for {filename}")
    native_match = re.search(rf'\{{"{re.escape(logical_id)}", "{re.escape(filename)}", (\d+), (\d+),', native_slots)
    if (asset.get("format") != "rgb565a8" or not native_match or
            (int(native_match.group(1)), int(native_match.group(2))) != (asset["width"], asset["height"])):
        errors.append(f"native slot format/dimensions mismatch for {filename}")
    if not native_match or filename.removesuffix(".bin") not in browser_loader:
        errors.append(f"native/browser consumer missing {filename}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"Shared asset outputs valid: {len(assets)} assets; Display/Clock fallbacks verified: {len(fallback_assets)}")
