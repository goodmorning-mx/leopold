"""Compile the production CloudClient header and lifecycle methods with RTOS stubs."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "device/src/cloud_client.cpp"
BASELINE = "4c6210a208d0fe11c9b7a4b6707b633feaebe2eb"


ARDUINO = r'''
#pragma once
#include <stddef.h>
#include <stdint.h>
struct Queue;
using QueueHandle_t = Queue*;
using TaskHandle_t = void*;
'''

HARNESS = r'''
#include "cloud_client.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct Queue {
  int depth;
  size_t itemSize;
  std::vector<std::vector<unsigned char>> items;
};
static std::vector<Queue*> queues;
static int allocations, deletions, taskAttempts, workers, failure;
static QueueHandle_t requestQueue, responseQueue;
constexpr int pdPASS = 1, pdTRUE = 1;

QueueHandle_t xQueueCreate(int depth, size_t itemSize) {
  if (++allocations == failure && failure <= 2) return nullptr;
  auto* queue = new Queue{depth, itemSize, {}};
  queues.push_back(queue);
  return queue;
}

void vQueueDelete(QueueHandle_t queue) {
  const auto found = std::find(queues.begin(), queues.end(), queue);
  assert(found != queues.end() && workers == 0);
  queues.erase(found);
  delete queue;
  ++deletions;
}

int xTaskCreatePinnedToCore(void (*entry)(void*), const char* name, int stack,
                           void* context, int priority, TaskHandle_t* out, int core) {
  assert(entry && context && queues.size() == 2);
  assert(std::strcmp(name, "leopold-cloud") == 0 && stack == 6144 && priority == 1 && core == 0);
  assert(queues[0]->depth == 6 && queues[1]->depth == 4);
  ++taskAttempts;
  if (failure == 3) return 0;
  ++workers;
  requestQueue = queues[0];
  responseQueue = queues[1];
  *out = context;
  return pdPASS;
}

int xQueueSend(QueueHandle_t queue, const void* item, int ticks) {
  assert(std::find(queues.begin(), queues.end(), queue) != queues.end() && ticks == 0);
  if (queue->items.size() == static_cast<size_t>(queue->depth)) return 0;
  const auto* bytes = static_cast<const unsigned char*>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return pdTRUE;
}

int xQueueReceive(QueueHandle_t queue, void* item, int ticks) {
  assert(std::find(queues.begin(), queues.end(), queue) != queues.end() && ticks == 0);
  if (queue->items.empty()) return 0;
  std::memcpy(item, queue->items.front().data(), queue->itemSize);
  queue->items.erase(queue->items.begin());
  return pdTRUE;
}

static size_t fixtureStrlcpy(char* out, const char* input, size_t size) {
  return std::snprintf(out, size, "%s", input);
}

#define strlcpy fixtureStrlcpy

namespace leopold {
namespace config { constexpr char kFirmwareVersion[] = "test"; }
void CloudClient::taskEntry(void*) {}
'''

CASES = r'''
}  // namespace leopold

using namespace leopold;

static void assertUnready(CloudClient& client) {
  CloudResponse response;
  assert(!client.requestConnect(nullptr));
  assert(!client.requestOnboardingStatus());
  assert(!client.requestUi(nullptr));
  assert(!client.requestAsset("fixture"));
  assert(!client.requestHeartbeat(-45, 90, 12));
  assert(!client.sendEvent("{}"));
  assert(!client.requestFirmwareUpdate());
  assert(!client.poll(response));
}

int main(int argc, char** argv) {
  assert(argc == 2);
  CloudClient client;
  failure = std::atoi(argv[1]);
  assertUnready(client);
  if (failure) {
    assert(!client.begin());
#ifdef BASELINE_PROBE
    // The old task failure admits undeliverable work and reports false readiness.
    assert(failure == 3 && queues.size() == 2 && workers == 0);
    assert(client.requestConnect(nullptr));
    assert(client.begin() && taskAttempts == 1 && workers == 0);
    return 0;
#endif
    assertUnready(client);
    assert(allocations == (failure == 1 ? 1 : 2));
    assert(deletions == failure - 1 && queues.empty() && workers == 0);
    assert(taskAttempts == (failure == 3 ? 1 : 0));
  }

  failure = 0;
  const int priorAllocations = allocations;
  const int priorAttempts = taskAttempts;
  assert(client.begin() && client.begin());
  assert(allocations == priorAllocations + 2 && taskAttempts == priorAttempts + 1);
  assert(queues.size() == 2 && workers == 1);

  assert(client.requestConnect(nullptr));
  assert(client.requestOnboardingStatus());
  assert(client.requestUi("fixture-etag"));
  assert(client.requestAsset("fixture"));
  assert(client.requestHeartbeat(-45, 90, 12));
  assert(client.sendEvent("{}"));
  assert(!client.requestFirmwareUpdate());  // Depth six, without a consumer.
  assert(requestQueue->items.size() == 6);
  requestQueue->items.clear();  // Stand in for the worker draining queued requests.
  assert(client.requestFirmwareUpdate());

  CloudResponse response;
  assert(!client.poll(response));
  CloudResponse published;
  published.operation = CloudOperation::kFirmwareUpdate;
  published.httpCode = 204;
  assert(xQueueSend(responseQueue, &published, 0) == pdTRUE);
  assert(client.poll(response) && response.operation == published.operation && response.httpCode == 204);
  assert(!client.poll(response));
  // The real CloudClient lives for the firmware lifetime.
  for (Queue* queue : queues) delete queue;
}
'''


class CloudClientInitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("c++")
        if not cls.compiler:
            raise unittest.SkipTest("host C++ compiler unavailable")
        temp = tempfile.TemporaryDirectory(prefix="cloud-client-init-")
        cls.addClassCleanup(temp.cleanup)
        cls.directory = Path(temp.name)
        (cls.directory / "Arduino.h").write_text(ARDUINO)
        cls.binary = cls.compile_fixture(SOURCE.read_text(), "lifecycle")

    @classmethod
    def compile_fixture(cls, source, name, *flags):
        # This contiguous span contains begin(), every request builder, poll(),
        # and enqueue(). The network implementation and worker are outside it.
        methods = source[source.index("bool CloudClient::begin()"):
                         source.index("void CloudClient::taskEntry(")]
        harness = cls.directory / (name + ".cpp")
        binary = cls.directory / name
        harness.write_text(HARNESS + methods + CASES)
        result = subprocess.run([
            cls.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
            "-I", str(cls.directory), "-I", str(ROOT / "device/include"),
            str(harness), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise AssertionError(result.stderr)
        return binary

    def run_case(self, scenario, binary=None):
        result = subprocess.run([str(binary or self.binary), str(scenario)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_success_is_idempotent(self):
        self.run_case(0)

    def test_first_queue_failure_and_retry(self):
        self.run_case(1)

    def test_response_queue_failure_and_retry(self):
        self.run_case(2)

    def test_task_creation_failure_and_retry(self):
        self.run_case(3)

    def test_historical_task_failure_reproduces_false_readiness(self):
        source = subprocess.run(
            ["git", "show", BASELINE + ":Code/device/src/cloud_client.cpp"],
            cwd=ROOT, capture_output=True, text=True, timeout=10,
        )
        if source.returncode:
            self.skipTest("historical baseline object unavailable")
        self.run_case(3, self.compile_fixture(source.stdout, "baseline", "-DBASELINE_PROBE"))
