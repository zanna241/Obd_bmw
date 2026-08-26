#include "diagnostic_profile.h"

// Metadata only: no unverified BMW DID is transmitted automatically.
static const DiagnosticParameterInfo params[] = {
    {"rpm", "DDE", "OBD-II", 0x01, 0x0C, "rpm", DiagnosticConfidence::CONFIRMED_CAR, "Regime motore"},
    {"coolant", "DDE", "OBD-II", 0x01, 0x05, "C", DiagnosticConfidence::CONFIRMED_CAR, "Temperatura refrigerante"},
    {"intake", "DDE", "OBD-II", 0x01, 0x0F, "C", DiagnosticConfidence::CONFIRMED_CAR, "Temperatura aria aspirata"},
    {"maf", "DDE", "OBD-II", 0x01, 0x10, "g/s", DiagnosticConfidence::CONFIRMED_CAR, "Massa aria"},
    {"boost", "DDE", "OBD-II", 0x01, 0x70, "kPa", DiagnosticConfidence::STANDARD_CONFIRMED, "Boost pressure control"},
    {"rail", "DDE", "OBD-II", 0x01, 0x6D, "bar", DiagnosticConfidence::STANDARD_CONFIRMED, "Fuel pressure control system"},
    {"egt", "DDE", "OBD-II", 0x01, 0x78, "C", DiagnosticConfidence::STANDARD_CONFIRMED, "Exhaust gas temperature bank 1"},
    {"dpf_diff", "DDE", "OBD-II", 0x01, 0x7A, "hPa", DiagnosticConfidence::STANDARD_CONFIRMED, "DPF differential pressure"},
    {"dpf_regen", "DDE", "OBD-II", 0x01, 0x8B, "%", DiagnosticConfidence::STANDARD_CONFIRMED, "Diesel aftertreatment / regen status"},
    {"accelerator", "DDE", "OBD-II", 0x01, 0x49, "%", DiagnosticConfidence::STANDARD_CONFIRMED, "Accelerator pedal position D"},
    {"nox", "DDE", "OBD-II", 0x01, 0x83, "ppm", DiagnosticConfidence::STANDARD_CONFIRMED, "NOx sensor"},
    {"lambda", "DDE", "OBD-II", 0x01, 0x8C, "lambda", DiagnosticConfidence::STANDARD_CONFIRMED, "Wide range O2 sensor"},
    {"oil", "DDE", "BMW D70BX7A0", 0x00, 0x0000, "C", DiagnosticConfidence::BMW_PROFILE_PENDING, "Temperatura olio motore"},
    {"dpf_soot", "DDE", "BMW D70BX7A0", 0x00, 0x0000, "g", DiagnosticConfidence::BMW_PROFILE_PENDING, "Massa fuliggine"},
    {"dpf_ash", "DDE", "BMW D70BX7A0", 0x00, 0x0000, "g", DiagnosticConfidence::BMW_PROFILE_PENDING, "Massa cenere/olio"},
    {"dpf_since_regen", "DDE", "BMW D70BX7A0", 0x00, 0x0000, "km", DiagnosticConfidence::BMW_PROFILE_PENDING, "Distanza ultima rigenerazione"},
    {"gear", "EGS", "BMW EGS", 0x00, 0x0000, "", DiagnosticConfidence::BMW_PROFILE_PENDING, "Marcia reale"},
    {"atf", "EGS", "BMW EGS", 0x00, 0x0000, "C", DiagnosticConfidence::BMW_PROFILE_PENDING, "Temperatura olio cambio"},
    {"converter_slip", "EGS", "BMW EGS", 0x00, 0x0000, "rpm", DiagnosticConfidence::BMW_PROFILE_PENDING, "Slittamento convertitore"},
    {"lockup", "EGS", "BMW EGS", 0x00, 0x0000, "", DiagnosticConfidence::BMW_PROFILE_PENDING, "Lock-up convertitore"},

    // Confirmed members of the D70BX7A0 STATUS_BLOCK_LESEN / DPF block.
    // Their semantic assignment (soot/ash/distance/etc.) is deliberately NOT
    // guessed: the discovery logs are intended to correlate/validate them.
    {"d70_dpf_44f8", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44F8, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_4506", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x4506, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_4500", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x4500, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_44be", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44BE, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_44c4", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44C4, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_5308", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x5308, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_44bc", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44BC, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_44b7", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44B7, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"},
    {"d70_dpf_44bb", "DDE", "BMW STATUS_BLOCK_ID", 0x00, 0x44BB, "", DiagnosticConfidence::CANDIDATE, "D70BX7A0 DPF block member"}
};

int diagnostic_parameter_count(){ return sizeof(params)/sizeof(params[0]); }
const DiagnosticParameterInfo& diagnostic_parameter(int index){ return params[index]; }
const char* diagnostic_confidence_name(DiagnosticConfidence c){
    switch(c){
        case DiagnosticConfidence::CONFIRMED_CAR: return "CONFIRMED_CAR";
        case DiagnosticConfidence::STANDARD_CONFIRMED: return "STANDARD_CONFIRMED";
        case DiagnosticConfidence::BMW_PROFILE_PENDING: return "BMW_PROFILE_PENDING";
        default: return "CANDIDATE";
    }
}
