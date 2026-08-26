#pragma once
#include <Arduino.h>

struct DiscoveryCanEntry {
    uint32_t id = 0;
    bool extended = false;
    uint8_t dlc = 0;
    uint32_t count = 0;
    uint32_t firstMs = 0;
    uint32_t lastMs = 0;
    uint8_t last[8] = {0};
    uint8_t minv[8] = {0};
    uint8_t maxv[8] = {0};
    uint32_t changes[8] = {0};
};

void discovery_begin();
void discovery_observe(uint32_t id, bool extended, uint8_t dlc, const uint8_t *data);
void discovery_reset();
int discovery_count();
bool discovery_get(int index, DiscoveryCanEntry &out);
uint32_t discovery_passive_frames();
uint32_t discovery_passive_ids();
