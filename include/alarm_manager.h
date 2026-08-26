#pragma once
#include <Arduino.h>
enum AlarmLevel{ALARM_NONE=0,ALARM_WARNING,ALARM_CRITICAL};
struct AlarmState{AlarmLevel level;String title;String message;};
void alarm_begin();
void alarm_loop();
const AlarmState& alarm_state();
float alarm_limit_coolant();
float alarm_limit_oil();
float alarm_limit_gearbox();
float alarm_limit_dpf();
void alarm_set_limits(float coolant,float oil,float gearbox,float dpf);
