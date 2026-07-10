#pragma once

#include <stddef.h>
#include <stdio.h>

enum TempUnit { TEMP_C = 0, TEMP_F = 1 };

struct Climate {
  bool ok = false;
  float tempC = 0;
  float hum = 0;
};

static inline float climateTemperature(TempUnit unit, float tempC) {
  return unit == TEMP_F ? tempC * 9.0f / 5.0f + 32.0f : tempC;
}

static inline const char* climateUnit(TempUnit unit) {
  return unit == TEMP_F ? "F" : "C";
}

static inline void formatTemperature(char* out, size_t outSize, TempUnit unit, float tempC) {
  snprintf(out, outSize, unit == TEMP_F ? "%.0f" : "%.1f", climateTemperature(unit, tempC));
}

static inline void formatHumidity(char* out, size_t outSize, float hum) {
  snprintf(out, outSize, "%.0f%%", hum);
}
