#pragma once
#include <Arduino.h>

enum DisplayThemeMode : uint8_t {
    DISPLAY_THEME_AUTO = 0,
    DISPLAY_THEME_DAY = 1,
    DISPLAY_THEME_NIGHT = 2
};

void display_settings_begin();
void display_settings_loop();

uint8_t display_day_brightness();
uint8_t display_night_brightness();
void display_set_day_brightness(uint8_t value);
void display_set_night_brightness(uint8_t value);

DisplayThemeMode display_theme_mode();
void display_set_theme_mode(DisplayThemeMode mode);
bool display_theme_is_night();
String display_theme_status();

// Reserved for future BMW/KOMBI light-state decoding.
void display_set_vehicle_night_state(bool known, bool night);

String display_theme_source();
