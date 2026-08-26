#pragma once
#include <Arduino.h>

struct HistoryPoint {
    uint32_t ageMinutes;
    float coolant;
    float oil;
    float intake;
    float turbo;
    float dpf;
    float gearbox;
    float rpm;
    float speed;
    float rail;
    float dpfDiff;
    float egt1;
    float egt2;
};

void history_begin();
void history_loop();
int history_count();
bool history_get(int logicalIndex, HistoryPoint &out);
