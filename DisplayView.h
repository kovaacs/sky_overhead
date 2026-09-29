#pragma once

#include <stdint.h>

#include "RetainedState.h"

struct DisplayIconSet {
  char planeGlyph;
  uint8_t planeSize;
  char helicopterGlyph;
  uint8_t helicopterSize;
  char clearGlyph;
  uint8_t clearSize;
};

struct LeftColumnView {
  char glyph = 0;
  uint8_t glyphSize = 0;
  String title;
  String titleFallback;
  String routeFrom;
  String routeTo;
  String line1;
  String line2;
  String position;
};

static inline String frameFooterRefreshedText(const String& refreshedText) {
  return "Last refreshed: " + refreshedText;
}

static inline String frameFooterSourceText(const String& sourceText) {
  return "Source: " + sourceText;
}

static inline void cascadeDuplicateLines(LeftColumnView& v) {
  String* lines[] = { &v.title, &v.line1, &v.line2, &v.position };
  const uint8_t lineCount = sizeof(lines) / sizeof(lines[0]);

  for (uint8_t i = 0; i < lineCount; i++) {
    if (!lines[i]->length()) continue;
    for (uint8_t j = 0; j < i; j++) {
      if (lines[j]->length() && sameAircraftText(*lines[i], *lines[j])) {
        *lines[i] = "";
        break;
      }
    }
  }
}

static inline LeftColumnView makeLiveAircraftView(
  const Plane& p,
  HeightUnit height,
  SpeedUnit speed,
  const DisplayIconSet& icons
) {
  const bool helicopter = isHelicopter(p);
  LeftColumnView v;
  v.glyph = helicopter ? icons.helicopterGlyph : icons.planeGlyph;
  v.glyphSize = helicopter ? icons.helicopterSize : icons.planeSize;
  v.titleFallback = aircraftLabel(p);
  v.title = p.typeDesc.length() ? p.typeDesc : v.titleFallback;
  v.routeFrom = p.fromCode;
  v.routeTo = p.toCode;
  v.line1 = aircraftIdentity(p);
  v.line2 = p.airline;
  v.position = motionText(p, height, speed);
  cascadeDuplicateLines(v);
  return v;
}

static inline bool hasRetainedAircraft(const RetainedAircraftState& retained) {
  return retained.lastAircraft.length()
      || retained.lastIdentity.length()
      || retained.lastSeen.length();
}

static inline LeftColumnView makeRetainedAircraftView(
  const RetainedAircraftState& retained,
  HeightUnit height,
  SpeedUnit speed,
  const DisplayIconSet& icons
) {
  LeftColumnView v;
  if (!hasRetainedAircraft(retained)) {
    v.glyph = icons.clearGlyph;
    v.glyphSize = icons.clearSize;
    v.title = "Clear skies";
    return v;
  }

  const bool helicopter = retained.lastCategory == "A7";
  v.glyph = helicopter ? icons.helicopterGlyph : icons.planeGlyph;
  v.glyphSize = helicopter ? icons.helicopterSize : icons.planeSize;
  v.titleFallback = retained.lastAircraft;
  v.title = retained.lastType.length() ? retained.lastType : retained.lastAircraft;
  if (!v.title.length()) v.title = "Aircraft";
  v.line1 = retained.lastIdentity.length() ? retained.lastIdentity : retained.lastSeen;
  v.line2 = retained.lastAirline;
  v.routeFrom = retained.lastFrom;
  v.routeTo = retained.lastTo;
  v.position = retainedMotionText(retained, height, speed);
  cascadeDuplicateLines(v);
  return v;
}
