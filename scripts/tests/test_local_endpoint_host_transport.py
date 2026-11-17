import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SHIMS = pathlib.Path(__file__).resolve().parent / "host_shims"


class LocalEndpointHostTransportTests(unittest.TestCase):
    def compile_and_run(self, name, sources, defines=(), historical_four_result=False):
        with tempfile.TemporaryDirectory(prefix="leopold-local-endpoint-") as directory:
            temp_dir = pathlib.Path(directory)
            executable = temp_dir / name
            resolved_sources = []
            for source in sources:
                source_path = ROOT / source
                if historical_four_result and source == "device/src/local_endpoint_transport.cpp":
                    original = source_path.read_text()
                    old = "MDNS_TYPE_A, kResolveDeadlineMs, 1, nullptr)"
                    if original.count(old) != 1:
                        self.fail("expected exactly one production mDNS max_results=1 call")
                    historical = temp_dir / "local_endpoint_transport_historical.cpp"
                    historical.write_text(original.replace(old, "MDNS_TYPE_A, kResolveDeadlineMs, 4, nullptr)"))
                    resolved_sources.append(str(historical))
                else:
                    resolved_sources.append(str(source_path))
            command = [
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DLEOPOLD_LOCAL_ENDPOINT_HOST_TEST",
                *[f"-D{define}" for define in defines],
                f"-I{SHIMS}", f"-I{ROOT / 'device/include'}",
                *resolved_sources,
                "-o", str(executable),
            ]
            subprocess.run(command, check=True, cwd=ROOT)
            subprocess.run([str(executable)], check=True, cwd=ROOT)

    def test_project_lifecycle_and_pinned_compatible_http_transport(self):
        self.compile_and_run(
            "lifecycle",
            ["scripts/tests/test_local_endpoint_policy.cpp"],
        )
        self.compile_and_run(
            "transport",
            [
                "device/src/local_endpoint_transport.cpp",
                "scripts/tests/test_local_endpoint_host_transport.cpp",
            ],
        )
        lifecycle_sources = [
            "device/src/local_endpoint_transport.cpp",
            "scripts/tests/test_local_endpoint_query_lifecycle.cpp",
        ]
        self.compile_and_run(
            "single-result-query",
            lifecycle_sources,
            ["LEOPOLD_EXPECTED_MDNS_MAX_RESULTS=1"],
        )
        self.compile_and_run(
            "historical-four-result-query",
            lifecycle_sources,
            ["LEOPOLD_EXPECTED_MDNS_MAX_RESULTS=4"],
            historical_four_result=True,
        )


if __name__ == "__main__":
    unittest.main()
