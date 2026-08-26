#include "display_settings.h"
#include "display.h"
#include "time_manager.h"
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static uint8_t dayBrightness = 90;
static uint8_t nightBrightness = 28;
static DisplayThemeMode themeMode = DISPLAY_THEME_AUTO;
static bool vehicleNightKnown = false;
static bool vehicleNight = false;
static bool effectiveNight = false;
static uint32_t lastEval = 0;
static bool firstApply = true;
static uint8_t appliedBrightness = 0;

static uint8_t clamp_pct(uint8_t v) {
    if (v < 5) return 5;
    if (v > 100) return 100;
    return v;
}

static bool time_night() {
    if (!time_valid()) return false;
    time_t now; time(&now);
    struct tm t; localtime_r(&now, &t);
    // Conservative fallback only until BMW lighting state is decoded.
    return t.tm_hour < 7 || t.tm_hour >= 19;
}

static void evaluate_and_apply() {
    bool night = false;
    if (themeMode == DISPLAY_THEME_NIGHT) night = true;
    else if (themeMode == DISPLAY_THEME_DAY) night = false;
    else if (vehicleNightKnown) night = vehicleNight;
    else night = time_night();

    uint8_t wantedBrightness = night ? nightBrightness : dayBrightness;
    bool changed = firstApply || (effectiveNight != night) || (appliedBrightness != wantedBrightness);
    effectiveNight = night;
    if (changed) {
        display_set_brightness(wantedBrightness);
        appliedBrightness = wantedBrightness;
        firstApply = false;
    }
}

void display_settings_begin() {
    prefs.begin("bmwdisplay", false);
    dayBrightness = clamp_pct(prefs.getUChar("day", 90));
    nightBrightness = clamp_pct(prefs.getUChar("night", 28));
    uint8_t m = prefs.getUChar("theme", DISPLAY_THEME_AUTO);
    themeMode = m <= DISPLAY_THEME_NIGHT ? (DisplayThemeMode)m : DISPLAY_THEME_AUTO;
    evaluate_and_apply();
}

void display_settings_loop() {
    uint32_t now = millis();
    if (now - lastEval < 1000) return;
    lastEval = now;
    evaluate_and_apply();
}

uint8_t display_day_brightness() { return dayBrightness; }
uint8_t display_night_brightness() { return nightBrightness; }

void display_set_day_brightness(uint8_t value) {
    dayBrightness = clamp_pct(value);
    prefs.putUChar("day", dayBrightness);
    evaluate_and_apply();
}

void display_set_night_brightness(uint8_t value) {
    nightBrightness = clamp_pct(value);
    prefs.putUChar("night", nightBrightness);
    evaluate_and_apply();
}

DisplayThemeMode display_theme_mode() { return themeMode; }
void display_set_theme_mode(DisplayThemeMode mode) {
    if ((uint8_t)mode > DISPLAY_THEME_NIGHT) mode = DISPLAY_THEME_AUTO;
    themeMode = mode;
    prefs.putUChar("theme", (uint8_t)themeMode);
    evaluate_and_apply();
}

bool display_theme_is_night() { return effectiveNight; }

String display_theme_status() {
    if (themeMode == DISPLAY_THEME_DAY) return "GIORNO MANUALE";
    if (themeMode == DISPLAY_THEME_NIGHT) return "NOTTE MANUALE";
    if (vehicleNightKnown) return effectiveNight ? "AUTO: BMW NIGHT" : "AUTO: BMW DAY";
    if (time_valid()) return effectiveNight ? "AUTO: ORARIO NIGHT" : "AUTO: ORARIO DAY";
    return "AUTO: DAY (BMW NON ANCORA MAPPATO)";
}

void display_set_vehicle_night_state(bool known, bool night) {
    vehicleNightKnown = known;
    vehicleNight = night;
    evaluate_and_apply();
}

String display_theme_source() {
    if (themeMode == DISPLAY_THEME_DAY) return "MANUAL_DAY";
    if (themeMode == DISPLAY_THEME_NIGHT) return "MANUAL_NIGHT";
    if (vehicleNightKnown) return "BMW";
    if (time_valid()) return "CLOCK";
    return "FALLBACK_DAY";
}
