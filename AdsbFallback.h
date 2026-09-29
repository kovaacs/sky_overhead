#pragma once

#include "AdsbParser.h"

template <typename PublicFetcher, typename LocalFetcher>
static inline FetchResult fetchPublicThenLocalSource(
  Plane& best,
  PublicFetcher&& publicAdsb,
  LocalFetcher&& local,
  String& source
) {
  source = "";
  FetchResult result = publicAdsb(best);
  if (result != FETCH_ERROR) {
    source = "adsb.lol";
    return result;
  }

  FetchResult localResult = local(best);
  if (localResult != FETCH_ERROR) {
    source = "local feed";
    return localResult;
  }
  return result;
}

static inline String dataSourceText(const String& aircraftSource, bool routeSourceUsed, bool retainedRouteUsed = false) {
  String text = aircraftSource;
  if (routeSourceUsed) {
    if (text.length()) text += " & ";
    text += "adsb.im";
  }
  if (retainedRouteUsed) {
    if (text.length()) text += " & ";
    text += "retained route";
  }
  return text;
}
