#include "time_manager.h"
#include "wifi_manager.h"
#include <time.h>
static bool configured=false;
static uint32_t lastCheck=0;
static const char *TZ_ROME="CET-1CEST,M3.5.0,M10.5.0/3";
void time_manager_begin(){setenv("TZ",TZ_ROME,1);tzset();}
void time_manager_loop(){uint32_t now=millis();if(now-lastCheck<5000)return;lastCheck=now;if(wifi_connected()&&!configured){configTzTime(TZ_ROME,"pool.ntp.org","time.google.com");configured=true;Serial.println("NTP: configurazione avviata");}}
bool time_valid(){time_t now;time(&now);return now>1700000000;}
String time_iso(){if(!time_valid())return "-";time_t now;time(&now);struct tm t;localtime_r(&now,&t);char b[32];strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",&t);return String(b);}
String time_hhmm(){if(!time_valid())return "--:--";time_t now;time(&now);struct tm t;localtime_r(&now,&t);char b[8];strftime(b,sizeof(b),"%H:%M",&t);return String(b);}
uint32_t time_epoch(){if(!time_valid())return 0;time_t now;time(&now);return (uint32_t)now;}
