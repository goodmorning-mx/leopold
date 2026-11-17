# ESP32-S3 hardware interface

The [firmware board profile](../boards/leopold_s3.json) targets the Waveshare
ESP32-S3 Touch AMOLED 1.43 family with a 466 × 466 display, 16 MB flash and 8 MB
OPI PSRAM. Confirm the purchased board revision before making hardware or driver
changes. The [display driver](../device/src/display_manager.cpp) uses the
CO5300 initialization in the current source.

| Interface | Current firmware mapping |
| --- | --- |
| AMOLED | CS 9, CLK 10, data 11–14, reset 21, enable 42 |
| Touch | I²C address 0x38, SDA 47, SCL 48 |
| microSD | CS 38, MOSI 39, MISO 40, SCLK 41 |
| Battery | ADC 4, nominal 2:1 divider and 3.3–4.2 V estimation range |
| BOOT | GPIO 0 |
| USB | Native CDC and HID availability depend on firmware and the selected board profile |

The [GPIO source](../device/include/board_config.h) owns these definitions.
Audio expansion, actuator selection, power limits, and mechanical fit remain
hardware verification items. The partition layout is defined in
[`partitions.csv`](../device/src/partitions.csv).
