#pragma once

#include "AdsbParser.h"

template <typename PrimaryFetcher, typename FallbackFetcher>
static inline FetchResult fetchWithFallback(Plane& best, PrimaryFetcher&& primary, FallbackFetcher&& fallback) {
  FetchResult result = primary(best);
  if (result != FETCH_ERROR) return result;

  FetchResult fallbackResult = fallback(best);
  if (fallbackResult != FETCH_ERROR) return fallbackResult;
  return result;
}

template <typename PrimaryFetcher, typename FallbackFetcher>
static inline FetchResult fetchWithFallbackSource(
  Plane& best,
  PrimaryFetcher&& primary,
  const char* primarySource,
  FallbackFetcher&& fallback,
  const char* fallbackSource,
  String& source
) {
  source = "";
  FetchResult result = primary(best);
  if (result == FETCH_FOUND) {
    source = primarySource;
    return result;
  }
  bool hasPrimaryNonError = result != FETCH_ERROR;

  FetchResult fallbackResult = fallback(best);
  if (fallbackResult != FETCH_ERROR) {
    source = fallbackSource;
    return fallbackResult;
  }
  if (hasPrimaryNonError) {
    source = primarySource;
  }
  return result;
}

static inline String dataSourceText(const String& aircraftSource, bool routeSourceUsed, bool retainedRouteUsed = false) {
  String text = aircraftSource;
  if (routeSourceUsed) {
    if (textHasLength(text)) text += " + ";
    text += "adsb.im";
  }
  if (retainedRouteUsed) {
    if (textHasLength(text)) text += " + ";
    text += "retained route";
  }
  return text;
}
