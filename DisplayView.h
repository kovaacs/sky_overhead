#pragma once

#include <stdint.h>

#include "Aircraft.h"

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
  String routeFrom;
  String routeTo;
  String line1;
  String line2;
  String position;
};

struct RetainedAircraftView {
  String lastSeen;
  String lastFrom;
  String lastTo;
  String lastAircraft;
  String lastIdentity;
  String lastAirline;
  String lastCategory;
  String lastType;
  String lastMotion;
};

template <typename View, typename SameText>
static inline void cascadeDuplicateLines(View& v, SameText sameText) {
  decltype(&v.title) lines[] = { &v.title, &v.line1, &v.line2, &v.position };
  const uint8_t lineCount = sizeof(lines) / sizeof(lines[0]);

  for (uint8_t i = 0; i < lineCount; i++) {
    if (!textHasLength(*lines[i])) continue;
    for (uint8_t j = 0; j < i; j++) {
      if (textHasLength(*lines[j]) && sameText(*lines[i], *lines[j])) {
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
  v.title = aircraftLabel(p);
  v.routeFrom = p.fromCode;
  v.routeTo = p.toCode;
  v.line1 = aircraftIdentity(p);
  v.line2 = p.airline;
  v.position = motionText(p, height, speed);
  cascadeDuplicateLines(v, sameAircraftText);
  return v;
}

static inline bool hasRetainedAircraft(const RetainedAircraftView& retained) {
  return textHasLength(retained.lastAircraft)
      || textHasLength(retained.lastIdentity)
      || textHasLength(retained.lastSeen);
}

static inline LeftColumnView makeRetainedAircraftView(
  const RetainedAircraftView& retained,
  const DisplayIconSet& icons
) {
  LeftColumnView v;
  if (!hasRetainedAircraft(retained)) {
    v.glyph = icons.clearGlyph;
    v.glyphSize = icons.clearSize;
    v.title = "Clear skies";
    return v;
  }

  const bool helicopter = isHelicopterCategory(retained.lastCategory.c_str());
  v.glyph = helicopter ? icons.helicopterGlyph : icons.planeGlyph;
  v.glyphSize = helicopter ? icons.helicopterSize : icons.planeSize;
  v.title = textHasLength(retained.lastAircraft) ? retained.lastAircraft : retained.lastType;
  if (!textHasLength(v.title)) v.title = "Aircraft";
  v.line1 = textHasLength(retained.lastIdentity) ? retained.lastIdentity : retained.lastSeen;
  v.line2 = retained.lastAirline;
  v.routeFrom = retained.lastFrom;
  v.routeTo = retained.lastTo;
  v.position = retained.lastMotion;
  cascadeDuplicateLines(v, sameAircraftText);
  return v;
}
