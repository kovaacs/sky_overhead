#pragma once

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include "Aircraft.h"
#include "JsonHelpers.h"

enum FetchResult {
  FETCH_ERROR,
  FETCH_EMPTY,
  FETCH_FOUND
};

static inline double toRad(double d) { return d * M_PI / 180.0; }

static inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  double dLat = toRad(lat2 - lat1), dLon = toRad(lon2 - lon1);
  double h = sin(dLat / 2) * sin(dLat / 2) +
             cos(toRad(lat1)) * cos(toRad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  return R * 2 * atan2(sqrt(h), sqrt(1 - h));
}

static inline FetchResult parseOverheadAircraft(
  const JsonDocument& doc,
  double observerLat,
  double observerLon,
  double observerAltM,
  Plane& best,
  double maxGroundKm = 0
) {
  best = Plane();
  double bestSlant = 1e9;
  JsonArrayConst aircraft = doc["ac"].as<JsonArrayConst>();
  if (aircraft.isNull()) aircraft = doc["aircraft"].as<JsonArrayConst>();

  for (JsonObjectConst a : aircraft) {
    if (a["lat"].isNull() || a["lon"].isNull()) continue;
    double altFt = altFeet(a["alt_geom"]);
    if (altFt < 0) altFt = altFeet(a["alt_baro"]);
    if (altFt < 0) continue;

    double lat = a["lat"], lon = a["lon"];
    double groundKm = haversineKm(observerLat, observerLon, lat, lon);
    double heightKm = (altFt * 0.3048 - observerAltM) / 1000.0;
    double slantKm  = sqrt(groundKm * groundKm + heightKm * heightKm);
    if (maxGroundKm > 0 && groundKm > maxGroundKm) continue;
    if (slantKm >= bestSlant) continue;

    bestSlant = slantKm;
    best.found = true;
    best.hex = jsonText(a["hex"]);
    best.callsign = jsonText(a["flight"]);
    best.category = jsonText(a["category"]);
    best.lat = lat;
    best.lon = lon;
    best.altFt = altFt;
    best.typeCode = jsonText(a["t"]);
    best.typeDesc = jsonText(a["desc"]);
    if (!textHasLength(best.typeDesc)) best.typeDesc = best.typeCode;
    best.reg = jsonText(a["r"]);
    best.slantKm = slantKm;
    best.hasGs = !a["gs"].isNull();
    best.gsKt = best.hasGs ? a["gs"].as<double>() : 0;
    best.hasVrate = !a["baro_rate"].isNull();
    best.vrateFpm = best.hasVrate ? a["baro_rate"].as<double>() : 0;
  }
  return best.found ? FETCH_FOUND : FETCH_EMPTY;
}
