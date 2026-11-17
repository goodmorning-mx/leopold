#!/usr/bin/env python3
"""Validate the repository-owned Arduino CLI firmware definition."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
config = json.loads((ROOT / "device" / "firmware-build.json").read_text())
sketch = (ROOT / "device" / "src" / "sketch.yaml").read_text()
required = ("FlashSize=16M", "CPUFreq=240", "FlashMode=qio", "PSRAM=opi", "PartitionScheme=custom")
assert config["sketch"] == "device/src"
assert config["profile"] == "leopold"
assert all(option in config["fqbn"] for option in required)
assert config["fqbn"] in sketch
assert ((ROOT / "platformio.ini").is_file() and (ROOT / "device/arduino-profile/src.ino").is_file() and (ROOT / "device/src/main.cpp").is_file()) or (ROOT / config["sketch"] / "src.ino").is_file()
assert (ROOT / config["flash"]["partition_file"]).is_file()
assert any(path.suffix == ".cpp" for path in (ROOT / config["sketch"]).iterdir())
assert not (ROOT / "device" / "platformio.ini").exists()
assert not (ROOT / "device" / ".pio").exists()
print("Arduino CLI firmware configuration valid")
