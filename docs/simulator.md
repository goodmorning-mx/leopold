# Leopold browser simulator

The simulator is a local test surface for the physical 466 × 466 Leopold UI.
Its firmware identity is the exact source/build recorded in the current parity
handoff. A release label or UI revision alone does not distinguish builds.
Firmware binaries are produced only by GitHub Actions and downloaded as a
checksummed package; local build output is not an authoritative artifact. The
simulator is available at `/simulator` in the Vite app
and uses safe mock values by default. It does not call Device Cloud, open USB,
or use device credentials.

## Run and validate

```bash
cd frontend
npm ci
npm run dev
```

Open <http://localhost:5173/simulator>. The device surface accepts mouse and
touch pointer input:

- swipe left/right: move through the SDUI `screens` array;
- swipe up: open `sky-home`;
- hold `sky-clock` for at least 700 ms: toggle 12/24-hour time;
- tap buttons: emit a `tap` event and update the matching mock state;
- drag sliders: calculate values with the same orientation and 16 px touch
  padding used by the firmware.

The JSON editor accepts the Cloud's `schema_version: 1` document. Unknown
component types are ignored, matching `device/src/sdui.cpp`; known components
are validated against the same 466 × 466 coordinate limits. The event log is
local evidence of simulator behavior, not proof of a Cloud event or device
deployment.

## Firmware renderer mapping

`shared/firmware-ui.json` is the fallback shell definition consumed by the
browser debug editor and used by the Arduino CLI generator to produce
`device/include/generated_firmware_ui.h`. The generated file is ignored and
must not be edited by hand. Runtime-only values such as brightness, clock,
weather, and Cloud/USB state are injected by their respective adapters.

`shared/rendering.json` is the renderer contract consumed by both adapters. It
defines status-bar visibility and colors, canonical USB geometry, Spotify
geometry, and shared asset scaling. Arduino CLI generates corresponding C++
constants; the Vite build imports the synchronized JSON copy. Changes to these
values must be made in `shared/`, never directly in the simulator or firmware.

The same contract defines shared theme roles, Light/Dark neutral surfaces, and
the Red, Orange, Yellow, Green, Blue, Indigo, and Violet accent palettes.
Accent is independent of Light/Dark; Display → Appearance owns the user picker.
Blue is the default because its bright tone retains Leopold's existing cyan
selection treatment. New controls must resolve normal, selected, expanded,
focused, pressed, and disabled states through the shared palette. Review text
contrast, yellow in Light mode, color accessibility, 466×466 touch targets,
navigation gestures, and preference restoration on restart. Verify both
simulator/shared-LVGL behavior and device behavior separately.

Settings acceptance uses `FirmwareLvglCanvas` and the native shared
`shared/lvgl_renderer.cpp` compiled to WebAssembly. Firmware compiles that same
renderer with synchronized assets. The legacy `FirmwareCanvas.tsx` remains for
other browser surfaces; it cannot be the sole acceptance path for firmware
Settings parity. Compare LVGL theme/DPI/default styles too. Geometry, assets,
typography, persistence, and touch thresholds belong in `shared/`; screen
definitions must not be duplicated in either target.

## SDUI and asset mapping

The simulator types in `frontend/src/simulator/contract.ts` mirror the
Pydantic models in `backend/app/schemas.py` and the parser's six known types in
`device/include/sdui.h`: `text`, `indicator`, `button`, `progress`, `slider`,
and `icon`. The browser uses the component `icon` names as metadata, while the
firmware canvas loads the same RGB565A8 bitmap assets used by the LVGL
renderer. This keeps JSON portable without replacing the device's asset-backed
rendering with web placeholders.

Gesture thresholds originate in `shared/interaction.json` and its generated
device constants. The simulator handles firmware-local effects
(clock format, settings, and mocked USB results) in browser state and records
the equivalent event. USB buttons never execute host commands in the browser.

## Publish compatible SDUI

1. Test the document in the simulator and use **Apply JSON**.
2. Run the frontend typecheck/build and backend tests.
3. Review the JSON against `docs/device-api.md` and publish only through the
   authenticated admin `PUT /v1/admin/devices/{device_id}/ui` endpoint.
4. Let the server assign the next revision; do not copy a simulator revision
   into the publish request.

Do not put a device ID, device secret, admin token, or API token in this file,
the simulator, or the browser bundle.

## Firmware changes

For UI-only SDUI work, validate the browser and backend contracts without
flashing hardware. If firmware changes are required, wait for the GitHub
Actions artifact from the same commit:

```bash
python3 scripts/flash_firmware.py --package <downloaded-package> --port <authorized-port> --backup <new-private-backup.bin> --dry-run
python3 scripts/flash_firmware.py --package <downloaded-package> --port <authorized-port> --backup <new-private-backup.bin>
python3 scripts/verify_firmware.py --port <authorized-port> --commit <source-commit> --ui-revision <expected-ui>
```

The simulator does not replace a physical-device smoke test. Report firmware
build, flash, and device validation separately.
