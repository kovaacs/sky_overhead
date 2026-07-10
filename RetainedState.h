#pragma once

#include <math.h>
#include <stdio.h>

#include "Aircraft.h"

struct RetainedAircraftState {
  String lastSeen;
  String lastFrom;
  String lastTo;
  String lastRouteKey;
  String lastCities;
  String lastAircraft;
  String lastIdentity;
  String lastAirline;
  String lastCategory;
  String lastType;
  String lastReg;
  String lastMotion;
  long lastEpoch = 0;
};

static inline void rememberLastSeen(
  RetainedAircraftState& state,
  const Plane& p,
  HeightUnit height,
  SpeedUnit speed,
  long epoch
) {
  state.lastSeen = textHasLength(p.airline) ? p.airline : p.callsign;
  state.lastAirline = p.airline;
  String key = routeKey(p);
  if (p.routeOk) {
    state.lastRouteKey = key;
    state.lastFrom = p.fromCode;
    state.lastTo = p.toCode;
    state.lastCities = routeCities(p);
  } else if (key != state.lastRouteKey) {
    state.lastRouteKey = "";
    state.lastFrom = "";
    state.lastTo = "";
    state.lastCities = "";
  }
  state.lastAircraft = aircraftLabel(p);
  state.lastIdentity = aircraftIdentity(p);
  state.lastCategory = p.category;
  state.lastType = p.typeDesc;
  state.lastReg = p.reg;
  state.lastMotion = motionText(p, height, speed);
  if (epoch > 0) state.lastEpoch = epoch;
}

static inline void applyRetainedRouteIfSame(Plane& p, const RetainedAircraftState& state) {
  if (p.routeOk || !textHasLength(state.lastRouteKey)) return;
  if (routeKey(p) != state.lastRouteKey) return;
  p.fromCode = state.lastFrom;
  p.toCode = state.lastTo;
}

static inline String foundRenderSignature(const Plane& p, int lowBucket) {
  char sig[320];
  snprintf(sig, sizeof(sig), "F|%s|%s|%s|%s|%s|%s|%s|%s|%s|%d|%d|%ld|%ld|%d",
           p.hex.c_str(), p.category.c_str(), p.airline.c_str(), p.callsign.c_str(),
           p.fromCode.c_str(), p.toCode.c_str(), p.typeCode.c_str(), p.typeDesc.c_str(),
           p.reg.c_str(), p.hasVrate ? 1 : 0, p.hasGs ? 1 : 0,
           lround(p.altFt / 500.0), lround(p.slantKm / 5.0), lowBucket);
  return String(sig);
}

static inline String emptyRenderSignature(const RetainedAircraftState& state, int lowBucket) {
  char sig[320];
  snprintf(sig, sizeof(sig), "E|%d|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s",
           lowBucket, state.lastSeen.c_str(), state.lastFrom.c_str(), state.lastTo.c_str(),
           state.lastCities.c_str(), state.lastAircraft.c_str(), state.lastIdentity.c_str(),
           state.lastAirline.c_str(), state.lastCategory.c_str(), state.lastType.c_str(),
           state.lastReg.c_str(), state.lastMotion.c_str());
  return String(sig);
}
