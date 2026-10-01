# Simulator parity workflow

The browser simulator is a pre-firmware validation tool. Its definitions come
from `shared/firmware-ui.json`; its interaction constants, persistence defaults,
typography mapping, and asset manifest come from the other files in `shared/`.

## Change workflow

1. Edit the committed files in `shared/`.
2. Run `python3 scripts/validate_shared_ui.py`.
3. Run `python3 scripts/sync_shared_assets.py` and
   `python3 scripts/verify_shared_outputs.py`.
4. Run `cd frontend && npm run typecheck && npm run build`.
5. Run the firmware build with the repository-local PlatformIO environment:
   `device/.tools/platformio-venv/bin/pio run`.
6. Open `/simulator` and exercise every state in
   `shared/reference-states.json`.
7. Capture a 466×466 reference image for each state when visual changes are
   intentional. Keep references and their review notes in GitHub.

The browser may use a renderer-specific implementation for Canvas and the
firmware may use LVGL, but neither may maintain a second screen definition.
Renderer-specific code is limited to drawing primitives, dynamic state adapters,
and hardware/browser integrations.

## Pixel-accuracy definition

Pixel accuracy means identical viewport size, component bounds, colors, assets,
state values, interaction results, and screen transitions. Font rasterization,
LVGL anti-aliasing, display blending, and physical panel behavior require
framebuffer or photographed-device evidence and are tracked as explicit known
differences until captured.

## Rollback

Before the shared-source work, the repository was checkpointed at the annotated
tag `pre-shared-source-truth-20260930` and branch
`backup/pre-shared-source-truth-20260930`. Preserve these references until the
new workflow has passed physical validation. Firmware upload remains a separate
explicitly authorized action.
