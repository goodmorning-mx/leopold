# Leopold Phase 0 verification — 2026-10-15

This is a diagnostic Phase 0 record for the preserved checkout at
`fix/live-device-time-controls`, `bd52ee3`. No commit, merge, push, deploy, or
flash is part of this record.

## Reference identity

All three current source references are lossless PNGs and are retained under
`shared/references/`. The 466 × 466 generated PNG is a deterministic Pillow
12.3.0 RGBA uniform-contain derivative, centered with black letterboxing and no
crop. The RGB565A8 `.bin` is generated from that PNG and is not an independent
artwork source.

| State | Source | Source dimensions | Source SHA-256 | Canonical PNG | PNG SHA-256 | Device RGB565A8 SHA-256 |
| --- | --- | ---: | --- | --- | --- | --- |
| Home | `shared/references/home-source.png` | 1310 × 1200 RGBA | `5a1d2f08b32d38eb22ad13ef683ddec753c92a65ecbb15fc32a890cad5c58abe` | `shared/assets/home-reference-466.png` | `d5d8e07fb4a4e56f4880a1bc4eb952ceb4989081c984b4e3a0da4689e6921902` | `8a892908dc0b639959833365c266eb63555cf7119b973a96cf3ad8b4eb4e4bca` |
| Settings | `shared/references/settings-source.png` | 1310 × 1200 RGBA | `dca1b4ca563f70626733b4c64ce0ca1442f80bd9ca3e7ccd4d3afbdb65ed02ac` | `shared/assets/settings-reference-466.png` | `c4308a17a0f07da146eecf73da0de20271e60f376c7115c0fd2eb1a24b8ac10e` | `695070c45e875d00d821f84b55e7eff0e793323c300f9bf0935d52c71e6446ca` |
| Display | `shared/references/display-source.png` | 1254 × 1254 RGB | `e3bd058ddc0c3470bb0eb64111365de8613bcf3eda5fe536208179d76f528b4c` | `shared/assets/display-reference-466.png` | `1616bd95bf6cc2fcf37cdbf48f2993e2d09ec1e325b1351fec2b5499fdbf5316` | `2f74bb55b50b1380f1c95109f56ea9062fbf1185902706d0435dc7af0218200a` |

The same canonical PNGs and RGB565A8 files are byte-identical in `backend/assets`,
`frontend/public/firmware-assets`, and `device/assets`; the synchronized
reference manifests agree. The Home status-banner source is also preserved:
660 × 206 RGBA, SHA-256
`879555f5ee1d81444dff31fead0ca25ad327ae1b61bd2d5ecd67db21e84c7bb2`.

## State and region ownership

The current reference set is exact-reference asset mode. “Missing” means the
state is required by the product contract but has no approved transparent asset
or live renderer that is authoritative yet; it must not be approximated.

