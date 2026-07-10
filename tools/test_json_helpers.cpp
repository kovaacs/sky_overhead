#include <ArduinoJson.h>
#include <cstdlib>
#include <iostream>

#include "../JsonHelpers.h"

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
    "trimmed": " NEAR1 ",
    "nullString": "null",
    "actualNull": null,
    "number": 12000,
    "ground": "ground"
  })json");

  expectEqual("json text trims strings", jsonText(doc["trimmed"]), "NEAR1");
  expectEqual("json text converts literal null to empty", jsonText(doc["nullString"]), "");
  expectEqual("json text converts actual null to empty", jsonText(doc["actualNull"]), "");
  expectTrue("altitude reads number", altFeet(doc["number"]) == 12000);
  expectTrue("altitude rejects ground string", altFeet(doc["ground"]) < 0);
  expectTrue("altitude rejects null", altFeet(doc["actualNull"]) < 0);

  std::cout << "json helper tests passed\n";
  return 0;
}
