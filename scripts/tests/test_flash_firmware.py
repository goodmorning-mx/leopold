from __future__ import annotations

import contextlib
import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts import flash_firmware as flash


class FlashFirmwareTests(unittest.TestCase):
    def make_package(self, name: str = "package") -> Path:
        temp = Path(self.temp_dir.name)
        package = temp / name
        package.mkdir()
        for name, data in {
            "bootloader.bin": b"bootloader",
            "partitions.bin": b"partition-table",
            "firmware.bin": b"firmware",
        }.items():
            (package / name).write_bytes(data)
        merged = package / "merged-flash.bin"
        with merged.open("wb") as stream:
            stream.truncate(flash.FLASH_BYTES)
        images = {}
        for name, offset in (
            ("bootloader.bin", "0x0000"),
            ("partitions.bin", "0x8000"),
            ("firmware.bin", "0x10000"),
            ("merged-flash.bin", "0x0000"),
        ):
            path = package / name
            size = path.stat().st_size
            images[name] = {
                "filename": name,
                "bytes": size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "flash_offset": offset,
                "end_address": hex(int(offset, 0) + size),
            }
        manifest = {
            "schema_version": 2,
            "product": "Leopold",
            "chip": "esp32s3",
            "flash_size": "16MB",
            "flash_size_bytes": flash.FLASH_BYTES,
            "images": images,
            "normal_update": {
                "images": [
                    {"name": name, "flash_offset": offset}
                    for name, offset in (("bootloader.bin", "0x0000"), ("partitions.bin", "0x8000"), ("firmware.bin", "0x10000"))
                ],
                "destructive": False,
                "verify_partition_table": True,
            },
            "factory_reset": {
                "image": "merged-flash.bin",
                "destructive": True,
                "requires_explicit_flag": True,
                "requires_verified_full_flash_backup": True,
            },
            "preserved_data_regions": [
                {"name": "nvs", "start": "0x9000", "end": "0xE000", "preserved": True}
            ],
            "helpers": ["flash_firmware.py"],
            "commands": {
                "update_command": "python3 flash_firmware.py",
                "backup_command": "python3 flash_firmware.py --backup-only",
                "factory_reset_command": "python3 flash_firmware.py --factory-reset",
                "verify_command": "python3 verify_firmware.py",
                "calibration_commands": ["python3 capture_device_screenshot.py"],
            },
        }
        (package / "flash_firmware.py").write_text("# helper\n")
        (package / "manifest.json").write_text(json.dumps(manifest) + "\n")
        checksum_lines = []
        for path in sorted(package.iterdir()):
            checksum_lines.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
        (package / "SHA256SUMS.txt").write_text("".join(checksum_lines))
        return package

    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def rewrite_manifest(self, package: Path, mutate) -> None:
        manifest_path = package / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        mutate(manifest)
        manifest_path.write_text(json.dumps(manifest) + "\n")
        sums = []
        for path in sorted(package.iterdir()):
            if path.name != "SHA256SUMS.txt":
                sums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
        (package / "SHA256SUMS.txt").write_text("".join(sums))

    def test_normal_dry_run_only_selects_three_images_and_uses_current_python(self) -> None:
        package = self.make_package("offset-package")
        output = io.StringIO()
        with mock.patch.object(flash.subprocess, "run") as run, contextlib.redirect_stdout(output):
            self.assertEqual(flash.main(["--package", str(package), "--port", "TEST", "--dry-run"]), 0)
        run.assert_not_called()
        text = output.getvalue()
        self.assertIn("bootloader.bin", text)
        self.assertIn("partitions.bin", text)
        self.assertIn("firmware.bin", text)
        self.assertNotIn("merged-flash.bin", text)
        self.assertIn("partition table will be read and compared", text)
        self.assertEqual(flash.command_for({"chip": "esp32s3"}, "TEST")[0], __import__("sys").executable)

    def test_normal_update_checks_partition_table_before_write(self) -> None:
        package = self.make_package()
        calls = []

        def run(command):
            calls.append(command)
            if command[7] == "read-flash":
                Path(command[-1]).write_bytes((package / "partitions.bin").read_bytes())
            return mock.Mock(returncode=0)

        with mock.patch.object(flash.subprocess, "run", side_effect=run):
            self.assertEqual(flash.main(["--package", str(package), "--port", "TEST"]), 0)
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0][7], "read-flash")
        self.assertEqual(calls[1][7], "write-flash")

    def test_partition_mismatch_stops_before_write(self) -> None:
        package = self.make_package()
        calls = []

        def run(command):
            calls.append(command)
            Path(command[-1]).write_bytes(b"different")
            return mock.Mock(returncode=0)

        with mock.patch.object(flash.subprocess, "run", side_effect=run):
            self.assertEqual(flash.main(["--package", str(package), "--port", "TEST"]), 2)
        self.assertEqual(len(calls), 1)

    def test_factory_reset_is_explicit_and_backup_precedes_write(self) -> None:
        package = self.make_package()
        backup = Path(self.temp_dir.name) / "backup.bin"
        calls = []

        def run(command):
            calls.append(command)
            if command[7] == "read-flash":
                with backup.open("wb") as stream:
                    stream.truncate(flash.FLASH_BYTES)
            return mock.Mock(returncode=0)

        with mock.patch.object(flash.subprocess, "run", side_effect=run):
            self.assertEqual(flash.main(["--package", str(package), "--port", "TEST", "--factory-reset", "--backup", str(backup)]), 0)
        self.assertEqual([call[7] for call in calls], ["read-flash", "write-flash"])
        self.assertTrue((backup.with_name("backup.bin.sha256")).is_file())

    def test_backup_only_never_writes(self) -> None:
        backup = Path(self.temp_dir.name) / "backup.bin"
        with mock.patch.object(flash.subprocess, "run") as run:
            self.assertEqual(flash.main(["--backup-only", "--port", "TEST", "--backup", str(backup), "--dry-run"]), 0)
        run.assert_not_called()

    def test_rejects_checksum_paths_and_duplicate_entries(self) -> None:
        package = self.make_package()
        sums = package / "SHA256SUMS.txt"
        valid = sums.read_text()
        sums.write_text(valid + valid.splitlines()[0].replace("  bootloader.bin", "  ../bootloader.bin") + "\n")
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        sums.write_text(valid + valid.splitlines()[0] + "\n")
        with self.assertRaises(SystemExit):
            flash.load_package(package)

    def test_rejects_missing_merged_checksum(self) -> None:
        package = self.make_package()
        sums = package / "SHA256SUMS.txt"
        sums.write_text("\n".join(line for line in sums.read_text().splitlines() if not line.endswith("  merged-flash.bin")) + "\n")
        with self.assertRaises(SystemExit):
            flash.load_package(package)

    def test_rejects_corrupt_checksum_and_duplicate_update_offset(self) -> None:
        package = self.make_package()
        (package / "firmware.bin").write_bytes(b"tampered")
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        package = self.make_package("offset-package")
        self.rewrite_manifest(
            package,
            lambda manifest: (
                manifest["images"]["firmware.bin"].update(flash_offset="0x8000", end_address="0x8008"),
                manifest["normal_update"]["images"][2].update(flash_offset="0x8000"),
            ),
        )
        with self.assertRaises(SystemExit):
            flash.load_package(package)

    def test_rejects_wrong_schema_chip_offsets_and_ranges(self) -> None:
        package = self.make_package()
        self.rewrite_manifest(package, lambda manifest: manifest.update(schema_version=1))
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        self.rewrite_manifest(package, lambda manifest: manifest.update(schema_version=2, chip="esp32"))
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        self.rewrite_manifest(package, lambda manifest: manifest["images"]["firmware.bin"].update(flash_offset="not-an-offset"))
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        self.rewrite_manifest(package, lambda manifest: manifest["images"]["merged-flash.bin"].update(end_address="0x1000001"))
        with self.assertRaises(SystemExit):
            flash.load_package(package)

    def test_rejects_missing_binary_and_overlapping_ranges(self) -> None:
        package = self.make_package()
        (package / "firmware.bin").unlink()
        with self.assertRaises(SystemExit):
            flash.load_package(package)
        with self.assertRaises(SystemExit):
            flash.validate_ranges(
                [{"name": "a", "offset": 0, "end_address": 10}, {"name": "b", "offset": 9, "end_address": 20}],
                "test",
            )

    def test_rejects_ambiguous_arguments(self) -> None:
        with self.assertRaises(SystemExit):
            flash.main(["--backup-only", "--factory-reset", "--port", "TEST", "--backup", "x"])
        with self.assertRaises(SystemExit):
            flash.main(["--backup-only", "--port", "TEST", "--backup", "x", "--package", "x"])
        with self.assertRaises(SystemExit):
            flash.main(["--port", "TEST", "--backup", "x"])
        package = self.make_package()
        with self.assertRaises(SystemExit):
            flash.main(["--package", str(package), "--port", "TEST", "--backup", "x", "--dry-run"])


if __name__ == "__main__":
    unittest.main()
