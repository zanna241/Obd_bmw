#include "can.h"
#include "hardware.h"
#include "vehicle_data.h"
#include "logger.h"
#include "version.h"
#include "discovery.h"

#include <driver/twai.h>
#include <esp_timer.h>
#include <math.h>

static bool driverReady=false;
static bool recovering=false;
static uint32_t lastFrameMillis=0,totalFrames=0,framesThisSecond=0,currentFps=0,fpsTimer=0;
static uint32_t txRequests=0,txFailed=0,obdReplies=0,busOffCount=0,rxQueueFullAlerts=0;
static uint32_t requestsThisSecond=0,repliesThisSecond=0,currentRequestRate=0,currentReplyRate=0;
static uint32_t lastRequestMs=0;
static String lastStatus="inizializzazione";
static String stateText="STOPPED";

// Explicitly user-triggered read-only ECU presence scan. It only sends UDS
// TesterPresent (3E 00) to 7E0..7E7 and records any 7E8..7EF responder.
static bool readonlyScan=false;
static uint8_t readonlyScanIndex=0;
static uint8_t readonlyScanMask=0;
static uint32_t readonlyScanLastMs=0;

static String readonlyScanResult="non eseguita";

// BMW F-series Extended 11-bit ISO-TP discovery.
// Addresses corroborated by BMW community diagnostics:
// 12 DME/DDE, 18 EGS, 5E GWS, 60 KOMBI.
// This first implementation is deliberately presence-only and read-only.
static constexpr uint8_t BMW_TESTER_ADDR=0xF1;
static constexpr uint32_t BMW_TESTER_CAN_ID=0x600u | BMW_TESTER_ADDR; // 0x6F1
static const uint8_t bmwTargets[]={0x12,0x18,0x5E,0x60};
static const char *bmwTargetNames[]={"DDE/DME","EGS/ZF8","GWS","KOMBI"};
static constexpr uint8_t BMW_TARGET_COUNT=sizeof(bmwTargets)/sizeof(bmwTargets[0]);
static bool bmwExtScan=false;
static uint8_t bmwExtScanIndex=0;
static uint8_t bmwExtRequestIndex=0;
static uint8_t bmwExtScanMask=0;
static uint32_t bmwExtScanLastMs=0;
static bool bmwExtWaiting=false;
static String bmwExtScanResult="non eseguita";

static BmwScannerEcuInfo bmwScannerInfo[BMW_TARGET_COUNT];
static const uint16_t bmwReadDids[]={0x0000,0xF190,0xF187,0xF189};
static constexpr uint8_t BMW_READ_COUNT=sizeof(bmwReadDids)/sizeof(bmwReadDids[0]);

struct BmwExtIsoTpSession {
    bool active=false;
    uint8_t source=0;
    uint16_t expected=0;
    uint16_t used=0;
    uint8_t nextSeq=1;
    uint32_t lastMs=0;
    uint8_t data[256];
};
static BmwExtIsoTpSession bmwExtIso;

static int bmw_target_index(uint8_t addr){
    for(uint8_t i=0;i<BMW_TARGET_COUNT;i++) if(bmwTargets[i]==addr) return i;
    return -1;
}


static constexpr uint32_t OBD_FUNCTIONAL_ID=0x7DF;
static constexpr uint32_t DDE_REQUEST_ID=0x7E0;
static constexpr uint32_t DDE_RESPONSE_ID=0x7E8;
static constexpr uint32_t AFTERTREAT_REQUEST_ID=0x7E4; // paired with responder 0x7EC seen in Car Scanner logs
static constexpr uint32_t MIN_REQUEST_GAP_MS=70;

struct PollItem {
    uint8_t pid;
    uint16_t intervalMs;
    uint8_t priority;
    uint32_t lastMs;
};

