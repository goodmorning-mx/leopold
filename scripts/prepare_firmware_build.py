#!/usr/bin/env python3
"""Create a non-secret CI firmware configuration without overwriting local credentials."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
target = ROOT / "device" / "include" / "leopold_config.h"
example = ROOT / "device" / "include" / "leopold_config.example.h"

if not target.exists():
    content = example.read_text(encoding="utf-8")
    content = content.replace('"LPD-CHANGE-ME"', '"LPD-CI-BUILD"')
    content = content.replace('"CHANGE-ME"', '"ci-build-no-device-secret"')
    target.write_text(content, encoding="utf-8")
    print(f"Created synthetic non-secret build configuration: {target}")
else:
    print("Using existing local firmware configuration")
