#include <cstdlib>
#include <iostream>
#include <string>

#include "../Climate.h"

// These tests pin the display-facing climate strings. They are deliberately
// host-only so formatting can be checked without Arduino display libraries.
static void expectEqual(const char* name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

int main() {
  char buf[16];

  // Celsius is shown with one decimal on the e-paper panel.
  formatTemperature(buf, sizeof(buf), TEMP_C, 21.24f);
  expectEqual("celsius keeps one decimal", buf, "21.2");

  // Fahrenheit is less precise in the UI, so it rounds to a whole degree.
  formatTemperature(buf, sizeof(buf), TEMP_F, 21.24f);
  expectEqual("fahrenheit rounds whole degrees", buf, "70");

  // The unit is drawn separately from the number, next to the degree mark.
  expectEqual("celsius unit", climateUnit(TEMP_C), "C");
  expectEqual("fahrenheit unit", climateUnit(TEMP_F), "F");

  // Humidity is shown as a whole-number percentage.
  formatHumidity(buf, sizeof(buf), 48.6f);
  expectEqual("humidity rounds with percent", buf, "49%");

  std::cout << "climate format tests passed\n";
  return 0;
}
