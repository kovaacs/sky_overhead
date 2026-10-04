#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../Aircraft.h"

inline Plane samplePlane() {
  Plane p;
  p.found = true;
  p.hex = "3C65C2";
  p.callsign = "DLH4JA";
  p.category = "A3";
  p.airline = "Lufthansa";
  p.fromCode = "MUC";
  p.toCode = "BUD";
  p.routeOk = true;
  p.typeCode = "A20N";
  p.typeDesc = "Airbus A320neo";
  p.reg = "D-AINZ";
  p.altFt = 33000;
  p.slantKm = 8.2;
  p.gsKt = 421;
  p.vrateFpm = 850;
  p.hasGs = true;
  p.hasVrate = true;
  return p;
}

inline void expectEqual(const char* name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;
  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

inline void expectEqual(const char* name, int64_t actual, int64_t expected) {
  if (actual == expected) return;
  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

inline void expectTrue(const char* name, bool ok) {
  if (ok) return;
  std::cerr << "FAIL " << name << "\n";
  std::exit(1);
}
