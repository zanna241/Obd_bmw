#pragma once
#include <Arduino.h>

enum class DiagnosticConfidence : uint8_t {
    CONFIRMED_CAR,
    STANDARD_CONFIRMED,
    BMW_PROFILE_PENDING,
    CANDIDATE
};

struct DiagnosticParameterInfo {
    const char *key;
    const char *ecu;
    const char *protocol;
    uint8_t service;
    uint16_t identifier;
    const char *unit;
    DiagnosticConfidence confidence;
    const char *description;
};

int diagnostic_parameter_count();
const DiagnosticParameterInfo& diagnostic_parameter(int index);
const char* diagnostic_confidence_name(DiagnosticConfidence c);
