// Arduino CLI does not select QRCode@0.0.1 automatically because the ESP32
// platform exposes another qrcode.h. Compile the pinned library source once
// so the existing DisplayManager QR API links without changing behavior.
extern "C" {
#include "qrcode.c"
}
