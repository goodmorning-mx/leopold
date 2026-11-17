#pragma once

namespace leopold::identity {

// Seeds the existing provisioned identity once, then always uses the NVS copy.
// OTA application images cannot replace an already provisioned device secret.
bool begin();
const char* deviceId();
const char* deviceSecret();

}  // namespace leopold::identity
