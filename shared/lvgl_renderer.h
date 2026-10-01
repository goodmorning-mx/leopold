#pragma once

#include <lvgl.h>

#include "sdui.h"

namespace leopold {

using SharedImageResolver = const lv_image_dsc_t* (*)(const char* key);

struct SharedRenderState {
  const char* timeText;
  uint8_t batteryPercent;
  bool wifiConnected;
  bool cloudConnected;
  bool bluetoothEnabled;
  bool bluetoothConnected;
  uint8_t brightnessPercent;
};

void renderSharedLvglScreen(
    lv_obj_t* root,
    const SduiScreen& screen,
    SharedImageResolver resolveImage,
    const char* pressedComponentId,
    const SharedRenderState* state = nullptr);

}  // namespace leopold
