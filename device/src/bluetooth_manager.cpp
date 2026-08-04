#include "bluetooth_manager.h"

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
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
volatile bool scanInProgress = false;
BLEScan* scanner = nullptr;
BLEClient* peripheralClient = nullptr;
BluetoothScanResult scanResults[kMaxBluetoothScanResults];
volatile uint8_t foundCount = 0;
char peripheralName[33] = {};

class ScanCallbacks final : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertised) override {
    const String address(advertised.getAddress().toString().c_str());
    for (uint8_t index = 0; index < foundCount; ++index) {
      if (address.equalsIgnoreCase(scanResults[index].address)) {
        scanResults[index].rssi = advertised.getRSSI();
        return;
      }
    }
    if (foundCount >= kMaxBluetoothScanResults) {
      return;
    }
    const uint8_t index = foundCount;
    foundCount = index + 1;
    strlcpy(scanResults[index].address, address.c_str(), sizeof(scanResults[index].address));
    const String advertisedName = advertised.getName();
    strlcpy(
        scanResults[index].name,
        advertisedName.isEmpty() ? "Dispositivo BLE" : advertisedName.c_str(),
        sizeof(scanResults[index].name));
    scanResults[index].rssi = advertised.getRSSI();
  }
};

void scanComplete(BLEScanResults) {
  scanInProgress = false;
}

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
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new ScanCallbacks(), false, true);
  scanner->setActiveScan(true);
  scanner->setInterval(96);
  scanner->setWindow(48);
  enabled_ = true;
  Serial.printf("Bluetooth LE advertising as %s\n", advertisedName);
  return true;
}

void BluetoothManager::loop() {
  if (enabled_ && restartAdvertising && server != nullptr) {
    restartAdvertising = false;
    server->startAdvertising();
  }
  if (peripheralClient != nullptr && !peripheralClient->isConnected()) {
    delete peripheralClient;
    peripheralClient = nullptr;
    peripheralName[0] = '\0';
  }
}

bool BluetoothManager::enabled() const {
  return enabled_;
}

bool BluetoothManager::connected() const {
  return bleConnected ||
         (peripheralClient != nullptr && peripheralClient->isConnected());
}

bool BluetoothManager::startScan() {
  if (!enabled_ || scanner == nullptr || scanInProgress) {
    return false;
  }
  scanner->clearResults();
  memset(scanResults, 0, sizeof(scanResults));
  foundCount = 0;
  scanInProgress = scanner->start(4, scanComplete, false);
  return scanInProgress;
}

bool BluetoothManager::scanning() const {
  return scanInProgress;
}

uint8_t BluetoothManager::scanResultCount() const {
  return foundCount;
}

const BluetoothScanResult& BluetoothManager::scanResult(uint8_t index) const {
  static BluetoothScanResult empty;
  return index < foundCount ? scanResults[index] : empty;
}

bool BluetoothManager::connectTo(uint8_t index) {
  if (index >= foundCount || scanInProgress) {
    return false;
  }
  if (peripheralClient != nullptr) {
    if (peripheralClient->isConnected()) {
      peripheralClient->disconnect();
    }
    delete peripheralClient;
  }
  peripheralClient = BLEDevice::createClient();
  if (peripheralClient == nullptr ||
      !peripheralClient->connect(BLEAddress(scanResults[index].address))) {
    delete peripheralClient;
    peripheralClient = nullptr;
    return false;
  }
  strlcpy(peripheralName, scanResults[index].name, sizeof(peripheralName));
  return true;
}

const char* BluetoothManager::connectedPeripheralName() const {
  return peripheralName;
}

}  // namespace leopold
