#pragma once
#include <Arduino.h>
void time_manager_begin();
void time_manager_loop();
bool time_valid();
String time_iso();
String time_hhmm();
uint32_t time_epoch();
