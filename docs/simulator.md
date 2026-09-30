# Leopold browser simulator

The simulator is a local test surface for the physical 466 × 466 Leopold UI.
Its firmware reference is release tag `firmware-v0.8.1`, created from the
simulator parity commit on `fix/live-device-time-controls`, the source snapshot identified
for the firmware release. The compiled
`firmware.bin` is intentionally not expected in GitHub; PlatformIO output is
local and ignored. The simulator is available at `/simulator` in the Vite app
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
browser debug editor and used by PlatformIO's pre-build generator to produce
`device/include/generated_firmware_ui.h`. The generated file is ignored and
must not be edited by hand. Runtime-only values such as brightness, clock,
weather, and Cloud/USB state are injected by their respective adapters.

The visible device surface is a direct browser port of the firmware display
paths in `device/src/display_manager.cpp`, implemented in
`frontend/src/simulator/FirmwareCanvas.tsx`. It uses the same screen IDs,
466 × 466 coordinates, font-size mapping, special control-center and Spotify
layouts, status bar positions, and binary assets copied from `device/assets/`.
The browser cannot execute the ESP32/LVGL binary, so the canvas is the
maintained rendering mapping; changes to C++ display geometry must be reflected
there and validated against the firmware source.

## SDUI and asset mapping

The simulator types in `frontend/src/simulator/contract.ts` mirror the
Pydantic models in `backend/app/schemas.py` and the parser's six known types in
`device/include/sdui.h`: `text`, `indicator`, `button`, `progress`, `slider`,
and `icon`. The browser uses the component `icon` names as metadata, while the
firmware canvas loads the same RGB565A8 bitmap assets used by the LVGL
renderer. This keeps JSON portable without replacing the device's asset-backed
rendering with web placeholders.

The gesture thresholds are intentionally copied from
`device/src/touch_input.cpp`. The simulator handles firmware-local effects
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
flashing hardware. If firmware changes are required, build from `device/`:

```bash
pio run
```

Flashing is a separate, explicitly authorized action:

```bash
pio run --target upload --upload-port <authorized-port>
```

The simulator does not replace a physical-device smoke test. Report firmware
build, flash, and device validation separately.
