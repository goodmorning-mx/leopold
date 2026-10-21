from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

from scripts import verify_firmware_package as gate
from scripts.tests import test_flash_firmware as fixtures


class FirmwarePackageGateTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.package = fixtures.FlashFirmwareTests.make_package(self)
        self.assets = self.root / "shared/asset-manifest.json"
        self.assets.parent.mkdir()
        self.assets.write_text(json.dumps({"assets": {"sample": {"file": "sample.bin"}}}))
        for directory in ["shared/assets", "backend/assets", "device/assets", "frontend/public/firmware-assets"]:
            path = self.root / directory
            path.mkdir(parents=True, exist_ok=True)
            (path / "sample.bin").write_bytes(b"canonical")
        asset_hash = hashlib.sha256(json.dumps(json.loads(self.assets.read_text()), sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        fixtures.FlashFirmwareTests.rewrite_manifest(self, self.package, lambda m: m.update(asset_manifest_sha256=asset_hash))
        self.archive = self.root / "artifact.zip"
        with zipfile.ZipFile(self.archive, "w") as stream:
            for path in self.package.iterdir():
                stream.write(path, path.name)
        self.digest = gate.sha256(self.archive)

    def verify(self, **overrides):
        args = dict(package=self.package, repo=self.root, expected_source="a" * 40,
                    head="c" * 40, base="b" * 40, asset_manifest=self.assets,
                    archive=self.archive, digest=self.digest)
        args.update(overrides)
        with mock.patch.object(gate.subprocess, "run", return_value=mock.Mock(
                returncode=0, stdout="a" * 40 + "\n" + "b" * 40 + " " + "c" * 40 + "\n")):
            return gate.verify(**args)

    def test_complete_offline_gate_checks_archive_and_consumers(self):
        result = self.verify()
        self.assertTrue(result["passed"])
        self.assertFalse(result["serial_access"])
        self.assertFalse(result["network_access"])

    def test_stale_source_wrong_parent_and_digest_fail_closed(self):
        for kwargs, stage in [({"expected_source": "d" * 40}, "source-identity"),
                              ({"head": "d" * 40}, "source-identity"),
                              ({"base": "d" * 40}, "source-identity"),
                              ({"digest": "0" * 64}, "archive-identity")]:
            result = self.verify(**kwargs)
            self.assertFalse(result["passed"])
            self.assertEqual(result["first_failed_gate"], stage)

    def test_mismatched_runtime_asset_and_zip_content_rejected(self):
        runtime = self.root / "device/assets/sample.bin"
        runtime.write_bytes(b"different")
        self.assertEqual(self.verify()["first_failed_gate"], "shared-asset-identity")
        runtime.write_bytes(b"canonical")
        with zipfile.ZipFile(self.archive, "w") as stream:
            for path in self.package.iterdir():
                stream.writestr(path.name, b"bad" if path.name == "firmware.bin" else path.read_bytes())
        result = self.verify(digest=gate.sha256(self.archive))
        self.assertEqual(result["first_failed_gate"], "archive-identity")

    def test_missing_git_object_is_not_accepted_as_prior_proof(self):
        with mock.patch.object(gate.subprocess, "run", return_value=mock.Mock(returncode=1)), self.assertRaises(ValueError):
            gate.git_provenance(self.root, "a" * 40, "c" * 40, "b" * 40)


if __name__ == "__main__":
    unittest.main()
