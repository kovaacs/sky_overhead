#pragma once

#include "Aircraft.h"

constexpr const char* DEFAULT_AIRCRAFT_INFO_URL = "https://www.flightradar24.com/{hex}";
constexpr size_t AIRCRAFT_INFO_URL_MAX = 58;

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

static inline bool aircraftLinkHttpUrl(const String& url) {
#if defined(ARDUINO)
  return url.startsWith("https://") || url.startsWith("http://");
#else
  return url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
#endif
}

static inline String aircraftInfoUrl(const Plane& p, String urlTemplate = DEFAULT_AIRCRAFT_INFO_URL) {
  String id = normalizeAircraftLinkId(p.hex, 8);
  if (!textHasLength(id)) return "";

  size_t start = 0;
  size_t end = urlTemplate.length();
  while (start < end && aircraftLinkWhitespace(urlTemplate[start])) start++;
  while (end > start && aircraftLinkWhitespace(urlTemplate[end - 1])) end--;
#if defined(ARDUINO)
  urlTemplate = urlTemplate.substring(start, end);
  if (urlTemplate.indexOf("{hex}") < 0) return "";
  urlTemplate.replace("{hex}", id);
#else
  urlTemplate = urlTemplate.substr(start, end - start);
  size_t token = urlTemplate.find("{hex}");
  if (token == String::npos) return "";
  while (token != String::npos) {
    urlTemplate.replace(token, 5, id);
    token = urlTemplate.find("{hex}", token + id.length());
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
