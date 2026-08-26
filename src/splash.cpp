#include "splash.h"
#include "display.h"
#include "splash_assets.h"
#include <Arduino.h>

static uint16_t *blendBuf = nullptr;

static inline uint16_t blend565(uint16_t a, uint16_t b, uint8_t alpha)
{
    // alpha 0 -> a, 255 -> b
    uint32_t ar = (a >> 11) & 0x1F;
    uint32_t ag = (a >> 5) & 0x3F;
    uint32_t ab = a & 0x1F;

    uint32_t br = (b >> 11) & 0x1F;
    uint32_t bg = (b >> 5) & 0x3F;
    uint32_t bb = b & 0x1F;

    uint32_t r = (ar * (255-alpha) + br * alpha) / 255;
    uint32_t g = (ag * (255-alpha) + bg * alpha) / 255;
    uint32_t bl = (ab * (255-alpha) + bb * alpha) / 255;

    return (r << 11) | (g << 5) | bl;
}

static inline uint16_t scale565(uint16_t c, uint8_t level)
{
    uint32_t r = ((c >> 11) & 0x1F) * level / 255;
    uint32_t g = ((c >> 5) & 0x3F) * level / 255;
    uint32_t b = (c & 0x1F) * level / 255;
    return (r << 11) | (g << 5) | b;
}

static void drawLogo(const uint16_t *img, uint8_t brightness)
{
    const int x = (480 - SPLASH_LOGO_W) / 2;
    const int y = (320 - SPLASH_LOGO_H) / 2;

    for (int i = 0; i < SPLASH_LOGO_W*SPLASH_LOGO_H; ++i) {
        blendBuf[i] = scale565(pgm_read_word(&img[i]), brightness);
    }

    gfx->fillScreen(RGB565_BLACK);
    gfx->draw16bitRGBBitmap(x, y, blendBuf, SPLASH_LOGO_W, SPLASH_LOGO_H);
    display.flush();
}

static void drawBlend(uint8_t alpha)
{
    const int x = (480 - SPLASH_LOGO_W) / 2;
    const int y = (320 - SPLASH_LOGO_H) / 2;

    for (int i = 0; i < SPLASH_LOGO_W*SPLASH_LOGO_H; ++i) {
        uint16_t a = pgm_read_word(&SPLASH_BMW[i]);
        uint16_t b = pgm_read_word(&SPLASH_MZ[i]);
        blendBuf[i] = blend565(a, b, alpha);
    }

    gfx->fillScreen(RGB565_BLACK);
    gfx->draw16bitRGBBitmap(x, y, blendBuf, SPLASH_LOGO_W, SPLASH_LOGO_H);
    display.flush();
}

void splash_run()
{
    blendBuf = (uint16_t *)ps_malloc(
        SPLASH_LOGO_W * SPLASH_LOGO_H * sizeof(uint16_t)
    );

    if (!blendBuf) {
        Serial.println("Splash buffer allocation failed");
        return;
    }

    // ~2.7 seconds total
    // 0.0 - 0.45: BMW fade in
    for (int i = 0; i <= 9; ++i) {
        drawLogo(SPLASH_BMW, (uint8_t)(i * 28));
        delay(50);
    }

    // 0.45 - 1.00: BMW hold
    delay(550);

    // 1.00 - 1.90: BMW -> MZ crossfade
    for (int i = 0; i <= 18; ++i) {
        drawBlend((uint8_t)(i * 255 / 18));
        delay(50);
    }

    // 1.90 - 2.40: MZ hold
    delay(500);

    // 2.40 - 2.70: MZ fade out
    for (int i = 9; i >= 0; --i) {
        drawLogo(SPLASH_MZ, (uint8_t)(i * 28));
        delay(35);
    }

    gfx->fillScreen(RGB565_BLACK);
    display.flush();

    free(blendBuf);
    blendBuf = nullptr;
}
