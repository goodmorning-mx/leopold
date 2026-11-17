import json
import math
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class ComputerControlsContractTest(unittest.TestCase):
    def test_controls_and_status_fit_round_framebuffer(self):
        contract = json.loads((ROOT / "shared/firmware-ui.json").read_text())
        screen = next(item for item in contract["screens"] if item["id"] == "computer-controls")
        status = next(item for item in screen["components"] if item["id"] == "computer-status")
        self.assertEqual(status["text"], "Not connected")
        controls = [item for item in screen["components"] if item.get("id", "").startswith("computer-") and item["type"] == "button"]
        self.assertEqual({item["id"] for item in controls}, {
            "computer-volume-down", "computer-volume-mute", "computer-volume-up", "computer-screenshot"
        })
        for control in controls:
            for x in (control["x"], control["x"] + control["width"]):
                for y in (control["y"], control["y"] + control["height"]):
                    self.assertLessEqual(math.hypot(x - 233, y - 233), 233)

    def test_shared_lvgl_uses_home_glass_for_computer_capsules(self):
        renderer = (ROOT / "shared/lvgl_renderer.cpp").read_text()
        self.assertIn('const bool computerControls = strcmp(screen.screenId, "computer-controls") == 0;', renderer)
        self.assertIn('(home || computerControls)\n          ? makeHomeButton(root, component, selectedRowId)', renderer)

    def test_firmware_routes_volume_and_mute_through_usb_consumer_control(self):
        firmware = (ROOT / "device/src/main.cpp").read_text()
        action = firmware[firmware.index("void sendComputerControlOverUsb") : firmware.index("const SduiComponent* firstButton")]
        self.assertIn("sendComputerControl(componentId)", action)
        self.assertIn("CAPTURE unavailable", action)
        self.assertNotIn("LEOPOLD_CMD:", action)
        transport = (ROOT / "device/src/computer_control.cpp").read_text()
        self.assertIn("tud_mounted()", transport)
        self.assertIn("tud_hid_n_ready(0)", transport)


if __name__ == "__main__":
    unittest.main()
