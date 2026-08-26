#include "power_manager.h"
#include "vehicle_data.h"
#include "display.h"
#include "logger.h"
#include "hardware.h"
#include "can.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

static constexpr uint32_t DATA_STALE_MS = 5000;
static constexpr uint32_t BACKLIGHT_OFF_MS = 10000;
static constexpr uint32_t MIN_OFFLINE_BEFORE_SLEEP_MS = 45000;
static constexpr uint32_t BUS_QUIET_REQUIRED_MS = 10000;
static uint32_t offlineSince = 0;
static bool dataCleared = false;
static bool backlightOff = false;
static String state = "ACTIVE";

static void enter_sleep(){
    state="SLEEP PREP";
    if(logger_active()) logger_stop();
    display_set_backlight(false);
    delay(30);
    // ESP32-S3 GPIO18 is RTC capable (GPIO0..21). MCP2562FD RXD is recessive HIGH
    // on an idle CAN bus and goes LOW on dominant bus activity. EXT1 ANY_LOW
    // therefore wakes on the first dominant CAN level while the transceiver stays powered.
    pinMode(CAN_RX_PIN, INPUT_PULLUP);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_ext1_wakeup(1ULL << CAN_RX_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
    Serial.println("POWER: deep sleep; wake source CAN RX GPIO18 LOW");
    Serial.flush();
    esp_deep_sleep_start();
}

void power_manager_begin(){
    auto cause=esp_sleep_get_wakeup_cause();

    // EXT1 wake leaves the wake pin under RTC IO control on ESP32-S3.
    // Release GPIO18 before TWAI is installed, otherwise the CAN RX input may
    // not behave as a normal digital GPIO after a deep-sleep wake.
    if(cause==ESP_SLEEP_WAKEUP_EXT1) {
        rtc_gpio_deinit((gpio_num_t)CAN_RX_PIN);
        Serial.println("POWER: wake from CAN RX; GPIO18 returned to digital IO");
    }

    offlineSince=0; dataCleared=false; backlightOff=false; state="ACTIVE";
}

void power_manager_loop(){
    VehicleData &v=vehicle_data();
    uint32_t now=millis();

    bool engineLive = v.lastDdeReplyMs && (now-v.lastDdeReplyMs < DATA_STALE_MS);
    bool busAwake = can_bus_recent_activity(3000);

    if(engineLive){
        offlineSince=0;
        dataCleared=false;
        state="ENGINE ACTIVE";
        if(backlightOff){ display_set_backlight(true); backlightOff=false; }
        return;
    }

    if(!offlineSince) offlineSince=now;
    uint32_t dead=now-offlineSince;

    // Engine/DDE is no longer answering: never leave stale measurements visible.
    if(dead>=DATA_STALE_MS && !dataCleared){
        vehicle_data_invalidate_live();
        dataCleared=true;
    }

    // BMW can keep the gateway/CAN awake for ~1 minute after engine-off.
    // We expose that as a separate state and do not deep-sleep while frames
    // are still physically arriving.
    if(busAwake){
        state = "ENGINE OFF / CAN AWAKE";
    }else{
        state = "BUS QUIET";
    }

    if(dead>=BACKLIGHT_OFF_MS && !backlightOff){
        display_set_backlight(false);
        backlightOff=true;
    }

    bool busQuietLongEnough =
        !busAwake && can_last_frame_ms() &&
        (uint32_t)(now-can_last_frame_ms()) >= BUS_QUIET_REQUIRED_MS;

    if(dead>=MIN_OFFLINE_BEFORE_SLEEP_MS && busQuietLongEnough){
        state="BUS SLEEP";
        enter_sleep();
    }
}
String power_manager_state(){return state;}
