"""Embed the shared wallpapers and essential Home artwork for offline rendering."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

KEYS = (
    "background-flower", "background-chevron", "background-mechanical-flower",
    "background-black-white-circle",
    "open-clock", "open-weather", "open-navigation", "open-whatsapp",
    "open-spotify", "spotify-large", "home-settings-gear",
    "status-cloud-icon", "status-bluetooth-icon", "status-wifi-icon", "home-usb-icon",
    "home-glass-rim",
)


def generate(root: Path) -> list[dict[str, object]]:
    assets = json.loads((root / "shared/asset-manifest.json").read_text())["assets"]
    lines = ["#pragma once", "#include <Arduino.h>", "namespace leopold::sky {",
             "struct EmbeddedHomeAsset { const char* file; const uint8_t* data; size_t bytes; }; "]
    records = []
    for index, key in enumerate(KEYS):
        asset = assets[key]
        payload = (root / "shared/assets" / asset["file"]).read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if (asset["format"] != "rgb565a8" or
                len(payload) != asset["width"] * asset["height"] * 3 or
                (asset.get("device_sha256") is not None and digest != asset["device_sha256"])):
            raise ValueError(f"invalid canonical Home asset: {key}")
        lines.append(f"const uint8_t kHomeRuntimeAsset{index}[] PROGMEM = {{")
        for offset in range(0, len(payload), 32):
            lines.append("  " + ",".join(f"0x{v:02X}" for v in payload[offset:offset + 32]) + ",")
        lines.append("};")
        records.append({"key": key, "file": asset["file"], "width": asset["width"],
                        "height": asset["height"], "bytes": len(payload), "sha256": digest})
    lines.append("const EmbeddedHomeAsset kEmbeddedHomeAssets[] = {")
    for index, asset in enumerate(records):
        lines.append(f'  {{"{asset["file"]}", kHomeRuntimeAsset{index}, sizeof(kHomeRuntimeAsset{index})}},')
    lines.extend(["};", "}  // namespace leopold::sky", ""])
    target = root / "device/include/generated_home_runtime_assets.h"
    content = "\n".join(lines)
    if not target.exists() or target.read_text() != content:
        target.write_text(content)
    return records


if __name__ == "__main__":
    generate(Path(__file__).resolve().parents[1])
