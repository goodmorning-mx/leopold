"""Source-level guard for the local-first startup sequencing contract."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT / "device/src/main.cpp").read_text()
CACHE = (ROOT / "device/src/persistent_asset_cache.cpp").read_text()
STORE = (ROOT / "device/src/sdui.cpp").read_text()
DISPLAY = (ROOT / "device/src/display_manager.cpp").read_text()


class StartupOfflineContractTests(unittest.TestCase):
    def test_first_home_flush_precedes_optional_services(self):
        setup = MAIN.split("void setup() {", 1)[1].split("namespace {\nvoid startupServicesTask", 1)[0]
        order = [setup.index(token) for token in (
            "screen.begin()", "loadEmbeddedHomeAsset()", "syncControlCenterScreen()",
            "screen.renderSduiScreen(currentUi)", "screen.loop();", "touch.begin()",
            "startupPending = true;")]
        self.assertEqual(order, sorted(order))
        self.assertNotIn("identity::begin()", setup)
        self.assertNotIn("uiStore.begin()", setup)
        self.assertNotIn("cloud.begin()", setup)
        self.assertNotIn("wifi.begin(", setup)
        self.assertIn('xTaskCreatePinnedToCore(\n      startupServicesTask, "leopold-startup", 8192', MAIN)

    def test_optional_cache_has_bounded_heap_scratch_and_preserves_corrupt_data(self):
        self.assertIn("constexpr size_t kIoChunkBytes = 1024", (ROOT / "device/include/persistent_asset_cache.h").read_text())
        self.assertIn("heap_caps_malloc(kIoChunkBytes", CACHE)
        self.assertNotIn("uint8_t buffer[kIoChunkBytes]", CACHE)
        startup_cache = CACHE.split("bool begin()", 1)[1].split("bool mounted()", 1)[0]
        self.assertNotIn("LittleFS.remove", startup_cache)

    def test_local_mode_skips_optional_sd_and_protects_local_navigation(self):
        self.assertIn("#if !defined(LEOPOLD_LOCAL_DEVELOPMENT)\n  mountSdCard();", STORE)
        self.assertIn("#if defined(LEOPOLD_LOCAL_DEVELOPMENT)\n      hasUi ||", MAIN)
        self.assertIn("Remote SDUI ignored in local-development mode", MAIN)
        self.assertIn("Cloud welcome UI ignored in local-development mode", MAIN)
        self.assertIn("local UI remains active", MAIN)

    def test_app_console_diagnostics_are_retrievable_without_claiming_panel_success(self):
        setup = MAIN.split("void setup() {", 1)[1].split("namespace {\nvoid startupServicesTask", 1)[0]
        self.assertLess(setup.index("Serial.enableReboot(false)"), setup.index("Serial.begin(115200)"))
        self.assertIn('strcmp(usbCommand, "LEOPOLD_DIAGNOSTICS") == 0', MAIN)
        self.assertIn("if (Serial && !usbDiagnosticsAnnounced)", MAIN)
        self.assertIn("LEOPOLD_DIAGNOSTICS build=%s commit=%s", MAIN)
        self.assertIn("home_flush_callback_observed", MAIN)
        self.assertIn("driver_return_feedback=none", MAIN)
        self.assertNotIn('stage=home_flushed', MAIN)
        flush = DISPLAY.split("void lvglFlush(", 1)[1].split("const lv_font_t*", 1)[0]
        self.assertLess(flush.index("draw16bitRGBBitmap"), flush.index("++lvglFlushCallbacks"))


if __name__ == "__main__":
    unittest.main()
