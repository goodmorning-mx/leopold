#pragma once

namespace leopold {

void beginComputerControl();
bool computerControlConnected();
bool sendComputerControl(const char* componentId);

}  // namespace leopold
