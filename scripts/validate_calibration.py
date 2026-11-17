"""Validate the shared physical-display calibration contract."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "shared" / "calibration.json"


def main() -> None:
    calibration = json.loads(CONTRACT.read_text(encoding="utf-8"))
    assert (calibration["width"], calibration["height"]) == (466, 466)
    assert calibration["background"] == "#000000"
    assert calibration["border"] == {"color": "#FFFFFF", "width": 1}
    assert calibration["corner_markers"] == {"color": "#FFFFFF", "size": 10}
    assert calibration["grid"] == {"color": "#1C9CFF", "spacing": 50, "width": 1}
    assert calibration["crosshair"] == {"color": "#FF3B30", "width": 2}
    assert calibration["circles"] == {"color": "#30D158", "width": 2, "radii": [50, 100, 150, 200]}
    assert calibration["width"] % calibration["grid"]["spacing"] == 16
    assert max(calibration["circles"]["radii"]) == calibration["width"] // 2 - 33
    print("Calibration contract valid: 466x466 border, markers, 50px grid, crosshair, and four circles")


if __name__ == "__main__":
    main()
