#include "computer_control.h"
#include "computer_control_protocol.h"

#include <Arduino.h>
#include <USB.h>
#include <USBHIDConsumerControl.h>
#include <tusb.h>

namespace leopold {
namespace {

USBHIDConsumerControl consumerControl;
bool initialized = false;

class UsbConsumerControlSink final : public ConsumerControlSink {
 public:
  bool connected() const override {
    return initialized && tud_mounted() && tud_hid_n_ready(0);
  }

  bool press(uint16_t usage) override {
    return consumerControl.press(usage) > 0;
  }

  bool release() override { return consumerControl.release() > 0; }
};

UsbConsumerControlSink sink;

}  // namespace

void beginComputerControl() {
  consumerControl.begin();
  USB.begin();
  initialized = true;
}

bool computerControlConnected() {
  return sink.connected();
}

bool sendComputerControl(const char* componentId) {
  return sendComputerControlTap(sink, componentId);
}

}  // namespace leopold
