from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts import package_firmware


class PackageFirmwareTests(unittest.TestCase):
    def test_artifact_contains_self_contained_helpers_and_safe_command_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            package = root / "package"
            build.mkdir()
            (build / "leopold.ino.bin").write_bytes(b"firmware")
            (build / "leopold.bootloader.bin").write_bytes(b"bootloader")
            (build / "leopold.partitions.bin").write_bytes(b"partition-table")
            with (build / "leopold.merged.bin").open("wb") as stream:
                stream.truncate(package_firmware.FLASH_BYTES)
            with mock.patch.object(sys, "argv", ["package_firmware.py", str(build), str(package)]):
                self.assertEqual(package_firmware.main(), 0)

            helpers = {
                "flash_firmware.py",
                "verify_firmware.py",
                "capture_device_screenshot.py",
                "ppm_to_png.py",
                "verify_firmware_package.py",
                "detect_esp32.py",
            }
            self.assertTrue(helpers.issubset({path.name for path in package.iterdir()}))
            manifest = json.loads((package / "manifest.json").read_text())
            self.assertEqual(manifest["schema_version"], 3)
            self.assertTrue(manifest["normal_update"]["requires_verified_full_flash_backup"])
            self.assertIn("--backup NEW_BACKUP_PATH", manifest["commands"]["update_command"])
            self.assertIn("--ui-revision 12", manifest["commands"]["verify_command"])
            self.assertEqual(manifest["normal_update"]["images"], [
                {"name": "bootloader.bin", "flash_offset": "0x0000"},
                {"name": "partitions.bin", "flash_offset": "0x8000"},
                {"name": "firmware.bin", "flash_offset": "0x10000"},
            ])
            self.assertTrue(manifest["factory_reset"]["destructive"])
            self.assertTrue(manifest["factory_reset"]["requires_verified_full_flash_backup"])
            self.assertEqual(
                set(manifest["commands"]),
                {"update_command", "backup_command", "factory_reset_command", "verify_command", "calibration_commands"},
            )
            sums = (package / "SHA256SUMS.txt").read_text().splitlines()
            checksum_names = {line.split("  ", 1)[1] for line in sums}
            self.assertTrue(helpers.issubset(checksum_names))
            for helper in helpers:
                self.assertEqual(
                    hashlib.sha256((package / helper).read_bytes()).hexdigest(),
                    next(line.split("  ", 1)[0] for line in sums if line.endswith("  " + helper)),
                )


if __name__ == "__main__":
    unittest.main()
