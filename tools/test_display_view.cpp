#include <cstdlib>
#include <iostream>
#include <string>

#include "../DisplayView.h"

#include "TestHelpers.h"

int main() {
  expectEqual("footer refresh text", frameFooterRefreshedText("14:32"), "Last refreshed: 14:32");
  expectEqual("footer source text", frameFooterSourceText("adsb.lol & adsb.im"), "Source: adsb.lol & adsb.im");

  // Live aircraft data becomes a fully prepared left-column view; the renderer
  // should not need to know aircraft formatting rules.
  DisplayIconSet icons { 1, 100, 2, 200, 3, 120 };
  LeftColumnView live = makeLiveAircraftView(samplePlane(), HGT_FTFL, SPD_KTS, icons);
  expectEqual("live glyph", live.glyph, 1);
  expectEqual("live glyph size", live.glyphSize, 100);
  expectEqual("live title", live.title, "Airbus A320neo");
  expectEqual("live title fallback", live.titleFallback, "A20N");
  expectEqual("live identity", live.line1, "DLH4JA (D-AINZ)");
  expectEqual("live airline", live.line2, "Lufthansa");
  expectEqual("live route from", live.routeFrom, "MUC");
  expectEqual("live route to", live.routeTo, "BUD");
  expectEqual("live motion", live.position, "FL330  ...  climb.  ...  421 kts");

  // Helicopters keep the same text rules but select the helicopter icon.
  Plane heli = samplePlane();
  heli.category = "A7";
  LeftColumnView heliView = makeLiveAircraftView(heli, HGT_FTFL, SPD_KTS, icons);
  expectEqual("helicopter glyph", heliView.glyph, 2);
  expectEqual("helicopter glyph size", heliView.glyphSize, 200);

  // With no retained aircraft, the empty view is the clear-skies placeholder.
  RetainedAircraftState none;
  LeftColumnView clear = makeRetainedAircraftView(none, HGT_FTFL, SPD_KTS, icons);
  expectEqual("clear glyph", clear.glyph, 3);
  expectEqual("clear title", clear.title, "Clear skies");
  expectEqual("clear rows", clear.line1, "");

  // Retained aircraft data chooses saved metadata and route/motion fields.
  RetainedAircraftState retained;
  retained.lastAircraft = "A20N";
  retained.lastType = "Airbus A320neo";
  retained.lastIdentity = "DLH4JA";
  retained.lastSeen = "Lufthansa";
  retained.lastAirline = "Lufthansa";
  retained.lastCategory = "A7";
  retained.lastFrom = "MUC";
  retained.lastTo = "BUD";
  retained.lastAltFt = 33000;
  LeftColumnView retainedView = makeRetainedAircraftView(retained, HGT_FTFL, SPD_KTS, icons);
  expectEqual("retained glyph", retainedView.glyph, 2);
  expectEqual("retained title", retainedView.title, "Airbus A320neo");
  expectEqual("retained title fallback", retainedView.titleFallback, "A20N");
  expectEqual("retained identity", retainedView.line1, "DLH4JA");
  expectEqual("retained airline", retainedView.line2, "Lufthansa");
  expectEqual("retained route", retainedView.routeFrom + ">" + retainedView.routeTo, "MUC>BUD");
  expectEqual("retained motion", retainedView.position, "FL330");
  retained.lastGsKt = 421;
  retained.lastHasGs = true;
  expectEqual("retained view uses current units",
              makeRetainedAircraftView(retained, HGT_METRIC, SPD_KPH, icons).position,
              "10058 m  ...  780 km/h");

  std::cout << "display view tests passed\n";
  return 0;
}
