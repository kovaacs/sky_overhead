#include <ArduinoJson.h>
#include <cstdlib>
#include <iostream>

#include "../AdsbParser.h"

static void expectEqual(const char* name, const String& actual, const String& expected) {
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
  JsonDocument doc;
  deserializeJson(doc, R"json({
    "ac": [
      {"hex":"GROUND","flight":"SKIP","lat":47.0,"lon":19.0,"alt_baro":"ground"},
      {"hex":"FAR","flight":"FAR1","lat":47.20,"lon":19.20,"alt_geom":32000,"category":"A3","t":"A21N","desc":"Airbus A321neo","r":"D-AABC","gs":400,"baro_rate":-500},
      {"hex":"NEAR","flight":"NEAR1 ","lat":47.0005,"lon":19.0005,"alt_baro":12000,"category":"A7","t":"H60","desc":"","r":"N12345","gs":95,"baro_rate":0}
    ]
  })json");

  Plane best;
  FetchResult result = parseOverheadAircraft(doc, 47.0, 19.0, 130.0, best);
  expectTrue("found aircraft", result == FETCH_FOUND);
  expectEqual("nearest hex", best.hex, "NEAR");
  expectEqual("trimmed callsign", best.callsign, "NEAR1");
  expectEqual("type desc falls back", best.typeDesc, "H60");
  expectTrue("groundspeed present", best.hasGs);
  expectTrue("vertical rate present", best.hasVrate);
  expectTrue("altitude fallback", best.altFt == 12000);

  JsonDocument empty;
  deserializeJson(empty, R"json({"ac":[]})json");
  Plane none;
  expectTrue("empty result", parseOverheadAircraft(empty, 47.0, 19.0, 130.0, none) == FETCH_EMPTY);

  Plane reused = best;
  expectTrue("empty result resets reused output", parseOverheadAircraft(empty, 47.0, 19.0, 130.0, reused) == FETCH_EMPTY);
  expectTrue("reused output no longer found", !reused.found);

  JsonDocument local;
  deserializeJson(local, R"json({
    "now": 1720000000,
    "aircraft": [
      {"hex":"OUT","flight":"OUT1","lat":47.40,"lon":19.40,"alt_baro":5000},
      {"hex":"IN","flight":"IN1","lat":47.0003,"lon":19.0003,"alt_baro":6000}
    ]
  })json");

  Plane localBest;
  expectTrue("local aircraft array found", parseOverheadAircraft(local, 47.0, 19.0, 130.0, localBest, 30) == FETCH_FOUND);
  expectEqual("local nearest hex", localBest.hex, "IN");

  Plane tooFar;
  expectTrue("radius excludes by ground distance", parseOverheadAircraft(local, 47.0, 19.0, 130.0, tooFar, 0.01) == FETCH_EMPTY);

  JsonDocument highOverhead;
  deserializeJson(highOverhead, R"json({
    "aircraft": [
      {"hex":"HIGH","flight":"HIGH1","lat":47.0,"lon":19.0,"alt_baro":41000}
    ]
  })json");

  Plane high;
  expectTrue("ground radius includes high overhead aircraft", parseOverheadAircraft(highOverhead, 47.0, 19.0, 130.0, high, 1) == FETCH_FOUND);
  expectEqual("high overhead selected", high.hex, "HIGH");

  JsonDocument stale;
  deserializeJson(stale, R"json({
    "aircraft": [
      {"hex":"STALE","flight":"OLD1","lat":47.0,"lon":19.0,"alt_baro":4000,"seen_pos":900},
      {"hex":"FRESH","flight":"NEW1","lat":47.001,"lon":19.001,"alt_baro":5000,"seen_pos":5}
    ]
  })json");

  Plane fresh;
  expectTrue("stale local positions ignored", parseOverheadAircraft(stale, 47.0, 19.0, 130.0, fresh, 30) == FETCH_FOUND);
  expectEqual("fresh aircraft selected over stale closer aircraft", fresh.hex, "FRESH");

  std::cout << "adsb parser tests passed\n";
  return 0;
}
