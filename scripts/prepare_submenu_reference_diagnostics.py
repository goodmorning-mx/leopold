"""Create non-authoritative 466px comparison derivatives for supplied submenu refs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "shared" / "references"
OUTPUT = REFERENCE / "display-submenus" / "adapted-466-diagnostic"
DISPLAY_STATES = {
    "Display_Default.png": "reference=display",
    "Display_Background_Expanded.png": "reference=display-background",
    "Display_Appearance_Expanded.png": "reference=display-appearance",
    "Display_Weather_Location_Expanded.png": "reference=display-location",
    "Display_Clock_Submenu.png": "reference=clock-settings",
    "Display_Units_Expanded.png": "reference=display-units",
    "Display_Language_Expanded.png": "reference=display-language",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_derivative(source: Path, destination: Path) -> dict[str, object]:
    with Image.open(source) as image:
        image.load()
        source_size = list(image.size)
        rgba = image.convert("RGBA")
        derivative = rgba.resize((466, 466), Image.Resampling.LANCZOS)
        derivative.save(destination, format="PNG", optimize=False)
    return {
        "source": str(source.relative_to(ROOT)),
        "source_sha256": sha256(source),
        "source_dimensions": source_size,
        "derivative": str(destination.relative_to(ROOT)),
        "derivative_sha256": sha256(destination),
        "derivative_dimensions": [466, 466],
        "transform": "full-canvas Pillow Lanczos resize to 466x466; no crop; RGBA",
    }


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    states = []
    for filename, route in DISPLAY_STATES.items():
        source = REFERENCE / "display-submenus" / "Sub Menu" / filename
        derivative = OUTPUT / filename.replace(".png", "-466-diagnostic.png")
        record = make_derivative(source, derivative)
        record.update({"name": filename.removesuffix(".png"), "route": route, "acceptance": "diagnostic-only; no approved comparison profile"})
        states.append(record)
    wifi_source = REFERENCE / "Leopold_WiFi_Reference.png"
    wifi_derivative = OUTPUT / "Leopold_WiFi_Reference-466-diagnostic.png"
    wifi = make_derivative(wifi_source, wifi_derivative)
    wifi.update({"name": "WiFi-overview", "route": "screen=wifi", "acceptance": "diagnostic-only; visual reference contains illustrative SSID/state art"})
    manifest = {
        "schema_version": 1,
        "purpose": "Source identity and diagnostic adaptation map for submenu implementation review",
        "comparison_profile": None,
        "profile_status": "unapproved; no match or device-readiness claim is permitted",
        "states": states,
        "wifi_reference": wifi,
        "additional_simulator_states": [
            "reference=display-keypad&country=US",
            "reference=display-keypad&country=MX",
            "reference=display-invalid&country=US",
            "reference=display-not-found&country=MX",
            "reference=display-offline&country=US",
            "reference=display-pending&country=MX",
            "reference=display&scroll=0",
            "reference=display-background&scroll=232&art=0|1|2",
            "reference=display&language=en|es&units=imperial|metric",
            "reference=clock-settings&format=12|24&face=0|1|2&clock-background=0|1|2&clock-weather=no|yes",
            "screen=weather|usb|navigation|spotify&art=0|1|2&appearance=dark|light",
            "screen=spotify&reference=spotify-playing&art=0|1|2",
            "screen=wifi",
        ],
    }
    (ROOT / "shared" / "submenu-reference-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
