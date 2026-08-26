#pragma once
#include <Arduino.h>

struct WifiNetworkInfo {
    String ssid;
    int32_t rssi;
    bool secure;
};

struct ApClientInfo {
    String hostname;
    String ip;
    String mac;
};

void wifi_manager_begin();
void wifi_manager_loop();

int wifi_scan();
bool wifi_scan_running();
int wifi_scan_count();
WifiNetworkInfo wifi_scan_get(int index);

void wifi_connect(const String &ssid, const String &password);
void wifi_forget();

bool wifi_connected();
String wifi_ssid();
String wifi_ip();
String wifi_ap_ssid();
String wifi_ap_ip();
int wifi_rssi();

bool wifi_ap_active();
int wifi_ap_client_count();
int wifi_ap_clients(ApClientInfo *out, int maxClients);

void wifi_factory_reset();
