#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <algorithm>
#include <cctype>
#include <string>
using String = std::string;
#endif

enum SpeedUnit { SPD_KPH = 0, SPD_MPH = 1, SPD_KTS = 2 };
enum HeightUnit { HGT_FTFL = 0, HGT_METRIC = 1 };

struct Plane {
  bool found = false;
  bool hasGs = false;
  bool hasVrate = false;
  bool routeOk = false;
  String callsign, hex, category, typeCode, typeDesc, reg, airline;
  String fromCity, fromCode, toCity, toCode;
  double lat = 0, lon = 0, altFt = 0;
  double slantKm = 0;
  double gsKt = 0, vrateFpm = 0;
};

static inline bool textHasLength(const String& s) {
#if defined(ARDUINO)
  return s.length() > 0;
#else
  return !s.empty();
#endif
}

static inline String textSubstring(const String& s, size_t start) {
#if defined(ARDUINO)
  return s.substring(start);
#else
  return s.substr(start);
#endif
}

static inline void textRemove(String& s, size_t index, size_t count) {
#if defined(ARDUINO)
  s.remove(index, count);
#else
  s.erase(index, count);
#endif
}

static inline String altStr(double ft, HeightUnit height) {
  char b[24];
  if (height == HGT_METRIC)
    snprintf(b, sizeof(b), "%.0f m", ft * 0.3048);
  else if (ft >= 10000)
    snprintf(b, sizeof(b), "FL%.0f", ft / 100.0);
  else
    snprintf(b, sizeof(b), "%.0f ft", ft);
  return String(b);
}

static inline String speedStr(double kt, SpeedUnit speed) {
  char b[24];
  if      (speed == SPD_MPH) snprintf(b, sizeof(b), "%.0f mph",  kt * 1.151);
  else if (speed == SPD_KTS) snprintf(b, sizeof(b), "%.0f kts",  kt);
  else                       snprintf(b, sizeof(b), "%.0f km/h", kt * 1.852);
  return String(b);
}

static inline const char* trendWord(double fpm) {
  if (fpm >  150) return "climb.";
  if (fpm < -150) return "desc.";
  return "level";
}

static inline String aircraftLabel(const Plane& p) {
  String s = textHasLength(p.typeCode) ? p.typeCode : p.typeDesc;
  if (!textHasLength(s)) s = textHasLength(p.callsign) ? p.callsign : p.hex;
  if (!textHasLength(s)) s = "Aircraft";
  return s;
}

static inline String normalizeAircraftText(String s) {
#if defined(ARDUINO)
  s.toUpperCase();
  s.replace(" ", "");
  s.replace("·", "");
#else
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
#endif
  return s;
}

static inline bool sameAircraftText(const String& a, const String& b) {
  return normalizeAircraftText(a) == normalizeAircraftText(b);
}

static inline String routeKey(const Plane& p) {
  if (textHasLength(p.callsign)) return p.callsign;
  return p.hex;
}

static inline String aircraftIdentity(const Plane& p) {
  String s = textHasLength(p.callsign) ? p.callsign : "";
  if (textHasLength(p.reg)) {
    if (!textHasLength(s)) s = p.reg;
    else if (!sameAircraftText(s, p.reg)) {
      s += " (";
      s += p.reg;
      s += ")";
    }
  }
  if (!textHasLength(s)) s = p.hex;
  return s;
}

static inline bool isHelicopter(const Plane& p) {
  return p.category == "A7";
}

static inline bool isHelicopterCategory(const char* category) {
  return strcmp(category, "A7") == 0;
}

static inline String routeCities(const Plane& p) {
  String cities;
  if (textHasLength(p.fromCity)) cities = p.fromCity;
  if (textHasLength(p.toCity)) {
    if (textHasLength(cities)) cities += " to ";
    cities += p.toCity;
  }
  return cities;
}

static inline void appendMotionPart(String& text, const String& part) {
  if (!textHasLength(part)) return;
  if (textHasLength(text)) text += "  ...  ";
  text += part;
}

static inline String motionText(const Plane& p, HeightUnit height, SpeedUnit speed) {
  String text;
  appendMotionPart(text, altStr(p.altFt, height));
  if (p.hasVrate) appendMotionPart(text, trendWord(p.vrateFpm));
  if (p.hasGs) appendMotionPart(text, speedStr(p.gsKt, speed));
  return text;
}