| State | Immutable base owner | Exact transparent state asset owner | Live overlay owner | Interaction-only owner | Current finding |
| --- | --- | --- | --- | --- | --- |
| Idle Settings | `settings-reference-466.png`: background, title, row shells, labels, chevrons, slider shell | None registered | Clock mask only; status/Wi-Fi/cloud/connection and brightness masks are required but not implemented as live overlays | Shared row hit targets and slider hit target | Static capture passes; baked blue Wi-Fi row remains a known defect |
| Settings Wi-Fi row pressed | Same base outside the pressed row | Missing approved pressed-row/icon/bubble asset | None | `settings-wifi` hit target | Press may be hidden by the bitmap |
| Settings Bluetooth row pressed | Same base outside the pressed row | Missing approved pressed-row/icon/bubble asset | None | `settings-bluetooth` hit target | Not proven |
| Settings Display row pressed | Same base outside the pressed row | Missing approved pressed-row/icon/bubble asset | None | `settings-display` hit target | Navigation exists, pressed state not proven |
| Settings Sound & Vibration row pressed | Same base outside the pressed row | Missing approved pressed-row/icon/bubble asset | None | `settings-sound` hit target | Not proven |
| Wi-Fi connected | Settings base | Missing connected-state icon/status asset | Required Wi-Fi status mask; no authoritative live renderer | Wi-Fi row hit target | Current bitmap is permanently connected/blue-looking |
| Wi-Fi disconnected | Settings base | Missing disconnected-state icon/status asset | Required Wi-Fi status mask | Wi-Fi row hit target | No reference artwork supplied; do not invent |
| Cloud connected | Settings base | Missing cloud connected-state asset | Required cloud status mask | None | Simulator currently reports cloud unavailable |
| Cloud disconnected | Settings base | Missing cloud disconnected-state asset | Required cloud status mask | None | No exact state asset registered |
| Simulator/ESP32 connected | Settings base | Missing connection badge/status asset | Required device-connection mask | None | No authoritative visual mapping exists |
| Simulator/ESP32 disconnected | Settings base | Missing connection badge/status asset | Required device-connection mask | None | No authoritative visual mapping exists |
| Live time | Base everywhere outside the clock rectangle | None | `live-clock` mask: Settings `[218,72,49,26]`; Display `[211,39,57,29]` | None | Simulator overlay exists; device parity is unverified |
| Brightness 5% | Base outside slider region | Missing exact 5% knob/label asset family | Required slider fill/knob/percentage mask `[123,336,220,61]` | Shared slider hit target | State is in logic but the Settings bitmap still contains a ~50% knob |
| Brightness 50% | Base outside slider region | Missing exact 50% knob/label asset family | Required slider fill/knob/percentage mask `[123,336,220,61]` | Shared slider hit target | Reference is visually around 50%; not proof of live control |
| Brightness 100% | Base outside slider region | Missing exact 100% knob/label asset family | Required slider fill/knob/percentage mask `[123,336,220,61]` | Shared slider hit target | Not visually proven |

The masks are intentionally component-sized, not full-screen masks: clock,
status badges, and the brightness control must be covered independently. Until
the missing live owners are implemented, the current comparison profile only
proves the immutable static composition.

## Calibration

The temporary capture-only calibration state is available at
`/simulator?calibration=1&capture=1`. It renders a 466 × 466 black framebuffer
with a one-pixel outer border, 10-pixel corner markers, 50-pixel grid lines,
center crosshair, and concentric circles at radii 50, 100, 150, and 200. Fresh
capture: `docs/parity/evidence/2026-10-15/calibration-framebuffer.png`.

The simulator framebuffer export is lossless PNG (`data:image/png;base64,` with
the PNG signature `iVBORw0K`), and the decoded capture is 466 × 466 RGBA. A
physical calibration capture and straight-on AMOLED photo are still blocked by
the missing authorized device session.

## Fresh simulator evidence

The capture route was served by the local Vite process from this checkout. The
normal presentation remained circular; only the canvas artifact was exported.

| State | Fresh capture | Numeric report | Difference image | Result |
| --- | --- | --- | --- | --- |
| Settings | `docs/parity/evidence/2026-10-15/settings-framebuffer.png` | `docs/parity/evidence/2026-10-15/settings-report-v2/settings-pixel-report.json` | `docs/parity/evidence/2026-10-15/settings-report-v2/settings-difference.png` | Static reference pass: 201,034 compared pixels; mean error 0; max delta 0; 0 over delta |
| Display | `docs/parity/evidence/2026-10-15/display-framebuffer.png` | `docs/parity/evidence/2026-10-15/display-report-v2/display-pixel-report.json` | `docs/parity/evidence/2026-10-15/display-report-v2/display-difference.png` | Static reference pass: 214,027 compared pixels; mean error 0; max delta 0; 0 over delta |

The reports predate no current changes: they were generated after the current
checkout was loaded and after the capture-only export was added. They do not
claim interactive parity. The Display comparison can be resolved in the
simulator because the canonical PNG is available and capture is lossless; the
compressed-display concern cannot be resolved in the simulator and still
requires the physical AMOLED calibration capture/photo.

## Validation and blockers

