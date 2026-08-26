#include "trip_manager.h"
#include "vehicle_data.h"
#include "time_manager.h"
#include <math.h>
static TripStats s={};
static float vmax2(float a,float b){if(isnan(b))return a;if(isnan(a))return b;return b>a?b:a;}
void trip_reset_peaks(){s.maxCoolant=NAN;s.maxOil=NAN;s.maxIntake=NAN;s.maxTurbo=NAN;s.maxDpf=NAN;s.maxGearbox=NAN;}
void trip_begin(){s={};trip_reset_peaks();}
void trip_loop(){
 VehicleData &v=vehicle_data();
 if(v.canOnline&&!s.active){s.active=true;s.startMillis=millis();s.startEpoch=time_epoch();trip_reset_peaks();}
 if(!v.canOnline&&s.active)s.active=false;
 if(s.active){
  s.durationSec=(millis()-s.startMillis)/1000;
  s.maxCoolant=vmax2(s.maxCoolant,v.coolant);s.maxOil=vmax2(s.maxOil,v.oil);
  s.maxIntake=vmax2(s.maxIntake,v.intake);s.maxTurbo=vmax2(s.maxTurbo,v.turbo);
  s.maxDpf=vmax2(s.maxDpf,v.dpf);s.maxGearbox=vmax2(s.maxGearbox,v.gearbox);
 }
}
const TripStats& trip_stats(){return s;}
