#pragma once
#include <JC3248W535.h>
extern JC3248W535_Display display;
extern JC3248W535_Touch boardTouch;
extern Arduino_Canvas *gfx;
bool display_init();
void display_set_backlight(bool on);
void display_set_brightness(uint8_t percent);
uint8_t display_get_brightness();
