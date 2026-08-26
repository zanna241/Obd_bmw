#include "discovery.h"

static constexpr int MAX_DISCOVERY_IDS = 96;
static DiscoveryCanEntry entries[MAX_DISCOVERY_IDS];
static int entryCount = 0;
static uint32_t passiveFrames = 0;

static bool diagnostic_id(uint32_t id)
{
    return id >= 0x7E0 && id <= 0x7EF;
}

void discovery_begin()
{
    discovery_reset();
}

void discovery_reset()
{
    memset(entries, 0, sizeof(entries));
    entryCount = 0;
    passiveFrames = 0;
}

static DiscoveryCanEntry *entry_for(uint32_t id, bool extended, uint8_t dlc, const uint8_t *data)
{
    for (int i = 0; i < entryCount; ++i) {
        if (entries[i].id == id && entries[i].extended == extended) return &entries[i];
    }

    if (entryCount >= MAX_DISCOVERY_IDS) return nullptr;

    DiscoveryCanEntry &e = entries[entryCount++];
    e.id = id;
    e.extended = extended;
    e.dlc = min((int)dlc, 8);
    e.firstMs = millis();
    e.lastMs = e.firstMs;

    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t v = (data && i < e.dlc) ? data[i] : 0;
        e.last[i] = v;
        e.minv[i] = v;
        e.maxv[i] = v;
    }
    return &e;
}

void discovery_observe(uint32_t id, bool extended, uint8_t dlc, const uint8_t *data)
{
    DiscoveryCanEntry *e = entry_for(id, extended, dlc, data);
    if (!e) return;

    if (!diagnostic_id(id)) passiveFrames++;

    uint8_t n = min((int)dlc, 8);
    if (e->count == 0) {
        e->count = 1;
        e->dlc = n;
        e->firstMs = millis();
        e->lastMs = e->firstMs;
        return;
    }

    e->count++;
    e->lastMs = millis();
    if (n > e->dlc) e->dlc = n;

    for (uint8_t i = 0; i < n; ++i) {
        uint8_t v = data[i];
        if (v != e->last[i]) e->changes[i]++;
        if (v < e->minv[i]) e->minv[i] = v;
        if (v > e->maxv[i]) e->maxv[i] = v;
        e->last[i] = v;
    }
}

int discovery_count() { return entryCount; }

bool discovery_get(int index, DiscoveryCanEntry &out)
{
    if (index < 0 || index >= entryCount) return false;
    out = entries[index];
    return true;
}

uint32_t discovery_passive_frames() { return passiveFrames; }

uint32_t discovery_passive_ids()
{
    uint32_t n = 0;
    for (int i = 0; i < entryCount; ++i) {
        if (!diagnostic_id(entries[i].id)) n++;
    }
    return n;
}
