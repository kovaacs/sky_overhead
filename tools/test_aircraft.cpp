#include <cstdlib>
#include <iostream>
#include <string>

#include "../Aircraft.h"

static void expectEqual(const char* name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

static void expectTrue(const char* name, bool ok) {
  if (ok) return;

  std::cerr << "FAIL " << name << "\n";
  std::exit(1);
}

int main() {
  Plane p;
  p.callsign = "DLH4JA";
  p.hex = "3C65C2";
  p.typeCode = "A20N";
  p.typeDesc = "Airbus A320neo";
  p.reg = "D-AINZ";
  p.altFt = 33000;
  p.gsKt = 421;
  p.vrateFpm = 850;
  p.hasGs = true;
  p.hasVrate = true;

  // Prefer compact aircraft type codes for the main display title.
  expectEqual("aircraft label prefers type code", aircraftLabel(p), "A20N");

  // Identity combines callsign and registration, but avoids repeating the same
  // normalized value twice.
  expectEqual("identity combines callsign and reg", aircraftIdentity(p), "DLH4JA (D-AINZ)");
  p.reg = " dlh4ja ";
  expectEqual("identity suppresses duplicate reg", aircraftIdentity(p), "DLH4JA");

  // Motion text preserves the visible separators used by the renderer.
  expectEqual("motion text metric speed", motionText(p, HGT_FTFL, SPD_KPH), "FL330  ...  climb.  ...  780 km/h");
  expectEqual("motion text knots", motionText(p, HGT_FTFL, SPD_KTS), "FL330  ...  climb.  ...  421 kts");
  expectEqual("motion text metric altitude", motionText(p, HGT_METRIC, SPD_MPH), "10058 m  ...  climb.  ...  485 mph");

  expectEqual("low altitude feet", altStr(8500, HGT_FTFL), "8500 ft");
  expectEqual("descending trend", trendWord(-300), "desc.");
  expectEqual("level trend", trendWord(25), "level");

  // Route helpers support retained-route lookup and empty-sky city summaries.
  p.fromCity = "Munich";
  p.toCity = "Budapest";
  expectEqual("route key callsign", routeKey(p), "DLH4JA");
  expectEqual("route cities", routeCities(p), "Munich to Budapest");

  p.category = "A7";
  expectTrue("helicopter category", isHelicopter(p));
  expectTrue("same text normalization", sameAircraftText("A 20N", "a20n"));

  std::cout << "aircraft tests passed\n";
  return 0;
}