// The priority scheduler intentionally gives dashboard values more bandwidth,
// while slow/static values are still refreshed periodically.
static PollItem pollItems[] = {
    {0x0C, 180,5,0}, // RPM
    {0x0D, 250,4,0}, // speed
    {0x70, 250,5,0}, // boost actual/target
    {0x6D, 300,5,0}, // fuel rail pressure
    {0x10, 350,4,0}, // MAF
    {0x04, 400,4,0}, // calculated load
    {0x49, 400,3,0}, // accelerator pedal D
    {0x7A, 450,5,0}, // DPF pressure
    {0x78, 650,4,0}, // EGT sensors
    {0x05,1000,3,0}, // coolant
    {0x42,1500,2,0}, // ECU voltage
    {0x69,1500,2,0}, // EGR
    {0x8B,1500,3,0}, // diesel aftertreatment status
    {0x83,2000,2,0}, // NOx sensors
    {0x8C,2000,2,0}, // wide range O2 / lambda
    {0x0F,2000,2,0}, // intake temp
    {0x1F,5000,1,0}, // runtime
    {0x33,5000,1,0}, // BARO
    {0x46,5000,1,0}  // ambient
};
static constexpr int POLL_COUNT=sizeof(pollItems)/sizeof(pollItems[0]);
static bool discoverySent=false;
static const uint8_t aftertreatProbePids[]={0x78,0x85,0x88};
static uint8_t aftertreatProbeIndex=0;
static uint32_t lastAftertreatProbeMs=0;

static constexpr int CATALOG_MAX=256;
static CanCatalogEntry catalog[CATALOG_MAX];
static int catalogCount=0;

struct IsoTpSession {
    uint32_t rxId=0;
    uint16_t expected=0;
    uint16_t used=0;
    uint8_t nextSeq=1;
    uint32_t lastMs=0;
    bool active=false;
    uint8_t data[192];
};
static IsoTpSession isoSessions[3];

static CanCatalogEntry *find_or_add(uint32_t id,bool ext){
    for(int i=0;i<catalogCount;++i) if(catalog[i].id==id&&catalog[i].extended==ext) return &catalog[i];
    if(catalogCount>=CATALOG_MAX) return nullptr;
    CanCatalogEntry &e=catalog[catalogCount++]; memset(&e,0,sizeof(e)); e.id=id;e.extended=ext; return &e;
}
void can_catalog_clear(){ memset(catalog,0,sizeof(catalog));catalogCount=0; }

void can_stats_reset(){
    totalFrames=framesThisSecond=currentFps=txRequests=txFailed=obdReplies=busOffCount=rxQueueFullAlerts=0;
    requestsThisSecond=repliesThisSecond=currentRequestRate=currentReplyRate=0;
    lastFrameMillis=0;fpsTimer=millis();can_catalog_clear();discovery_reset();
    VehicleData &v=vehicle_data();v.canOnline=false;v.obdActive=false;v.lastObdReplyMs=0;
    lastStatus="statistiche azzerate";
}

static void update_state_text(){
    if(!driverReady){stateText="DRIVER ERROR";return;}
    twai_status_info_t s={}; if(twai_get_status_info(&s)!=ESP_OK){stateText="STATUS ERROR";return;}
    switch(s.state){case TWAI_STATE_STOPPED:stateText="STOPPED";break;case TWAI_STATE_RUNNING:stateText="RUNNING";break;case TWAI_STATE_BUS_OFF:stateText="BUS-OFF";break;case TWAI_STATE_RECOVERING:stateText="RECOVERING";break;default:stateText="UNKNOWN";}
}

static bool send_can(uint32_t id,const uint8_t *data,uint8_t len){
    if(!driverReady||recovering)return false;
    twai_status_info_t s={}; if(twai_get_status_info(&s)!=ESP_OK||s.state!=TWAI_STATE_RUNNING)return false;
    twai_message_t m={};m.identifier=id;m.data_length_code=len;m.ss=0;memcpy(m.data,data,len);
    esp_err_t err=twai_transmit(&m,0);
    if(err==ESP_OK){
        txRequests++;requestsThisSecond++;
        logger_log_can_tx((uint64_t)esp_timer_get_time(),id,false,false,len,data);
        return true;
    }
    txFailed++;return false;
}

static bool send_mode01_to(uint32_t requestId,uint8_t pid){
    uint8_t d[8]={0x02,0x01,pid,0,0,0,0,0};
    bool ok=send_can(requestId,d,8);
    if(ok){char b[40];snprintf(b,sizeof(b),"TX %03lX 01%02X",(unsigned long)requestId,pid);lastStatus=b;}
    return ok;
}

static bool send_mode01(uint8_t pid,bool functional=false){
    return send_mode01_to(functional?OBD_FUNCTIONAL_ID:DDE_REQUEST_ID,pid);
}

static bool send_tester_present(uint32_t requestId){
    uint8_t d[8]={0x02,0x3E,0x00,0,0,0,0,0};
    bool ok=send_can(requestId,d,8);
    if(ok){
        char b[48];
        snprintf(b,sizeof(b),"SCAN TX %03lX 3E00",(unsigned long)requestId);
        lastStatus=b;
    }
    return ok;
}

