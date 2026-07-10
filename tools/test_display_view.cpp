#include <cstdlib>
#include <iostream>
#include <string>

#include "../DisplayView.h"

static void expectEqual(const char* name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

static void expectEqual(const char* name, int actual, int expected) {
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

static Plane samplePlane() {
  Plane p;
  p.found = true;
  p.callsign = "DLH4JA";
  p.hex = "3C65C2";
  p.category = "A3";
  p.airline = "Lufthansa";
  p.fromCode = "MUC";
  p.toCode = "BUD";
  p.typeCode = "A20N";
  p.reg = "D-AINZ";
  p.altFt = 33000;
  p.gsKt = 421;
  p.vrateFpm = 850;
  p.hasGs = true;
  p.hasVrate = true;
  return p;
}

int main() {
  // Live aircraft data becomes a fully prepared left-column view; the renderer
  // should not need to know aircraft formatting rules.
  DisplayIconSet icons { 1, 100, 2, 200, 3, 120 };
  LeftColumnView live = makeLiveAircraftView(samplePlane(), HGT_FTFL, SPD_KTS, icons);
  expectEqual("live glyph", live.glyph, 1);
  expectEqual("live glyph size", live.glyphSize, 100);
  expectEqual("live title", live.title, "A20N");
  expectEqual("live identity", live.line1, "DLH4JA (D-AINZ)");
  expectEqual("live airline", live.line2, "Lufthansa");
  expectEqual("live route from", live.routeFrom, "MUC");
  expectEqual("live route to", live.routeTo, "BUD");
  expectEqual("live motion", live.position, "FL330  ...  climbing  ...  421 kts");

  // Helicopters keep the same text rules but select the helicopter icon.
  Plane heli = samplePlane();
  heli.category = "A7";
  LeftColumnView heliView = makeLiveAircraftView(heli, HGT_FTFL, SPD_KTS, icons);
  expectEqual("helicopter glyph", heliView.glyph, 2);
  expectEqual("helicopter glyph size", heliView.glyphSize, 200);

  // With no retained aircraft, the empty view is the clear-skies placeholder.
  RetainedAircraftView none;
  LeftColumnView clear = makeRetainedAircraftView(none, icons);
  expectEqual("clear glyph", clear.glyph, 3);
  expectEqual("clear title", clear.title, "Clear skies");
  expectEqual("clear rows", clear.line1, "");

  // Retained aircraft data chooses saved metadata and route/motion fields.
  RetainedAircraftView retained;
  retained.lastAircraft = "A20N";
  retained.lastIdentity = "DLH4JA";
  retained.lastSeen = "Lufthansa";
  retained.lastAirline = "Lufthansa";
  retained.lastCategory = "A7";
  retained.lastFrom = "MUC";
  retained.lastTo = "BUD";
  retained.lastMotion = "FL330";
  LeftColumnView retainedView = makeRetainedAircraftView(retained, icons);
  expectEqual("retained glyph", retainedView.glyph, 2);
  expectEqual("retained title", retainedView.title, "A20N");
  expectEqual("retained identity", retainedView.line1, "DLH4JA");
  expectEqual("retained airline", retainedView.line2, "Lufthansa");
  expectEqual("retained route", retainedView.routeFrom + ">" + retainedView.routeTo, "MUC>BUD");
  expectEqual("retained motion", retainedView.position, "FL330");

  std::cout << "display view tests passed\n";
  return 0;
}
