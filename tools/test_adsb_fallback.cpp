#include <cstdlib>
#include <iostream>

#include "../AdsbFallback.h"

static void expectEqual(const char* name, int actual, int expected) {
  if (actual == expected) return;
  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

static void expectTrue(const char* name, bool ok) {
  if (ok) return;
  std::cerr << "FAIL " << name << "\n";
  std::exit(1);
}

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
  {
    FakeFetcher local { FETCH_FOUND, planeWithHex("LOCAL") };
    FakeFetcher publicAdsb { FETCH_FOUND, planeWithHex("PUBLIC") };
    Plane out;
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("local found result", result, FETCH_FOUND);
    expectEqual("local called once", local.calls, 1);
    expectEqual("public not called after local found", publicAdsb.calls, 0);
    expectTrue("local aircraft kept", out.hex == "LOCAL");
    expectTrue("local source kept", source == "local feed");
  }

  {
    FakeFetcher local { FETCH_EMPTY, Plane() };
    FakeFetcher publicAdsb { FETCH_FOUND, planeWithHex("PUBLIC") };
    Plane out = planeWithHex("OLD");
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("public found after local empty", result, FETCH_FOUND);
    expectEqual("local empty called once", local.calls, 1);
    expectEqual("public called after local empty", publicAdsb.calls, 1);
    expectTrue("public aircraft used after local empty", out.hex == "PUBLIC");
    expectTrue("public source used after local empty", source == "adsb.lol");
  }

  {
    FakeFetcher local { FETCH_EMPTY, Plane() };
    FakeFetcher publicAdsb { FETCH_EMPTY, Plane() };
    Plane out = planeWithHex("OLD");
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("public empty after local empty", result, FETCH_EMPTY);
    expectEqual("public empty called after local empty", publicAdsb.calls, 1);
    expectTrue("public empty output kept", !out.found);
    expectTrue("public empty source used", source == "adsb.lol");
  }

  {
    FakeFetcher local { FETCH_EMPTY, Plane() };
    FakeFetcher publicAdsb { FETCH_ERROR, Plane() };
    Plane out = planeWithHex("OLD");
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("local empty kept after public error", result, FETCH_EMPTY);
    expectEqual("public error called after local empty", publicAdsb.calls, 1);
    expectTrue("local empty output kept after public error", !out.found);
    expectTrue("local source kept after public error", source == "local feed");
  }

  {
    FakeFetcher local { FETCH_ERROR, Plane() };
    FakeFetcher publicAdsb { FETCH_FOUND, planeWithHex("PUBLIC") };
    Plane out;
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("public found after local error", result, FETCH_FOUND);
    expectEqual("public called after local error", publicAdsb.calls, 1);
    expectTrue("public aircraft used", out.hex == "PUBLIC");
    expectTrue("public source used", source == "adsb.lol");
  }

  {
    FakeFetcher local { FETCH_ERROR, Plane() };
    FakeFetcher publicAdsb { FETCH_EMPTY, Plane() };
    Plane out = planeWithHex("OLD");
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("public empty after local error", result, FETCH_EMPTY);
    expectEqual("public empty called after local error", publicAdsb.calls, 1);
    expectTrue("public empty source after local error", source == "adsb.lol");
    expectTrue("public empty output after local error", !out.found);
  }

  {
    FakeFetcher local { FETCH_ERROR, Plane() };
    FakeFetcher publicAdsb { FETCH_ERROR, Plane() };
    Plane out = planeWithHex("OLD");
    String source;

    FetchResult result = fetchWithFallbackSource(out, local, "local feed", publicAdsb, "adsb.lol", source);
    expectEqual("both error result", result, FETCH_ERROR);
    expectEqual("public error called", publicAdsb.calls, 1);
    expectTrue("both error leaves public output", !out.found);
    expectTrue("both error no source", source == "");
  }

  expectTrue("source aircraft only", dataSourceText("local feed", false) == "local feed");
  expectTrue("source with route", dataSourceText("local feed", true) == "local feed + adsb.im");
  expectTrue("source with retained route", dataSourceText("local feed", false, true) == "local feed + retained route");
  expectTrue("route only source", dataSourceText("", true) == "adsb.im");

  std::cout << "adsb fallback tests passed\n";
  return 0;
}
