#include "display.h"
#include <Arduino.h>

JC3248W535_Display display;
JC3248W535_Touch boardTouch;
Arduino_Canvas *gfx = nullptr;

static constexpr uint8_t BACKLIGHT_PIN = 1;
static uint8_t currentBrightness = 100;

void display_set_brightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    currentBrightness = percent;
    pinMode(BACKLIGHT_PIN, OUTPUT);
    uint8_t duty = (uint8_t)((uint16_t)percent * 255u / 100u);
    analogWrite(BACKLIGHT_PIN, duty);
}

uint8_t display_get_brightness()
{
    return currentBrightness;
}

bool display_init()
{
    if (!display.begin()) {
        return false;
    }

    // A soft reset can leave the previous LCD framebuffer visible for a few
    // milliseconds. Hide the panel while rotation/canvas are prepared, paint
    // black first, then enable the backlight. This removes the HOME flash
    // before the BMW/MZ splash animation.
    display.backlightOff();

    display.setRotation(ROTATION_90);

    gfx = display.getCanvas();
    if (!gfx) return false;

    if (!boardTouch.begin()) {
        Serial.println("WARNING: board touch init failed");
    }

    display.setTouchRotation(&boardTouch);

    gfx->fillScreen(RGB565_BLACK);
    display.flush();

    // Start visible only after a known black frame is already on the panel.
    display_set_brightness(100);

    Serial.printf("Display canvas: %d x %d rotation=%d\n",
                  display.width(), display.height(), display.getRotation());

    return display.width() == 480 && display.height() == 320;
}

void display_set_backlight(bool on)
{
    if (on) display_set_brightness(currentBrightness ? currentBrightness : 100);
    else analogWrite(BACKLIGHT_PIN, 0);
}
