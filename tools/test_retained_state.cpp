#include <cstdlib>
#include <iostream>
#include <string>

#include "../RetainedState.h"

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

static Plane samplePlane() {
  Plane p;
  p.found = true;
  p.hex = "3C65C2";
  p.callsign = "DLH4JA";
  p.category = "A3";
  p.airline = "Lufthansa";
  p.fromCode = "MUC";
  p.toCode = "BUD";
  p.routeOk = true;
  p.fromCity = "Munich";
  p.toCity = "Budapest";
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

int main() {
  RetainedAircraftState state;
  Plane p = samplePlane();
  rememberLastSeen(state, p, "adsb.lol & adsb.im", HGT_FTFL, SPD_KTS, 1234);
  expectEqual("last seen prefers airline", state.lastSeen, "Lufthansa");
  expectEqual("route key", state.lastRouteKey, "DLH4JA");
  expectEqual("route cities", state.lastCities, "Munich to Budapest");
  expectEqual("identity", state.lastIdentity, "DLH4JA (D-AINZ)");
  expectEqual("motion", state.lastMotion, "FL330  ...  climb.  ...  421 kts");
  expectEqual("source", state.lastSource, "adsb.lol & adsb.im");
  expectTrue("epoch stored", state.lastEpoch == 1234);

  Plane retained = samplePlane();
  retained.routeOk = false;
  retained.fromCode = "";
  retained.toCode = "";
  expectTrue("retained route applied", applyRetainedRouteIfSame(retained, state));
  expectEqual("retained from", retained.fromCode, "MUC");
  expectEqual("retained to", retained.toCode, "BUD");

  Plane other = retained;
  other.callsign = "OTHER";
  other.fromCode = "";
  other.toCode = "";
  expectTrue("other route not applied", !applyRetainedRouteIfSame(other, state));
  expectEqual("other route untouched", other.fromCode, "");

  expectEqual(
    "found signature",
    foundRenderSignature(p, 1),
    "F|3C65C2|A3|Lufthansa|DLH4JA|MUC|BUD|A20N|Airbus A320neo|D-AINZ|1|1|66|2|1");

  expectEqual(
    "empty signature",
    emptyRenderSignature(state, 0),
    "E|0|Lufthansa|MUC|BUD|Munich to Budapest|A20N|DLH4JA (D-AINZ)|Lufthansa|A3|Airbus A320neo|D-AINZ|FL330  ...  climb.  ...  421 kts");

  expectEqual("live display source", displaySourceForResult(true, "adsb.lol", state), "adsb.lol");
  expectEqual("retained display source", displaySourceForResult(false, "adsb.lol", state), "adsb.lol & adsb.im");
  state.lastSource = "";
  expectEqual("clear sky source", displaySourceForResult(false, "adsb.lol", state), "adsb.lol");

  std::cout << "retained state tests passed\n";
  return 0;
}
