#pragma once
#include <JC3248W535.h>

// TouchPoint is defined by JC3248W535_Touch.h.
// Do not redefine it here.

bool touch_init();
bool touch_read(TouchPoint &point);
