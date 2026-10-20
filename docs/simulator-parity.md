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
python3 scripts/check_settings_overlay_generation.py
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

The Settings profile is `shared/settings-acceptance-profile.json`, including
explicit seam bounds and allowances. Dynamic masks include live status and
brightness regions; they are not restricted to the clock. A normal simulator run must preserve the circular panel
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
7. For Settings, use `scripts/compare_settings_component_acceptance.py` and the
   approved component profile, plus full-screen visual inspection. Preserve the
   legacy full-row comparator as diagnostic; never loosen limits to hide errors.
   The test fixtures contain a hash-recorded good simulator composition and a
   real historical bad Wi-Fi seam. These are regression inputs, not new approved
   design references or physical evidence.

Settings acceptance must use the same native LVGL renderer compiled to WASM
and ESP32. A separate TypeScript/Canvas implementation is not sufficient.
Platform-specific adapters remain limited to runtime state and hardware/browser
integration. Compare themes, DPI, defaults, clipping, and pixel conversion too.

Settings status RGBA inputs live in `shared/settings-overlay-sources/` with
checksums in `shared/settings-overlay-sources.json`. They preserve the accepted
bounded component alpha and cover pixels byte for byte. The generator reads
these independent inputs, never its own generated outputs. The isolated
generation check runs twice and fails if either run differs from committed
assets; CI and release builds enforce it before synchronization/packaging.

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
