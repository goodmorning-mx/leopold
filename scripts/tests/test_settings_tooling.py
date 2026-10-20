from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from PIL import Image

from scripts.check_settings_overlay_generation import check
from scripts.generate_settings_overlay_masks import main as generate
from scripts.compare_settings_component_acceptance import evaluate, load_capture, compare_outside

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).parent / "fixtures/settings"


class SettingsToolingTests(unittest.TestCase):
    def test_generation_reproduces_current_assets_twice(self):
        self.assertEqual(check(), {"passed": True, "first_run_changed": [], "second_run_changed": [],
                                   "checkout_assets_modified": False})

    def test_invalid_source_stops_before_mutating_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / "shared", root / "shared")
            sources = json.loads((root / "shared/settings-overlay-sources.json").read_text())
            entry = next(iter(sources["status_components"].values()))
            (root / "shared" / entry["file"]).write_bytes(b"corrupted source")
            before = {p.name: p.read_bytes() for p in (root / "shared/assets").iterdir() if p.is_file()}
            with self.assertRaises(ValueError):
                generate(root)
            self.assertEqual(before, {p.name: p.read_bytes() for p in (root / "shared/assets").iterdir() if p.is_file()})

    def test_encoding_checkpoint_cannot_override_reference_pixels(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(ROOT / "shared", root / "shared")
            manifest = root / "shared/settings-overlay-sources.json"
            data = json.loads(manifest.read_text())
            entry = next(iter(data["row_encodings"].values()))
            checkpoint = root / "shared" / entry["file"]
            with Image.open(checkpoint) as image:
                changed = image.copy()
            changed.putpixel((0, 0), (255, 0, 0, 255))
            changed.save(checkpoint)
            entry["sha256"] = hashlib.sha256(checkpoint.read_bytes()).hexdigest()
            manifest.write_text(json.dumps(data))
            before = {p.name: p.read_bytes() for p in (root / "shared/assets").iterdir() if p.is_file()}
            with self.assertRaisesRegex(ValueError, "reference-derived pixels differ"):
                generate(root)
            self.assertEqual(before, {p.name: p.read_bytes() for p in (root / "shared/assets").iterdir() if p.is_file()})

    def test_good_composition_passes_and_real_historical_seam_fails(self):
        for entry in json.loads((FIXTURES / "provenance.json").read_text()):
            self.assertEqual(hashlib.sha256((FIXTURES / entry["fixture"]).read_bytes()).hexdigest(), entry["sha256"])
        self.assertTrue(evaluate(FIXTURES)["passed"])
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            shutil.copytree(FIXTURES, root / "captures")
            shutil.copyfile(FIXTURES / "historical-wifi-seam.png", root / "captures/settings-wifi-pressed.png")
            report = evaluate(root / "captures")
            self.assertFalse(report["passed"])
            self.assertFalse(report["captures"]["settings-wifi"]["seam_boundaries"]["passed"])

    def test_wrong_dimensions_and_lossy_capture_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "capture.png"
            Image.new("RGB", (465, 466)).save(path)
            with self.assertRaises(ValueError):
                load_capture(path)
            Image.new("RGB", (466, 466)).save(path, format="JPEG")
            with self.assertRaises(ValueError):
                load_capture(path)

    def test_overlapping_masks_count_pixels_once(self):
        image = Image.new("RGB", (466, 466))
        result = compare_outside(image, image, [(0, 0, 10, 10), (0, 0, 5, 5)])
        self.assertEqual(result["compared_pixels"], 466 * 466 - 100)


if __name__ == "__main__":
    unittest.main()