static bool send_bmw_extended_tester_present(uint8_t target){
    // Extended addressing adds target as byte 0; ISO-TP SF PCI is byte 1.
    // UDS payload = 3E 00. Remaining bytes are zero padded.
    uint8_t d[8]={target,0x02,0x3E,0x00,0,0,0,0};
    bool ok=send_can(BMW_TESTER_CAN_ID,d,8);
    if(ok){
        char b[64];
        snprintf(b,sizeof(b),"BMW EXT TX 6F1 -> %02X 3E00",target);
        lastStatus=b;
    }
    return ok;
}

static bool send_bmw_extended_did(uint8_t target,uint16_t did){
    uint8_t d[8]={target,0x03,0x22,(uint8_t)(did>>8),(uint8_t)did,0,0,0};
    bool ok=send_can(BMW_TESTER_CAN_ID,d,8);
    if(ok){
        char b[72];snprintf(b,sizeof(b),"BMW SCAN TX %02X 22%04X",target,did);lastStatus=b;
    }
    return ok;
}

static void send_bmw_extended_flow_control(uint8_t target){
    uint8_t d[8]={target,0x30,0x00,0x00,0,0,0,0};
    send_can(BMW_TESTER_CAN_ID,d,8);
}

static String scanner_value(const uint8_t *data,uint16_t len){
    while(len && (data[len-1]==0 || data[len-1]==0xFF || data[len-1]==' ')) len--;
    if(!len) return "";
    bool printable=true;
    for(uint16_t i=0;i<len;i++) if(data[i]<32 || data[i]>126){printable=false;break;}
    String out;
    if(printable){for(uint16_t i=0;i<len;i++)out+=(char)data[i];return out;}
    char b[4];
    for(uint16_t i=0;i<len;i++){if(i)out+=' ';snprintf(b,sizeof(b),"%02X",data[i]);out+=b;}
    return out;
}

static void decode_bmw_extended_payload(uint8_t source,const uint8_t *p,uint16_t n){
    int idx=bmw_target_index(source);if(idx<0||!bmwExtScan||n<1)return;
    BmwScannerEcuInfo &info=bmwScannerInfo[idx];
    info.present=true;bmwExtScanMask|=(uint8_t)(1u<<idx);bmwExtWaiting=false;
    if(source==0x12)vehicle_data().ddeDetected=true;
    if(source==0x18)vehicle_data().egsDetected=true;
    if(p[0]==0x7E){info.lastResponse="TesterPresent OK";}
    else if(p[0]==0x62 && n>=3){
        uint16_t did=((uint16_t)p[1]<<8)|p[2];String value=scanner_value(p+3,n-3);
        if(did==0xF190)info.vin=value;
        else if(did==0xF187)info.partNumber=value;
        else if(did==0xF189)info.softwareVersion=value;
        info.lastResponse="DID "+String(did,HEX)+" = "+value;
    }else if(p[0]==0x7F && n>=3){
        info.lastResponse="NRC 0x"+String(p[2],HEX)+" per SID 0x"+String(p[1],HEX);
    }else info.lastResponse="Risposta SID 0x"+String(p[0],HEX);
    logger_mark_event("BMW_SCANNER_RESP target=0x"+String(source,HEX)+" "+info.lastResponse);
}

static void observe_bmw_extended_response(uint32_t id,const twai_message_t &m){
    if(!bmwExtScan || id<0x600 || id>0x6FF || m.data_length_code<3) return;
    uint8_t source=(uint8_t)(id & 0xFF);
    int idx=bmw_target_index(source);
    if(idx<0) return;

    // BMW Extended 11-bit addressing: response first byte must target tester F1.
    if(m.data[0]!=BMW_TESTER_ADDR) return;

    uint8_t pci=m.data[1],type=pci>>4;
    if(type==0x0){uint8_t len=pci&0x0F;if(len&&len+2<=m.data_length_code)decode_bmw_extended_payload(source,&m.data[2],len);return;}
    if(type==0x1){
        uint16_t expected=((uint16_t)(pci&0x0F)<<8)|m.data[2];
        if(expected>sizeof(bmwExtIso.data))return;
        bmwExtIso=BmwExtIsoTpSession();bmwExtIso.active=true;bmwExtIso.source=source;
        bmwExtIso.expected=expected;bmwExtIso.lastMs=millis();
        uint8_t copy=min((int)expected,(int)m.data_length_code-3);
        if(copy){memcpy(bmwExtIso.data,&m.data[3],copy);bmwExtIso.used=copy;}
        send_bmw_extended_flow_control(source);return;
    }
    if(type==0x2 && bmwExtIso.active && bmwExtIso.source==source){
        uint8_t seq=pci&0x0F;if(seq!=(bmwExtIso.nextSeq&0x0F)){bmwExtIso.active=false;return;}
        bmwExtIso.nextSeq++;bmwExtIso.lastMs=millis();
        uint16_t remaining=bmwExtIso.expected-bmwExtIso.used;
        uint8_t copy=min((int)remaining,(int)m.data_length_code-2);
        if(copy){memcpy(bmwExtIso.data+bmwExtIso.used,&m.data[2],copy);bmwExtIso.used+=copy;}
        if(bmwExtIso.used>=bmwExtIso.expected){decode_bmw_extended_payload(source,bmwExtIso.data,bmwExtIso.expected);bmwExtIso.active=false;}
    }
}

