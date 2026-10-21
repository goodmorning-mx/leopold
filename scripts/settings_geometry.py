"""Shared, deterministic Settings slider geometry used by validation tests."""

MINIMUM = 5
MAXIMUM = 100
SLIDER_X = 98
SLIDER_WIDTH = 272


def clamp_brightness(value: int) -> int:
    return max(MINIMUM, min(MAXIMUM, value))


def brightness_knob_x(value: int) -> float:
    value = clamp_brightness(value)
    if value <= 50:
        ratio = 0.5 * (value - MINIMUM) / (50 - MINIMUM)
    else:
        ratio = 0.5 + 0.5 * (value - 50) / (MAXIMUM - 50)
    return SLIDER_X + SLIDER_WIDTH * ratio
