"""Exercise the time portion of refreshIndicators with a deterministic host clock."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


MAIN = Path(__file__).resolve().parents[2] / "device/src/main.cpp"

HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

constexpr uint32_t kTimeSyncRetryMs = 10000;
static uint32_t lastTimeSyncAttemptAt;
static unsigned attempts, waits;
static char rendered[9];
static struct {
  bool ready = false;
  const char* clock = "12:15";
  bool timeSynchronized() const { return ready; }
  void formattedTime(char* out) const { std::strcpy(out, ready ? clock : "--:--"); }
} telemetry;

void configTzTime(const char* zone, const char* first, const char* second, const char* third) {
  assert(std::strcmp(zone, "CST6") == 0);
  assert(std::strcmp(first, "pool.ntp.org") == 0);
  assert(std::strcmp(second, "time.google.com") == 0);
  assert(std::strcmp(third, "time.cloudflare.com") == 0);
  ++attempts;
}

[[maybe_unused]] bool getLocalTime(tm*, uint32_t timeout) {
  assert(timeout == 1000);
  ++waits;
  return false;
}

[[maybe_unused]] static struct {
  void println(const char*) {}
} Serial;
'''

CASES = r'''
  std::strcpy(rendered, timeText);
}

int main() {
  tick(1000, false);
  assert(attempts == 0 && std::strcmp(rendered, "--:--") == 0);
  tick(9999, true);
  assert(attempts == 0);
  tick(10000, true);
  if (waits) {
    std::fputs("indicator tick attempted a blocking clock wait\n", stderr);
    return 1;
  }
  assert(attempts == 1 && std::strcmp(rendered, "--:--") == 0);
  tick(19999, true);
  assert(attempts == 1);
  tick(20000, false);
  assert(attempts == 1 && std::strcmp(rendered, "--:--") == 0);
  tick(20001, true);
  assert(attempts == 2 && waits == 0);
  telemetry.ready = true;
  tick(21000, true);
  assert(attempts == 2 && std::strcmp(rendered, "12:15") == 0);
  telemetry.clock = "12:16";
  tick(30000, false);
  tick(31000, true);
  assert(attempts == 2 && std::strcmp(rendered, "12:16") == 0);
}
'''


class IndicatorTimeRetryTests(unittest.TestCase):
    def test_retry_never_waits_and_render_uses_available_time(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("host C++ compiler unavailable")
        source = MAIN.read_text()
        refresh = source.index("void refreshIndicators(uint32_t now) {")
        start = source.index("  if (wifiConnected && !telemetry.timeSynchronized()", refresh)
        end = source.index("  screen.drawIndicators(", start)
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory) / "clock.cpp"
            executable = fixture.with_suffix("")
            fixture.write_text(HARNESS + "\nvoid tick(uint32_t now, bool wifiConnected) {\n" + source[start:end] + CASES)
            subprocess.run([compiler, "-std=c++17", str(fixture), "-o", str(executable)], check=True, timeout=60)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
