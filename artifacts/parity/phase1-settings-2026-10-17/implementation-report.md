# Leopold Phase 1 Settings implementation report

Date: 2026-10-17

Repository: `goodmorning-mx/leopold`

Branch: `fix/live-device-time-controls`

Starting head: `3dced8d154b475001b5b55e19ceb98fde165d189`

## Scope

Implemented only the approved Settings slice: neutral composition, four pressed-row patches, live status composition, live brightness, and existing row navigation. Display submenu internals, Wi-Fi credential redesign, BLE application features, cloud caching, Phase 2, GoodMorning infrastructure, PlatformIO, serial access, physical flashing, and release operations remain deferred.

The path named as the authoritative ZIP in the supplied request was unavailable. The supplied Desktop ZIP was used as the source pack and copied unchanged into the repository. Its SHA-256 is `970c6bf98dda342fb77a63ca17b508b1678838715551020b11a847b44f7a62f1`.

## Reference registration

Stable source files and the unchanged ZIP are under `shared/references/phase1-settings/`. All seven supplied PNG checksums match `reference-pack.json`; the source neutral checksum is `391c361fd5dc73548e9bc8912d5e29e72382ac26688ad3b75eb2350daa19c7cd`.

Generated neutral asset:

- `shared/assets/settings-neutral-466.png` — source-identical PNG, SHA-256 `391c361fd5dc73548e9bc8912d5e29e72382ac26688ad3b75eb2350daa19c7cd`.
- `shared/assets/settings-neutral-466.bin` — RGB565A8, SHA-256 `5516678f7e55f1fa5dfbcc3ecf0b03c6f904e129ba497e2637c8c090201ea939`.

Pressed patch bounds are exact and lossless:

- Wi-Fi `[72,127,322,56]`; PNG `3d2728f78608da7805ee171d8e1038688189fd46b88a42b11d20dc69a1a9364b`; RGB565A8 `1c86ab72c44d4cbd70a2105226f3bd19f13e5f8dacba93d55a115dad0f863467`.
- Bluetooth `[72,181,322,56]`; PNG `512dc4ccc6ab17fd14a043665fabc061849fa1821b635c842d9503cb5d64fa57`; RGB565A8 `4cb4c72731b7d73c6783c6a1d6322afa84dd15794b4539e33b1b623d8525338c`.
- Display `[72,235,322,56]`; PNG `4d0300a663f937c271824f1d3a9e2c67d679e49f78e7e175df62576698d06f81`; RGB565A8 `04376aa1e78c680b4f6c5d3b28d41d148076bd6cd988f7b38eb428dd99c44053`.
- Sound & Vibration `[72,289,322,56]`; PNG `e307fb3e324ef8e1c89189dc3ad9db282890e68724ff044d05c7d2f5ce6f0158`; RGB565A8 `895b28cdc4f519a6b70591cb5cd94c02ca01f87fe5b299e862cd0ed03c5ff70c`.

The old permanently blue Wi-Fi base remains preserved as inactive legacy provenance and is not selected for idle rendering.

## Runtime ownership and interaction

The neutral base is the immutable framebuffer authority. Simulator and device use the same patch bounds and synchronized RGB565A8 derivatives. Press-down selects the exact patch; dragging outside or cancelling restores neutral; release inside invokes the existing destination. Blue is present only in the selected pressed patch.

The status shell is `[137,39,195,38]`. Independent masks are Bluetooth `[153,45,22,22]`, Wi-Fi `[181,45,22,22]`, live clock `[216,45,49,26]`, cloud `[264,45,24,24]`, and battery `[294,45,31,24]`. The left status icon is Bluetooth ownership; simulator and ESP32 map to the same connected visual treatment.

The complete live brightness mask is `[63,336,342,88]`, covering the baked control and endpoint glow. Shared geometry is x `98`, width `272`, y `358`, height `28`; values map to x `98` at 5%, x `234` at 50%, and x `370` at 100%. Values clamp only at 5 and 100, update continuously, persist through the existing state, and apply through existing device brightness behavior.

## Captures and reports

Fresh raw neutral capture: `artifacts/parity/phase1-settings-2026-10-17/settings-neutral-capture.png`.

Numeric report and difference image: `artifacts/parity/phase1-settings-2026-10-17/neutral-report/settings-pixel-report.json` and `settings-difference.png`. The neutral comparison passes with zero mean error, zero maximum color delta, and zero pixels over delta after declared dynamic masks.

All ten deterministic simulator captures are now present under `artifacts/parity/phase1-settings-2026-10-17/captures/`. The machine-readable acceptance report and ten lossless difference images are under `artifacts/parity/phase1-settings-2026-10-17/acceptance/`; all ten states pass their declared outside-mask and selected-region checks.

## Validation

- Shared UI validation: passed.
- Synchronized asset-output validation: passed (`26 assets`).
- Arduino CLI firmware configuration validation: passed.
- Geometry assertions: passed for 5%, 50%, and 100%.
- Firmware package/flash safety unit tests: passed (`12 tests` via unittest); no device was contacted.
- Frontend typecheck and production build: passed. Vite reported the existing host Node 20.11.0 compatibility warning.
- `pytest` was unavailable in the system Python; equivalent geometry and firmware package tests were run with the repository's unittest suite.
- Git diff whitespace check: passed.
- GitHub Actions and replacement artifact verification are pending the authorized push.

Nothing was merged, deployed, released, tagged, flashed, or connected to a serial device.