static void send_flow_control(uint32_t rxId){
    if(rxId<0x7E8||rxId>0x7EF)return;
    uint8_t d[8]={0x30,0x00,0x00,0,0,0,0,0};
    send_can(rxId-8,d,8);
}

static bool isotp_busy(){
    for(auto &s:isoSessions) if(s.active) return true;
    return false;
}

static IsoTpSession* session_for(uint32_t id){
    for(auto &s:isoSessions) if(s.active&&s.rxId==id)return &s;
    for(auto &s:isoSessions) if(!s.active){s.rxId=id;return &s;}
    return &isoSessions[0];
}

static int16_t be16s(uint8_t a,uint8_t b){return (int16_t)(((uint16_t)a<<8)|b);} 
static uint16_t be16(uint8_t a,uint8_t b){return ((uint16_t)a<<8)|b;}

static float lambda_value(uint8_t hi,uint8_t lo){
    uint16_t raw=be16(hi,lo);
    if(raw==0 || raw==0xFFFF) return NAN;
    return raw*0.000122f;
}

static float nox_value(uint8_t hi,uint8_t lo){
    uint16_t raw=be16(hi,lo);
    if(raw==0xFFFF) return NAN;
    return (float)raw;
}

static void decode_obd_payload(uint32_t id,const uint8_t *p,uint16_t n){
    if(n<1)return;
    VehicleData &v=vehicle_data();

    if(p[0]==0x7E){
        // Positive response to UDS TesterPresent (0x3E). Count only this
        // response type for the read-only presence scan so a late OBD reply
        // cannot create a false-positive responder bit.
        if(readonlyScan && id>=0x7E8 && id<=0x7EF)
            readonlyScanMask |= (uint8_t)(1u << (id-0x7E8));
        lastStatus="UDS TesterPresent response";
        return;
    }
    if(p[0]==0x7F){
        // Negative response layout: 7F <requested service> <NRC>. A negative
        // answer to 3E still proves that the addressed ECU is present.
        if(readonlyScan && n>=2 && p[1]==0x3E && id>=0x7E8 && id<=0x7EF)
            readonlyScanMask |= (uint8_t)(1u << (id-0x7E8));
        lastStatus="diagnostic negative response";
        return;
    }
    if(n<2 || p[0]!=0x41)return;

    obdReplies++;repliesThisSecond++;
    v.obdActive=true;v.lastObdReplyMs=millis();
    if(id==DDE_RESPONSE_ID){v.ddeDetected=true;v.lastDdeReplyMs=millis();}
    if(id!=DDE_RESPONSE_ID)return;

    uint8_t pid=p[1];const uint8_t *d=p+2;uint16_t len=n-2;
    uint32_t now=millis();

    switch(pid){
        case 0x04: if(len>=1){v.engineLoad=d[0]*100.0f/255.0f;v.lastFastDataMs=now;}break;
        case 0x05: if(len>=1)v.coolant=(float)d[0]-40.0f;break;
        case 0x0C: if(len>=2){v.rpm=be16(d[0],d[1])/4.0f;v.lastFastDataMs=now;}break;
        case 0x0D: if(len>=1){v.speed=d[0];v.lastFastDataMs=now;}break;
        case 0x0F: if(len>=1)v.intake=(float)d[0]-40.0f;break;
        case 0x10: if(len>=2){v.maf=be16(d[0],d[1])/100.0f;v.lastFastDataMs=now;}break;
        case 0x1F: if(len>=2)v.engineRuntimeSec=(float)be16(d[0],d[1]);break;
        case 0x33: if(len>=1){v.baro=d[0]; if(!isnan(v.boostAbsKpa))v.turbo=(v.boostAbsKpa-v.baro)/100.0f;}break;
        case 0x42: if(len>=2)v.voltage=be16(d[0],d[1])/1000.0f;break;
        case 0x46: if(len>=1)v.ambient=(float)d[0]-40.0f;break;
        case 0x49: if(len>=1){v.accelerator=d[0]/2.55f;v.lastFastDataMs=now;}break;
        case 0x69:
            if(len>=7){uint8_t s=d[0]; if(s&0x01)v.egrCommanded=d[1]/2.55f; if(s&0x02)v.egrActual=d[2]/2.55f; if(s&0x04)v.egrError=d[3]/1.28f-100.0f;}
            break;
        case 0x6D:
            if(len>=5){uint8_t s=d[0]; if(s&0x01)v.railTargetBar=be16(d[1],d[2])*0.1f; if(s&0x02)v.railBar=be16(d[3],d[4])*0.1f; if((s&0x04)&&len>=6)v.fuelTemp=(float)d[5]-40.0f;v.lastFastDataMs=now;}
            break;
        case 0x70:
            if(len>=5){uint8_t s=d[0]; if(s&0x01)v.boostTargetKpa=be16(d[1],d[2])/32.0f; if(s&0x02)v.boostAbsKpa=be16(d[3],d[4])/32.0f; if(!isnan(v.boostAbsKpa)&&!isnan(v.baro))v.turbo=(v.boostAbsKpa-v.baro)/100.0f;v.lastFastDataMs=now;}
            break;
        case 0x78:
            if(len>=3){uint8_t s=d[0]; if(s&0x01)v.egt1=be16(d[1],d[2])/10.0f-40.0f; if((s&0x02)&&len>=5)v.egt2=be16(d[3],d[4])/10.0f-40.0f; if((s&0x04)&&len>=7)v.egt3=be16(d[5],d[6])/10.0f-40.0f; if((s&0x08)&&len>=9)v.egt4=be16(d[7],d[8])/10.0f-40.0f;v.lastDpfDataMs=now;}
            break;
        case 0x7A:
            if(len>=7){uint8_t s=d[0]; if(s&0x01)v.dpfDiffPressureHpa=be16s(d[1],d[2])*0.1f; if(s&0x02)v.dpfInletPressureKpa=be16(d[3],d[4])*0.01f; if(s&0x04)v.dpfOutletPressureKpa=be16(d[5],d[6])*0.01f;v.lastDpfDataMs=now;}
            break;
        case 0x83:
            if(len>=9){uint8_t s=d[0]; if(s&0x01)v.nox1Ppm=nox_value(d[1],d[2]); if(s&0x02)v.nox2Ppm=nox_value(d[3],d[4]);v.lastDpfDataMs=now;}
            break;
        case 0x8B:
            if(len>=7){uint8_t support=d[0],state=d[1];
                v.dpfRegenKnown=support&0x01; if(v.dpfRegenKnown)v.dpfRegen=state&0x01;
                v.dpfRegenTypeKnown=support&0x02; if(v.dpfRegenTypeKnown)v.dpfRegenActiveType=state&0x02;
                if(support&0x10){v.dpfNormalizedTrigger=d[2]/2.55f;v.dpf=v.dpfNormalizedTrigger;}
                if(support&0x20)v.dpfAvgRegenTimeMin=be16(d[3],d[4]);
                if(support&0x40)v.dpfAvgRegenDistanceKm=be16(d[5],d[6]);
                v.lastDpfDataMs=now;
            }
            break;
        case 0x8C:
            if(len>=17){uint8_t s=d[0]; if(s&0x10)v.lambda1=lambda_value(d[9],d[10]); if(s&0x20)v.lambda2=lambda_value(d[11],d[12]);v.lastDpfDataMs=now;}
            break;
        default:break;
    }
    lastStatus="OBD-II DDE OK";
}

