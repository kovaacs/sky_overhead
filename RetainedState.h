#pragma once

#include <stdint.h>
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
  String lastSource;
  double lastAltFt = 0;
  double lastGsKt = 0;
  double lastVrateFpm = 0;
  bool lastHasGs = false;
  bool lastHasVrate = false;
  long lastEpoch = 0;
};

static inline bool periodicRefreshDue(uint32_t interval, long now, long lastRefresh) {
  if (interval == 0 || now <= 0) return false;
  if (lastRefresh <= 0) return true;
  return now >= lastRefresh && (uint32_t)(now - lastRefresh) >= interval;
}

static inline void rememberLastSeen(
  RetainedAircraftState& state,
  const Plane& p,
  const String& source,
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
  state.lastAltFt = p.altFt;
  state.lastGsKt = p.gsKt;
  state.lastVrateFpm = p.vrateFpm;
  state.lastHasGs = p.hasGs;
  state.lastHasVrate = p.hasVrate;
  state.lastSource = source;
  if (epoch > 0) state.lastEpoch = epoch;
}

static inline String retainedMotionText(
  const RetainedAircraftState& state,
  HeightUnit height,
  SpeedUnit speed
) {
  Plane p;
  p.altFt = state.lastAltFt;
  p.gsKt = state.lastGsKt;
  p.vrateFpm = state.lastVrateFpm;
  p.hasGs = state.lastHasGs;
  p.hasVrate = state.lastHasVrate;
  return motionText(p, height, speed);
}

static inline String displaySourceForResult(
  bool found,
  const String& currentSource,
  const RetainedAircraftState& retained
) {
  if (!found && textHasLength(retained.lastSource)) return retained.lastSource;
  return currentSource;
}

static inline bool applyRetainedRouteIfSame(Plane& p, const RetainedAircraftState& state) {
  if (p.routeOk || !textHasLength(state.lastRouteKey)) return false;
  if (routeKey(p) != state.lastRouteKey) return false;
  p.fromCode = state.lastFrom;
  p.toCode = state.lastTo;
  return textHasLength(p.fromCode) || textHasLength(p.toCode);
}

static inline String foundRenderSignature(const Plane& p, int lowBucket) {
  char sig[320];
  snprintf(sig, sizeof(sig), "A|%d|%s|%s|%s|%s|%s|%s|%s",
           lowBucket, p.category.c_str(), p.airline.c_str(), aircraftIdentity(p).c_str(),
           p.fromCode.c_str(), p.toCode.c_str(), aircraftLabel(p).c_str(), p.typeDesc.c_str());
  return String(sig);
}

static inline String emptyRenderSignature(const RetainedAircraftState& state, int lowBucket) {
  if (!textHasLength(state.lastAircraft)
      && !textHasLength(state.lastIdentity)
      && !textHasLength(state.lastSeen)) {
    char clearSig[16];
    snprintf(clearSig, sizeof(clearSig), "C|%d", lowBucket);
    return String(clearSig);
  }

  char sig[320];
  snprintf(sig, sizeof(sig), "A|%d|%s|%s|%s|%s|%s|%s|%s",
           lowBucket, state.lastCategory.c_str(), state.lastAirline.c_str(),
           state.lastIdentity.c_str(), state.lastFrom.c_str(), state.lastTo.c_str(),
           state.lastAircraft.c_str(), state.lastType.c_str());
  return String(sig);
}
