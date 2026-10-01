"""Generate the firmware fallback-screen builder from shared/firmware-ui.json."""

from __future__ import annotations

import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sync_shared_assets import sync

from SCons.Script import Import

Import("env")
root = Path(env.subst("$PROJECT_DIR")).resolve().parent
sync()
source = root / "shared" / "firmware-ui.json"
target = root / "device" / "include" / "generated_firmware_ui.h"
document = json.loads(source.read_text(encoding="utf-8"))


def q(value: object) -> str:
    return json.dumps(str(value))


lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "#include \"sdui.h\"",
    "",
    "namespace leopold {",
    "namespace generated_ui {",
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
