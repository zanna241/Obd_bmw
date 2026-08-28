#include "gui.h"
#include "display.h"
#include "touch.h"
#include "wifi_manager.h"
#include "header_logo.h"
#include "vehicle_data.h"
#include "history.h"
#include "can.h"
#include "logger.h"
#include "alarm_manager.h"
#include "time_manager.h"
#include "display_settings.h"
#include "power_manager.h"
#include "online_ota.h"
#include "version.h"
#include <cstring>

static const int UI_W = 480;
static const int UI_H = 320;

static lv_display_t *lv_disp = nullptr;
static lv_indev_t *lv_touch = nullptr;
static uint16_t *lv_buffer = nullptr;

enum PageId {
    PAGE_HOME = 0,
    PAGE_ENGINE,
    PAGE_DPF,
    PAGE_GEARBOX,
    PAGE_LIVE,
    PAGE_DIAG,
    PAGE_CHARTS,
    PAGE_COUNT
};

enum GestureAction {
    G_NONE = 0,
    G_LEFT,
    G_RIGHT,
    G_UP,
    G_DOWN
};

static PageId currentPage = PAGE_HOME;
static lv_obj_t *pages[PAGE_COUNT] = {};
static lv_obj_t *wifiIconBox = nullptr;
static lv_obj_t *wifiBars[4] = {nullptr,nullptr,nullptr,nullptr};
static lv_obj_t *apLabel = nullptr;
static lv_obj_t *wifiClientsLabel = nullptr;
static lv_obj_t *canLabel = nullptr;
static lv_obj_t *clockLabel = nullptr;
static lv_obj_t *alarmOverlay = nullptr;
static lv_obj_t *alarmTitle = nullptr;
static lv_obj_t *alarmMessage = nullptr;

static lv_obj_t *settingsOverlay = nullptr;
static lv_obj_t *touchOverlay = nullptr;
static lv_obj_t *touchDot = nullptr;
static lv_obj_t *touchCoord = nullptr;
static lv_obj_t *touchCountLabel = nullptr;
static lv_obj_t *loggerStatus = nullptr;
static lv_obj_t *powerModeStatus = nullptr;
static lv_obj_t *otaOverlay = nullptr;
static lv_obj_t *otaTitleLabel = nullptr;
static lv_obj_t *otaMessageLabel = nullptr;
static lv_obj_t *otaProgressBar = nullptr;
static lv_obj_t *otaInstallButton = nullptr;
static OnlineOtaInfo otaInfo;
static lv_obj_t *wifiOverlay = nullptr;
static lv_obj_t *displayOverlay = nullptr;
static lv_obj_t *daySlider = nullptr;
static lv_obj_t *nightSlider = nullptr;
static lv_obj_t *dayValueLabel = nullptr;
static lv_obj_t *nightValueLabel = nullptr;
static lv_obj_t *themeStatusLabel = nullptr;
static bool guiNightTheme = false;

// Every visible value is backed by the single shared VehicleData model.
static lv_obj_t *homeCoolantValue=nullptr,*homeIntakeValue=nullptr,*homeOilValue=nullptr;
static lv_obj_t *homeDpfValue=nullptr,*homeTurboValue=nullptr,*homeGearboxValue=nullptr;
static lv_obj_t *engineRpmValue=nullptr,*engineCoolantValue=nullptr,*engineOilValue=nullptr;
static lv_obj_t *engineIntakeValue=nullptr,*engineTurboValue=nullptr,*engineRailValue=nullptr;
static lv_obj_t *engineLoadValue=nullptr,*engineVoltageValue=nullptr,*engineMafValue=nullptr,*engineAccelValue=nullptr;
static lv_obj_t *dpfRegenValue=nullptr,*dpfEgt1Value=nullptr,*dpfEgt2Value=nullptr;
static lv_obj_t *dpfDiffValue=nullptr,*dpfTriggerValue=nullptr,*dpfSootValue=nullptr;
static lv_obj_t *dpfAshValue=nullptr,*dpfAvgDistanceValue=nullptr,*dpfEgt3Value=nullptr,*dpfNoxValue=nullptr,*dpfLambdaValue=nullptr;
static lv_obj_t *gearValue=nullptr,*gearOilValue=nullptr,*gearSlipValue=nullptr,*gearLockValue=nullptr;
static lv_obj_t *diagCanValue=nullptr,*diagDdeValue=nullptr,*diagEgsValue=nullptr,*diagScannerValue=nullptr;
static lv_obj_t *liveVal[6]={}, *livePeak[6]={};
static lv_obj_t *historyChart=nullptr; static lv_chart_series_t *historyCool=nullptr,*historyOil=nullptr;
static lv_obj_t *wifiListBox = nullptr;
static lv_obj_t *wifiStatusLabel = nullptr;
static lv_obj_t *wifiPasswordOverlay = nullptr;
static lv_obj_t *wifiPasswordTa = nullptr;
static lv_obj_t *wifiKeyboard = nullptr;
static String selectedWifiSSID;


static bool settingsOpen = false;
static bool touchTestOpen = false;
static bool loggerEnabled = false;
static uint32_t touchCount = 0;

static bool touchWasPressed = false;
static int16_t touchStartX = 0;
static int16_t touchStartY = 0;
static int16_t touchLastX = 0;
static int16_t touchLastY = 0;
static GestureAction pendingGesture = G_NONE;

static lv_color_t BG()       { return guiNightTheme ? lv_color_hex(0x020100) : lv_color_hex(0x030405); }
static lv_color_t PANEL()    { return guiNightTheme ? lv_color_hex(0x0B0502) : lv_color_hex(0x111315); }
static lv_color_t PANEL2()   { return guiNightTheme ? lv_color_hex(0x160903) : lv_color_hex(0x181B1F); }
static lv_color_t LINE()     { return guiNightTheme ? lv_color_hex(0x512006) : lv_color_hex(0x32373D); }
static lv_color_t TEXT()     { return guiNightTheme ? lv_color_hex(0xFF6A00) : lv_color_hex(0xF4F5F6); }
static lv_color_t MUTED()    { return guiNightTheme ? lv_color_hex(0xA83A00) : lv_color_hex(0x9CA4AD); }
static lv_color_t YELLOW()   { return guiNightTheme ? lv_color_hex(0xFF8A00) : lv_color_hex(0xFFC400); }
static lv_color_t BLUE()     { return guiNightTheme ? lv_color_hex(0xE85100) : lv_color_hex(0x1F8CFF); }
static lv_color_t GREEN()    { return lv_color_hex(0x4DD17A); }
static lv_color_t RED()      { return lv_color_hex(0xFF4A45); }

