# Leopold firmware build and release

GitHub Actions is the only authoritative Leopold firmware compiler. CI and
release tags both use `scripts/build_firmware.py`, the `device/src/sketch.yaml`
profile, and `device/firmware-build.json`; they cannot silently select
different board or dependency settings.

## Migration audit

| Previous build setting | Arduino CLI replacement |
| --- | --- |
| `esp32-s3-devkitc-1`, Arduino framework | `esp32:esp32:esp32s3` with the pinned profile in `device/src/sketch.yaml` |
| `platform-espressif32` `55.03.38-1` | Espressif `esp32:esp32` core `3.2.0` from the pinned package index |
| 16 MB flash | `FlashSize=16M` and `flash_size: 16MB` |
| 240 MHz CPU | `CPUFreq=240` |
| 80 MHz flash | `FlashFreq=80m` supplied by the ESP32-S3 board profile |
| QIO flash mode | `FlashMode=qio` |
| `qio_opi` memory type | `FlashMode=qio`, `PSRAM=opi`, and `BOARD_HAS_PSRAM` |
| `default_16MB.csv` partitions | `PartitionScheme=custom` with the identical repository-owned `device/src/partitions.csv` |
| `ARDUINO_USB_MODE=1` | `USBMode=hwcdc` plus the explicit build define |
| `ARDUINO_USB_CDC_ON_BOOT=1` | `CDCOnBoot=cdc` plus the explicit build define |
| `-Wall -Wextra` | `compiler.warning_flags` in `firmware-build.json` |
| `-I include` | `-I{root}/device/include` |
| `-I ../shared` | `-I{root}/shared` |
| `LV_CONF_INCLUDE_SIMPLE`, `BOARD_HAS_PSRAM`, `WM_NODEBUG` | Same explicit compiler defines |
| GFX Library for Arduino `1.6.7` | Same pinned library in `sketch.yaml` |
| LVGL `9.3.0` | Same pinned library in `sketch.yaml` |
| ArduinoJson `7.4.2` | Same pinned library in `sketch.yaml` |
| WiFiManager `2.0.17` | Same pinned library in `sketch.yaml` |
| QRCode `0.0.1` | Same pinned library in `sketch.yaml` |
| SCons pre-build generator | `python3 scripts/generate_firmware_ui.py` before compilation |

The Arduino CLI sketch root is `device/src`, which contains the existing
firmware sources. No second firmware source tree was created. The generated
headers remain derived outputs from `shared/`.

CI creates a synthetic, non-secret `leopold_config.h` only when the ignored
local configuration is absent. It is suitable for compilation and never
contains device credentials. A provisioned device build must still use the
authorized local configuration flow.

## Package and flashing

The package contains the application, bootloader, partition table, merged flash
image, six self-contained helper scripts, `manifest.json`, and
`SHA256SUMS.txt`. Manifest schema 3 records exact image ranges and checksums,
the non-destructive update set, preserved data regions, the destructive factory
reset image, partition-table verification requirements, and separate update,
backup, factory-reset, revision-verification, and calibration commands.

```bash
python3 scripts/flash_firmware.py --package <package> --port <authorized-port> --backup <new-private-backup.bin> --dry-run
python3 scripts/flash_firmware.py --package <package> --port <authorized-port> --backup <new-private-backup.bin>
python3 scripts/verify_firmware.py --port <authorized-port> --commit <source-commit> --ui-revision <expected-ui> --report <new-revision-proof.json>
```

The default update writes only `bootloader.bin` at `0x0000`, `partitions.bin` at
`0x8000`, and `firmware.bin` at `0x10000`. It never selects
`merged-flash.bin`. Before a real write, it reads the installed partition table
and compares it byte-for-byte with the packaged table. A mismatch stops before
any write and requires an explicitly authorized partition migration or factory
reset. Therefore NVS and other data regions are preserved only after that
compatibility check passes; absence of a whole-flash erase alone is not enough.
`--dry-run` validates the artifact, prints the exact command and address ranges,
and explains that the connected-device partition check is required, while
never contacting a device.

Every real normal update probes the chip/flash size and MAC, creates a new
16 MiB backup, verifies size and SHA-256, and writes private checksum/JSON
receipt files. Existing backup/evidence paths are refused. Before writing it
compares installed partitions, confirms the device identity is unchanged, and
rechecks backup integrity. Missing/partial/corrupted backups, wrong flash size,
a different device, or partition mismatch prevent the write. Old backups cannot
be substituted. Files use mode 0600 and may contain credentials; never commit
or publish them. A failed read leaves its private partial file and requires a
new path for a retry. Schema-2 packages remain preserved historical artifacts;
the schema-3 updater rejects them.

## Hardware-free package gate and USB identification

The checksum-covered `verify_firmware_package.py` helper reuses package
validation, checks actual Git merge parents, compares synchronized runtime
assets, and verifies every ZIP member against the extracted package. Fetch the
verified PR merge ref into local Git through an authenticated route beforehand;
a missing object or unauthenticated private API 404 is not provenance proof.

```bash
python3 scripts/verify_firmware_package.py --package <package> --repo . \
  --source-commit <ci-merge-sha> --pr-head <head-sha> --expected-base <base-sha> \
  --asset-manifest shared/asset-manifest.json --zip <artifact.zip> \
  --zip-sha256 <verified-github-digest> --report <new-package-report.json>
```

This is an offline package/provenance result, not a live CI or physical parity
result. Confirm the exact GitHub run completed successfully separately.
`python3 scripts/detect_esp32.py --report <new-usb-inventory.json>` enumerates
USB serial candidates without opening a port. It reports absence/ambiguity;
`--port` or `--serial-number` can identify one observed candidate. USB IDs alone
do not prove chip type; the updater probes the actual chip/flash/MAC.

Running-revision proof still requires serial access before an update and again
after reboot. For an intentionally hardware-free session, use offline checks
and mocked tests only; do not run update, revision, or capture commands.

## Backup and explicitly destructive provisioning

For a read-only full-flash backup, use the backup-only operation. It verifies
that the resulting file is exactly 16 MiB and writes a SHA-256 sidecar:

```bash
python3 scripts/flash_firmware.py --backup-only --port <authorized-port> \
  --backup <path>/leopold-full-flash.bin
```

The merged image is reserved for blank-device provisioning or an explicitly
authorized factory-reset provisioning flow. Factory-reset mode always creates
and verifies a fresh 16 MiB backup before writing:

```bash
python3 scripts/flash_firmware.py --package <package> \
  --port <authorized-port> --factory-reset \
  --backup <path>/leopold-before-factory-reset.bin
```

The helper only consumes the downloaded package and invokes Espressif
`esptool`; it does not compile and it contains no credentials. Flashing remains
explicitly authorized and is not performed by CI.

After a normal update, run the packaged revision command. Only after revision
verification should calibration capture be performed with the packaged
`capture_device_screenshot.py` and `ppm_to_png.py` helpers. If recovery is
needed, preserve the verified `.bin` backup and its `.sha256` sidecar, then use
the device-specific recovery procedure to restore it; never treat a merged
image as an in-place update.

The revision helper requires both commit and UI revision. Optional JSON proof
contains only revision fields and port, never arbitrary serial output. Earlier
calibration can be reused when its inputs are unchanged and the task does not
call for another pass; physical Settings parity requires fresh post-update
matching-state captures and interaction evidence.
