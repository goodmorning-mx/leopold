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
