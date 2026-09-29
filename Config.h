#pragma once

#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <math.h>

#include "Aircraft.h"
#include "AircraftLink.h"
#include "Climate.h"

constexpr uint16_t MAX_RADIUS_KM = 463;

struct Settings {
  SpeedUnit speed = SPD_KPH;
  HeightUnit height = HGT_FTFL;
  TempUnit temp = TEMP_C;
  uint16_t radius = 30;
  bool night = false;
  uint16_t nightStart = 0;
  uint16_t nightEnd = 0;
  uint16_t busy = 60;
  uint32_t maxRefresh = 0;
  bool demo = false;
  bool sdLog = false;
};

struct RuntimeConfig {
  String wifiSSID;
  String wifiPass;
  String tzInfo;
  String localAdsbBaseUrl;
  String qrUrlTemplate = DEFAULT_AIRCRAFT_INFO_URL;
  double myLat = 0.0;
  double myLon = 0.0;
  double myAltM = 0.0;
  bool hasLat = false;
  bool hasLon = false;
  bool hasAlt = false;
};

static inline String lowerValue(String s) {
  s = trimCopy(s);
#if defined(ARDUINO)
  s.toLowerCase();
#else
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
#endif
  return s;
}

static inline int stringToInt(const char* s) {
  return (int)strtol(s, nullptr, 10);
}

static inline bool parseDoubleStrict(const String& s, double& value) {
  String text = trimCopy(s);
  if (!text.length()) return false;
  char* end = nullptr;
  double parsed = strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !isfinite(parsed)) return false;
  value = parsed;
  return true;
}

static inline bool hasRequiredRuntimeConfig(const RuntimeConfig& runtime) {
  return runtime.wifiSSID.length() && runtime.tzInfo.length() &&
         runtime.hasLat && runtime.hasLon && runtime.hasAlt;
}

static inline bool parseHHMM(const String& value, uint16_t& minuteOfDay) {
  String text = trimCopy(value);
  std::string_view sv(text.c_str(), text.length());
  size_t colon = sv.find(':');
  if (colon == std::string_view::npos || sv.find(':', colon + 1) != std::string_view::npos) return false;
  if (colon == 0 || colon > 2) return false;
  if (sv.length() - colon - 1 != 2) return false;
  for (size_t i = 0; i < sv.length(); i++) {
    if (i == colon) continue;
    if (sv[i] < '0' || sv[i] > '9') return false;
  }
  int h = stringToInt(text.c_str());
  int m = stringToInt(text.c_str() + colon + 1);
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  minuteOfDay = (uint16_t)(h * 60 + m);
  return true;
}

static inline bool parseNightMode(const String& value, uint16_t& start, uint16_t& end) {
  String range = trimCopy(value);
  if (!range.length()) return false;

  std::string_view sv(range.c_str(), range.length());
  size_t dash = sv.find('-');
  if (dash == std::string_view::npos || sv.find('-', dash + 1) != std::string_view::npos) return false;
  String startText = trimCopy(String(sv.data(), dash));
  String endText = trimCopy(String(sv.data() + dash + 1, sv.length() - dash - 1));

  uint16_t parsedStart = 0;
  uint16_t parsedEnd = 0;
  if (!parseHHMM(startText, parsedStart) || !parseHHMM(endText, parsedEnd)) return false;

  start = parsedStart;
  end = parsedEnd;
  return true;
}

static inline String buildLocalAdsbAircraftUrl(String baseUrl) {
  baseUrl = trimCopy(baseUrl);
  if (!baseUrl.length()) return "";

#if defined(ARDUINO)
  if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
    baseUrl = "http://" + baseUrl;
  }
  while (baseUrl.endsWith("/")) baseUrl.remove(baseUrl.length() - 1);
  if (baseUrl.endsWith("/data/aircraft.json")) return baseUrl;
#else
  if (!baseUrl.starts_with("http://") && !baseUrl.starts_with("https://")) {
    baseUrl = "http://" + baseUrl;
  }
  while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  if (baseUrl.ends_with("/data/aircraft.json")) return baseUrl;
#endif

  baseUrl += "/data/aircraft.json";
  return baseUrl;
}

static inline void applyConfigValue(Settings& cfg, RuntimeConfig& runtime, String key, String val) {
  key = trimCopy(key);
#if defined(ARDUINO)
  key.toUpperCase();
#else
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
#endif
  val = trimCopy(val);

  if      (key == "SSID") runtime.wifiSSID = val;
  else if (key == "PASS") runtime.wifiPass = val;
  else if (key == "LAT") {
    double parsed = 0;
    runtime.hasLat = parseDoubleStrict(val, parsed) && parsed >= -90.0 && parsed <= 90.0;
    if (runtime.hasLat) runtime.myLat = parsed;
  }
  else if (key == "LON") {
    double parsed = 0;
    runtime.hasLon = parseDoubleStrict(val, parsed) && parsed >= -180.0 && parsed <= 180.0;
    if (runtime.hasLon) runtime.myLon = parsed;
  }
  else if (key == "ALT") {
    double parsed = 0;
    runtime.hasAlt = parseDoubleStrict(val, parsed);
    if (runtime.hasAlt) runtime.myAltM = parsed;
  }
  else if (key == "TZ")   runtime.tzInfo = val;
  else if (key == "LOCAL_ADSB_URL") runtime.localAdsbBaseUrl = val;
  else if (key == "QR_URL") runtime.qrUrlTemplate = val;
  else if (key == "SPEED") {
    String v = lowerValue(val);
    if      (v == "mph") cfg.speed = SPD_MPH;
    else if (v == "kts") cfg.speed = SPD_KTS;
    else                 cfg.speed = SPD_KPH;
  }
  else if (key == "HEIGHT") cfg.height = (lowerValue(val) == "metric") ? HGT_METRIC : HGT_FTFL;
  else if (key == "TEMP") cfg.temp = (lowerValue(val) == "f") ? TEMP_F : TEMP_C;
  else if (key == "RADIUS") cfg.radius = (uint16_t)std::clamp(stringToInt(val.c_str()), 1, (int)MAX_RADIUS_KM);
  else if (key == "NIGHT_MODE") {
    uint16_t start = 0, end = 0;
    cfg.night = parseNightMode(val, start, end);
    if (cfg.night) {
      cfg.nightStart = start;
      cfg.nightEnd = end;
    }
  }
  else if (key == "BUSY") cfg.busy = (uint16_t)std::clamp(stringToInt(val.c_str()), 15, 600);
  else if (key == "MAX_REFRESH") {
    int seconds = stringToInt(val.c_str());
    cfg.maxRefresh = seconds <= 0 ? 0 : (uint32_t)std::clamp(seconds, 60, 86400);
  }
  else if (key == "DEMO") {
    String v = lowerValue(val);
    cfg.demo = (v == "1" || v == "true" || v == "on");
  }
  else if (key == "SD_LOG") {
    String v = lowerValue(val);
    cfg.sdLog = (v == "1" || v == "true" || v == "on");
  }
}

static inline bool applyConfigLine(Settings& cfg, RuntimeConfig& runtime, String line) {
  line = trimCopy(line);
  if (!line.length() || line[0] == '#') return false;

  std::string_view sv(line.c_str(), line.length());
  size_t eq = sv.find('=');
  if (eq == std::string_view::npos) return false;

  applyConfigValue(cfg, runtime, String(sv.data(), eq),
                   String(sv.data() + eq + 1, sv.length() - eq - 1));
  return true;
}
