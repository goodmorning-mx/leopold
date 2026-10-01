# Leopold SD-card assets

The canonical image bytes live in `shared/assets/` in GitHub. The firmware no
longer embeds those image bytes in flash. At boot it loads the matching
RGB565A8 files from `/sd/assets/` into PSRAM and uses the embedded shared
renderer for both the browser and device.

To update the SD card installed inside the device, connect the ESP32 by USB-C
and run:

```bash
python3 scripts/provision_device_assets.py /dev/cu.usbmodemYOUR_DEVICE
```

The firmware accepts only the filenames and exact byte sizes in the shared
manifest, writes a temporary file, verifies SHA-256, and atomically replaces
the active asset. This updates the internal card without removing it.

The USB serial connection is not a generic mass-storage drive; it is a
firmware-assisted transfer protocol. The canonical files stay in GitHub, while
the physical SD contents remain device-local. The removable-card script remains
available for recovery if a card reader is ever used.
