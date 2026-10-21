from scripts.settings_geometry import brightness_knob_x, clamp_brightness


def test_brightness_endpoints_and_midpoint() -> None:
    assert clamp_brightness(0) == 5
    assert clamp_brightness(5) == 5
    assert brightness_knob_x(5) == 98
    assert brightness_knob_x(50) == 234.0
    assert brightness_knob_x(100) == 370
    assert clamp_brightness(101) == 100
