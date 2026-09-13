#pragma once

#include <stdint.h>

#include "Aircraft.h"

constexpr const char* DEFAULT_AIRCRAFT_INFO_URL = "https://www.flightradar24.com/data/aircraft/{reg}";
constexpr size_t AIRCRAFT_INFO_URL_MAX = 53;

static inline bool aircraftLinkWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline String normalizeAircraftLinkId(const String& raw, size_t maxLength) {
  size_t start = 0;
  size_t end = raw.length();
  while (start < end && aircraftLinkWhitespace(raw[start])) start++;
  while (end > start && aircraftLinkWhitespace(raw[end - 1])) end--;
  if (end - start == 0 || end - start > maxLength) return "";

  String normalized;
  for (size_t i = start; i < end; i++) {
    char c = raw[i];
    bool allowed = (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9')
                || c == '-';
    if (!allowed) return "";
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    normalized += c;
  }
  return normalized;
}

static inline char aircraftLinkLower(char c) {
  return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static inline bool aircraftLinkStartsWith(const String& text, const char* prefix) {
  size_t i = 0;
  while (prefix[i]) {
    if (i >= text.length() || aircraftLinkLower(text[i]) != prefix[i]) return false;
    i++;
  }
  return true;
}

static inline bool aircraftLinkHttpUrl(const String& url) {
  size_t authorityStart = 0;
  if (aircraftLinkStartsWith(url, "https://")) authorityStart = 8;
  else if (aircraftLinkStartsWith(url, "http://")) authorityStart = 7;
  else return false;

  size_t authorityEnd = url.length();
  for (size_t i = 0; i < url.length(); i++) {
    unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= ' ' || c == 127) return false;
    if (i >= authorityStart && authorityEnd == url.length()
        && (c == '/' || c == '?' || c == '#')) authorityEnd = i;
  }
  return authorityEnd > authorityStart;
}

static inline String aircraftInfoUrl(const Plane& p, String urlTemplate = DEFAULT_AIRCRAFT_INFO_URL) {
  String reg = normalizeAircraftLinkId(p.reg, 12);
  if (!textHasLength(reg)) return "";

  size_t start = 0;
  size_t end = urlTemplate.length();
  while (start < end && aircraftLinkWhitespace(urlTemplate[start])) start++;
  while (end > start && aircraftLinkWhitespace(urlTemplate[end - 1])) end--;
#if defined(ARDUINO)
  urlTemplate = urlTemplate.substring(start, end);
  if (urlTemplate.indexOf("{reg}") < 0) return "";
  urlTemplate.replace("{reg}", reg);
#else
  urlTemplate = urlTemplate.substr(start, end - start);
  size_t token = urlTemplate.find("{reg}");
  if (token == String::npos) return "";
  while (token != String::npos) {
    urlTemplate.replace(token, 5, reg);
    token = urlTemplate.find("{reg}", token + reg.length());
  }
#endif
  if (!aircraftLinkHttpUrl(urlTemplate) || urlTemplate.length() > AIRCRAFT_INFO_URL_MAX) return "";
  return urlTemplate;
}

static inline uint32_t aircraftInfoUrlHash(const String& url) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < url.length(); i++) {
    hash ^= static_cast<uint8_t>(url[i]);
    hash *= 16777619u;
  }
  return hash;
}
