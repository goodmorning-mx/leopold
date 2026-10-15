# Simulator parity workflow

The browser simulator is a pre-firmware validation tool. Its screen definitions
come from `shared/firmware-ui.json`; its rendering geometry and visibility rules
come from `shared/rendering.json`; its interaction constants, persistence
defaults, typography mapping, and asset manifest come from the other files in
`shared/`.

## Exact-reference asset mode

Home and Settings use the exact-reference asset mode declared in
`shared/reference-manifest.json`. The immutable originals live in
`shared/references/`; `scripts/generate_exact_reference_asset.py` creates the
canonical 466×466 lossless PNGs with uniform contain scaling and no crop, plus
RGB565A8 device derivatives. The browser renders the canonical PNGs directly.
Hit targets, navigation, the brightness slider, persistence, and the live clock
remain active above the bitmap layer.

Regenerate and verify the assets with the pinned tooling dependency:

```bash
python3 -m pip install -r scripts/requirements.txt
python3 scripts/generate_exact_reference_asset.py
python3 scripts/generate_exact_reference_asset.py --check
python3 scripts/sync_shared_assets.py
python3 scripts/verify_shared_outputs.py
```

For pixel acceptance, load the simulator with `?reference=home&capture=1` or
`?reference=settings&capture=1`, save the raw `data-framebuffer-png` value from
the canvas, and run:

```bash
python3 scripts/compare_reference_capture.py home <home-capture.png> <report-directory>
python3 scripts/compare_reference_capture.py settings <settings-capture.png> <report-directory>
```

The committed profile permits no mismatched pixels outside each state's narrow
live-clock mask. A normal simulator run must still preserve the circular panel
mask; capture mode exposes the square framebuffer only for measurement.

## Change workflow

1. Edit the committed files in `shared/`.
2. Run `python3 scripts/validate_shared_ui.py`.
3. Run `python3 scripts/sync_shared_assets.py` and
   `python3 scripts/verify_shared_outputs.py`.
4. Run `cd frontend && npm run typecheck && npm run build`.
5. Treat the successful GitHub Actions firmware artifact as the only supported
   firmware input. Use `scripts/flash_firmware.py` only after explicit hardware
   authorization; local firmware compilation is not part of this workflow.
6. Open `/simulator` and exercise every state in
   `shared/reference-states.json`.
7. For exact-reference states, run the raw framebuffer comparator and require
   every whole-frame and region result to pass. For reconstructed states,
   capture a 466×466 reference image when visual changes are intentional.

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
