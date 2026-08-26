#include "touch.h"
#include "display.h"

bool touch_init()
{
    // Touch is initialized in display_init() through boardTouch.begin().
    // Rotation is synchronized by display.setTouchRotation(&boardTouch).
    return true;
}

bool touch_read(TouchPoint &point)
{
    // The dedicated JC3248W535 driver already returns coordinates
    // mapped to the current display rotation (480x320 in V0.3.2).
    return boardTouch.read(point);
}
