from __future__ import annotations

from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest

from scripts.build_mode import (
    append_compile_defines,
    private_usb_environment,
    resolve_build_mode,
    validate_nonproduction_api_origin,
)


ROOT = Path(__file__).resolve().parents[2]
OTA_POLL_GUARD = "#if !defined(LEOPOLD_LOCAL_DEVELOPMENT) || defined(LEOPOLD_OTA_TEST_MODE)"


class PrivateBuildModeTests(unittest.TestCase):
    def test_default_usb_mode_retains_local_ui_and_omits_ota_polling(self) -> None:
        environment = private_usb_environment(
            {"LEOPOLD_OTA_TEST_MODE": "1", "EXAMPLE": "kept"}, False
        )
        mode = resolve_build_mode(environment)

        self.assertEqual(environment["LEOPOLD_LOCAL_DEVELOPMENT"], "1")
        self.assertNotIn("LEOPOLD_OTA_TEST_MODE", environment)
        self.assertEqual(environment["EXAMPLE"], "kept")
        self.assertEqual(mode["compile_defines"], ["LEOPOLD_LOCAL_DEVELOPMENT=1"])
        self.assertEqual(
            mode["build_identity"],
            {
                "local_development": True,
                "ota_test_mode": False,
                "ota_polling_enabled": False,
                "runtime_mode": "local-development",
            },
        )

    def test_opt_in_reaches_compiler_and_changes_build_identity(self) -> None:
        environment = private_usb_environment({}, True)
        mode = resolve_build_mode(environment)

        class FakePlatformIOEnvironment:
            def __init__(self) -> None:
                self.defines: list[str] = []

            def Append(self, **kwargs: object) -> None:
                self.defines.extend(kwargs["CPPDEFINES"])

        platform_env = FakePlatformIOEnvironment()
        append_compile_defines(platform_env, mode)
        self.assertEqual(environment["LEOPOLD_LOCAL_DEVELOPMENT"], "1")
        self.assertEqual(environment["LEOPOLD_OTA_TEST_MODE"], "1")
        self.assertEqual(
            platform_env.defines,
            ["LEOPOLD_LOCAL_DEVELOPMENT=1", "LEOPOLD_OTA_TEST_MODE=1"],
        )
        self.assertEqual(
            mode["build_identity"],
            {
                "local_development": True,
                "ota_test_mode": True,
                "ota_polling_enabled": True,
                "runtime_mode": "local-development-ota-test",
            },
        )
        self.assertNotEqual(
            resolve_build_mode(environment)["runtime_mode"],
            resolve_build_mode({"LEOPOLD_LOCAL_DEVELOPMENT": "1"})["runtime_mode"],
        )

    def test_normal_runtime_still_enables_polling(self) -> None:
        mode = resolve_build_mode({})
        self.assertTrue(mode["ota_polling_enabled"])
        self.assertEqual(mode["compile_defines"], [])
        self.assertEqual(mode["runtime_mode"], "standard")

    def test_ota_test_mode_cannot_be_used_without_local_ui_guards(self) -> None:
        with self.assertRaisesRegex(ValueError, "requires the local-development"):
            resolve_build_mode({"LEOPOLD_OTA_TEST_MODE": "1"})

    def test_ota_test_mode_requires_a_nonproduction_https_origin(self) -> None:
        validate_nonproduction_api_origin(
            '#define LEOPOLD_API_BASE_URL "https://api.dev.example.test"\n'
        )
        for origin in (
            "https://api.example.test",
            "http://api.dev.example.test",
            "https://api.dev.example.test/v1",
            "https://api.dev.example.test/",
            "https://api.dev.example.test:443",
            "https://user@api.dev.example.test",
            "https://api.dev.example.test?route=/health",
            "https://api.dev.example.test ",
        ):
            with self.subTest(origin_class=origin.split(":", 1)[0]):
                with self.assertRaises(ValueError):
                    validate_nonproduction_api_origin(
                        f'#define LEOPOLD_API_BASE_URL "{origin}"\n'
                    )

    def test_local_ota_endpoint_requires_explicit_flag_and_private_https_ipv4(self) -> None:
        local = '#define LEOPOLD_API_BASE_URL "https://192.168.68.151:8443"\n'
        with self.assertRaises(ValueError):
            validate_nonproduction_api_origin(local)
        validate_nonproduction_api_origin(local, allow_local_endpoint=True)
        for origin in (
            "http://192.168.68.151:8443",
            "https://192.168.68.151",
            "https://127.0.0.1:8443",
            "https://8.8.8.8:8443",
            "https://192.168.68.151:443",
            "https://user@192.168.68.151:8443",
            "https://192.168.68.151:8443/v1",
        ):
            with self.subTest(origin=origin):
                with self.assertRaises(ValueError):
                    validate_nonproduction_api_origin(
                        f'#define LEOPOLD_API_BASE_URL "{origin}"\n',
                        allow_local_endpoint=True,
                    )

    def test_local_endpoint_accepts_only_valid_dot_local_dns_in_explicit_mode(self) -> None:
        origin = '#define LEOPOLD_API_BASE_URL "https://leopold-dev.local:8443"\n'
        with self.assertRaises(ValueError):
            validate_nonproduction_api_origin(origin)
        validate_nonproduction_api_origin(origin, allow_local_endpoint=True)
        for host in ("-bad.local", "bad-.local", "bad..local", "bad_local.local",
                     "bad.local.evil", "bad.local:443", "bad.local/path"):
            candidate = f'#define LEOPOLD_API_BASE_URL "https://{host}:8443"\n'
            with self.subTest(host=host), self.assertRaises(ValueError):
                validate_nonproduction_api_origin(candidate, allow_local_endpoint=True)

    def test_local_endpoint_mode_requires_explicit_ota_test_mode(self) -> None:
        with self.assertRaisesRegex(ValueError, "requires OTA test mode"):
            resolve_build_mode({"LEOPOLD_LOCAL_OTA_TEST_ENDPOINT": "1"})

    def test_local_endpoint_opt_in_is_explicit_and_embedded_in_identity(self) -> None:
        environment = private_usb_environment({}, True, True)
        mode = resolve_build_mode(environment)
        self.assertEqual(environment["LEOPOLD_LOCAL_OTA_TEST_ENDPOINT"], "1")
        self.assertTrue(mode["local_ota_test_endpoint"])
        self.assertTrue(mode["ota_polling_enabled"])
        self.assertEqual(mode["build_identity"]["local_ota_test_endpoint"], True)


