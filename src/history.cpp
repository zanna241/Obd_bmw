#include "history.h"
#include "vehicle_data.h"
// Two-hour rolling history, sampled every 10 s: 720 points.
static constexpr int CAPACITY=720;
static constexpr uint32_t SAMPLE_MS=10000;
static HistoryPoint points[CAPACITY];
static int head=0,count=0; static uint32_t lastSample=0;
void history_begin(){head=0;count=0;lastSample=0;}
static void add_point(){VehicleData &v=vehicle_data();HistoryPoint p={};p.ageMinutes=0;p.coolant=v.coolant;p.oil=v.oil;p.intake=v.intake;p.turbo=v.turbo;p.dpf=v.dpf;p.gearbox=v.gearbox;p.rpm=v.rpm;p.speed=v.speed;p.rail=v.railBar;p.dpfDiff=v.dpfDiffPressureHpa;p.egt1=v.egt1;p.egt2=v.egt2;points[head]=p;head=(head+1)%CAPACITY;if(count<CAPACITY)count++;}
void history_loop(){uint32_t now=millis();if(now-lastSample<SAMPLE_MS)return;lastSample=now;add_point();}
int history_count(){return count;}
bool history_get(int logicalIndex,HistoryPoint &out){if(logicalIndex<0||logicalIndex>=count)return false;int idx=(head-count+logicalIndex+CAPACITY)%CAPACITY;out=points[idx];out.ageMinutes=(uint32_t)((count-1-logicalIndex)*SAMPLE_MS/60000UL);return true;}
