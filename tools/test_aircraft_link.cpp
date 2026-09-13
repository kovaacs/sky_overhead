#include <cstdlib>
#include <iostream>
#include <string>

#include "../AircraftLink.h"

static void expectEqual(const char* name, const std::string& actual, const std::string& expected) {
  if (actual == expected) return;

  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

int main() {
  Plane p;
  p.reg = "D-AINZ";
  p.callsign = "DLH4JA";
  p.hex = " 3C65C2 ";
  expectEqual("default ICAO hex link", aircraftInfoUrl(p),
              "https://www.flightradar24.com/3c65c2");
  expectEqual("custom provider", aircraftInfoUrl(p, "https://globe.adsb.lol/?icao={hex}"),
              "https://globe.adsb.lol/?icao=3c65c2");
  expectEqual("trim template", aircraftInfoUrl(p, " https://example.com/{hex} "),
              "https://example.com/3c65c2");
  expectEqual("missing placeholder", aircraftInfoUrl(p, "https://example.com/aircraft"), "");
  expectEqual("reject non-http URL", aircraftInfoUrl(p, "javascript:{hex}"), "");
  expectEqual("blank template disables QR", aircraftInfoUrl(p, ""), "");
  expectEqual("reject oversized URL", aircraftInfoUrl(p,
              "https://example.com/a-very-long-aircraft-information-provider/path/{hex}"), "");

  p.hex = "";
  expectEqual("missing identifier", aircraftInfoUrl(p), "");

  p.hex = "3C65_C2";
  expectEqual("unsafe identifier", aircraftInfoUrl(p), "");

  p.hex = "123456789";
  expectEqual("oversized identifier", aircraftInfoUrl(p), "");

  std::cout << "aircraft link tests passed\n";
  return 0;
}
