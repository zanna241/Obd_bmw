#pragma once
#include <Arduino.h>
String config_export_json();
bool config_import_json(const String &json,String &error);
