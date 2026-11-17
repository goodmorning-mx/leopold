#include "device_identity.h"

#include <Preferences.h>
#include <Arduino.h>

#include "config_guard.h"

namespace leopold::identity {
namespace {
String storedId;
String storedSecret;
bool ready = false;
}

bool begin() {
  if (ready) return true;
  Preferences prefs;
  if (!prefs.begin("lpd-identity", false)) return false;
  const bool hasId = prefs.isKey("device_id");
  const bool hasSecret = prefs.isKey("device_secret");
  if (!hasId && !hasSecret) {
    if (prefs.putString("device_id", LEOPOLD_DEVICE_ID) == 0 ||
        prefs.putString("device_secret", LEOPOLD_DEVICE_SECRET) == 0) {
      prefs.end();
      return false;
    }
  } else if (!hasId || !hasSecret) {
    prefs.end();
    return false;
  }
  storedId = prefs.getString("device_id", "");
  storedSecret = prefs.getString("device_secret", "");
  prefs.end();
  ready = storedId.length() >= 8 && storedId.length() < 32 &&
          storedSecret.length() >= 32 && storedSecret.length() < 128;
  return ready;
}

const char* deviceId() { return ready ? storedId.c_str() : ""; }
const char* deviceSecret() { return ready ? storedSecret.c_str() : ""; }

}  // namespace leopold::identity
