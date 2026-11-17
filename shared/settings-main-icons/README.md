# Leopold — main Settings screen icon handoff

This self-contained package contains ONLY Gear, Wi-Fi, Bluetooth, Remote and Speaker, in 40×40 and 60×60 pixel variants. It is the first implementation handoff for these prepared assets. The larger icon library is not needed.

The user expects further glyph changes throughout Leopold later. This task is limited to the MAIN Settings screen. No Home changes, status-banner changes, submenu changes, new menu items or broad glyph/font replacements are authorized by this handoff. Shared renderer support may be added, but use the new assets only at explicit main-Settings call sites.

## Mapping

| Requested glyph | Package ID | Runtime ID | Existing main-Settings role |
|---|---|---|---|
| Gear | gear | settings-main/gear | Existing Settings title/header gear or existing gear placement |
| Wi-Fi | wifi | settings-main/wifi | Existing Wi-Fi row/control |
| Bluetooth | bluetooth | settings-main/bluetooth | Existing Bluetooth row/control |
| Remote | remote | settings-main/remote | Existing Remote row/control |
| Speaker | speaker | settings-main/speaker | Existing Sound/Audio row/control; preserve its current label |

The earlier source-library IDs `settings` and `sound` are renamed `gear` and `speaker` in this pack for clarity. These are asset names, not instructions to rename UI labels or create new rows. Map the existing screen deliberately; report any unmapped item without inventing a new menu item. Keep menu order, row and title geometry, labels, touch targets and destinations.

## Why these assets exist

The user supplied reflective liquid-glass glyph artwork and selected this visual direction for Leopold. Plain silver PNGs do not automatically inherit the system color while preserving their reflections. Flat full-opacity recoloring would erase the shading. These assets separate the colored glass body, pale reflections and transparent coverage so the selected system accent can color each glyph without losing depth.

These five glyphs come from the original group in the supplied artwork; they were not generatively redrawn in this handoff. Their source extractions were cleaned by removing very faint alpha values ≤8/255, trimmed, and converted to neutral material channels. Original baked colors become luminance shading. The extracted masters and hashes are included. Keep their silhouettes, especially Remote’s naturally narrow aspect ratio.

## Included files

- `40px/*.png` and `60px/*.png`: ten neutral transparent RGBA PNGs. These are reference/fallback views, not the complete tinting mechanism.
- `<size>/masks/*-body.png`, `*-highlight.png`, `*-alpha.png`: thirty L8 material-channel PNGs.
- `<size>/material/*.bha`: ten compact BHA8 inputs, no header; three interleaved unsigned bytes per pixel: body, highlight, alpha. Row-major. Use manifest dimensions and byte counts.
- `masters/*-source.png` and `*-neutral.png`: five cleaned source extractions and neutral masters.
- `integration/settings_icons.h`: portable allocation-free C++ material renderer. Namespace `leopold_settings_icons`.
- `integration/settings_icon_data.h`: immutable embedded material data for ONLY these ten variants. Include this generated header in one implementation unit to avoid duplicate static data.
- `examples/<color>/<size>/*.png`: fifty reference outputs, covering white, cyan, green, purple and orange. Colors are review examples, not a request to add theme choices.
- `manifest.json`: IDs, upstream provenance, exact dimensions, format contract and material hashes.
- `validation.json`: package-specific verification results. This does not claim device or simulator acceptance.
- `SHA256SUMS.txt`: file-integrity checksums.
- `settings-icon-preview.png`: glyphs pasted at native sizes, neutral on the left and cyan on the right within each pair.
- `tools/build_assets.py`, `tools/validate_assets.py`, `tools/check_renderer.cpp`, `requirements.txt`: reproducible source conversion and verification. Rebuild with Python 3 and Pillow 13.0.0. Validation additionally uses clang++ or g++ with C++11.
- `UI-COORDINATOR-PROMPT.md`: complete implementation prompt.

## Runtime coloring

For each RGB channel, use unsigned integer math:

`out = min(255, (body * selectedAccentChannel + 127) / 255 + highlight)`

Division is integer division; alpha is the separate coverage channel. Optional state opacity multiplies alpha with the same rounding as the supplied code. All math operates on sRGB-encoded byte values with straight alpha; do not apply a different color-space conversion on just one target.

For the five main-Settings row glyphs, white is the resting tint and the selected system appearance color is used only while that row is pressed. White accent reconstructs the neutral material; the press accent preserves the supplied body shading and pale highlights. Release and cancel return the glyph to white. Keep this treatment local to these row glyphs; the title gear and glyphs on Home, the banner, and submenus retain their existing mappings. A held row is input feedback, not radio connection state.

The master preparation computes luminance Y using Pillow RGB-to-L, highlight H = round(max(0,Y−160) × 0.65 × 255/95), and body B = Y−H. Coverage comes from source alpha. The source masters already contain the declared alpha cleanup and transparent trim; the included build script starts from those stored masters.

The three mask images are DATA inputs to that formula. They are not ordinary layers to source-over, and multiplying or fully recoloring the neutral PNG is not equivalent. Do not apply a second full-image LVGL recolor to the rendered result.

## Integration details

Use the shared firmware/WASM drawing path and the project’s existing live theme preference. Add explicit screen-local mappings such as `settings-main/wifi`; do not globally rebind a generic `wifi` or `settings` icon used elsewhere. Newly rendered buffers should be cached by glyph ID, size, accent RGB and any pixel-affecting state. Refresh visible main-Settings glyphs when the accent changes and when the page reopens. Keep cached image descriptors and data alive while LVGL references them.

`renderRgba` emits straight RGBA8888. `renderRgb565A8` emits a little-endian RGB565 color plane followed by an A8 plane, not interleaved 565/alpha pixels. Match the project’s installed LVGL version, image descriptor conventions, actual color format and byte order. Any flush-time byte swapping is separate. Reuse a compatible existing shared helper if available without changing callers outside this screen.

All ten immutable BHA materials total 78,000 bytes before compression and descriptor overhead. A 40px RGB565A8 output uses 4,800 bytes; a 60px output uses 10,800 bytes. Cache only required variants within the measured device budget. PNG files are editable/reference assets; production firmware may consume generated data without a runtime PNG decoder.

Both sizes are supplied for layout flexibility. Use the existing screen’s measured icon bounds: 40px is the default row candidate; use 60px only where an existing larger title/control placement supports it. Do not enlarge rows or touch targets simply to use 60px. Canvases have 1px clear padding at 40px and 2px at 60px. Center without stretching; do not widen Remote to match the other silhouettes.

## Validation and acceptance

Run `python tools/build_assets.py` to rebuild from the included masters and `python tools/validate_assets.py` to check the encoded channels and cross-language renderer. This package is independent of all previous full-library folders.

Before integrating, establish the actual owning checkout, current project state, applicable AGENTS.md and existing shared asset pipeline. Preserve current work. After integration, inspect the main Settings screen at its native framebuffer size using the project’s actual shared renderer. Check normal, held, released/cancelled and disabled treatments where supported; actual touch targets; theme updates while the page stays open; reopening the page; and preference persistence. Compare actual supported accent options, including dark/light extremes, against the supplied reference output behavior.

Retain normal navigation. Opening an existing submenu for an interaction check does not authorize restyling that submenu. Verify that Home, submenus and the shared status banner keep their existing glyphs. Reuse approved screen references and acceptance profiles; these previews do not replace them. Report source/build identity, affected call sites, visual/input checks and remaining device verification. Firmware or physical-device actions follow the project’s normal authorization rules; this handoff itself requests asset integration on the main Settings screen.
