# Settings release and Wi-Fi scan follow-up (UI16)

This focused continuation preserves UI15's glass/gears artwork, geometry,
comparison thresholds, existing three-network store and Display-on-Settings
behavior. Brightness refinement and the ten-network redesign remain deferred.

## Settings contact

A contact captures its original shared row. Every controller sample can cancel
that capture, including samples between throttled drag events. Leaving the row
permanently cancels the contact; crossing to another row cannot transfer it.
Tap, stationary long-press and neutral release inside the original row execute
its declared action. Debounced releases clear feedback through a distinct
cancel event. The clock keeps its separate long-press format action. Generic
SDUI tap handling excludes Settings so a cancelled contact cannot queue an
unrelated row's tap.

The simulator follows `shared/interaction.json`'s release contract. Wi-Fi and
Bluetooth open their existing panels; Display and Sound retain their existing
undefined actions and remain on Settings.

## Scan lifecycle findings

The old loop sampled `now` before processing input. Scan startup then stored a
later `millis()`. For `now=100` and `started=101`, unsigned subtraction produces
4,294,967,295, passing the 30-second timeout immediately. The host regression
reproduces this defect and checks that the replacement scan remains pending
in that same loop, at 29,999 ms, expires at 30,000 ms and survives rollover.

The replacement polls a fresh clock after startup and the completion query.
A completed result wins over timeout at the boundary. Startup failure,
asynchronous completion and genuine zero results remain distinct outcomes.

`scanDelete()` clears Arduino bookkeeping but does not stop the radio. The
owner now stops the radio and drains completion before clearing state or
starting another request. A cancelled completion cannot become a new empty
result. Cancellation continues draining after leaving the scan panel.
WiFiManager shutdown also calls `scanDelete()`, so a portal-owned scan must
finish stopping before portal shutdown and station preparation occur.
Disconnected association attempts are stopped with credential erasure disabled;
working connections remain active, automatic reconnect pauses during selection,
and existing credentials/NVS identity are preserved.

Safe diagnostics record scan start return code, completion count and elapsed
time; they do not print credentials. These fixes address proven code defects
and lifecycle conflicts. They do **not** establish that every physical empty
result had the same cause. Live discovery, connection, connected banner and
restart reconnection require the replacement firmware's physical gate.

Primary implementation references:

- [Arduino ESP32 3.2.0 scan implementation](https://github.com/espressif/arduino-esp32/blob/3.2.0/libraries/WiFi/src/WiFiScan.cpp)
- [WiFiManager 2.0.17 portal shutdown](https://github.com/tzapu/WiFiManager/blob/v2.0.17/WiFiManager.cpp)
- [ESP-IDF 5.4 scan-done event semantics](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-guides/wifi.html#wifi-event-scan-done)

## Validation and handoff

Host tests compile the production touch input, Settings contact, Wi-Fi scan and
portal code against synthetic adapters. Tests cover threshold boundaries,
rollover, cancellation, delayed completion, portal transition, retry cleanup,
working-link preservation and credential rollback/save-after-success.

Current software, simulator, CI and exact package evidence lives in
`docs/parity/current-handoff.json` (local continuation record). Original UI15
physical evidence remains historical for this replacement. No hardware access,
flash, merge or deployment is part of preparation. Any later normal update
requires separate authorization, fresh identification/revision, a new full
16 MiB backup, partition equality and individual-image writes only.
