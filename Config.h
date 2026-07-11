#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "Aircraft.h"
#include "Climate.h"

struct Settings {
  SpeedUnit speed = SPD_KPH;
  HeightUnit height = HGT_FTFL;
  TempUnit temp = TEMP_C;
  uint16_t radius = 30;
  bool night = false;
  uint16_t nightStart = 0;
  uint16_t nightEnd = 0;
  uint16_t busy = 60;
  bool demo = false;
};

struct RuntimeConfig {
  String wifiSSID;
  String wifiPass;
  String tzInfo;
  String localAdsbBaseUrl;
  double myLat = 0.0;
  double myLon = 0.0;
  double myAltM = 0.0;
};

static inline int clampInt(int value, int lo, int hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

static inline String trimCopy(String s) {
#if defined(ARDUINO)
  s.trim();
#else
  const char* ws = " \t\r\n";
  size_t start = s.find_first_not_of(ws);
  if (start == String::npos) return "";
  size_t end = s.find_last_not_of(ws);
  s = s.substr(start, end - start + 1);
#endif
  return s;
}

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

static inline int stringToInt(const String& s) {
#if defined(ARDUINO)
  return s.toInt();
#else
  return std::atoi(s.c_str());
#endif
}

static inline double stringToDouble(const String& s) {
#if defined(ARDUINO)
  return s.toDouble();
#else
  return std::atof(s.c_str());
#endif
}

static inline bool isDigitChar(char c) {
  return c >= '0' && c <= '9';
}

static inline bool parseHHMM(const String& value, uint16_t& minuteOfDay) {
  String text = trimCopy(value);
#if defined(ARDUINO)
  int colon = text.indexOf(':');
  if (colon < 0) return false;
  if (text.indexOf(':', colon + 1) >= 0) return false;
#else
  size_t colon = text.find(':');
  if (colon == String::npos) return false;
  if (text.find(':', colon + 1) != String::npos) return false;
#endif
  if (colon == 0 || colon > 2) return false;
  size_t minuteDigits = text.length() - colon - 1;
  if (minuteDigits != 2) return false;
  for (size_t i = 0; i < text.length(); i++) {
    if (i == colon) continue;
    if (!isDigitChar(text[i])) return false;
  }
  int h = stringToInt(textSubstring(text, 0));
  textRemove(text, 0, colon + 1);
  int m = stringToInt(text);
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  minuteOfDay = (uint16_t)(h * 60 + m);
  return true;
}

static inline bool parseNightMode(const String& value, uint16_t& start, uint16_t& end) {
  String range = trimCopy(value);
  if (!textHasLength(range)) return false;

#if defined(ARDUINO)
  int dash = range.indexOf('-');
  if (dash < 0 || range.indexOf('-', dash + 1) >= 0) return false;
  String startText = trimCopy(range.substring(0, dash));
  String endText = trimCopy(range.substring(dash + 1));
#else
  size_t dash = range.find('-');
  if (dash == String::npos || range.find('-', dash + 1) != String::npos) return false;
  String startText = trimCopy(range.substr(0, dash));
  String endText = trimCopy(range.substr(dash + 1));
#endif

  uint16_t parsedStart = 0;
  uint16_t parsedEnd = 0;
  if (!parseHHMM(startText, parsedStart) || !parseHHMM(endText, parsedEnd)) return false;

  start = parsedStart;
  end = parsedEnd;
  return true;
}

static inline String buildLocalAdsbAircraftUrl(String baseUrl) {
  baseUrl = trimCopy(baseUrl);
  if (!textHasLength(baseUrl)) return "";

#if defined(ARDUINO)
  if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
    baseUrl = "http://" + baseUrl;
  }
  while (baseUrl.endsWith("/")) baseUrl.remove(baseUrl.length() - 1);
  if (baseUrl.endsWith("/data/aircraft.json")) return baseUrl;
#else
  if (baseUrl.rfind("http://", 0) != 0 && baseUrl.rfind("https://", 0) != 0) {
    baseUrl = "http://" + baseUrl;
  }
  while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
  const String suffix = "/data/aircraft.json";
  if (baseUrl.size() >= suffix.size() && baseUrl.compare(baseUrl.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return baseUrl;
  }
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
  else if (key == "LAT")  runtime.myLat = stringToDouble(val);
  else if (key == "LON")  runtime.myLon = stringToDouble(val);
  else if (key == "ALT")  runtime.myAltM = stringToDouble(val);
  else if (key == "TZ")   runtime.tzInfo = val;
  else if (key == "LOCAL_ADSB_URL") runtime.localAdsbBaseUrl = val;
  else if (key == "SPEED") {
    String v = lowerValue(val);
    if      (v == "mph") cfg.speed = SPD_MPH;
    else if (v == "kts") cfg.speed = SPD_KTS;
    else                 cfg.speed = SPD_KPH;
  }
  else if (key == "HEIGHT") cfg.height = (lowerValue(val) == "metric") ? HGT_METRIC : HGT_FTFL;
  else if (key == "TEMP") cfg.temp = (lowerValue(val) == "f") ? TEMP_F : TEMP_C;
  else if (key == "RADIUS") cfg.radius = (uint16_t)clampInt(stringToInt(val), 1, 500);
  else if (key == "NIGHT_MODE") {
    uint16_t start = 0, end = 0;
    cfg.night = parseNightMode(val, start, end);
    if (cfg.night) {
      cfg.nightStart = start;
      cfg.nightEnd = end;
    }
  }
  else if (key == "BUSY") cfg.busy = (uint16_t)clampInt(stringToInt(val), 15, 600);
  else if (key == "DEMO") {
    String v = lowerValue(val);
    cfg.demo = (v == "1" || v == "true" || v == "on");
  }
}

static inline bool applyConfigLine(Settings& cfg, RuntimeConfig& runtime, String line) {
  line = trimCopy(line);
  if (!textHasLength(line) || line[0] == '#') return false;

#if defined(ARDUINO)
  int eq = line.indexOf('=');
  if (eq < 0) return false;
  String key = line.substring(0, eq);
  String val = line.substring(eq + 1);
#else
  size_t eq = line.find('=');
  if (eq == String::npos) return false;
  String key = line.substr(0, eq);
  String val = line.substr(eq + 1);
#endif

  applyConfigValue(cfg, runtime, key, val);
  return true;
}
