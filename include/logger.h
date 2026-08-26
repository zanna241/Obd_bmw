#pragma once
#include <Arduino.h>

bool logger_begin();
void logger_loop();

bool logger_start();
void logger_stop();
bool logger_active();

void logger_log_can_frame(
    uint64_t timestampUs,
    uint32_t id,
    bool extended,
    bool rtr,
    uint8_t dlc,
    const uint8_t *data
);


void logger_log_can_tx(
    uint64_t timestampUs,
    uint32_t id,
    bool extended,
    bool rtr,
    uint8_t dlc,
    const uint8_t *data
);

uint32_t logger_frames_written();
uint32_t logger_frames_dropped();
uint32_t logger_stall_count();
uint32_t logger_max_write_ms();
uint32_t logger_max_flush_ms();
uint32_t logger_last_io_ms();

bool logger_storage_ready();
String logger_storage_name();
uint64_t logger_storage_total();
uint64_t logger_storage_used();

String logger_current_file();
size_t logger_current_size();

int logger_file_count();
String logger_file_name(int index);
size_t logger_file_size(int index);

bool logger_delete(const String &path);
void logger_delete_all();

void logger_mark_event(const String &label);
