#pragma once
#include <Arduino.h>

struct CanCatalogEntry {
    uint32_t id;
    uint32_t count;
    uint32_t lastMillis;
    uint8_t dlc;
    uint8_t data[8];
    bool extended;
};

void can_init();
void can_update();

bool can_is_online();
bool can_driver_ready();
bool obd_is_active();

uint32_t can_total_frames();
uint32_t can_frames_per_second();
uint32_t can_rx_missed();
uint32_t can_bus_errors();
uint32_t can_tx_requests();
uint32_t can_tx_failed();
uint32_t can_obd_replies();
uint32_t can_bus_off_count();
uint32_t can_request_rate();
uint32_t can_reply_rate();

int can_catalog_count();
bool can_catalog_get(int index, CanCatalogEntry &out);
void can_catalog_clear();
void can_stats_reset();

String obd_last_status();
String can_state_text();

// Optional read-only ISO-TP ECU presence scan (UDS TesterPresent 0x3E00).
void can_start_readonly_scan();
bool can_readonly_scan_active();
uint8_t can_readonly_scan_response_mask();
String can_readonly_scan_result();

// True when any physical CAN frame has been received recently.
bool can_bus_recent_activity(uint32_t withinMs = 3000);
uint32_t can_last_frame_ms();

// BMW F-series diagnostic presence scan using ISO-TP Extended 11-bit addressing.
// Tester source F1 -> CAN ID 6F1, first payload byte = target ECU.
// Read-only: sends only UDS TesterPresent 3E 00.
void can_start_bmw_extended_scan();
bool can_bmw_extended_scan_active();
uint8_t can_bmw_extended_scan_response_mask();
String can_bmw_extended_scan_result();
