#include "alarm_manager.h"
#include "vehicle_data.h"
#include <Preferences.h>
#include <math.h>
static Preferences prefs;
static AlarmState st={};
static float cLim=110.0f,oLim=130.0f,gLim=115.0f,dLim=750.0f;
void alarm_begin(){prefs.begin("alarms",false);cLim=prefs.getFloat("coolant",cLim);oLim=prefs.getFloat("oil",oLim);gLim=prefs.getFloat("gearbox",gLim);dLim=prefs.getFloat("dpf",dLim);}
static void setA(AlarmLevel l,const char*t,const String&m){if(l<st.level)return;st.level=l;st.title=t;st.message=m;}
void alarm_loop(){
 // Was running on every main-loop iteration (~1000x/sec via delay(1)),
 // reallocating 4 String objects each time even when nothing changed.
 // Vehicle temperatures move on the order of seconds, not milliseconds,
 // and the CAN task itself only refreshes each PID every ~250ms, so
 // throttling this to 5x/sec removes needless heap churn (relevant for
 // long drives where fragmentation from repeated allocations adds up)
 // without any perceptible change in alarm responsiveness.
 static uint32_t lastCheck=0;
 uint32_t now=millis();
 if(now-lastCheck<200)return;
 lastCheck=now;

 st.level=ALARM_NONE;st.title="";st.message="";
 VehicleData &v=vehicle_data();
 if(!isnan(v.coolant)&&v.coolant>=cLim)setA(ALARM_CRITICAL,"TEMPERATURA ACQUA",String(v.coolant,1)+" C");
 if(!isnan(v.oil)&&v.oil>=oLim)setA(ALARM_CRITICAL,"TEMPERATURA OLIO",String(v.oil,1)+" C");
 if(!isnan(v.gearbox)&&v.gearbox>=gLim)setA(ALARM_WARNING,"TEMPERATURA CAMBIO",String(v.gearbox,1)+" C");
 if(!isnan(v.egt1)&&v.egt1>=dLim)setA(ALARM_WARNING,"TEMPERATURA SCARICO / DPF",String(v.egt1,1)+" C");
}
const AlarmState& alarm_state(){return st;}
float alarm_limit_coolant(){return cLim;} float alarm_limit_oil(){return oLim;}
float alarm_limit_gearbox(){return gLim;} float alarm_limit_dpf(){return dLim;}
void alarm_set_limits(float c,float o,float g,float d){cLim=c;oLim=o;gLim=g;dLim=d;prefs.putFloat("coolant",cLim);prefs.putFloat("oil",oLim);prefs.putFloat("gearbox",gLim);prefs.putFloat("dpf",dLim);}
