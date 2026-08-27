#pragma once
#include <Arduino.h>
void power_manager_begin();
void power_manager_loop();
String power_manager_state();
bool power_can_sleep_enabled();
void power_set_can_sleep_enabled(bool enabled);
