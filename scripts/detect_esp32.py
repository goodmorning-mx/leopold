#!/usr/bin/env python3
"""Identify USB serial candidates without opening any serial port."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path

# Native Espressif USB and common USB/UART bridges. These are candidates,
# not proof of chip type; flash_firmware probes the actual chip/MAC/size.
USB_IDS = {(0x303A, 0x1001), (0x10C4, 0xEA60), (0x1A86, 0x7523),
           (0x1A86, 0x55D4), (0x0403, 0x6001)}


def inventory(ports, requested_port: str | None = None, serial_number: str | None = None) -> dict[str, object]:
    devices = [{"port": p.device, "vid": p.vid, "pid": p.pid,
                "serial_number": p.serial_number, "description": p.description,
                "location": p.location} for p in ports]
    candidates = [p for p in devices if (p["vid"], p["pid"]) in USB_IDS]
    eligible = [p for p in candidates if (not requested_port or p["port"] == requested_port)
                and (not serial_number or p["serial_number"] == serial_number)]
    selected = eligible[0] if len(eligible) == 1 else None
    return {"observed_at_utc": datetime.now(timezone.utc).isoformat(), "ports": devices,
            "candidates": candidates, "selected": selected,
            "status": "identified-usb-candidate" if selected else "ambiguous" if len(eligible) > 1 else "absent",
            "chip_verification": "pending esptool probe; USB identity alone is insufficient",
            "serial_port_opened": False}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--serial-number")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    try:
        from serial.tools import list_ports
    except ImportError as exc:
        raise SystemExit("install pyserial for USB inventory; no port was opened") from exc
    result = inventory(list_ports.comports(), args.port, args.serial_number)
    if args.report:
        with args.report.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
    print(json.dumps(result, indent=2))
    return 0 if result["selected"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
