#pragma once

#include <ArduinoJson.h>

#include "Aircraft.h"
#include "JsonHelpers.h"

static inline void applyRouteResponse(const JsonDocument& doc, Plane& p) {
  p.routeOk = false;
  p.fromCity = "";
  p.toCity = "";
  p.fromCode = "";
  p.toCode = "";

  JsonObjectConst route = doc[0];
  if (route.isNull()) return;

  if (route["airline"].is<JsonObjectConst>()) {
    String airline = jsonText(route["airline"]["name"]);
#if defined(ARDUINO)
    if (airline.equalsIgnoreCase("unknown")) airline = "";
#else
    if (sameAircraftText(airline, "unknown")) airline = "";
#endif
    if (textHasLength(airline)) p.airline = airline;
  }

  if (route["plausible"].is<bool>() && !route["plausible"].as<bool>()) return;

  JsonArrayConst airports = route["_airports"].as<JsonArrayConst>();
  if (airports.size() < 2) return;

  JsonObjectConst origin = airports[0];
  JsonObjectConst destination = airports[airports.size() - 1];
  p.fromCity = jsonText(origin["location"]);
  p.toCity = jsonText(destination["location"]);
  p.fromCode = jsonText(origin["iata"]);
  p.toCode = jsonText(destination["iata"]);
  if (!textHasLength(p.fromCode)) p.fromCode = jsonText(origin["icao"]);
  if (!textHasLength(p.toCode)) p.toCode = jsonText(destination["icao"]);
  p.routeOk = textHasLength(p.fromCode) || textHasLength(p.toCode);
}
