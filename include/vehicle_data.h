#pragma once
#include <Arduino.h>

// Single shared data model. Display, web, logger/history and alarms all read
// from this structure so a value cannot diverge between interfaces.
struct VehicleData {
    // Generic / engine OBD-II values confirmed on this vehicle.
    float coolant = NAN;
    float oil = NAN;              // BMW-specific mapping still pending
    float intake = NAN;
    float turbo = NAN;            // gauge boost, bar (derived from PID 70 + BARO)
    float dpf = NAN;              // normalized DPF regen trigger, % (PID 8B)
    float gearbox = NAN;          // ATF temperature, BMW EGS mapping pending

    float rpm = NAN;
    float speed = NAN;
    float maf = NAN;
    float engineLoad = NAN;
    float baro = NAN;
    float ambient = NAN;
    float voltage = NAN;
    float throttle = NAN;
    float accelerator = NAN;
    float engineRuntimeSec = NAN;

    // Advanced standard diesel OBD-II values supported by the B47 responder.
    float boostAbsKpa = NAN;
    float boostTargetKpa = NAN;
    float railBar = NAN;
    float railTargetBar = NAN;
    float fuelTemp = NAN;
    float egrCommanded = NAN;
    float egrActual = NAN;
    float egrError = NAN;
    float egt1 = NAN;
    float egt2 = NAN;
    float egt3 = NAN;
    float egt4 = NAN;

    // Additional standard diesel sensors confirmed as supported by the DDE bitmap.
    float nox1Ppm = NAN;
    float nox2Ppm = NAN;
    float lambda1 = NAN;
    float lambda2 = NAN;

    // DPF / aftertreatment. Values backed by SAE PIDs are populated now;
    // BMW-only quantities stay NAN until D70BX7A0 mappings are validated.
    float dpfDiffPressureHpa = NAN;
    float dpfInletPressureKpa = NAN;
    float dpfOutletPressureKpa = NAN;
    float dpfNormalizedTrigger = NAN;
    float dpfAvgRegenTimeMin = NAN;
    float dpfAvgRegenDistanceKm = NAN;
    float dpfSootMassG = NAN;     // BMW-specific pending
    float dpfAshMassG = NAN;      // BMW-specific pending
    float dpfTempIn = NAN;        // exact sensor->DPF mapping pending
    float dpfTempOut = NAN;       // exact sensor->DPF mapping pending
    float distanceSinceRegenKm = NAN; // BMW-specific pending
    float dpfRemainingLifeKm = NAN;   // BMW-specific pending
    uint32_t successfulRegens = 0;
    bool successfulRegensKnown = false;
    bool dpfRegen = false;
    bool dpfRegenKnown = false;
    bool dpfRegenActiveType = false;
    bool dpfRegenTypeKnown = false;

    // ZF8 / EGS placeholders are fully wired to UI/API but are deliberately
    // not fabricated until the EGS diagnostic mapping is identified.
    int gear = -1;
    float gearboxInputRpm = NAN;
    float gearboxOutputRpm = NAN;
    float converterSlipRpm = NAN;
    float gearboxTorqueNm = NAN;
    bool lockup = false;
    bool lockupKnown = false;

    // Diagnostic state.
    bool canOnline = false;
    bool obdActive = false;
    bool ddeDetected = false;
    bool egsDetected = false;
    uint32_t lastObdReplyMs = 0;
    uint32_t lastDdeReplyMs = 0;
    uint32_t lastEgsReplyMs = 0;
    uint32_t lastFastDataMs = 0;
    uint32_t lastDpfDataMs = 0;
};

VehicleData& vehicle_data();
void vehicle_data_set_can(bool online);
void vehicle_data_invalidate_live();
