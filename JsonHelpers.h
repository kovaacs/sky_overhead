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
  String s(raw);
#if defined(ARDUINO)
  s.trim();
#else
  const char* ws = " \t\r\n";
  size_t start = s.find_first_not_of(ws);
  if (start == String::npos) return "";
  size_t end = s.find_last_not_of(ws);
  s = s.substr(start, end - start + 1);
#endif
  return s == "null" ? "" : s;
}
