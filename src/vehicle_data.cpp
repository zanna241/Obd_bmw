#include "vehicle_data.h"

static VehicleData data;

VehicleData& vehicle_data()
{
    return data;
}

void vehicle_data_set_can(bool online)
{
    data.canOnline = online;
}


void vehicle_data_invalidate_live()
{
    // Clear only live/session measurements. Discovery counters and diagnostic
    // identity flags remain available for post-drive inspection.
    data.coolant=data.oil=data.intake=data.turbo=data.dpf=data.gearbox=NAN;
    data.rpm=data.speed=data.maf=data.engineLoad=data.baro=data.ambient=NAN;
    data.voltage=data.throttle=data.accelerator=data.engineRuntimeSec=NAN;
    data.boostAbsKpa=data.boostTargetKpa=data.railBar=data.railTargetBar=NAN;
    data.fuelTemp=data.egrCommanded=data.egrActual=data.egrError=NAN;
    data.egt1=data.egt2=data.egt3=data.egt4=NAN;
    data.nox1Ppm=data.nox2Ppm=data.lambda1=data.lambda2=NAN;
    data.dpfDiffPressureHpa=data.dpfInletPressureKpa=data.dpfOutletPressureKpa=NAN;
    data.dpfNormalizedTrigger=data.dpfAvgRegenTimeMin=data.dpfAvgRegenDistanceKm=NAN;
    data.dpfSootMassG=data.dpfAshMassG=data.dpfTempIn=data.dpfTempOut=NAN;
    data.distanceSinceRegenKm=data.dpfRemainingLifeKm=NAN;
    data.dpfRegenKnown=data.dpfRegenTypeKnown=false;
    data.gear=-1; data.gearboxInputRpm=data.gearboxOutputRpm=data.converterSlipRpm=data.gearboxTorqueNm=NAN;
    data.lockupKnown=false; data.canOnline=false; data.obdActive=false;
}
