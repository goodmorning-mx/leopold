#pragma once

#include <lvgl.h>

#include "sdui.h"

namespace leopold {

using SharedImageResolver = const lv_image_dsc_t* (*)(const char* key);

enum class FirmwareLifecyclePreview : uint8_t {
  kBoot = 0,
  kWelcome = 1,
  kUpdateRequired = 2,
  kCalibration = 3,
};

struct SharedRenderState {
  const char* timeText = "--:--";
  const char* dateText = "";
  uint8_t batteryPercent;
  uint8_t clockSecond = 0;
  bool wifiConnected;
  bool cloudConnected;
  bool bluetoothEnabled;
  bool bluetoothConnected;
  uint8_t brightnessPercent;
  uint8_t backgroundMode;
  uint8_t backgroundArt = 255;
  bool appearanceDark = true;
  uint8_t clockBackground = 255;
  uint8_t clockFace = 0;
  bool clockShowWeather = false;
  uint8_t accentIndex = 4;
};

void renderSharedLvglScreen(
    lv_obj_t* root,
    const SduiScreen& screen,
    SharedImageResolver resolveImage,
    const char* pressedComponentId,
    const SharedRenderState* state = nullptr);

void renderSharedLvglStatusBanner(
    lv_obj_t* root,
    const SharedRenderState& state,
    SharedImageResolver resolveImage);

void renderFirmwareLifecyclePreview(lv_obj_t* root, FirmwareLifecyclePreview preview, uint8_t step);

}  // namespace leopold
