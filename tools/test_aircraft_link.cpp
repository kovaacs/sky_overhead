#include <cstdlib>
#include <iostream>
#include <string>

#include "../AircraftLink.h"

#include "TestHelpers.h"

int main() {
  expectTrue("mixed-case HTTP scheme", aircraftLinkHttpUrl("hTtP://example.com/"));
  expectTrue("reject truncated scheme", !aircraftLinkHttpUrl("https:"));
  expectTrue("reject empty URL", !aircraftLinkHttpUrl(""));
  expectEqual("default registration link", aircraftInfoUrl("D-AINB"),
              "https://www.flightradar24.com/data/aircraft/d-ainb");
  expectEqual("custom provider", aircraftInfoUrl("D-AINB", "https://example.com/aircraft/{reg}"),
              "https://example.com/aircraft/d-ainb");
  expectEqual("trim template", aircraftInfoUrl("D-AINB", " https://example.com/{reg} "),
              "https://example.com/d-ainb");
  expectEqual("missing placeholder", aircraftInfoUrl("D-AINB", "https://example.com/aircraft"), "");
  expectEqual("reject non-http URL", aircraftInfoUrl("D-AINB", "javascript:{reg}"), "");
  expectEqual("uppercase scheme", aircraftInfoUrl("D-AINB", "HTTPS://example.com/{reg}"),
              "HTTPS://example.com/d-ainb");
  expectEqual("reject missing authority", aircraftInfoUrl("D-AINB", "https:///{reg}"), "");
  expectEqual("reject URL whitespace", aircraftInfoUrl("D-AINB", "https://example.com/{reg} extra"), "");
  expectEqual("blank template disables QR", aircraftInfoUrl("D-AINB", ""), "");
  expectEqual("reject oversized URL", aircraftInfoUrl("D-AINB",
              "https://example.com/a-very-long-aircraft-information-provider/path/{reg}"), "");

  expectEqual("unsafe identifier", aircraftInfoUrl("D_AINB"), "");
  expectEqual("oversized identifier", aircraftInfoUrl("ABCDEFGHIJKLM"), "");

  std::cout << "aircraft link tests passed\n";
  return 0;
}
