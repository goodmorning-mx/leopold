#pragma once

#include <Arduino.h>
#include "boot_health_diagnostics.h"

namespace leopold::firmware_ota {

bool beginBootVerification();
void setLocalHealth(bool displayReady, bool touchReady, bool settingsRestored, bool setupReady,
                    BootHealthDiagnostics::InitStage initStage);
void loopHealth(uint32_t now);
void reportBootOutcomeWhenOnline();
void cancel();
int checkAndInstall();
bool busy();

}  // namespace leopold::firmware_ota
