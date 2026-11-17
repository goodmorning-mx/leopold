"""Render the real shared LVGL screen twice to catch stale-pixel compositing."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DisplayRepaintTests(unittest.TestCase):
    def test_transparent_wallpapers_do_not_accumulate_previous_menu_pixels(self):
        node = shutil.which("node")
        wasm = ROOT / "frontend/public/wasm/lvgl.wasm"
        if node is None or not wasm.exists():
            if os.environ.get("LEOPOLD_REQUIRE_DISPLAY_WASM") == "1":
                self.fail("Rebuilt WASM and Node are required")
            self.skipTest("Rebuilt WASM and Node unavailable")
        result = subprocess.run(
            [str(node), str(Path(__file__).with_name("display_repaint.mjs")), str(ROOT)],
            check=True, capture_output=True, text=True, timeout=60,
        )
        states = json.loads(result.stdout)
        self.assertEqual([state["background"] for state in states], [0, 1, 2, 3])
        for state in states:
            with self.subTest(background=state["background"]):
                self.assertEqual(state["changed_pixels_on_identical_redraw"], 0)


if __name__ == "__main__":
    unittest.main()
