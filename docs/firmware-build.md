# Portable ESP32-S3 firmware build

From the repository root, use Python 3.12 in a virtual environment.

```bash
python -m pip install platformio==6.1.19 -r scripts/requirements.txt
pio run --project-dir device
```

The `leopold` environment uses pioarduino 54.03.20 (Arduino-ESP32 3.2.0),
the reviewed ESP32-S3 board profile, LVGL 9.3.0, and the [partition
table](../device/src/partitions.csv). PlatformIO generates the firmware UI,
embedded assets, and synthetic placeholder configuration before compilation.
This build serves as a compile check. Owner device configuration and the
physical installation and recovery procedures are maintained separately.

The partition table defines two 0x640000-byte application slots. The build
provides source-commit metadata. A successful build establishes compilation and
size fit for these inputs. Device installation and runtime verification require
the separately approved owner workflow.
