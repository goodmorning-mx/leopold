#include "computer_control_protocol.h"

#include <cassert>
#include <cstdint>
#include <vector>

class FakeSink final : public leopold::ConsumerControlSink {
 public:
  bool ready = true;
  bool pressResult = true;
  bool releaseResult = true;
  std::vector<uint16_t> reports;

  bool connected() const override { return ready; }
  bool press(uint16_t usage) override {
    reports.push_back(usage);
    return pressResult;
  }
  bool release() override {
    reports.push_back(0);
    return releaseResult;
  }
};

int main() {
  FakeSink sink;
  assert(leopold::sendComputerControlTap(sink, "computer-volume-down"));
  assert((sink.reports == std::vector<uint16_t>{0x00EA, 0}));

  sink.reports.clear();
  assert(leopold::sendComputerControlTap(sink, "computer-volume-mute"));
  assert((sink.reports == std::vector<uint16_t>{0x00E2, 0}));

  sink.reports.clear();
  assert(leopold::sendComputerControlTap(sink, "computer-volume-up"));
  assert((sink.reports == std::vector<uint16_t>{0x00E9, 0}));

  sink.reports.clear();
  sink.ready = false;
  assert(!leopold::sendComputerControlTap(sink, "computer-volume-up"));
  assert(sink.reports.empty());

  sink.ready = true;
  assert(!leopold::sendComputerControlTap(sink, "computer-screenshot"));
  assert(sink.reports.empty());

  sink.pressResult = false;
  assert(!leopold::sendComputerControlTap(sink, "computer-volume-up"));
  assert((sink.reports == std::vector<uint16_t>{0x00E9, 0}));
}
