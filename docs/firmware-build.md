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

The package contains the application, bootloader, partition table, optional
`boot_app0.bin`, merged flash image, `manifest.json`, and `SHA256SUMS.txt`.
The manifest records exact offsets, source commit, UI revision, asset-manifest
hash, toolchain versions, and both flash and revision-verification commands.

```bash
python3 scripts/flash_firmware.py --package <package> --port <authorized-port>
python3 scripts/verify_firmware.py --port <authorized-port> --commit <source-commit>
```

The helper only consumes the downloaded package and invokes Espressif
`esptool`; it does not compile and it contains no credentials. Flashing remains
explicitly authorized and is not performed by CI.