static void handle_isotp(uint32_t id,const twai_message_t &m){
    if(m.data_length_code==0)return;
    uint8_t type=m.data[0]>>4;
    if(type==0x0){uint8_t l=m.data[0]&0x0F;if(l>7)l=7;if(l&&m.data_length_code>1)decode_obd_payload(id,&m.data[1],min((int)l,(int)m.data_length_code-1));return;}
    if(type==0x1){
        IsoTpSession *s=session_for(id);s->active=true;s->rxId=id;s->expected=((m.data[0]&0x0F)<<8)|m.data[1];s->used=0;s->nextSeq=1;s->lastMs=millis();
        if(s->expected>sizeof(s->data)){s->active=false;return;}
        int copy=min((int)s->expected,min(6,(int)m.data_length_code-2));if(copy>0){memcpy(s->data,&m.data[2],copy);s->used=copy;}send_flow_control(id);return;
    }
    if(type==0x2){
        IsoTpSession *s=nullptr;for(auto &x:isoSessions)if(x.active&&x.rxId==id){s=&x;break;}if(!s)return;
        uint8_t seq=m.data[0]&0x0F;if(seq!=(s->nextSeq&0x0F)){s->active=false;return;}s->nextSeq++;s->lastMs=millis();
        int remaining=s->expected-s->used;int copy=min(remaining,min(7,(int)m.data_length_code-1));if(copy>0&&s->used+copy<=sizeof(s->data)){memcpy(s->data+s->used,&m.data[1],copy);s->used+=copy;}
        if(s->used>=s->expected){decode_obd_payload(id,s->data,s->expected);s->active=false;}return;
    }
}