Passed: exact-reference generator check, shared UI validation (13 screens, 71
components, 10 assets), synchronized-output verification (16 assets), frontend
typecheck, and frontend production build. The build emitted a Node compatibility
warning under the system Node 20.11; the simulator itself was run with the
bundled Node 24 runtime.

Firmware build was not run. The documented local PlatformIO venv is absent, and
the requested isolated PlatformIO bootstrap was stopped before compilation when
the user clarified that PlatformIO was not needed for this audit. This is an
environment/tooling blocker, not a firmware pass or failure result.

No physical-device parity evidence exists in this run. No flash, asset upload,
deployment, merge, push, or commit was performed.

## Phase 1 audit

Usable now: the canonical Display base is synchronized to browser/backend/device
asset paths; Display navigation exists; Background toggling exists in the
simulator and device path; static Display capture passes.

Incomplete: Settings still shows the baked blue Wi-Fi row; visible row-press
feedback may be hidden by the bitmap; Display only gives Background behavior;
Language, Clock, Units, Back, and live brightness behavior are not complete;
Display has no live time overlay; prior pixel reports are stale relative to the
fresh captures above; and no physical-device parity evidence exists.

Revision required before Phase 1 approval: register authoritative transparent
state artwork and live masks/owners, implement shared navigation and press
states, add the Display live clock and control overlays, and obtain physical
calibration evidence. Do not start Phase 2, Wi-Fi credential redesign, BLE,
cloud caching, or GoodMorning infrastructure work.

## October 18 physical evidence update — 2026-10-18

This dated addendum preserves the Phase 0 and Phase 1 evidence captured on
October 18. It does not rewrite the earlier audit and does not promote missing
captures to passes.

- **Phase 0 calibration: passed with circular-panel-mask note.** The raw
  466 × 466 framebuffer matched the shared border, corner markers, grid,
  crosshair, and concentric circles at `(233,233)` with radii 50/100/150/200.
  The straight-on photo review confirmed the lit circular panel; its circular
  mask hides the square framebuffer corners, as expected. Evidence and numeric
  comparison remain under
  `artifacts/device-captures/phase1-calibration-2026-10-18/`.
- **Post-flash 50% brightness capture: passed.** The accepted capture is
  `artifacts/device-captures/phase1-settings-physical-2026-10-18/settings-brightness-50-fixed.png`,
  with its report and difference image in the matching
  `brightness-50-fixed-comparison/` directory.
- **Final 50% recapture: failed.** The final capture measured the brightness
  knob at 237.5 px rather than the 234 px target. It remains recorded as a
  failure in `settings-brightness-50-final.png` and its
  `brightness-50-final-comparison/` report.
- **5% and 100% brightness captures: missing.** The user performed the states,
  but no fresh framebuffer was captured during those pauses; they remain
  missing, not passed.
- **Settings rows: failed in the latest physical run.** Wi-Fi, Bluetooth,
  Display, and Sound & Vibration all failed to show pressed feedback or
  navigate. This is physical evidence from the current device session; the
  simulator's held-state checks and any historical physical captures do not
  override it.

The next software revision must diagnose touch-controller reads, raw
coordinates, gesture classification, shared hit testing, pressed rendering,
asset selection, and release navigation. Physical row behavior remains
unverified until that revision is flashed and captured in a later authorized
device session.

## October 18 touch-path diagnosis — 2026-10-18

The software audit found the first actionable loss after raw touch reads: the
device's Settings press and release path was not using the shared row bounds.
Held feedback depended on a periodic drag sample whose timing was not reset at
the start of a new contact, and release handling used ad hoc coordinate bands
that did not represent the four shared rows. Wi-Fi and Bluetooth had panel
actions; Display and Sound & Vibration had no destination. The focused fix now
generates device hit bounds from `shared/interaction.json`, resets the first
contact's held-state sample timing, and makes the two undefined destinations
explicit in both simulator and device behavior. This is a software diagnosis,
not physical proof; the device must still be flashed and recaptured later.
