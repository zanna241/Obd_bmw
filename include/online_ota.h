#pragma once
#include <Arduino.h>

struct OnlineOtaInfo {
    bool valid = false;
    bool updateAvailable = false;
    String version;
    String build;
    String url;
    String sha256;
    String notes;
    size_t size = 0;
    String error;
};

typedef void (*OnlineOtaProgress)(size_t received, size_t total, const char *phase);

void online_ota_begin();
bool online_ota_check(OnlineOtaInfo &info);
bool online_ota_install(const OnlineOtaInfo &info, OnlineOtaProgress progress, String &error);