static void service_bus_state(){
    if(!driverReady)return;twai_status_info_t s={};if(twai_get_status_info(&s)!=ESP_OK)return;
    if(s.state==TWAI_STATE_BUS_OFF&&!recovering){busOffCount++;recovering=true;lastStatus="BUS-OFF recovery";twai_initiate_recovery();return;}
    if(recovering&&s.state==TWAI_STATE_STOPPED){if(twai_start()==ESP_OK){recovering=false;lastStatus="CAN restarted";}return;}update_state_text();
}

static int select_poll_item(uint32_t now){
    int best=-1; float bestScore=1.0f;
    for(int i=0;i<POLL_COUNT;++i){
        uint32_t age=now-pollItems[i].lastMs;
        if(age<pollItems[i].intervalMs) continue;
        float score=((float)age/(float)pollItems[i].intervalMs)*(1.0f+0.18f*pollItems[i].priority);
        if(score>bestScore){bestScore=score;best=i;}
    }
    return best;
}

void can_init(){
    VehicleData &v=vehicle_data();v.canOnline=false;v.obdActive=false;can_catalog_clear();discovery_begin();
    twai_general_config_t g=TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN,(gpio_num_t)CAN_RX_PIN,TWAI_MODE_NORMAL);g.rx_queue_len=512;g.tx_queue_len=24;
    g.alerts_enabled=TWAI_ALERT_BUS_OFF|TWAI_ALERT_BUS_RECOVERED|TWAI_ALERT_TX_FAILED|TWAI_ALERT_RX_QUEUE_FULL|TWAI_ALERT_ERR_PASS|TWAI_ALERT_ABOVE_ERR_WARN;
    twai_timing_config_t t=TWAI_TIMING_CONFIG_500KBITS();twai_filter_config_t f=TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t err=twai_driver_install(&g,&t,&f);if(err!=ESP_OK){lastStatus="errore install TWAI";return;}err=twai_start();if(err!=ESP_OK){twai_driver_uninstall();lastStatus="errore start TWAI";return;}
    driverReady=true;fpsTimer=millis();lastRequestMs=millis();lastStatus="attesa DDE";update_state_text();
    Serial.printf("CAN %s: PROMISCUOUS ACCEPT-ALL 11bit 500k TX=%d RX=%d, RXQ=512 + ISO-TP\n",FW_VERSION,CAN_TX_PIN,CAN_RX_PIN);
}

