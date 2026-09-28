#pragma once

#include <ArduinoJson.h>

#include "Aircraft.h"

static inline double altFeet(JsonVariantConst v) {
  if (v.isNull()) return -1;
  if (v.is<const char*>()) return -1;
  return v.as<double>();
}

static inline String jsonText(JsonVariantConst v) {
  if (v.isNull()) return "";
  const char* raw = v.as<const char*>();
  if (!raw) return "";
  String s = trimCopy(raw);
  return s == "null" ? "" : s;
}
