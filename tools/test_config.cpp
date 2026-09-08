#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "../Config.h"

static void expectEqual(const char* name, int actual, int expected) {
  if (actual == expected) return;
  std::cerr << "FAIL " << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
  std::exit(1);
}

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

int main() {
  uint16_t minute = 0;
  expectTrue("parse valid hhmm", parseHHMM("6:05", minute));
  expectEqual("valid hhmm minutes", minute, 365);
  expectTrue("reject invalid hour", !parseHHMM("24:00", minute));
  expectTrue("reject invalid minutes", !parseHHMM("12:60", minute));
  expectTrue("reject extra colon", !parseHHMM("12:30:00", minute));

  Settings cfg;
  RuntimeConfig runtime;
  applyConfigValue(cfg, runtime, "SPEED", "kts");
  applyConfigValue(cfg, runtime, "HEIGHT", "metric");
  applyConfigValue(cfg, runtime, "TEMP", "f");
  applyConfigValue(cfg, runtime, "RADIUS", "999");
  applyConfigValue(cfg, runtime, "BUSY", "2");
  applyConfigValue(cfg, runtime, "MAX_REFRESH", "30");
  applyConfigValue(cfg, runtime, "DEMO", "on");
  applyConfigValue(cfg, runtime, "SD_LOG", "true");
  applyConfigValue(cfg, runtime, "NIGHT_MODE", "22:30-06:45");

  expectEqual("speed kts", cfg.speed, SPD_KTS);
  expectEqual("height metric", cfg.height, HGT_METRIC);
  expectEqual("temp fahrenheit", cfg.temp, TEMP_F);
  expectEqual("radius constrained high", cfg.radius, MAX_RADIUS_KM);
  expectEqual("busy constrained low", cfg.busy, 15);
  expectEqual("maximum refresh constrained low", cfg.maxRefresh, 60);
  expectTrue("demo true", cfg.demo);
  expectTrue("sd log true", cfg.sdLog);
  expectTrue("night true", cfg.night);
  expectEqual("night start", cfg.nightStart, 22 * 60 + 30);
  expectEqual("night end", cfg.nightEnd, 6 * 60 + 45);

  applyConfigValue(cfg, runtime, "RADIUS", "0");
  applyConfigValue(cfg, runtime, "BUSY", "999");
  applyConfigValue(cfg, runtime, "MAX_REFRESH", "0");
  applyConfigValue(cfg, runtime, "SD_LOG", "off");
  expectEqual("radius constrained low", cfg.radius, 1);
  expectEqual("busy constrained high", cfg.busy, 600);
  expectEqual("maximum refresh disabled", cfg.maxRefresh, 0);
  expectTrue("sd log false", !cfg.sdLog);

  cfg.nightStart = 123;
  cfg.nightEnd = 456;
  applyConfigValue(cfg, runtime, "NIGHT_MODE", "bad");
  expectTrue("bad night disables night", !cfg.night);
  expectEqual("bad night preserves start", cfg.nightStart, 123);
  expectEqual("bad night preserves end", cfg.nightEnd, 456);

  applyConfigValue(cfg, runtime, "SSID", "wifi");
  applyConfigValue(cfg, runtime, "PASS", "secret");
  applyConfigValue(cfg, runtime, "LAT", "47.5");
  applyConfigValue(cfg, runtime, "LON", "19.1");
  applyConfigValue(cfg, runtime, "ALT", "130");
  applyConfigValue(cfg, runtime, "TZ", "CET-1CEST");
  applyConfigValue(cfg, runtime, "LOCAL_ADSB_URL", "http://adsb-feeder.local:8080");
  expectEqual("ssid", runtime.wifiSSID, "wifi");
  expectEqual("pass", runtime.wifiPass, "secret");
  expectEqual("lat", (int)(runtime.myLat * 10), 475);
  expectEqual("lon", (int)(runtime.myLon * 10), 191);
  expectEqual("alt", (int)runtime.myAltM, 130);
  expectEqual("tz", runtime.tzInfo, "CET-1CEST");
  expectEqual("local adsb base url", runtime.localAdsbBaseUrl, "http://adsb-feeder.local:8080");
  expectTrue("complete runtime config valid", hasRequiredRuntimeConfig(runtime));
  expectEqual("local adsb url from base", buildLocalAdsbAircraftUrl(runtime.localAdsbBaseUrl), "http://adsb-feeder.local:8080/data/aircraft.json");
  expectEqual("local adsb url trims slash", buildLocalAdsbAircraftUrl("http://adsb-feeder.local:8080/"), "http://adsb-feeder.local:8080/data/aircraft.json");
  expectEqual("local adsb url adds scheme", buildLocalAdsbAircraftUrl("adsb-feeder.local:8080"), "http://adsb-feeder.local:8080/data/aircraft.json");
  expectEqual("local adsb url keeps full url", buildLocalAdsbAircraftUrl("http://adsb-feeder.local:8080/data/aircraft.json"), "http://adsb-feeder.local:8080/data/aircraft.json");

  // Full config-file lines should parse like SD card input, including
  // whitespace around the separator and case-insensitive setting names.
  Settings lineCfg;
  RuntimeConfig lineRuntime;
  expectTrue("line with whitespace applies", applyConfigLine(lineCfg, lineRuntime, " speed = mph "));
  expectTrue("lowercase key applies", applyConfigLine(lineCfg, lineRuntime, "temp=f"));
  expectTrue("ssid line applies", applyConfigLine(lineCfg, lineRuntime, " SSID = kitchen wifi "));
  expectEqual("line speed", lineCfg.speed, SPD_MPH);
  expectEqual("line temp", lineCfg.temp, TEMP_F);
  expectEqual("line ssid", lineRuntime.wifiSSID, "kitchen wifi");

  // Blank, comment, and malformed lines are intentionally ignored by the
  // loader so comments or accidental notes do not mutate the active settings.
  expectTrue("blank ignored", !applyConfigLine(lineCfg, lineRuntime, "   "));
  expectTrue("comment ignored", !applyConfigLine(lineCfg, lineRuntime, " # SPEED=kts"));
  expectTrue("missing equals ignored", !applyConfigLine(lineCfg, lineRuntime, "SPEED kts"));
  expectEqual("ignored line preserves speed", lineCfg.speed, SPD_MPH);

  RuntimeConfig invalidRuntime;
  applyConfigValue(cfg, invalidRuntime, "SSID", "wifi");
  applyConfigValue(cfg, invalidRuntime, "TZ", "UTC0");
  applyConfigValue(cfg, invalidRuntime, "LAT", "47,5");
  applyConfigValue(cfg, invalidRuntime, "LON", "181");
  applyConfigValue(cfg, invalidRuntime, "ALT", "not-a-number");
  expectTrue("malformed required values rejected", !hasRequiredRuntimeConfig(invalidRuntime));

  applyConfigValue(cfg, invalidRuntime, "LAT", "0");
  applyConfigValue(cfg, invalidRuntime, "LON", "0");
  applyConfigValue(cfg, invalidRuntime, "ALT", "0");
  expectTrue("valid zero coordinates accepted", hasRequiredRuntimeConfig(invalidRuntime));

  Settings exampleCfg;
  RuntimeConfig exampleRuntime;
  std::ifstream example("config.example.txt");
  expectTrue("example config opens", example.is_open());
  std::set<std::string> exampleKeys;
  std::string line;
  while (std::getline(example, line)) {
    applyConfigLine(exampleCfg, exampleRuntime, line);
    size_t eq = line.find('=');
    if (eq != std::string::npos && !line.empty() && line[0] != '#') {
      exampleKeys.insert(line.substr(0, eq));
    }
  }
  const std::set<std::string> supportedKeys = {
    "SSID", "PASS", "LOCAL_ADSB_URL", "LAT", "LON", "ALT", "TZ", "SPEED",
    "HEIGHT", "TEMP", "RADIUS", "NIGHT_MODE", "BUSY", "MAX_REFRESH", "DEMO", "SD_LOG"
  };
  expectTrue("example config contains every supported key", exampleKeys == supportedKeys);
  expectTrue("example config has required values", hasRequiredRuntimeConfig(exampleRuntime));
  expectEqual("example config ssid", exampleRuntime.wifiSSID, "your-wifi-name");
  expectEqual("example config speed", exampleCfg.speed, SPD_KTS);
  expectEqual("example config height", exampleCfg.height, HGT_FTFL);
  expectEqual("example config temperature", exampleCfg.temp, TEMP_C);
  expectEqual("example config radius", exampleCfg.radius, 3);
  expectTrue("example config night mode", exampleCfg.night);
  expectEqual("example config busy interval", exampleCfg.busy, 30);
  expectEqual("example config maximum refresh", exampleCfg.maxRefresh, 0);
  expectTrue("example config demo disabled", !exampleCfg.demo);
  expectTrue("example config sd log disabled", !exampleCfg.sdLog);
  expectEqual("example config local feed", exampleRuntime.localAdsbBaseUrl, "http://192.168.1.20:8080");

  std::cout << "config tests passed\n";
  return 0;
}
