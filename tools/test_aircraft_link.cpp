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
  p.reg = "D-AINB";
  expectEqual("default registration link", aircraftInfoUrl(p),
              "https://www.flightradar24.com/data/aircraft/d-ainb");
  expectEqual("custom provider", aircraftInfoUrl(p, "https://example.com/aircraft/{reg}"),
              "https://example.com/aircraft/d-ainb");
  expectEqual("trim template", aircraftInfoUrl(p, " https://example.com/{reg} "),
              "https://example.com/d-ainb");
  expectEqual("missing placeholder", aircraftInfoUrl(p, "https://example.com/aircraft"), "");
  expectEqual("reject non-http URL", aircraftInfoUrl(p, "javascript:{reg}"), "");
  expectEqual("uppercase scheme", aircraftInfoUrl(p, "HTTPS://example.com/{reg}"),
              "HTTPS://example.com/d-ainb");
  expectEqual("reject missing authority", aircraftInfoUrl(p, "https:///{reg}"), "");
  expectEqual("reject URL whitespace", aircraftInfoUrl(p, "https://example.com/{reg} extra"), "");
  expectEqual("blank template disables QR", aircraftInfoUrl(p, ""), "");
  expectEqual("reject oversized URL", aircraftInfoUrl(p,
              "https://example.com/a-very-long-aircraft-information-provider/path/{reg}"), "");

  p.reg = "D_AINB";
  expectEqual("unsafe identifier", aircraftInfoUrl(p), "");

  p.reg = "ABCDEFGHIJKLM";
  expectEqual("oversized identifier", aircraftInfoUrl(p), "");

  std::cout << "aircraft link tests passed\n";
  return 0;
}
