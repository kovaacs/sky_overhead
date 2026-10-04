#include <cstdlib>
#include <iostream>

#include "../AdsbFallback.h"

#include "TestHelpers.h"

static Plane planeWithHex(const char* hex) {
  Plane p;
  p.found = true;
  p.hex = hex;
  return p;
}

struct FakeFetcher {
  FetchResult result;
  Plane plane;
  int calls = 0;

  FetchResult operator()(Plane& out) {
    calls++;
    out = plane;
    return result;
  }
};

int main() {
  const struct {
    const char* name;
    FetchResult publicResult, localResult, expectedResult;
    int localCalls;
    const char* hex;
    const char* source;
  } cases[] = {
    { "public found", FETCH_FOUND, FETCH_FOUND, FETCH_FOUND, 0, "PUBLIC_PRIMARY", "adsb.lol" },
    { "public empty", FETCH_EMPTY, FETCH_FOUND, FETCH_EMPTY, 0, "", "adsb.lol" },
    { "local found", FETCH_ERROR, FETCH_FOUND, FETCH_FOUND, 1, "LOCAL_FALLBACK", "local feed" },
    { "local empty", FETCH_ERROR, FETCH_EMPTY, FETCH_EMPTY, 1, "", "local feed" },
    { "both error", FETCH_ERROR, FETCH_ERROR, FETCH_ERROR, 1, "", "" }
  };

  for (const auto& test : cases) {
    FakeFetcher publicAdsb { test.publicResult, test.publicResult == FETCH_FOUND ? planeWithHex("PUBLIC_PRIMARY") : Plane() };
    FakeFetcher local { test.localResult, test.localResult == FETCH_FOUND ? planeWithHex("LOCAL_FALLBACK") : Plane() };
    Plane out = planeWithHex("OLD");
    String source;
    String name = test.name;

    FetchResult result = fetchPublicThenLocalSource(out, publicAdsb, local, source);
    expectEqual((name + " result").c_str(), result, test.expectedResult);
    expectEqual((name + " public calls").c_str(), publicAdsb.calls, 1);
    expectEqual((name + " local calls").c_str(), local.calls, test.localCalls);
    expectTrue((name + " found").c_str(), out.found == (test.expectedResult == FETCH_FOUND));
    expectEqual((name + " aircraft").c_str(), out.hex, test.hex);
    expectEqual((name + " source").c_str(), source, test.source);
  }

  expectTrue("source aircraft only", dataSourceText("local feed", false) == "local feed");
  expectTrue("source with route", dataSourceText("local feed", true) == "local feed & adsb.im");
  expectTrue("source with retained route", dataSourceText("local feed", false, true) == "local feed & retained route");
  expectTrue("route only source", dataSourceText("", true) == "adsb.im");

  std::cout << "adsb fallback tests passed\n";
  return 0;
}
