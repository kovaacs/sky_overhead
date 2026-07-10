#include <ArduinoJson.h>
#include <cstdlib>
#include <iostream>

#include "../RouteParser.h"

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
  deserializeJson(doc, R"json([{
    "airline": {"name": "Lufthansa"},
    "plausible": true,
    "_airports": [
      {"location": "Munich", "iata": "MUC", "icao": "EDDM"},
      {"location": "Budapest", "iata": "", "icao": "LHBP"}
    ]
  }])json");

  Plane p;
  applyRouteResponse(doc, p);
  expectEqual("airline", p.airline, "Lufthansa");
  expectEqual("from city", p.fromCity, "Munich");
  expectEqual("to city", p.toCity, "Budapest");
  expectEqual("from iata", p.fromCode, "MUC");
  expectEqual("to icao fallback", p.toCode, "LHBP");
  expectTrue("route ok", p.routeOk);

  JsonDocument implausible;
  deserializeJson(implausible, R"json([{
    "airline": {"name": "unknown"},
    "plausible": false,
    "_airports": [
      {"location": "Ignore", "iata": "AAA"},
      {"location": "Ignore", "iata": "BBB"}
    ]
  }])json");
  Plane bad;
  bad.airline = "Existing";
  bad.fromCity = "Old";
  bad.toCity = "Old";
  bad.fromCode = "OLD";
  bad.toCode = "OLD";
  bad.routeOk = true;
  applyRouteResponse(implausible, bad);
  expectEqual("unknown airline ignored", bad.airline, "Existing");
  expectTrue("implausible route ignored", !bad.routeOk);
  expectEqual("implausible clears stale from", bad.fromCode, "");
  expectEqual("implausible clears stale to", bad.toCode, "");

  std::cout << "route parser tests passed\n";
  return 0;
}
