"""Exercise actual WifiPortal source against synthetic host adapters; no hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class WifiFlowTests(unittest.TestCase):
    def test_real_portal_state_machine(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ host compiler unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            for source in (ROOT / "scripts/tests/fixtures/wifi").iterdir():
                shutil.copyfile(source, path / source.name)
            # Copy production files unchanged; isolate config lookup from any local credentials.
            for source in (ROOT / "device/src/wifi_portal.cpp", ROOT / "device/include/wifi_portal.h"):
                shutil.copyfile(source, path / source.name)
            output = path / "wifi-flow"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-I", str(path), str(path / "wifi_flow.cpp"), str(path / "wifi_portal.cpp"), "-o", str(output)], check=True, capture_output=True, text=True)
            result = subprocess.run([str(output)], check=True, capture_output=True, text=True)
            self.assertIn("Wi-Fi host flow passed", result.stdout)

    def test_scan_lifecycle_and_settings_contact(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ host compiler unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "scan-flow"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-I", str(ROOT / "scripts/tests/fixtures/wifi"), "-I", str(ROOT / "device/include"), "-I", str(ROOT / "shared"), str(ROOT / "scripts/tests/fixtures/wifi/scan_flow.cpp"), "-o", str(output)], check=True, capture_output=True, text=True)
            result = subprocess.run([str(output)], check=True, capture_output=True, text=True)
            self.assertIn("Scan lifecycle and original-row release passed", result.stdout)

    def test_production_touch_path(self):
        import json
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ host compiler unavailable")
        touch = json.loads((ROOT / "shared/interaction.json").read_text())["touch"]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            for name in ("touch_input.cpp", "touch_input.h", "board_config.h"):
                folder = "src" if name.endswith("cpp") else "include"
                shutil.copyfile(ROOT / "device" / folder / name, path / name)
            names = {"kDragIntervalMs": "drag_interval_ms", "kSwipeUpDistance": "swipe_up_distance", "kSwipeDirectionRatio": "swipe_direction_ratio", "kSwipeUpMaxDurationMs": "swipe_up_max_duration_ms", "kSwipeHorizontalDistance": "swipe_horizontal_distance", "kSwipeHorizontalMaxDurationMs": "swipe_horizontal_max_duration_ms", "kLongPressDurationMs": "long_press_duration_ms", "kLongPressMaxMovement": "long_press_max_movement", "kTapMaxDurationMs": "tap_max_duration_ms", "kTapMaxMovement": "tap_max_movement"}
            (path / "generated_firmware_ui.h").write_text("namespace leopold::generated_ui {" + "".join(f"constexpr int {name} = {int(touch[key] * 10) if name == 'kSwipeDirectionRatio' else touch[key]};" for name, key in names.items()) + "}")
            output = path / "touch-flow"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-I", str(path), "-I", str(ROOT / "scripts/tests/fixtures/wifi"), "-I", str(ROOT / "shared"), str(ROOT / "scripts/tests/fixtures/wifi/touch_flow.cpp"), str(path / "touch_input.cpp"), "-o", str(output)], check=True, capture_output=True, text=True)
            result = subprocess.run([str(output)], check=True, capture_output=True, text=True)
            self.assertIn("Production touch path passed", result.stdout)