void can_update(){
    if(!driverReady){vehicle_data_set_can(false);return;}
    service_bus_state();
    uint32_t alerts=0;if(twai_read_alerts(&alerts,0)==ESP_OK&&alerts){if(alerts&TWAI_ALERT_TX_FAILED)txFailed++; if(alerts&TWAI_ALERT_RX_QUEUE_FULL)rxQueueFullAlerts++;}
    uint32_t now=millis();

    // Receive first. This prevents issuing a new diagnostic request while a
    // previous multi-frame response is waiting in the RX queue.
    twai_message_t msg;int processed=0; const uint64_t rxDrainStart=(uint64_t)esp_timer_get_time();
    // Promiscuous discovery: drain every accepted frame. A time budget prevents
    // a saturated bus from starving LVGL/web, while the 512-frame queue absorbs bursts.
    while(twai_receive(&msg,0)==ESP_OK){processed++;uint32_t nowMs=millis();uint64_t nowUs=(uint64_t)esp_timer_get_time();lastFrameMillis=nowMs;totalFrames++;framesThisSecond++;
        bool ext=msg.extd!=0;CanCatalogEntry *e=find_or_add(msg.identifier,ext);if(e){e->count++;e->lastMillis=nowMs;e->dlc=msg.data_length_code;memset(e->data,0,8);memcpy(e->data,msg.data,min((int)msg.data_length_code,8));}
        discovery_observe(msg.identifier,ext,msg.data_length_code,msg.data);
        logger_log_can_frame(nowUs,msg.identifier,ext,msg.rtr!=0,msg.data_length_code,msg.data);
        observe_bmw_extended_response(msg.identifier,msg);
        if(msg.identifier>=0x7E8&&msg.identifier<=0x7EF)handle_isotp(msg.identifier,msg);
        if(processed>=1024 || ((uint64_t)esp_timer_get_time()-rxDrainStart)>6000ULL) break;
    }

    for(auto &s:isoSessions)if(s.active&&now-s.lastMs>500)s.active=false;
    if(bmwExtIso.active&&now-bmwExtIso.lastMs>900){bmwExtIso.active=false;bmwExtWaiting=false;}

    if(!discoverySent&&now>3000&&!isotp_busy()) discoverySent=send_mode01(0x00,true);

    if(readonlyScan){
        // Keep normal PID polling paused during the short standard OBD scan.
        if(!recovering && !isotp_busy() && now-readonlyScanLastMs>=300){
            if(readonlyScanIndex<8){
                send_tester_present(0x7E0u + readonlyScanIndex);
                readonlyScanIndex++;
                readonlyScanLastMs=now;
            }else{
                readonlyScan=false;
                readonlyScanResult="responders mask 0x" + String(readonlyScanMask,HEX);
                logger_mark_event("ECU_SCAN_END mask=0x" + String(readonlyScanMask,HEX) + " heap=" + String(ESP.getFreeHeap()));
            }
        }
    }else if(bmwExtScan){
        // Full read-only inventory. Each request waits for its response or a
        // timeout; normal dashboard polling is paused but RX/web stay alive.
        if(bmwExtWaiting && now-bmwExtScanLastMs>1000){bmwExtWaiting=false;bmwExtIso.active=false;}
        if(!recovering && !isotp_busy() && !bmwExtWaiting && now-bmwExtScanLastMs>=120){
            if(bmwExtScanIndex<BMW_TARGET_COUNT){
                uint8_t target=bmwTargets[bmwExtScanIndex];
                uint16_t did=bmwReadDids[bmwExtRequestIndex];
                bool sent=did?send_bmw_extended_did(target,did):send_bmw_extended_tester_present(target);
                if(sent){bmwExtWaiting=true;bmwExtScanLastMs=now;}
                if(++bmwExtRequestIndex>=BMW_READ_COUNT){bmwExtRequestIndex=0;bmwExtScanIndex++;}
            }else{
                bmwExtScan=false;
                String found="";
                for(uint8_t i=0;i<BMW_TARGET_COUNT;i++){
                    if(bmwExtScanMask & (1u<<i)){
                        if(found.length()) found += ",";
                        found += String(bmwTargetNames[i])+"(0x"+String(bmwTargets[i],HEX)+")";
                    }
                }
                if(!found.length()) found="nessuno";
                bmwExtScanResult="mask 0x"+String(bmwExtScanMask,HEX)+" : "+found;
                logger_mark_event("BMW_SCANNER_END "+bmwExtScanResult+" heap="+String(ESP.getFreeHeap()));
            }
        }
    }else if(!recovering&&!isotp_busy()&&now-lastRequestMs>=MIN_REQUEST_GAP_MS){
        // A very slow secondary probe targets responder 7EC, which Car Scanner
        // identified as an emissions/aftertreatment ECU. Only standard Mode 01
        // PIDs that its support bitmap advertised are requested. Responses are
        // kept in RAW/discovery even when no decoder is assigned yet.
        if(now-lastAftertreatProbeMs>=3000){
            uint8_t pid=aftertreatProbePids[aftertreatProbeIndex++ % (sizeof(aftertreatProbePids)/sizeof(aftertreatProbePids[0]))];
            if(send_mode01_to(AFTERTREAT_REQUEST_ID,pid)){lastAftertreatProbeMs=now;lastRequestMs=now;}
        }else{
            int i=select_poll_item(now);
            if(i>=0&&send_mode01(pollItems[i].pid,false)){pollItems[i].lastMs=now;lastRequestMs=now;}
        }
    }

    if(now-fpsTimer>=1000){
        currentFps=framesThisSecond;framesThisSecond=0;
        currentRequestRate=requestsThisSecond;requestsThisSecond=0;
        currentReplyRate=repliesThisSecond;repliesThisSecond=0;
        fpsTimer=now;
    }

    VehicleData &v=vehicle_data();bool online=v.lastObdReplyMs&&now-v.lastObdReplyMs<2500;vehicle_data_set_can(online);if(!online&&v.lastObdReplyMs)v.obdActive=false;
}

