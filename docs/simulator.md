# Leopold browser simulator

The simulator is a 466 × 466 preview at `/simulator`. It compiles the same
LVGL renderer as the ESP32 and uses browser adapters for input, persistence
and simulated services.

The signed-in account page embeds this same route below devices and services.
The iframe loads lazily and is removed when the user signs out. Its heading
also provides a link to open the simulator in a separate tab.

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

`shared/rendering.json` is the renderer contract consumed by both adapters. It
defines status-bar visibility and colors, canonical USB geometry, Spotify
geometry, and shared asset scaling. PlatformIO generates corresponding C++
constants; the Vite build imports the synchronized JSON copy. Changes to these
values must be made in `shared/`, never directly in the simulator or firmware.

The visible device surface is a direct browser port of the firmware display
paths in `device/src/display_manager.cpp`, implemented in
`frontend/src/simulator/FirmwareCanvas.tsx`. It uses the same screen IDs,
466 × 466 coordinates, font-size mapping, special control-center and Spotify
layouts, status-bar visibility and positions, USB geometry, and binary assets
generated from `shared/`.
The browser cannot execute the ESP32/LVGL binary, so the canvas is the
renderer adapter. Geometry, assets, typography mapping, persistence defaults,
and touch thresholds are committed under `shared/`; screen definitions must not
be duplicated in either target.

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

## Rebuild LVGL WASM

The distribution files under `frontend/public/wasm/` are included so Coolify
can build from the existing `/frontend` Docker context. Rebuild both files
together after changing the renderer, adapter or generated headers:

```bash
git submodule update --init --depth 1
python -m pip install -r scripts/requirements.txt
python scripts/generate_firmware_ui.py
EMCC="$(command -v emcc)" python scripts/build_lvgl_wasm.py
LEOPOLD_REQUIRE_DISPLAY_WASM=1 python -m unittest scripts.tests.test_display_repaint
```

Use Emscripten 6.0.12 and Node.js 22. The LVGL submodule pins v9.3.0.
