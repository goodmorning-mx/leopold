"""Compile and exercise the firmware cache implementation against isolated LittleFS fixtures."""
from __future__ import annotations

import os
import sys
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "scripts/tests/fixtures/asset_cache_native"


class PersistentFirmwareAssetCacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = Path(tempfile.mkdtemp(prefix="leopold-device-cache-native-"))
        cls.binary = cls.temp / "cache-test"
        compiler = shutil.which("clang++") or shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("no local C++ compiler")
        subprocess.run([
            compiler, "-std=c++17", "-O1", "-Wno-deprecated-declarations",
            "-I", str(FIXTURE), "-I", str(ROOT / "device/include"),
            str(FIXTURE / "cache_test.cpp"), str(ROOT / "device/src/persistent_asset_cache.cpp"),
            *(["-lcrypto"] if sys.platform != "darwin" else []), "-o", str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls) -> None:
        shutil.rmtree(cls.temp, ignore_errors=True)

    def run_fixture(self, name: str, mode: str) -> Path:
        root = self.temp / name
        shutil.rmtree(root, ignore_errors=True)
        root.mkdir()
        env = os.environ | {"LEOPOLD_TEST_FS": str(root)}
        subprocess.run([str(self.binary), mode], check=True, capture_output=True, text=True, env=env)
        return root

    def test_stream_verification_failure_restart_and_corruption(self) -> None:
        root = self.temp / "restart"
        shutil.rmtree(root, ignore_errors=True)
        root.mkdir()
        env = os.environ | {"LEOPOLD_TEST_FS": str(root)}
        subprocess.run([str(self.binary), "seed"], check=True, capture_output=True, text=True, env=env)
        (root / "asset-stage.tmp").write_bytes(b"interrupted staging data")
        subprocess.run([str(self.binary), "verify"], check=True, capture_output=True, text=True, env=env)
        index = (root / "asset-cache.json").read_text().splitlines()
        pending_hash = index[2].split("|")[2]
        (root / f"a-{pending_hash}").write_bytes(b"truncated/corrupt")
        subprocess.run([str(self.binary), "verify-corrupt"], check=True, capture_output=True, text=True, env=env)

    def test_unpinned_lru_eviction(self) -> None:
        self.run_fixture("eviction", "evict")

    def test_pinned_global_and_clock_survive_storage_exhaustion(self) -> None:
        self.run_fixture("pinned", "pins")

    def test_rename_and_index_failures_preserve_evicted_entries(self) -> None:
        self.run_fixture("rename-failure", "transactional-rename")
        self.run_fixture("index-failure", "transactional-index")


if __name__ == "__main__":
    unittest.main()