bool can_is_online(){return vehicle_data().canOnline;} bool can_driver_ready(){return driverReady;} bool obd_is_active(){return vehicle_data().obdActive;}
uint32_t can_total_frames(){return totalFrames;} uint32_t can_frames_per_second(){return currentFps;} uint32_t can_tx_requests(){return txRequests;} uint32_t can_tx_failed(){return txFailed;} uint32_t can_obd_replies(){return obdReplies;} uint32_t can_bus_off_count(){return busOffCount;}
uint32_t can_request_rate(){return currentRequestRate;} uint32_t can_reply_rate(){return currentReplyRate;}
uint32_t can_rx_missed(){if(!driverReady)return 0;twai_status_info_t s={};return twai_get_status_info(&s)==ESP_OK?s.rx_missed_count:0;}
uint32_t can_bus_errors(){if(!driverReady)return 0;twai_status_info_t s={};return twai_get_status_info(&s)==ESP_OK?s.bus_error_count:0;}
int can_catalog_count(){return catalogCount;} bool can_catalog_get(int i,CanCatalogEntry &o){if(i<0||i>=catalogCount)return false;o=catalog[i];return true;}
String obd_last_status(){return lastStatus;} String can_state_text(){update_state_text();return stateText;}


void can_start_readonly_scan()
{
    if(readonlyScan || bmwExtScan) return;
    readonlyScan=true;
    readonlyScanIndex=0;
    readonlyScanMask=0;
    readonlyScanLastMs=millis()-400;
    readonlyScanResult="scansione in corso";
    logger_mark_event("ECU_SCAN_START 7E0-7E7 TesterPresent heap=" + String(ESP.getFreeHeap()));
}

bool can_readonly_scan_active(){ return readonlyScan; }
uint8_t can_readonly_scan_response_mask(){ return readonlyScanMask; }
String can_readonly_scan_result(){ return readonlyScanResult; }

bool can_bus_recent_activity(uint32_t withinMs)
{
    if (!lastFrameMillis) return false;
    return (uint32_t)(millis() - lastFrameMillis) <= withinMs;
}
uint32_t can_last_frame_ms(){ return lastFrameMillis; }

void can_start_bmw_extended_scan()
{
    if(readonlyScan || bmwExtScan) return;
    bmwExtScan=true;
    bmwExtScanIndex=0;
    bmwExtRequestIndex=0;
    bmwExtScanMask=0;
    bmwExtWaiting=false;
    bmwExtIso=BmwExtIsoTpSession();
    for(uint8_t i=0;i<BMW_TARGET_COUNT;i++){
        bmwScannerInfo[i]=BmwScannerEcuInfo();bmwScannerInfo[i].address=bmwTargets[i];bmwScannerInfo[i].name=bmwTargetNames[i];
    }
    bmwExtScanLastMs=millis()-500;
    bmwExtScanResult="scansione in corso";
    logger_mark_event("BMW_SCANNER_START source=F1 tx=6F1 targets=12,18,5E,60 DID=F190,F187,F189 READ_ONLY");
}
bool can_bmw_extended_scan_active(){ return bmwExtScan; }
uint8_t can_bmw_extended_scan_response_mask(){ return bmwExtScanMask; }
String can_bmw_extended_scan_result(){ return bmwExtScanResult; }
uint8_t can_bmw_scanner_progress(){
    if(!bmwExtScan)return 100;
    uint16_t done=(uint16_t)bmwExtScanIndex*BMW_READ_COUNT+bmwExtRequestIndex;
    return (uint8_t)min(99,(int)(done*100/(BMW_TARGET_COUNT*BMW_READ_COUNT)));
}
int can_bmw_scanner_ecu_count(){return BMW_TARGET_COUNT;}
bool can_bmw_scanner_ecu_get(int index,BmwScannerEcuInfo &out){if(index<0||index>=BMW_TARGET_COUNT)return false;out=bmwScannerInfo[index];return true;}