class MainFirmwarePollGuardTests(unittest.TestCase):
    def test_ota_runtime_identity_payload_is_complete_and_bounded(self) -> None:
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "a host C++ compiler is required for this source test")
        header = ROOT / "device/include/ota_test_connect_payload.h"
        harness_source = f'''#include <cstdio>
#include <cstring>
#include "{header}"
int main() {{
  char payload[640] = {{}};
  if (!leopold::ota_test::buildConnectPayload(payload, sizeof(payload), "0.3.0",
      nullptr, "A", "0123456789abcdef0123456789abcdef01234567", "0123456789abcdef",
      65536, "ota_0")) return 1;
  std::printf("%zu %s\\n", std::strlen(payload), payload);
  const char* token = "setup-\\\"token\\\\value-012345678901234567890123";
  if (!leopold::ota_test::buildConnectPayload(payload, sizeof(payload), "0.3.0",
      token, "B", "89abcdef0123456789abcdef0123456789abcdef", "fedcba9876543210",
      6750208, "ota_1")) return 2;
  std::printf("%zu %s\\n", std::strlen(payload), payload);
  char tooSmall[16] = {{}};
  return leopold::ota_test::buildConnectPayload(tooSmall, sizeof(tooSmall), "0.3.0",
      nullptr, "A", "0123456789abcdef0123456789abcdef01234567", "0123456789abcdef",
      65536, "ota_0") ? 3 : 0;
}}
'''
        with tempfile.TemporaryDirectory() as temporary:
            harness = Path(temporary) / "ota_identity.cpp"
            binary = Path(temporary) / "ota_identity"
            harness.write_text(harness_source)
            subprocess.run(
                [compiler, "-std=c++17", str(harness), "-o", str(binary)],
                check=True,
                capture_output=True,
                text=True,
            )
            lines = subprocess.run(
                [str(binary)], check=True, capture_output=True, text=True
            ).stdout.splitlines()
        self.assertEqual(len(lines), 2)
        payloads = []
        for line in lines:
            claimed_length, payload = line.split(" ", 1)
            self.assertEqual(int(claimed_length), len(payload.encode("utf-8")))
            self.assertLess(int(claimed_length), 640)
            payloads.append(json.loads(payload))
        self.assertNotIn("setup_token", payloads[0])
        self.assertEqual(payloads[0]["capabilities"]["ota_test_stage_marker"], "A")
        self.assertEqual(payloads[0]["capabilities"]["ota_test_running_partition_label"], "ota_0")
        self.assertEqual(payloads[0]["capabilities"]["ota_test_running_partition_address"], 65536)
        self.assertEqual(payloads[1]["setup_token"], 'setup-"token\\value-012345678901234567890123')
        self.assertEqual(payloads[1]["capabilities"]["ota_test_stage_marker"], "B")
        self.assertEqual(payloads[1]["capabilities"]["ota_test_running_partition_label"], "ota_1")

    def test_actual_poll_schedule_compiles_for_default_opt_in_and_standard_modes(self) -> None:
        compiler = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("No host C++ compiler is available")

        source = (ROOT / "device/src/main.cpp").read_text()
        match = re.search(
            rf"(?m)^{re.escape(OTA_POLL_GUARD)}\s*(.*?)^#endif\s*$",
            source,
            re.S | re.M,
        )
        self.assertIsNotNone(match, "main.cpp must retain the reviewed OTA poll guard")
        block = match.group(0)
        self.assertIn("cloud.requestFirmwareUpdate()", block)
        self.assertIn("nextFirmwareCheckAt", block)

        prefix = """#include <cstdint>
int pollCalls = 0;
bool cloudConnected = true;
bool onboardingPending = false;
bool firmwareUpdatePending = false;
std::uint8_t firmwareUpdateFailures = 0;
std::uint32_t now = 1;
std::uint32_t nextFirmwareCheckAt = 0;
std::uint32_t retryDelayMs(std::uint8_t) { return 5; }
template <typename T> T min(T a, T b) { return a < b ? a : b; }
struct Cloud {
  bool requestFirmwareUpdate() { ++pollCalls; return false; }
} cloud;
void scheduleFirmwarePoll() {
"""
        suffix = """
}
int main() {
  scheduleFirmwarePoll();
  return pollCalls == EXPECTED_POLL_CALLS ? 0 : 1;
}
"""
        modes = (
            ("default_local", ["-DLEOPOLD_LOCAL_DEVELOPMENT=1"], 0),
            (
                "ota_test_local",
                ["-DLEOPOLD_LOCAL_DEVELOPMENT=1", "-DLEOPOLD_OTA_TEST_MODE=1"],
                1,
            ),
            ("standard", [], 1),
        )
        with tempfile.TemporaryDirectory() as temporary:
            temp = Path(temporary)
            for name, defines, expected in modes:
                with self.subTest(mode=name):
                    harness = temp / f"{name}.cpp"
                    binary = temp / name
                    harness.write_text(
                        prefix
                        + block
                        + suffix.replace("EXPECTED_POLL_CALLS", str(expected))
                    )
                    subprocess.run(
                        [compiler, "-std=c++17", *defines, str(harness), "-o", str(binary)],
                        check=True,
                        capture_output=True,
                        text=True,
                    )
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
