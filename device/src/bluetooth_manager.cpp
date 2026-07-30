#include "bluetooth_manager.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace leopold {
namespace {

constexpr char kIdentityServiceUuid[] =
    "7c1f0001-6e65-4f70-6f6c-640000000001";
constexpr char kDeviceIdCharacteristicUuid[] =
    "7c1f0002-6e65-4f70-6f6c-640000000001";

BLEServer* server = nullptr;
volatile bool bleConnected = false;
volatile bool restartAdvertising = false;

class ServerCallbacks final : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleConnected = true;
  }

  void onDisconnect(BLEServer*) override {
    bleConnected = false;
    restartAdvertising = true;
  }
};

}  // namespace

bool BluetoothManager::begin(
    const char* advertisedName,
    const char* deviceId) {
  if (advertisedName == nullptr || deviceId == nullptr) {
    return false;
  }
  BLEDevice::init(advertisedName);
  server = BLEDevice::createServer();
  if (server == nullptr) {
    return false;
  }
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(kIdentityServiceUuid);
  BLECharacteristic* identity = service->createCharacteristic(
      kDeviceIdCharacteristicUuid,
      BLECharacteristic::PROPERTY_READ);
  identity->setValue(deviceId);
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kIdentityServiceUuid);
  advertising->setScanResponse(true);
  advertising->start();
  enabled_ = true;
  Serial.printf("Bluetooth LE advertising as %s\n", advertisedName);
  return true;
}

void BluetoothManager::loop() {
  if (enabled_ && restartAdvertising && server != nullptr) {
    restartAdvertising = false;
    server->startAdvertising();
  }
}

bool BluetoothManager::enabled() const {
  return enabled_;
}

bool BluetoothManager::connected() const {
  return bleConnected;
}

}  // namespace leopold
