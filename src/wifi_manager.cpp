#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "esp_wifi.h"
#include "esp_netif.h"

static Preferences prefs;
static WifiNetworkInfo networks[12];
static int networkCount = 0;
static String savedSSID, savedPassword;

static const char *AP_SSID = "BMW520xd-Monitor";
static const char *AP_PASS = "520xdmonitor";
static bool mdnsStarted = false;
static uint32_t staStartAt = 0;
static bool staStarted = false;
static bool scanRunning = false;

static String macToString(const uint8_t *mac)
{
    char buf[18];
    snprintf(buf,sizeof(buf),"%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return String(buf);
}

static String fallbackHostname(const uint8_t *mac)
{
    char buf[22];
    snprintf(buf,sizeof(buf),"client-%02X%02X%02X",mac[3],mac[4],mac[5]);
    return String(buf);
}

void wifi_manager_begin()
{
    prefs.begin("bmw520xd", false);
    savedSSID = prefs.getString("wifi_ssid", "");
    savedPassword = prefs.getString("wifi_pass", "");

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAPsetHostname("BMW520xd-Monitor");
    WiFi.softAP(AP_SSID, AP_PASS);

    staStartAt = millis() + 2500; // let SoftAP/DHCP become available first
    staStarted = false;
}

void wifi_manager_loop()
{
    if (!staStarted && savedSSID.length() && (int32_t)(millis()-staStartAt)>=0) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setHostname("BMW520xd-Monitor");
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        staStarted=true;
    }
    if (scanRunning) {
        int found=WiFi.scanComplete();
        if (found>=0) {
            networkCount=0;
            for(int i=0;i<found && networkCount<12;++i){
                String ssid=WiFi.SSID(i); if(!ssid.length()) continue;
                bool dup=false; for(int j=0;j<networkCount;++j) if(networks[j].ssid==ssid){ if(WiFi.RSSI(i)>networks[j].rssi) networks[j].rssi=WiFi.RSSI(i); dup=true; break; }
                if(dup) continue; networks[networkCount].ssid=ssid; networks[networkCount].rssi=WiFi.RSSI(i); networks[networkCount].secure=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN; networkCount++;
            }
            WiFi.scanDelete(); scanRunning=false;
        } else if(found==WIFI_SCAN_FAILED) scanRunning=false;
    }
    if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
        if (MDNS.begin("bmw520xd")) {
            mdnsStarted = true;
            MDNS.addService("http","tcp",80);
        }
    }
}

int wifi_scan()
{
    if (!scanRunning) {
        if (WiFi.getMode()==WIFI_AP) WiFi.mode(WIFI_AP_STA);
        int r=WiFi.scanNetworks(true, true);
        if (r==WIFI_SCAN_RUNNING || r>=0) scanRunning=true;
    }
    return networkCount;
}

bool wifi_scan_running(){ return scanRunning; }

int wifi_scan_count(){ return networkCount; }

WifiNetworkInfo wifi_scan_get(int index)
{
    if (index<0 || index>=networkCount) return WifiNetworkInfo();
    return networks[index];
}

void wifi_connect(const String &ssid,const String &password)
{
    savedSSID=ssid; savedPassword=password;
    prefs.putString("wifi_ssid",savedSSID);
    prefs.putString("wifi_pass",savedPassword);

    WiFi.disconnect(false,false);
    delay(100);
    WiFi.setHostname("BMW520xd-Monitor");
    WiFi.begin(savedSSID.c_str(),savedPassword.c_str());
}

void wifi_forget()
{
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    savedSSID=""; savedPassword="";
    WiFi.disconnect(false,true);
}

bool wifi_connected(){ return WiFi.status()==WL_CONNECTED; }
String wifi_ssid(){ return wifi_connected()?WiFi.SSID():String(); }
String wifi_ip(){ return wifi_connected()?WiFi.localIP().toString():String("-"); }
String wifi_ap_ssid(){ return String(AP_SSID); }
String wifi_ap_ip(){ return WiFi.softAPIP().toString(); }
int wifi_rssi(){ return wifi_connected()?WiFi.RSSI():0; }

bool wifi_ap_active()
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode)!=ESP_OK) return false;
    return mode==WIFI_MODE_AP || mode==WIFI_MODE_APSTA;
}

int wifi_ap_client_count()
{
    return (int)WiFi.softAPgetStationNum();
}

int wifi_ap_clients(ApClientInfo *out,int maxClients)
{
    if (!out || maxClients<=0) return 0;

    wifi_sta_list_t list = {};
    if (esp_wifi_ap_get_sta_list(&list)!=ESP_OK) return 0;

    int count = min((int)list.num,maxClients);
    if (count<=0) return 0;

    esp_netif_t *apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_pair_mac_ip_t *pairs =
        (esp_netif_pair_mac_ip_t*)calloc(count,sizeof(esp_netif_pair_mac_ip_t));
    if (!pairs) return 0;

    for (int i=0;i<count;++i) memcpy(pairs[i].mac,list.sta[i].mac,6);

    if (apNetif) esp_netif_dhcps_get_clients_by_mac(apNetif,count,pairs);

    for (int i=0;i<count;++i) {
        out[i].mac = macToString(list.sta[i].mac);
        if (pairs[i].ip.addr) {
            IPAddress ip(pairs[i].ip.addr);
            out[i].ip = ip.toString();
            // Hostname is not reliably exposed by the ESP32 SoftAP DHCP API
            // across Arduino-ESP32 versions. Use a stable MAC-derived client id.
            out[i].hostname = fallbackHostname(list.sta[i].mac);
        } else {
            out[i].ip = "-";
            out[i].hostname = fallbackHostname(list.sta[i].mac);
        }
    }
    free(pairs);
    return count;
}

void wifi_factory_reset()
{
    prefs.clear();
    WiFi.disconnect(false,true);
}