static int wifi_signal_level()
{
    if (!wifi_connected()) return 0;
    int rssi = wifi_rssi();
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

static void update_header_connectivity()
{
    if (!wifiIconBox || !apLabel) return;

    if (wifi_connected()) {
        lv_obj_clear_flag(wifiIconBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(apLabel, LV_OBJ_FLAG_HIDDEN);
        int level = wifi_signal_level();
        for (int i=0;i<4;++i)
            lv_obj_set_style_bg_color(wifiBars[i], i<level ? TEXT() : LINE(), 0);
    } else if (wifi_ap_active()) {
        lv_obj_add_flag(wifiIconBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(apLabel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(apLabel,"AP");
        lv_obj_set_style_text_color(apLabel, wifi_ap_client_count()>0 ? GREEN() : YELLOW(), 0);
    } else {
        lv_obj_add_flag(wifiIconBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(apLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_ap_clients_label()
{
    if (!wifiClientsLabel) return;

    ApClientInfo clients[3];
    int n = wifi_ap_clients(clients,3);

    if (n<=0) {
        lv_label_set_text(wifiClientsLabel,"Client AP: nessuno");
        lv_obj_set_style_text_color(wifiClientsLabel,MUTED(),0);
        return;
    }

    String s="Client AP: ";
    for (int i=0;i<n;++i) {
        if (i) s += " | ";
        s += clients[i].hostname;
        s += " ";
        s += clients[i].ip;
    }
    lv_label_set_text(wifiClientsLabel,s.c_str());
    lv_obj_set_style_text_color(wifiClientsLabel,GREEN(),0);
}


static void flush_cb(lv_display_t *d, const lv_area_t *, uint8_t *px)
{
    if (gfx) {
        gfx->draw16bitRGBBitmap(0,0,(uint16_t*)px,UI_W,UI_H);

        // Cache the header logo once. Rebuilding this 1600-pixel array on every
        // LVGL frame wastes CPU on a dashboard that updates several times/s.
        static uint16_t logo[HEADER_BMW_W*HEADER_BMW_H];
        static bool logoReady=false;
        if(!logoReady){
            for (int i=0;i<HEADER_BMW_W*HEADER_BMW_H;++i)
                logo[i]=pgm_read_word(&HEADER_BMW_LOGO[i]);
            logoReady=true;
        }

        gfx->draw16bitRGBBitmap(9,8,logo,HEADER_BMW_W,HEADER_BMW_H);
        display.flush();
    }
    lv_display_flush_ready(d);
}

static void queue_gesture_from_release()
{
    int dx = touchLastX - touchStartX;
    int dy = touchLastY - touchStartY;

    const int threshold = 55;

    if (abs(dx) < threshold && abs(dy) < threshold) return;

    if (abs(dx) > abs(dy)) {
        pendingGesture = dx < 0 ? G_LEFT : G_RIGHT;
    } else {
        pendingGesture = dy < 0 ? G_UP : G_DOWN;
    }
}

static void touch_cb(lv_indev_t *, lv_indev_data_t *data)
{
    TouchPoint p;

    if (touch_read(p)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = p.x;
        data->point.y = p.y;

        if (!touchWasPressed) {
            touchWasPressed = true;
            touchStartX = touchLastX = p.x;
            touchStartY = touchLastY = p.y;
        } else {
            touchLastX = p.x;
            touchLastY = p.y;
        }

        if (touchTestOpen && touchDot) {
            lv_obj_set_pos(touchDot, p.x - 5, p.y - 5);
            if (touchCoord) {
                lv_label_set_text_fmt(touchCoord, "X %u   Y %u", p.x, p.y);
            }
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        if (touchWasPressed) {
            touchWasPressed = false;
            if (touchTestOpen && touchCountLabel) {
                touchCount++;
                lv_label_set_text_fmt(
                    touchCountLabel,
                    "Tocchi: %lu",
                    (unsigned long)touchCount
                );
            }
            queue_gesture_from_release();
        }
    }
}

static lv_obj_t *L(lv_obj_t *p, const char *t, int x, int y,
                   const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, t);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_text_color(o, c, 0);
    return o;
}

static lv_obj_t *B(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, PANEL(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, LINE(), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, 8, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void line_obj(lv_obj_t *p, int x, int y, int w, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, 1);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_border_width(o, 0, 0);
}

static lv_obj_t *page(lv_obj_t *root)
{
    lv_obj_t *p = lv_obj_create(root);
    lv_obj_set_pos(p, 0, 54);
    lv_obj_set_size(p, UI_W, UI_H - 54);
    lv_obj_set_style_bg_color(p, BG(), 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static void build_header(lv_obj_t *root)
{
    L(root,"BMW 520xd",54,7,&lv_font_montserrat_24,TEXT());
    L(root,"PERFORMANCE MONITOR",55,34,&lv_font_montserrat_14,YELLOW());

    wifiIconBox=lv_obj_create(root);
    lv_obj_set_pos(wifiIconBox,424,9);
    lv_obj_set_size(wifiIconBox,45,32);
    lv_obj_set_style_bg_opa(wifiIconBox,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(wifiIconBox,0,0);
    lv_obj_set_style_pad_all(wifiIconBox,0,0);

    const int heights[4]={6,11,17,24};
    for(int i=0;i<4;++i){
        wifiBars[i]=lv_obj_create(wifiIconBox);
        lv_obj_set_pos(wifiBars[i],4+i*9,26-heights[i]);
        lv_obj_set_size(wifiBars[i],5,heights[i]);
        lv_obj_set_style_bg_color(wifiBars[i],LINE(),0);
        lv_obj_set_style_border_width(wifiBars[i],0,0);
        lv_obj_set_style_radius(wifiBars[i],2,0);
    }

    apLabel=L(root,"AP",438,10,&lv_font_montserrat_20,YELLOW());

    clockLabel = L(root, "--:--", 346, 11, &lv_font_montserrat_14, MUTED());

    canLabel = L(root, "C\nA\nN", 405, 5, &lv_font_montserrat_14, RED());
    lv_obj_set_style_text_align(canLabel, LV_TEXT_ALIGN_CENTER, 0);

    line_obj(root,0,53,480,LINE());
    update_header_connectivity();
}

static lv_obj_t *uiRoot = nullptr;

// Every content page is lazy-built.  At boot only HEADER + HOME exist.
static void build_home(lv_obj_t *r);
static void build_engine(lv_obj_t *r);
static void build_dpf(lv_obj_t *r);
static void build_gear(lv_obj_t *r);
static void build_live(lv_obj_t *r);
static void build_diag(lv_obj_t *r);
static void build_charts(lv_obj_t *r);

static void build_settings_overlay(lv_obj_t *root);
static void build_ota_overlay(lv_obj_t *root);
static void build_display_overlay(lv_obj_t *root);
static void build_touch_overlay(lv_obj_t *root);
static void build_wifi_overlays(lv_obj_t *root);

static void ensure_page_built(PageId id)
{
    if (!uiRoot || pages[id]) return;

    Serial.printf("GUI: lazy page %d start heap=%u\n",
                  (int)id, (unsigned)ESP.getFreeHeap());

    switch(id) {
        case PAGE_HOME:    build_home(uiRoot); break;
        case PAGE_ENGINE:  build_engine(uiRoot); break;
        case PAGE_DPF:     build_dpf(uiRoot); break;
        case PAGE_GEARBOX: build_gear(uiRoot); break;
        case PAGE_LIVE:    build_live(uiRoot); break;
        case PAGE_DIAG:    build_diag(uiRoot); break;
        case PAGE_CHARTS:  build_charts(uiRoot); break;
        default: break;
    }

    Serial.printf("GUI: lazy page %d done heap=%u\n",
                  (int)id, (unsigned)ESP.getFreeHeap());
}

static void show_page(PageId id)
{
    ensure_page_built(id);
    currentPage = id;

    for (int i = 0; i < PAGE_COUNT; i++) {
        if (!pages[i]) continue;
        if (i == id) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void icon(lv_obj_t *p,int x,int y,lv_color_t c,int type)
{
    lv_obj_t *o=lv_obj_create(p);
    lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,34,34);
    lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_pad_all(o,0,0);

    auto rect=[&](int rx,int ry,int rw,int rh){
        lv_obj_t *a=lv_obj_create(o);
        lv_obj_set_pos(a,rx,ry);
        lv_obj_set_size(a,rw,rh);
        lv_obj_set_style_bg_color(a,c,0);
        lv_obj_set_style_border_width(a,0,0);
        lv_obj_set_style_radius(a,1,0);
        return a;
    };

    if(type==0){ // coolant
        rect(15,2,5,20);
        lv_obj_t *d=rect(10,19,15,13);
        lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
    }
    else if(type==1){ // oil can
        lv_obj_t *body=lv_obj_create(o);
        lv_obj_set_pos(body,5,11);
        lv_obj_set_size(body,19,12);
        lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(body,c,0);
        lv_obj_set_style_border_width(body,2,0);
        lv_obj_set_style_radius(body,2,0);
        rect(3,8,9,3);
        rect(23,10,8,3);
        rect(28,8,4,3);
        lv_obj_t *drop=rect(27,22,6,8);
        lv_obj_set_style_radius(drop,LV_RADIUS_CIRCLE,0);
    }
    else if(type==2){ // turbo
        lv_obj_t *a=lv_obj_create(o);
        lv_obj_set_pos(a,4,4);
        lv_obj_set_size(a,26,26);
        lv_obj_set_style_radius(a,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(a,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(a,c,0);
        lv_obj_set_style_border_width(a,2,0);
        rect(16,8,2,11); rect(10,17,9,2);
    }
    else if(type==3){ // air
        for(int i=0;i<3;++i) rect(3,7+i*8,25-i*5,2);
    }
    else if(type==4){ // DPF
        lv_obj_t *a=lv_obj_create(o);
        lv_obj_set_pos(a,7,4);
        lv_obj_set_size(a,20,26);
        lv_obj_set_style_bg_opa(a,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(a,c,0);
        lv_obj_set_style_border_width(a,2,0);
        for(int yy=8;yy<=22;yy+=7)
            for(int xx=11;xx<=21;xx+=5){
                lv_obj_t *d=rect(xx,yy,2,2);
                lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
            }
    }
    else if(type==5){ // gearbox H pattern
        rect(7,5,2,24); rect(25,5,2,24); rect(8,16,18,2);
        int xs[2]={5,23}, ys[2]={3,25};
        for(int xi=0;xi<2;++xi) for(int yi=0;yi<2;++yi){
            lv_obj_t *d=rect(xs[xi],ys[yi],6,6);
            lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
        }
    }
    else if(type==6){ // gear D
        lv_obj_t *a=lv_obj_create(o);
        lv_obj_set_pos(a,4,4); lv_obj_set_size(a,26,26);
        lv_obj_set_style_radius(a,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(a,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(a,c,0);
        lv_obj_set_style_border_width(a,2,0);
        L(o,"D",11,7,&lv_font_montserrat_16,c);
    }
    else if(type==7){ // slip
        rect(4,10,20,2); rect(10,21,20,2);
        rect(22,7,3,8); rect(8,18,3,8);
    }
    else if(type==8){ // lock
        lv_obj_t *body=lv_obj_create(o);
        lv_obj_set_pos(body,7,15); lv_obj_set_size(body,20,15);
        lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(body,c,0);
        lv_obj_set_style_border_width(body,2,0);
        lv_obj_set_style_radius(body,3,0);
        lv_obj_t *sh=lv_obj_create(o);
        lv_obj_set_pos(sh,11,5); lv_obj_set_size(sh,12,14);
        lv_obj_set_style_bg_opa(sh,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(sh,c,0);
        lv_obj_set_style_border_width(sh,2,0);
        lv_obj_set_style_radius(sh,7,0);
    }
}

static lv_obj_t *card(lv_obj_t *p, const char *name, const char *unit,
                 int x, int y, int type, lv_color_t c)
{
    lv_obj_t *b = B(p, x, y, 220, 76);
    icon(b, 13, 22, c, type);

    // Fixed-width two-line title area prevents long labels from colliding
    // with the live value on the right side of the card.
    lv_obj_t *title = L(b, name, 54, strchr(name,'\n') ? 9 : 18,
                        &lv_font_montserrat_14, TEXT());
    lv_obj_set_width(title, 88);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(title, -2, 0);

    lv_obj_t *value = lv_label_create(b);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(value, c, 0);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, unit && unit[0] ? -42 : -15, 2);

    if (unit && unit[0]) {
        lv_obj_t *u = lv_label_create(b);
        lv_label_set_text(u, unit);
        lv_obj_set_style_text_font(u, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(u, MUTED(), 0);
        lv_obj_align(u, LV_ALIGN_RIGHT_MID, -11, 3);
    }

    return value;
}

static lv_obj_t *row(lv_obj_t *p, const char *name, const char *value, int y)
{
    L(p, name, 14, y, &lv_font_montserrat_14, MUTED());

    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, value);
    lv_obj_set_style_text_font(o, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(o, TEXT(), 0);
    lv_obj_align(o, LV_ALIGN_TOP_RIGHT, -14, y - 2);
    return o;
}

static void build_home(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    homeCoolantValue = card(p, "ACQUA", "C", 14, 13, 0, BLUE());
    homeIntakeValue  = card(p, "ARIA\nASPIRATA", "C", 246, 13, 3, BLUE());
    homeOilValue     = card(p, "OLIO\nMOTORE", "C", 14, 96, 1, YELLOW());
    homeDpfValue     = card(p, "DPF\nTRIGGER", "%", 246, 96, 4, YELLOW());
    homeTurboValue   = card(p, "TURBO", "bar", 14, 179, 2, MUTED());
    homeGearboxValue = card(p, "CAMBIO", "C", 246, 179, 5, MUTED());
    pages[PAGE_HOME] = p;
}

static void build_engine(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    L(p, "MOTORE / DDE B47", 16, 11, &lv_font_montserrat_16, YELLOW());
    lv_obj_t *l = B(p, 14, 42, 220, 205);
    lv_obj_t *q = B(p, 246, 42, 220, 205);
    engineRpmValue     = row(l, "Regime motore", "-- rpm", 15);
    engineCoolantValue = row(l, "Temperatura acqua", "-- C", 48);
    engineOilValue     = row(l, "Temperatura olio", "-- C", 81);
    engineIntakeValue  = row(l, "Aria aspirata", "-- C", 114);
    engineAccelValue   = row(l, "Acceleratore", "-- %", 147);

    engineTurboValue   = row(q, "Pressione turbo", "-- bar", 15);
    engineRailValue    = row(q, "Pressione rail", "-- bar", 48);
    engineMafValue     = row(q, "MAF", "-- g/s", 81);
    engineLoadValue    = row(q, "Carico motore", "-- %", 114);
    engineVoltageValue = row(q, "Tensione ECU", "-- V", 147);
    pages[PAGE_ENGINE] = p;
}

static void build_dpf(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    L(p, "FILTRO ANTIPARTICOLATO", 16, 11, &lv_font_montserrat_16, YELLOW());
    lv_obj_t *l = B(p, 14, 42, 220, 205);
    lv_obj_t *q = B(p, 246, 42, 220, 205);

    dpfRegenValue = row(l, "Rigenerazione", "--", 15);
    dpfEgt1Value  = row(l, "EGT sensore 1", "-- C", 48);
    dpfEgt2Value  = row(l, "EGT sensore 2", "-- C", 81);
    dpfEgt3Value  = row(l, "EGT sensore 3", "-- C", 114);
    dpfDiffValue  = row(l, "Press. differenziale", "-- hPa", 147);

    dpfTriggerValue = row(q, "Trigger rigeneraz.", "-- %", 15);
    dpfSootValue    = row(q, "Massa fuliggine", "-- g", 48);
    dpfAshValue     = row(q, "Massa cenere", "-- g", 81);
    dpfNoxValue     = row(q, "NOx sensore 1", "-- ppm", 114);
    dpfLambdaValue  = row(q, "Lambda", "--", 147);
    pages[PAGE_DPF] = p;
}

static void build_gear(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    L(p, "CAMBIO AUTOMATICO ZF8", 16, 11, &lv_font_montserrat_16, YELLOW());
    gearValue    = card(p, "MARCIA", "", 14, 48, 6, YELLOW());
    gearOilValue = card(p, "OLIO\nCAMBIO", "C", 246, 48, 1, YELLOW());
    gearSlipValue= card(p, "SLITTAMENTO", "rpm", 14, 137, 7, MUTED());
    gearLockValue= card(p, "LOCK-UP", "", 246, 137, 8, MUTED());
    pages[PAGE_GEARBOX] = p;
}

static void build_charts(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    L(p, "GRAFICI - ULTIME 2 ORE", 16, 11, &lv_font_montserrat_16, YELLOW());
    historyChart = lv_chart_create(p);
    lv_obj_set_pos(historyChart, 14, 42); lv_obj_set_size(historyChart, 452, 195);
    lv_chart_set_type(historyChart, LV_CHART_TYPE_LINE); lv_chart_set_point_count(historyChart, 120);
    lv_chart_set_range(historyChart, LV_CHART_AXIS_PRIMARY_Y, 0, 150);
    lv_obj_set_style_bg_color(historyChart, PANEL(), 0); lv_obj_set_style_border_color(historyChart, LINE(), 0);
    historyCool=lv_chart_add_series(historyChart, BLUE(), LV_CHART_AXIS_PRIMARY_Y);
    historyOil=lv_chart_add_series(historyChart, YELLOW(), LV_CHART_AXIS_PRIMARY_Y);
    L(p,"Acqua",18,242,&lv_font_montserrat_14,BLUE()); L(p,"Olio",84,242,&lv_font_montserrat_14,YELLOW());
    L(p,"-120 min                         ADESSO",250,242,&lv_font_montserrat_14,MUTED());
    pages[PAGE_CHARTS]=p;
}

static void build_live(lv_obj_t *r)
{
    lv_obj_t *p=page(r); L(p,"LIVE + PEAK HOLD 5s",16,11,&lv_font_montserrat_16,YELLOW());
    const char* names[6]={"RPM","BOOST","RAIL","EGT","DPF dP","ACQUA"};
    for(int i=0;i<6;i++){
        int col=i%3,rowi=i/3; int x=14+col*154,y=42+rowi*103;
        lv_obj_t *b=B(p,x,y,144,94); L(b,names[i],8,5,&lv_font_montserrat_14,MUTED());
        liveVal[i]=L(b,"--",8,27,&lv_font_montserrat_24,TEXT());
        livePeak[i]=L(b,"PEAK --",8,66,&lv_font_montserrat_14,YELLOW());
    }
    pages[PAGE_LIVE]=p;
}

static void diag_scanner_event(lv_event_t *e)
{
    if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
    if(!can_readonly_scan_active()&&!can_bmw_extended_scan_active())can_start_bmw_extended_scan();
}

static void build_diag(lv_obj_t *r)
{
    lv_obj_t *p = page(r);
    L(p, "DIAGNOSTICA SISTEMA", 16, 11, &lv_font_montserrat_16, YELLOW());
    lv_obj_t *b = B(p, 14, 40, 452, 151);
    diagCanValue = row(b, "Interfaccia CAN", "da testare", 12);
    diagDdeValue = row(b, "DDE / motore", "non interrogata", 48);
    diagEgsValue = row(b, "EGS / cambio", "non interrogata", 84);
    diagScannerValue = row(b, "Scanner BMW", "pronto", 120);
    lv_obj_t *scan=lv_button_create(p);lv_obj_set_pos(scan,14,201);lv_obj_set_size(scan,452,48);
    lv_obj_set_style_bg_color(scan,PANEL(),0);lv_obj_set_style_border_color(scan,YELLOW(),0);lv_obj_set_style_border_width(scan,2,0);
    lv_obj_add_event_cb(scan,diag_scanner_event,LV_EVENT_CLICKED,nullptr);
    lv_obj_t *sl=L(scan,"AVVIA SCANNER BMW READ-ONLY",0,0,&lv_font_montserrat_16,YELLOW());lv_obj_center(sl);
    pages[PAGE_DIAG] = p;
}

static void close_settings()
{
    if (settingsOverlay) lv_obj_add_flag(settingsOverlay, LV_OBJ_FLAG_HIDDEN);
    settingsOpen = false;
}

static void open_settings()
{
    if (!settingsOverlay && uiRoot) build_settings_overlay(uiRoot);
    if (settingsOverlay) lv_obj_clear_flag(settingsOverlay, LV_OBJ_FLAG_HIDDEN);
    settingsOpen = true;
}

static void close_touch_test()
{
    if (touchOverlay) lv_obj_add_flag(touchOverlay, LV_OBJ_FLAG_HIDDEN);
    touchTestOpen = false;
}

static void open_touch_test()
{
    if (!touchOverlay && uiRoot) build_touch_overlay(uiRoot);
    if (touchOverlay) lv_obj_clear_flag(touchOverlay, LV_OBJ_FLAG_HIDDEN);
    touchTestOpen = true;
}

static void reboot_board_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (logger_active()) logger_stop();
    Serial.println("GUI: reboot requested"); Serial.flush(); delay(120); ESP.restart();
}

static void close_settings_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) close_settings();
}

static void close_touch_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) close_touch_test();
}

static void logger_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (logger_active()) {
        logger_stop();
    } else {
        logger_start();
    }

    loggerEnabled = logger_active();

    if (loggerStatus) {
        lv_label_set_text(
            loggerStatus,
            loggerEnabled ? "ATTIVO" : "DISATTIVATO"
        );
        lv_obj_set_style_text_color(
            loggerStatus,
            loggerEnabled ? GREEN() : MUTED(),
            0
        );
    }
}

static void power_mode_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    power_set_can_sleep_enabled(!power_can_sleep_enabled());
    if (powerModeStatus) {
        bool automatic = power_can_sleep_enabled();
        lv_label_set_text(powerModeStatus, automatic ? "AUTO CAN" : "BANCO");
        lv_obj_set_style_text_color(powerModeStatus, automatic ? GREEN() : YELLOW(), 0);
    }
}

static void ota_close_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (otaOverlay) lv_obj_add_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void ota_progress(size_t received, size_t total, const char *phase)
{
    int percent = total ? (int)((received * 100ULL) / total) : 0;
    if (otaProgressBar) lv_bar_set_value(otaProgressBar, percent, LV_ANIM_OFF);
    if (otaMessageLabel) {
        if (!strcmp(phase, "DOWNLOAD"))
            lv_label_set_text_fmt(otaMessageLabel, "Download firmware... %d%%\n%u / %u byte", percent,
                                  (unsigned)received, (unsigned)total);
        else lv_label_set_text(otaMessageLabel, phase);
    }
    if (lv_disp) lv_refr_now(lv_disp);
}

static void ota_install_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (otaInstallButton) lv_obj_add_flag(otaInstallButton, LV_OBJ_FLAG_HIDDEN);
    if (otaProgressBar) {
        lv_obj_clear_flag(otaProgressBar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(otaProgressBar, 0, LV_ANIM_OFF);
    }
    String error;
    bool ok = online_ota_install(otaInfo, ota_progress, error);
    if (!ok) {
        lv_label_set_text_fmt(otaTitleLabel, "AGGIORNAMENTO FALLITO");
        lv_label_set_text(otaMessageLabel, error.c_str());
        if (otaProgressBar) lv_obj_add_flag(otaProgressBar, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(otaTitleLabel, "AGGIORNAMENTO COMPLETATO");
    lv_label_set_text(otaMessageLabel, "Firmware verificato. Riavvio in corso...");
    if (lv_disp) lv_refr_now(lv_disp);
    delay(1200);
    ESP.restart();
}

static void ota_check_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!otaOverlay && uiRoot) build_ota_overlay(uiRoot);
    if (!otaOverlay) return;
    lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(otaInstallButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(otaProgressBar, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(otaTitleLabel, "CONTROLLO AGGIORNAMENTI");
    lv_label_set_text(otaMessageLabel, "Connessione al repository GitHub...");
    if (lv_disp) lv_refr_now(lv_disp);

    if (!online_ota_check(otaInfo)) {
        lv_label_set_text(otaTitleLabel, "CONTROLLO NON RIUSCITO");
        lv_label_set_text(otaMessageLabel, otaInfo.error.c_str());
        return;
    }
    if (!otaInfo.updateAvailable) {
        lv_label_set_text(otaTitleLabel, "FIRMWARE AGGIORNATO");
        lv_label_set_text_fmt(otaMessageLabel, "Versione installata: %s\nNessun aggiornamento disponibile.", FW_VERSION);
        return;
    }
    lv_label_set_text(otaTitleLabel, "NUOVO FIRMWARE DISPONIBILE");
    lv_label_set_text_fmt(otaMessageLabel, "Installata: %s   Nuova: %s\n%s\nDimensione: %u byte\n\nScaricare e installare?",
                          FW_VERSION, otaInfo.version.c_str(), otaInfo.notes.c_str(), (unsigned)otaInfo.size);
    lv_obj_clear_flag(otaInstallButton, LV_OBJ_FLAG_HIDDEN);
}

static void touch_test_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    open_touch_test();
}

static lv_obj_t *menu_button(lv_obj_t *parent, const char *title,
                             const char *subtitle, int x, int y,
                             lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, 212, 70);
    lv_obj_set_style_bg_color(b, PANEL(), 0);
    lv_obj_set_style_border_color(b, LINE(), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *titleLabel = L(b, title, 12, 8, &lv_font_montserrat_16, TEXT());
    lv_obj_set_width(titleLabel, 188);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_WRAP);
    lv_obj_t *subLabel = L(b, subtitle, 12, 36, &lv_font_montserrat_14, MUTED());
    lv_obj_set_width(subLabel, 188);
    lv_label_set_long_mode(subLabel, LV_LABEL_LONG_WRAP);

    return b;
}

static void build_ota_overlay(lv_obj_t *root)
{
    otaOverlay = lv_obj_create(root);
    lv_obj_set_pos(otaOverlay, 0, 0);
    lv_obj_set_size(otaOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(otaOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(otaOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(otaOverlay, 0, 0);
    lv_obj_set_style_pad_all(otaOverlay, 0, 0);
    lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_SCROLLABLE);

    otaTitleLabel = L(otaOverlay, "AGGIORNAMENTO OTA", 20, 16,
                      &lv_font_montserrat_20, YELLOW());
    otaMessageLabel = L(otaOverlay, "Premere controllo aggiornamenti", 20, 58,
                        &lv_font_montserrat_16, TEXT());
    lv_obj_set_width(otaMessageLabel, 440);
    lv_label_set_long_mode(otaMessageLabel, LV_LABEL_LONG_WRAP);

    otaProgressBar = lv_bar_create(otaOverlay);
    lv_obj_set_pos(otaProgressBar, 20, 205);
    lv_obj_set_size(otaProgressBar, 440, 22);
    lv_bar_set_range(otaProgressBar, 0, 100);
    lv_obj_add_flag(otaProgressBar, LV_OBJ_FLAG_HIDDEN);

    otaInstallButton = lv_button_create(otaOverlay);
    lv_obj_set_pos(otaInstallButton, 20, 250);
    lv_obj_set_size(otaInstallButton, 285, 50);
    lv_obj_set_style_bg_color(otaInstallButton, GREEN(), 0);
    lv_obj_add_event_cb(otaInstallButton, ota_install_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *installLabel = lv_label_create(otaInstallButton);
    lv_label_set_text(installLabel, "SCARICA E INSTALLA");
    lv_obj_set_style_text_color(installLabel, lv_color_hex(0x000000), 0);
    lv_obj_center(installLabel);
    lv_obj_add_flag(otaInstallButton, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *cancel = lv_button_create(otaOverlay);
    lv_obj_set_pos(cancel, 320, 250);
    lv_obj_set_size(cancel, 140, 50);
    lv_obj_set_style_bg_color(cancel, PANEL2(), 0);
    lv_obj_add_event_cb(cancel, ota_close_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancelLabel = lv_label_create(cancel);
    lv_label_set_text(cancelLabel, "ANNULLA");
    lv_obj_center(cancelLabel);

    lv_obj_add_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
}


static void close_wifi_overlay()
{
    if (wifiOverlay) lv_obj_add_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void update_wifi_status_label()
{
    if (!wifiStatusLabel) return;

    if (wifi_connected()) {
        lv_label_set_text_fmt(
            wifiStatusLabel,
            "Connesso: %s   %s   %d dBm",
            wifi_ssid().c_str(),
            wifi_ip().c_str(),
            wifi_rssi()
        );
        lv_obj_set_style_text_color(wifiStatusLabel, GREEN(), 0);
    } else {
        lv_label_set_text_fmt(
            wifiStatusLabel,
            "Non connesso   AP: %s   %s",
            wifi_ap_ssid().c_str(),
            wifi_ap_ip().c_str()
        );
        lv_obj_set_style_text_color(wifiStatusLabel, MUTED(), 0);
    }
}

static void password_keyboard_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {
        const char *pass = lv_textarea_get_text(wifiPasswordTa);

        wifi_connect(selectedWifiSSID, pass ? String(pass) : String());

        lv_obj_add_flag(wifiPasswordOverlay, LV_OBJ_FLAG_HIDDEN);
        update_wifi_status_label();
    }
    else if (code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(wifiPasswordOverlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_network_event(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    WifiNetworkInfo n = wifi_scan_get(idx);

    selectedWifiSSID = n.ssid;

    lv_textarea_set_text(wifiPasswordTa, "");
    lv_obj_clear_flag(wifiPasswordOverlay, LV_OBJ_FLAG_HIDDEN);

    lv_keyboard_set_textarea(wifiKeyboard, wifiPasswordTa);
    lv_keyboard_set_mode(wifiKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_style_text_font(wifiKeyboard, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(wifiKeyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(wifiKeyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(wifiKeyboard, 2, LV_PART_MAIN);
}

static void populate_wifi_list()
{
    if (!wifiListBox) return;

    lv_obj_clean(wifiListBox);

    int n = wifi_scan();

    if (n <= 0) {
        L(wifiListBox, "Nessuna rete trovata", 10, 10,
          &lv_font_montserrat_14, MUTED());
        return;
    }

    int shown = min(n, 8);

    for (int i = 0; i < shown; ++i) {
        WifiNetworkInfo w = wifi_scan_get(i);

        lv_obj_t *b = lv_button_create(wifiListBox);
        lv_obj_set_width(b, 410);
        lv_obj_set_height(b, 44);
        lv_obj_set_style_bg_color(b, PANEL2(), 0);
        lv_obj_set_style_border_color(b, LINE(), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_add_event_cb(
            b,
            wifi_network_event,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)i
        );

        lv_obj_t *name = lv_label_create(b);
        lv_label_set_text_fmt(
            name,
            "%s   %d dBm%s",
            w.ssid.c_str(),
            w.rssi,
            w.secure ? "  *" : ""
        );
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
    }
}

static void wifi_scan_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_label_set_text(wifiStatusLabel, "Scansione reti...");
    lv_timer_handler();

    populate_wifi_list();
    update_wifi_status_label();
}

static void wifi_forget_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    wifi_forget();
    update_wifi_status_label();
}

static void wifi_open_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!wifiOverlay && uiRoot) build_wifi_overlays(uiRoot);
    if (wifiOverlay) {
        lv_obj_clear_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
        update_wifi_status_label();
        populate_wifi_list();
    }
}

static void wifi_close_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    close_wifi_overlay();
}

static void build_wifi_overlays(lv_obj_t *root)
{
    wifiOverlay = lv_obj_create(root);
    lv_obj_set_pos(wifiOverlay, 0, 0);
    lv_obj_set_size(wifiOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(wifiOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(wifiOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifiOverlay, 0, 0);
    lv_obj_set_style_pad_all(wifiOverlay, 0, 0);
    lv_obj_clear_flag(wifiOverlay, LV_OBJ_FLAG_SCROLLABLE);

    L(wifiOverlay, "WI-FI", 20, 12, &lv_font_montserrat_24, TEXT());

    wifiStatusLabel = L(
        wifiOverlay,
        "Non connesso",
        20,
        43,
        &lv_font_montserrat_14,
        MUTED()
    );

    lv_obj_t *scan = lv_button_create(wifiOverlay);
    lv_obj_set_pos(scan, 20, 68);
    lv_obj_set_size(scan, 130, 42);
    lv_obj_add_event_cb(scan, wifi_scan_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *st = lv_label_create(scan);
    lv_label_set_text(st, "SCANSIONA");
    lv_obj_center(st);

    lv_obj_t *forget = lv_button_create(wifiOverlay);
    lv_obj_set_pos(forget, 160, 68);
    lv_obj_set_size(forget, 150, 42);
    lv_obj_add_event_cb(forget, wifi_forget_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *ft = lv_label_create(forget);
    lv_label_set_text(ft, "DIMENTICA");
    lv_obj_center(ft);

    lv_obj_t *close = lv_button_create(wifiOverlay);
    lv_obj_set_pos(close, 420, 10);
    lv_obj_set_size(close, 42, 38);
    lv_obj_add_event_cb(close, wifi_close_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *ct = lv_label_create(close);
    lv_label_set_text(ct, "X");
    lv_obj_center(ct);

    wifiListBox = lv_obj_create(wifiOverlay);
    lv_obj_set_pos(wifiListBox, 20, 116);
    lv_obj_set_size(wifiListBox, 440, 122);
    lv_obj_set_flex_flow(wifiListBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wifiListBox, 6, 0);
    lv_obj_set_style_pad_row(wifiListBox, 5, 0);
    lv_obj_set_style_bg_color(wifiListBox, PANEL(), 0);
    lv_obj_set_style_border_color(wifiListBox, LINE(), 0);
    wifiClientsLabel = L(
        wifiOverlay,
        "Client AP: nessuno",
        22,
        247,
        &lv_font_montserrat_14,
        MUTED()
    );

    // Password overlay + keyboard.
    wifiPasswordOverlay = lv_obj_create(root);
    lv_obj_set_pos(wifiPasswordOverlay, 0, 0);
    lv_obj_set_size(wifiPasswordOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(wifiPasswordOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(wifiPasswordOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifiPasswordOverlay, 0, 0);
    lv_obj_set_style_pad_all(wifiPasswordOverlay, 0, 0);
    lv_obj_clear_flag(wifiPasswordOverlay, LV_OBJ_FLAG_SCROLLABLE);

    L(wifiPasswordOverlay, "PASSWORD WI-FI", 18, 10,
      &lv_font_montserrat_20, TEXT());

    wifiPasswordTa = lv_textarea_create(wifiPasswordOverlay);
    lv_obj_set_pos(wifiPasswordTa, 18, 42);
    lv_obj_set_size(wifiPasswordTa, 444, 42);
    lv_textarea_set_one_line(wifiPasswordTa, true);
    lv_textarea_set_password_mode(wifiPasswordTa, true);
    lv_textarea_set_placeholder_text(wifiPasswordTa, "Password rete");

    wifiKeyboard = lv_keyboard_create(wifiPasswordOverlay);
    lv_obj_set_pos(wifiKeyboard, 0, 88);
    lv_obj_set_size(wifiKeyboard, UI_W, 232);
    lv_keyboard_set_textarea(wifiKeyboard, wifiPasswordTa);
    lv_keyboard_set_mode(wifiKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_style_text_font(wifiKeyboard, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(wifiKeyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(wifiKeyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(wifiKeyboard, 2, LV_PART_MAIN);
    lv_obj_add_event_cb(
        wifiKeyboard,
        password_keyboard_event,
        LV_EVENT_ALL,
        nullptr
    );

    lv_obj_add_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifiPasswordOverlay, LV_OBJ_FLAG_HIDDEN);
}


static void build_alarm_overlay(lv_obj_t *root)
{
    alarmOverlay = lv_obj_create(root);
    lv_obj_set_pos(alarmOverlay,0,0);
    lv_obj_set_size(alarmOverlay,UI_W,UI_H);
    lv_obj_set_style_bg_color(alarmOverlay,lv_color_hex(0x170000),0);
    lv_obj_set_style_bg_opa(alarmOverlay,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(alarmOverlay,0,0);
    lv_obj_set_style_pad_all(alarmOverlay,0,0);
    lv_obj_clear_flag(alarmOverlay,LV_OBJ_FLAG_SCROLLABLE);

    alarmTitle=L(alarmOverlay,"ALLARME",28,72,&lv_font_montserrat_28,RED());
    alarmMessage=L(alarmOverlay,"--",28,126,&lv_font_montserrat_24,TEXT());
    L(alarmOverlay,"Controllare il parametro",28,177,&lv_font_montserrat_16,MUTED());

    lv_obj_add_flag(alarmOverlay,LV_OBJ_FLAG_HIDDEN);
}


static void close_display_overlay()
{
    if (displayOverlay) lv_obj_add_flag(displayOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void display_close_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) close_display_overlay();
}

static void refresh_display_controls()
{
    if (daySlider) lv_slider_set_value(daySlider, display_day_brightness(), LV_ANIM_OFF);
    if (nightSlider) lv_slider_set_value(nightSlider, display_night_brightness(), LV_ANIM_OFF);
    if (dayValueLabel) lv_label_set_text_fmt(dayValueLabel, "%u%%", display_day_brightness());
    if (nightValueLabel) lv_label_set_text_fmt(nightValueLabel, "%u%%", display_night_brightness());
    if (themeStatusLabel) lv_label_set_text(themeStatusLabel, display_theme_status().c_str());
}

static void display_open_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!displayOverlay && uiRoot) build_display_overlay(uiRoot);
    if (displayOverlay) {
        refresh_display_controls();
        lv_obj_clear_flag(displayOverlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(displayOverlay);
    }
}

static void brightness_slider_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *slider = which == 0 ? daySlider : nightSlider;
    if (!slider) return;
    int v = (int)lv_slider_get_value(slider);
    if (which == 0) display_set_day_brightness((uint8_t)v);
    else display_set_night_brightness((uint8_t)v);
    refresh_display_controls();
}

static void brightness_step_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int op = (int)(intptr_t)lv_event_get_user_data(e);
    bool night = op >= 2;
    int delta = (op & 1) ? 5 : -5;
    int v = night ? display_night_brightness() : display_day_brightness();
    v = max(5, min(100, v + delta));
    if (night) display_set_night_brightness((uint8_t)v);
    else display_set_day_brightness((uint8_t)v);
    refresh_display_controls();
}

static void theme_mode_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    DisplayThemeMode mode = (DisplayThemeMode)(intptr_t)lv_event_get_user_data(e);
    display_set_theme_mode(mode);
    // gui_update() detects the effective theme change and rebuilds safely
    // outside this LVGL event callback.
    if (themeStatusLabel) lv_label_set_text(themeStatusLabel, display_theme_status().c_str());
}

static lv_obj_t *small_button(lv_obj_t *p, const char *txt, int x, int y, int w,
                              lv_event_cb_t cb, intptr_t user)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 38);
    lv_obj_set_style_bg_color(b, PANEL2(), 0);
    lv_obj_set_style_border_color(b, LINE(), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)user);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, TEXT(), 0);
    lv_obj_center(l);
    return b;
}

static void build_display_overlay(lv_obj_t *root)
{
    displayOverlay = lv_obj_create(root);
    lv_obj_set_pos(displayOverlay, 0, 0);
    lv_obj_set_size(displayOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(displayOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(displayOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(displayOverlay, 0, 0);
    lv_obj_set_style_pad_all(displayOverlay, 0, 0);
    lv_obj_clear_flag(displayOverlay, LV_OBJ_FLAG_SCROLLABLE);

    L(displayOverlay, "DISPLAY / TEMA", 20, 11, &lv_font_montserrat_20, TEXT());
    L(displayOverlay, "Luminosita giorno", 20, 50, &lv_font_montserrat_14, MUTED());

    small_button(displayOverlay, "-", 20, 73, 44, brightness_step_event, 0);
    daySlider = lv_slider_create(displayOverlay);
    lv_obj_set_pos(daySlider, 78, 80);
    lv_obj_set_size(daySlider, 292, 22);
    lv_slider_set_range(daySlider, 5, 100);
    lv_obj_add_event_cb(daySlider, brightness_slider_event, LV_EVENT_VALUE_CHANGED, (void *)0);
    small_button(displayOverlay, "+", 384, 73, 44, brightness_step_event, 1);
    dayValueLabel = L(displayOverlay, "--%", 432, 81, &lv_font_montserrat_14, TEXT());

    L(displayOverlay, "Luminosita notte", 20, 116, &lv_font_montserrat_14, MUTED());
    small_button(displayOverlay, "-", 20, 139, 44, brightness_step_event, 2);
    nightSlider = lv_slider_create(displayOverlay);
    lv_obj_set_pos(nightSlider, 78, 146);
    lv_obj_set_size(nightSlider, 292, 22);
    lv_slider_set_range(nightSlider, 5, 100);
    lv_obj_add_event_cb(nightSlider, brightness_slider_event, LV_EVENT_VALUE_CHANGED, (void *)1);
    small_button(displayOverlay, "+", 384, 139, 44, brightness_step_event, 3);
    nightValueLabel = L(displayOverlay, "--%", 432, 147, &lv_font_montserrat_14, TEXT());

    L(displayOverlay, "Tema", 20, 188, &lv_font_montserrat_14, MUTED());
    small_button(displayOverlay, "AUTO",   20, 211, 126, theme_mode_event, DISPLAY_THEME_AUTO);
    small_button(displayOverlay, "GIORNO", 176, 211, 126, theme_mode_event, DISPLAY_THEME_DAY);
    small_button(displayOverlay, "NOTTE",  332, 211, 126, theme_mode_event, DISPLAY_THEME_NIGHT);

    themeStatusLabel = L(displayOverlay, "--", 20, 263, &lv_font_montserrat_14, YELLOW());
    L(displayOverlay, "AUTO: BMW se mappato; fallback orario locale 07-19",
      20, 286, &lv_font_montserrat_14, MUTED());

    lv_obj_t *close = lv_button_create(displayOverlay);
    lv_obj_set_pos(close, 426, 8);
    lv_obj_set_size(close, 42, 38);
    lv_obj_add_event_cb(close, display_close_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *ct = lv_label_create(close); lv_label_set_text(ct, "X"); lv_obj_center(ct);

    refresh_display_controls();
    lv_obj_add_flag(displayOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void build_settings_overlay(lv_obj_t *root)
{
    settingsOverlay = lv_obj_create(root);
    lv_obj_set_pos(settingsOverlay, 0, 0);
    lv_obj_set_size(settingsOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(settingsOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(settingsOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(settingsOverlay, 0, 0);
    lv_obj_set_style_pad_all(settingsOverlay, 0, 0);
    lv_obj_clear_flag(settingsOverlay, LV_OBJ_FLAG_SCROLLABLE);

    L(settingsOverlay, "IMPOSTAZIONI", 104, 13, &lv_font_montserrat_24, TEXT());
    L(settingsOverlay, "Swipe verso il basso per chiudere", 104, 43,
      &lv_font_montserrat_14, MUTED());

    lv_obj_t *close = lv_button_create(settingsOverlay);
    lv_obj_set_pos(close, 422, 10);
    lv_obj_set_size(close, 42, 38);
    lv_obj_set_style_bg_color(close, PANEL2(), 0);
    lv_obj_set_style_border_color(close, LINE(), 0);
    lv_obj_set_style_border_width(close, 1, 0);
    lv_obj_add_event_cb(close, close_settings_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *x = lv_label_create(close);
    lv_label_set_text(x, "X");
    lv_obj_center(x);

    menu_button(
        settingsOverlay,
        "WI-FI",
        "Rete e connessione",
        18, 62,
        wifi_open_event
    );

    lv_obj_t *logger = menu_button(
        settingsOverlay,
        "DATA LOGGER",
        "Avvia o arresta log",
        250, 62,
        logger_event
    );

    loggerStatus = lv_label_create(logger);
    lv_label_set_text(loggerStatus, "DISATTIVATO");
    lv_obj_set_style_text_font(loggerStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(loggerStatus, MUTED(), 0);
    lv_obj_align(loggerStatus, LV_ALIGN_TOP_RIGHT, -8, 8);

    menu_button(
        settingsOverlay,
        "DISPLAY",
        "Luminosita e tema",
        18, 143,
        display_open_event
    );

    lv_obj_t *powerMode = menu_button(
        settingsOverlay,
        "DEEP SLEEP CAN",
        "Auto CAN o banco",
        250, 143,
        power_mode_event
    );

    powerModeStatus = lv_label_create(powerMode);
    lv_label_set_text(powerModeStatus, power_can_sleep_enabled() ? "AUTO CAN" : "BANCO");
    lv_obj_set_style_text_font(powerModeStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(powerModeStatus, power_can_sleep_enabled() ? GREEN() : YELLOW(), 0);
    lv_obj_align(powerModeStatus, LV_ALIGN_TOP_RIGHT, -8, 8);

    menu_button(
        settingsOverlay,
        "AGGIORNAMENTO OTA",
        "Controlla nuovo firmware",
        18, 224,
        ota_check_event
    );

    menu_button(
        settingsOverlay,
        "RIAVVIA SCHEDA",
        "Riavvio software",
        250, 224,
        reboot_board_event
    );

    lv_obj_add_flag(settingsOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void build_touch_overlay(lv_obj_t *root)
{
    touchOverlay = lv_obj_create(root);
    lv_obj_set_pos(touchOverlay, 0, 0);
    lv_obj_set_size(touchOverlay, UI_W, UI_H);
    lv_obj_set_style_bg_color(touchOverlay, BG(), 0);
    lv_obj_set_style_bg_opa(touchOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(touchOverlay, 0, 0);
    lv_obj_set_style_pad_all(touchOverlay, 0, 0);
    lv_obj_clear_flag(touchOverlay, LV_OBJ_FLAG_SCROLLABLE);

    L(touchOverlay, "TEST TOUCH", 22, 13, &lv_font_montserrat_24, TEXT());
    L(touchOverlay, "Tocca bordi, angoli e centro", 22, 43,
      &lv_font_montserrat_14, MUTED());

    lv_obj_t *close = lv_button_create(touchOverlay);
    lv_obj_set_pos(close, 422, 10);
    lv_obj_set_size(close, 42, 38);
    lv_obj_set_style_bg_color(close, PANEL2(), 0);
    lv_obj_set_style_border_color(close, LINE(), 0);
    lv_obj_set_style_border_width(close, 1, 0);
    lv_obj_add_event_cb(close, close_touch_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *x = lv_label_create(close);
    lv_label_set_text(x, "X");
    lv_obj_center(x);

    lv_obj_t *zone = B(touchOverlay, 22, 76, 436, 190);
    L(zone, "AREA ATTIVA", 16, 12, &lv_font_montserrat_14, MUTED());

    touchCoord = L(zone, "X --   Y --", 160, 76, &lv_font_montserrat_20, TEXT());
    touchCountLabel = L(zone, "Tocchi: 0", 174, 110, &lv_font_montserrat_14, MUTED());

    touchDot = lv_obj_create(touchOverlay);
    lv_obj_set_size(touchDot, 10, 10);
    lv_obj_set_style_radius(touchDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touchDot, YELLOW(), 0);
    lv_obj_set_style_border_width(touchDot, 0, 0);
    lv_obj_set_pos(touchDot, 235, 155);

    L(touchOverlay, "Swipe giu per tornare alle impostazioni",
      112, 286, &lv_font_montserrat_14, MUTED());

    lv_obj_add_flag(touchOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void process_gesture(GestureAction g)
{
    if (g == G_NONE) return;

    // Touch test has highest priority.
    if (touchTestOpen) {
        if (g == G_DOWN) {
            close_touch_test();
            open_settings();
        }
        return;
    }

    // Settings drawer.
    if (settingsOpen) {
        if (g == G_DOWN) close_settings();
        return;
    }

    // Normal pages.
    if (g == G_UP) {
        open_settings();
        return;
    }

    if (g == G_LEFT) {
        int next = ((int)currentPage + 1) % PAGE_COUNT;
        show_page((PageId)next);
    } else if (g == G_RIGHT) {
        int prev = ((int)currentPage - 1 + PAGE_COUNT) % PAGE_COUNT;
        show_page((PageId)prev);
    }
}


static void reset_gui_refs()
{
    memset(pages, 0, sizeof(pages));
    wifiIconBox=nullptr; memset(wifiBars,0,sizeof(wifiBars)); apLabel=nullptr;
    wifiClientsLabel=nullptr; canLabel=nullptr; clockLabel=nullptr;
    alarmOverlay=nullptr; alarmTitle=nullptr; alarmMessage=nullptr;
    settingsOverlay=nullptr; touchOverlay=nullptr; touchDot=nullptr; touchCoord=nullptr;
    powerModeStatus=nullptr;
    otaOverlay=nullptr; otaTitleLabel=nullptr; otaMessageLabel=nullptr;
    otaProgressBar=nullptr; otaInstallButton=nullptr;
    touchCountLabel=nullptr; loggerStatus=nullptr; wifiOverlay=nullptr;
    displayOverlay=nullptr; daySlider=nullptr; nightSlider=nullptr;
    dayValueLabel=nullptr; nightValueLabel=nullptr; themeStatusLabel=nullptr;
    wifiListBox=nullptr; wifiStatusLabel=nullptr; wifiPasswordOverlay=nullptr;
    wifiPasswordTa=nullptr; wifiKeyboard=nullptr;
    homeCoolantValue=homeIntakeValue=homeOilValue=nullptr;
    homeDpfValue=homeTurboValue=homeGearboxValue=nullptr;
    engineRpmValue=engineCoolantValue=engineOilValue=nullptr;
    engineIntakeValue=engineTurboValue=engineRailValue=nullptr;
    engineLoadValue=engineVoltageValue=engineMafValue=engineAccelValue=nullptr;
    dpfRegenValue=dpfEgt1Value=dpfEgt2Value=nullptr;
    dpfDiffValue=dpfTriggerValue=dpfSootValue=nullptr;
    dpfAshValue=dpfAvgDistanceValue=dpfEgt3Value=dpfNoxValue=dpfLambdaValue=nullptr;
    gearValue=gearOilValue=gearSlipValue=gearLockValue=nullptr;
    for(int i=0;i<6;i++){liveVal[i]=livePeak[i]=nullptr;} historyChart=nullptr; historyCool=historyOil=nullptr;
    diagCanValue=diagDdeValue=diagEgsValue=diagScannerValue=nullptr;
    settingsOpen=false; touchTestOpen=false; touchWasPressed=false;
}

static void build_ui_tree(lv_obj_t *root)
{
    lv_obj_set_style_bg_color(root, BG(), 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    Serial.printf("GUI: header build start heap=%u\n", (unsigned)ESP.getFreeHeap());
    build_header(root);
    Serial.printf("GUI: header build OK heap=%u\n", (unsigned)ESP.getFreeHeap());

    Serial.printf("GUI: HOME build start heap=%u\n", (unsigned)ESP.getFreeHeap());
    build_home(root);
    Serial.printf("GUI: HOME build OK heap=%u\n", (unsigned)ESP.getFreeHeap());

    // Keep only the lightweight alarm overlay resident. Settings, display,
    // touch and Wi-Fi overlays are created on first use.
    build_alarm_overlay(root);
}

static void rebuild_for_theme()
{
    if (!lv_disp) return;
    PageId keep = currentPage;
    lv_obj_t *root = lv_screen_active();
    uiRoot = root;
    lv_obj_clean(root);
    reset_gui_refs();
    build_ui_tree(root);
    show_page(keep);
    lv_timer_handler();
}

void gui_init()
{
    Serial.printf("GUI: init start heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());

    lv_buffer = (uint16_t *)ps_malloc(UI_W * UI_H * sizeof(uint16_t));
    if (!lv_buffer) {
        Serial.println("ERROR LVGL buffer");
        return;
    }

    lv_init();
    Serial.printf("GUI: LVGL initialized heap=%u\n", (unsigned)ESP.getFreeHeap());

    lv_disp = lv_display_create(UI_W, UI_H);
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lv_disp, flush_cb);
    lv_display_set_buffers(
        lv_disp,
        lv_buffer,
        nullptr,
        UI_W * UI_H * sizeof(uint16_t),
        LV_DISPLAY_RENDER_MODE_FULL
    );

    lv_touch = lv_indev_create();
    lv_indev_set_type(lv_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lv_touch, touch_cb);
    Serial.printf("GUI: display+touch objects OK heap=%u\n", (unsigned)ESP.getFreeHeap());

    guiNightTheme = display_theme_is_night();
    lv_obj_t *root = lv_screen_active();
    uiRoot = root;
    reset_gui_refs();
    build_ui_tree(root);

    show_page(PAGE_HOME);

    // Give LVGL several immediate passes so the first HOME frame is guaranteed
    // to reach the panel before setup continues with Wi-Fi/CAN/SD.
    Serial.println("GUI: first render start");
    for (int i = 0; i < 4; ++i) {
        lv_tick_inc(5);
        lv_timer_handler();
        delay(2);
    }
    Serial.println("GUI: first render OK");

    Serial.printf("GUI: HOME ready heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

static void label_text_if_changed(lv_obj_t *label,const char *text)
{
    if(!label) return;
    const char *old=lv_label_get_text(label);
    if(old && strcmp(old,text)==0) return;
    lv_label_set_text(label,text);
}

static void set_card_value(lv_obj_t *label, float value, int decimals=0)
{
    if (!label) return;
    char buf[24];
    if (isnan(value)) strcpy(buf,"--");
    else if(decimals) snprintf(buf,sizeof(buf),"%.1f",value);
    else snprintf(buf,sizeof(buf),"%.0f",value);
    label_text_if_changed(label,buf);
}

static void set_row_label(lv_obj_t *label, float value, const char *suffix, int decimals)
{
    if (!label) return;
    char buf[32];
    if (isnan(value)) strcpy(buf,"--");
    else if(decimals) snprintf(buf,sizeof(buf),"%.1f %s",value,suffix);
    else snprintf(buf,sizeof(buf),"%.0f %s",value,suffix);
    label_text_if_changed(label,buf);
}

static void update_live_values()
{
    VehicleData &v=vehicle_data();

    // Only touch labels on the visible page. The old implementation rewrote
    // every hidden page every 400 ms, causing unnecessary full-frame flushes.
    switch(currentPage){
        case PAGE_HOME:
            set_card_value(homeCoolantValue,v.coolant);
            set_card_value(homeIntakeValue,v.intake);
            set_card_value(homeOilValue,v.oil);
            set_card_value(homeDpfValue,v.dpf,1);
            set_card_value(homeTurboValue,v.turbo,1);
            set_card_value(homeGearboxValue,v.gearbox);
            break;

        case PAGE_ENGINE:
            set_row_label(engineRpmValue,v.rpm,"rpm",0);
            set_row_label(engineCoolantValue,v.coolant,"C",0);
            set_row_label(engineOilValue,v.oil,"C",0);
            set_row_label(engineIntakeValue,v.intake,"C",0);
            set_row_label(engineAccelValue,v.accelerator,"%",0);
            set_row_label(engineTurboValue,v.turbo,"bar",1);
            set_row_label(engineRailValue,v.railBar,"bar",0);
            set_row_label(engineMafValue,v.maf,"g/s",1);
            set_row_label(engineLoadValue,v.engineLoad,"%",0);
            set_row_label(engineVoltageValue,v.voltage,"V",1);
            break;

        case PAGE_DPF:
            if(dpfRegenValue){
                const char *txt="--";
                if(v.dpfRegenKnown) txt=v.dpfRegen ? (v.dpfRegenTypeKnown&&v.dpfRegenActiveType?"ATTIVA":"IN CORSO") : "NO";
                label_text_if_changed(dpfRegenValue,txt);
                lv_obj_set_style_text_color(dpfRegenValue,v.dpfRegen?YELLOW():TEXT(),0);
            }
            set_row_label(dpfEgt1Value,v.egt1,"C",0);
            set_row_label(dpfEgt2Value,v.egt2,"C",0);
            set_row_label(dpfEgt3Value,v.egt3,"C",0);
            set_row_label(dpfDiffValue,v.dpfDiffPressureHpa,"hPa",1);
            set_row_label(dpfTriggerValue,v.dpfNormalizedTrigger,"%",1);
            set_row_label(dpfSootValue,v.dpfSootMassG,"g",1);
            set_row_label(dpfAshValue,v.dpfAshMassG,"g",1);
            set_row_label(dpfNoxValue,v.nox1Ppm,"ppm",0);
            set_row_label(dpfLambdaValue,v.lambda1,"",2);
            break;

        case PAGE_GEARBOX:
            if(gearValue){char b[8];if(v.gear<0)strcpy(b,"--");else snprintf(b,sizeof(b),"%d",v.gear);label_text_if_changed(gearValue,b);}
            set_card_value(gearOilValue,v.gearbox);
            set_card_value(gearSlipValue,v.converterSlipRpm);
            if(gearLockValue)label_text_if_changed(gearLockValue,!v.lockupKnown?"--":(v.lockup?"ON":"OFF"));
            break;

        case PAGE_LIVE: {
            float vals[6]={v.rpm,v.turbo,v.railBar,v.egt1,v.dpfDiffPressureHpa,v.coolant};
            const char* units[6]={"rpm","bar","bar","C","hPa","C"};
            static float held[6]={NAN,NAN,NAN,NAN,NAN,NAN};
            static uint32_t holdUntil[6]={0};
            for(int i=0;i<6;i++){
                char b[32]; if(isnan(vals[i])) strcpy(b,"--"); else if(i==1||i==4) snprintf(b,sizeof(b),"%.1f",vals[i]); else snprintf(b,sizeof(b),"%.0f",vals[i]);
                label_text_if_changed(liveVal[i],b);
                if(!isnan(vals[i]) && (isnan(held[i]) || vals[i]>held[i] || millis()>holdUntil[i])) {held[i]=vals[i];holdUntil[i]=millis()+5000;}
                char q[40]; if(isnan(held[i])) strcpy(q,"PEAK --"); else if(i==1||i==4) snprintf(q,sizeof(q),"PEAK %.1f %s",held[i],units[i]); else snprintf(q,sizeof(q),"PEAK %.0f %s",held[i],units[i]);
                label_text_if_changed(livePeak[i],q);
            }
            break;
        }

        case PAGE_DIAG:
            if(diagCanValue){
                char b[32];
                if(can_driver_ready()) snprintf(b,sizeof(b),"attiva - %lu req/s",(unsigned long)can_request_rate());
                else strcpy(b,"errore driver");
                label_text_if_changed(diagCanValue,b);
            }
            if(diagDdeValue)label_text_if_changed(diagDdeValue,v.ddeDetected?"DDE 7E8 OK":"nessuna risposta");
            if(diagEgsValue)label_text_if_changed(diagEgsValue,v.egsDetected?"EGS 0x18 OK":"nessuna risposta");
            if(diagScannerValue){
                String x=can_bmw_extended_scan_active()?String("in corso ")+String(can_bmw_scanner_progress())+"%":can_bmw_extended_scan_result();
                label_text_if_changed(diagScannerValue,x.c_str());
            }
            break;

        default: break;
    }
}

void gui_update()
{
    static uint32_t lastConnectivityUi = 0;
    static uint32_t lastValuesUi = 0;

    bool wantedNight = display_theme_is_night();
    if (wantedNight != guiNightTheme) {
        guiNightTheme = wantedNight;
        rebuild_for_theme();
    }

    if (pendingGesture != G_NONE) {
        GestureAction g = pendingGesture;
        pendingGesture = G_NONE;
        process_gesture(g);
    }

    uint32_t now = millis();

    // Live values refresh a bit faster than the header/connectivity block:
    // this is the actual purpose of the dashboard, the CAN task already
    // prioritizes live PIDs; 100ms gives a ~10 Hz visual ceiling
    // without re-rendering LVGL labels on every single loop() iteration.
    if (now - lastValuesUi >= 100) {
        lastValuesUi = now;
        update_live_values();
    }


    static uint32_t lastChartUi=0;
    if(currentPage==PAGE_CHARTS && historyChart && now-lastChartUi>=2000){
        lastChartUi=now; lv_chart_set_all_value(historyChart,historyCool,LV_CHART_POINT_NONE); lv_chart_set_all_value(historyChart,historyOil,LV_CHART_POINT_NONE);
        int hc=history_count();
        for(int i=0;i<120;i++){
            int idx = hc ? (i*hc)/120 : -1; HistoryPoint hp;
            lv_chart_set_next_value(historyChart,historyCool,(idx>=0&&history_get(idx,hp)&&!isnan(hp.coolant))?(int32_t)hp.coolant:LV_CHART_POINT_NONE);
            lv_chart_set_next_value(historyChart,historyOil,(idx>=0&&history_get(idx,hp)&&!isnan(hp.oil))?(int32_t)hp.oil:LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(historyChart);
    }
    if (now - lastConnectivityUi >= 1000) {
        lastConnectivityUi = now;
        update_header_connectivity();
        update_wifi_status_label();
        update_ap_clients_label();
        if (themeStatusLabel) lv_label_set_text(themeStatusLabel, display_theme_status().c_str());
        if (clockLabel) lv_label_set_text(clockLabel,time_hhmm().c_str());

        const AlarmState &as=alarm_state();
        if(alarmOverlay && as.level!=ALARM_NONE){
            lv_label_set_text(alarmTitle,as.title.c_str());
            lv_label_set_text(alarmMessage,as.message.c_str());
            lv_obj_set_style_text_color(alarmTitle,as.level==ALARM_CRITICAL?RED():YELLOW(),0);
            lv_obj_clear_flag(alarmOverlay,LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(alarmOverlay);
        }else if(alarmOverlay){
            lv_obj_add_flag(alarmOverlay,LV_OBJ_FLAG_HIDDEN);
        }
        if (canLabel) {
            lv_obj_set_style_text_color(
                canLabel,
                can_is_online() ? GREEN() : RED(),
                0
            );
        }
        if (loggerStatus) {
            loggerEnabled = logger_active();
            lv_label_set_text(
                loggerStatus,
                loggerEnabled ? "ATTIVO" : "DISATTIVATO"
            );
            lv_obj_set_style_text_color(
                loggerStatus,
                loggerEnabled ? GREEN() : MUTED(),
                0
            );
        }
        if (powerModeStatus) {
            bool automatic = power_can_sleep_enabled();
            lv_label_set_text(powerModeStatus, automatic ? "AUTO CAN" : "BANCO");
            lv_obj_set_style_text_color(powerModeStatus, automatic ? GREEN() : YELLOW(), 0);
        }
    }
}

void gui_set_can_status(bool online)
{
    (void)online;
}


const char *gui_current_page_name()
{
    static const char *names[PAGE_COUNT] = {
        "HOME", "MOTORE", "DPF", "ZF8", "LIVE", "DIAG", "GRAFICI"
    };
    return names[currentPage];
}
