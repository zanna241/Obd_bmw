#include <Arduino.h>
#include "display.h"
#include "touch.h"
#include "gui.h"
#include "logger.h"
#include "can.h"
#include "alarm_manager.h"
#include "trip_manager.h"
#include "time_manager.h"
#include "history.h"
#include "version.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "splash.h"
#include "display_settings.h"
#include "power_manager.h"

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("======================================");
    Serial.println(" BMW 520xd MONITOR V0.9.9.4 OTA RECOVERY");
    Serial.println(" NATIVE OTA / DEVICE AUTH / BENCH MODE");
    Serial.println("======================================");

    // Must run before TWAI setup after a deep-sleep EXT1 wake.
    power_manager_begin();

    if (!psramFound()) {
        Serial.println("ERROR: PSRAM NOT FOUND");
        while (true) delay(1000);
    }

    if (!display_init()) {
        Serial.println("ERROR: DISPLAY INIT / ROTATION FAILED");
        while (true) delay(1000);
    }

    touch_init();
    display_settings_begin();

    splash_run();
    gui_init();

    logger_begin();
    history_begin();
    time_manager_begin();
    trip_begin();
    alarm_begin();

    // Bring networking up first so AP/web remain responsive even if CAN is faulty.
    wifi_manager_begin();
    web_server_begin();

    can_init();

    Serial.println("SYSTEM READY");
}

void loop()
{
    // Keep network service ahead of the display and SD logger. The web server is
    // synchronous, so servicing it twice per loop prevents starvation when a
    // full LVGL frame or an SD write takes longer than usual.
    wifi_manager_loop();
    web_server_loop();

    can_update();

    lv_tick_inc(1);
    lv_timer_handler();
    gui_update();

    can_update();
    web_server_loop();

    logger_loop();
    history_loop();
    time_manager_loop();
    display_settings_loop();
    trip_loop();
    alarm_loop();
    power_manager_loop();
    delay(1);
}
