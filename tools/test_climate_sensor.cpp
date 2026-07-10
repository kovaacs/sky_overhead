#include <cstdlib>
#include <iostream>

#include "../ClimateSensor.h"

// Fake SHT-style sensor used to exercise retry behavior without hardware.
struct FakeSensor {
  int failures = 0;
  int resets = 0;
  float temp = 22.5f;
  float hum = 51.0f;

  void softReset() {
    resets++;
  }

  int measureHighPrecision(float& outTemp, float& outHum) {
    if (failures > 0) {
      failures--;
      return 1;
    }
    outTemp = temp;
    outHum = hum;
    return 0;
  }
};

static void expectTrue(const char* name, bool ok) {
  if (ok) return;
  std::cerr << "FAIL " << name << "\n";
  std::exit(1);
}

int main() {
  int delays = 0;
  // The production path waits briefly after each soft reset before measuring.
  auto delayFn = [&](int ms) {
    expectTrue("delay duration", ms == 12);
    delays++;
  };

  // A transient first-read failure should soft-reset and retry once, then
  // return the measured temperature and humidity.
  FakeSensor retrySensor;
  retrySensor.failures = 1;
  Climate retry = readClimateSensor(retrySensor, delayFn);
  expectTrue("retry succeeds", retry.ok);
  expectTrue("retry temperature", retry.tempC == 22.5f);
  expectTrue("retry humidity", retry.hum == 51.0f);
  expectTrue("retry resets twice", retrySensor.resets == 2);
  expectTrue("retry delays twice", delays == 2);

  // If both attempts fail, the display layer should receive ok=false and draw
  // placeholder dashes instead of stale or bogus numbers.
  FakeSensor failedSensor;
  failedSensor.failures = 2;
  Climate failed = readClimateSensor(failedSensor, delayFn);
  expectTrue("failed read marked not ok", !failed.ok);
  expectTrue("failed read resets twice", failedSensor.resets == 2);

  std::cout << "climate sensor tests passed\n";
  return 0;
}
