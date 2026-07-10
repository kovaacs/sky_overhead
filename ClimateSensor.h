#pragma once

#include <math.h>

#include "Climate.h"

template <typename Sensor, typename DelayFn>
static inline Climate readClimateSensor(Sensor& sensor, DelayFn delayFn) {
  Climate c;
  float t = 0;
  float h = 0;

  for (int attempt = 0; attempt < 2; attempt++) {
    sensor.softReset();
    delayFn(12);
    if (sensor.measureHighPrecision(t, h) == 0 && !isnan(t) && !isnan(h)) {
      c.ok = true;
      c.tempC = t;
      c.hum = h;
      return c;
    }
  }

  return c;
}
