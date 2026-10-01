"""Validate the committed Leopold UI contract without third-party packages."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHARED = ROOT / "shared"
ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
COLOR_RE = re.compile(r"^#[0-9A-Fa-f]{6}$")
KNOWN_TYPES = {"text", "indicator", "button", "progress", "slider", "icon"}


def fail(message: str) -> None:
    raise ValueError(message)


def integer(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def validate() -> None:
    document = json.loads((SHARED / "firmware-ui.json").read_text(encoding="utf-8"))
    if document.get("schema_version") != 1:
        fail("firmware-ui.json must use schema_version 1")
    screens = document.get("screens")
    if not isinstance(screens, list) or not 1 <= len(screens) <= 12:
        fail("firmware-ui.json screens must contain 1..12 screens")
    screen_ids: set[str] = set()
    component_ids: set[tuple[str, str]] = set()
    for screen in screens:
        screen_id = screen.get("id")
        if not isinstance(screen_id, str) or not ID_RE.fullmatch(screen_id):
            fail(f"invalid screen id: {screen_id!r}")
        if screen_id in screen_ids:
            fail(f"duplicate screen id: {screen_id}")
        screen_ids.add(screen_id)
        if not COLOR_RE.fullmatch(screen.get("background", "")):
            fail(f"invalid background for {screen_id}")
        components = screen.get("components")
        if not isinstance(components, list) or len(components) > 12:
            fail(f"invalid component list for {screen_id}")
        for component in components:
            component_id = component.get("id")
            if not isinstance(component_id, str) or not ID_RE.fullmatch(component_id):
                fail(f"invalid component id in {screen_id}: {component_id!r}")
            key = (screen_id, component_id)
            if key in component_ids:
                fail(f"duplicate component id: {screen_id}/{component_id}")
            component_ids.add(key)
            if component.get("type") not in KNOWN_TYPES:
                fail(f"unsupported component type: {component.get('type')}")
            for coordinate in ("x", "y"):
                value = component.get(coordinate)
                if not integer(value) or not 0 <= value <= 465:
                    fail(f"invalid {coordinate} for {screen_id}/{component_id}")
            if component.get("type") in {"button", "slider", "progress"}:
                for dimension in ("width", "height"):
                    value = component.get(dimension)
                    if not integer(value) or not 1 <= value <= 466:
                        fail(f"invalid {dimension} for {screen_id}/{component_id}")
            if "color" in component and not COLOR_RE.fullmatch(component["color"]):
                fail(f"invalid color for {screen_id}/{component_id}")
            if component.get("type") in {"slider", "progress"} and not 0 <= component.get("value", 0) <= 100:
                fail(f"invalid value for {screen_id}/{component_id}")
    manifest = json.loads((SHARED / "asset-manifest.json").read_text(encoding="utf-8"))
    for name, asset in manifest["assets"].items():
        path = SHARED / "assets" / asset["file"]
        if not path.is_file():
            fail(f"missing asset for {name}: {path}")
        expected = asset["width"] * asset["height"] * 3
        if path.stat().st_size != expected:
            fail(f"asset {name} has {path.stat().st_size} bytes; expected {expected}")
    rendering = json.loads((SHARED / "rendering.json").read_text(encoding="utf-8"))
    if rendering.get("schema_version") != 1:
        fail("rendering.json must use schema_version 1")
    canvas = rendering.get("canvas", {})
    if canvas != {"width": 466, "height": 466}:
        fail("rendering.json canvas must be 466x466")
    if not COLOR_RE.fullmatch(rendering.get("status_bar", {}).get("text_color", "")):
        fail("rendering.json status text color is invalid")
    visible_screens = rendering.get("status_bar", {}).get("visible_screens")
    if not isinstance(visible_screens, list) or not visible_screens or not all(item in screen_ids for item in visible_screens):
        fail("rendering.json status visible_screens must reference known screens")
    usb = rendering.get("home", {}).get("usb_icon", {})
    if not isinstance(usb.get("segments"), list) or not usb["segments"]:
        fail("rendering.json USB icon must contain segments")
    spotify = rendering.get("spotify", {})
    for key in ("arc", "cover", "controls", "seek"):
        if key not in spotify:
            fail(f"rendering.json Spotify definition is missing {key}")
    print(f"Shared UI valid: {len(screens)} screens, {len(component_ids)} components, {len(manifest['assets'])} assets")


if __name__ == "__main__":
    try:
        validate()
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"Shared UI validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
