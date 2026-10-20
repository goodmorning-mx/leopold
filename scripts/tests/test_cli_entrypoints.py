from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class CliEntrypointTests(unittest.TestCase):
    def test_hardware_free_help_works_without_repo_pythonpath(self):
        scripts = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as cwd:
            for name in ("flash_firmware.py", "verify_firmware.py", "verify_firmware_package.py",
                         "detect_esp32.py", "check_settings_overlay_generation.py",
                         "compare_settings_component_acceptance.py", "compare_settings_acceptance.py"):
                with self.subTest(script=name):
                    result = subprocess.run([sys.executable, str(scripts / name), "--help"],
                                            cwd=cwd, capture_output=True, text=True, timeout=20)
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
