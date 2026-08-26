#pragma once
#include <Arduino.h>
struct TripStats{
 bool active;
 uint32_t startEpoch;
 uint32_t startMillis;
 uint32_t durationSec;
 float maxCoolant,maxOil,maxIntake,maxTurbo,maxDpf,maxGearbox;
};
void trip_begin();
void trip_loop();
const TripStats& trip_stats();
void trip_reset_peaks();
