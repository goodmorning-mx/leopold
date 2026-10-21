# Shared Status Banner SOP

This is the project-level visual and behavior contract for the `$leopold-status-banner` skill.

## Gold-standard appearance and coverage

- The status pill in the Settings reference is the gold standard. Repeat the same complete shell on every screen except Home and Clock.
- On the 466 × 466 framebuffer, the shared shell is the `settings-status-banner` asset at x=143, y=43, width=181, height=35, with a 17 px rounded mask. Preserve the entire lower rim and rounded corners. The earlier 181 × 28 crop is incomplete and must not be reintroduced.
- Keep the shell's fill, outline, translucency, radius, and relative indicator placement identical across pages. Do not stretch, clip, recolor, rebuild per-page variants, or add an opaque panel beneath it.
- Home retains its separate centered status pill. Clock retains its separate screen design. Do not draw this top shared pill on either screen.

## Live content

The shared shell remains neutral. Draw live time, battery, Bluetooth, Wi-Fi, and cloud information in their declared dynamic regions. Connection changes update only the corresponding status element; they must not replace the entire pill or change its geometry. Keep the normal battery and time values readable within the existing shell.

## Source and delivery

- `shared/rendering.json` defines shared placement, size, and live-content positions.
- `shared/display-menu-asset-manifest.json` and `scripts/prepare_display_menu_assets.py` identify and generate the canonical 181 × 35 source asset and runtime derivative.
- `shared/asset-manifest.json` declares the runtime asset. Synchronize through `scripts/sync_shared_assets.py` and verify copies with `scripts/verify_shared_outputs.py`.
- The shared LVGL renderer is the common visual implementation for browser WASM and firmware. Keep adapters limited to platform state and input; never add a simulator-only banner fix.

## Review checklist

1. Verify all eligible pages use the same full 181 × 35 shell and Home/Clock are excluded.
2. Inspect the full lower edge, corners, and neighboring content at native 466 × 466 framebuffer size; confirm no parent clip or stale 181 × 28 asset is active.
3. Check time, battery, and neutral/connected Bluetooth, Wi-Fi, and cloud states. Confirm state changes stay within the declared indicator bounds.
4. Run the shared UI validator, output synchronization/verification, settings renderer parity check, and a fresh simulator visual check as relevant to the changed inputs.
5. Report simulator and physical-device evidence separately. Simulator review does not establish physical panel parity.
