#pragma once

#include "DisplayView.h"

static inline String fit(const String& s, int maxW) {
  if (epaper.textWidth(s) <= maxW) return s;
  String t = s;
  while (t.length() && epaper.textWidth(t + "…") > maxW) t.remove(t.length() - 1);
  return t + "…";
}

static inline void drawIcon(char glyph, int x, int y, uint8_t nativeSize) {
  epaper.setFreeFont(&SkyIcon24);
  uint8_t baseline = nativeSize > 127 ? 127 : nativeSize;
  epaper.drawChar(x, y + baseline, glyph, TFT_BLACK, TFT_WHITE, 1);
}

static inline void drawIconCentered(char glyph, int cx, int cy, uint8_t nativeSize) {
  drawIcon(glyph, cx - nativeSize / 2, cy - nativeSize / 2, nativeSize);
}

static inline void batteryGlyph(int x, int y, int pct) {
  if (pct < 0) return;
  char glyph = icon::BATTERY_EMPTY;
  if (pct >= 85) glyph = icon::BATTERY_FULL;
  else if (pct >= 45) glyph = icon::BATTERY_MEDIUM;
  else if (pct >= 15) glyph = icon::BATTERY_LOW;
  drawIcon(glyph, x, y - 6, icon::BATTERY_EMPTY_SIZE);
}

static inline void drawRouteArrow(int x, int y, int len) {
  drawIconCentered(icon::ARROW_RIGHT, x + len / 2, y, icon::ARROW_RIGHT_SIZE);
}

static inline void drawRouteCodes(const String& fromCode, const String& toCode, int cx, int cy) {
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.setTextDatum(TL_DATUM);
  int fromW = epaper.textWidth(fromCode);
  int arrowW = icon::ARROW_RIGHT_SIZE;
  int totalW = fromW + ui::ROUTE_GAP + arrowW + ui::ROUTE_GAP + epaper.textWidth(toCode);
  int x = cx - totalW / 2;
  int y = cy - ui::ROUTE_TEXT_HALF_H;
  epaper.drawString(fromCode, x, y);
  drawRouteArrow(x + fromW + ui::ROUTE_GAP, cy, arrowW);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString(toCode, x + fromW + ui::ROUTE_GAP + arrowW + ui::ROUTE_GAP, y);
}

static inline DisplayIconSet displayIcons() {
  DisplayIconSet icons;
  icons.planeGlyph = icon::PLANE_LARGE;
  icons.planeSize = icon::PLANE_LARGE_SIZE;
  icons.helicopterGlyph = icon::HELICOPTER_LARGE;
  icons.helicopterSize = icon::HELICOPTER_LARGE_SIZE;
  icons.clearGlyph = icon::CLOUDY_LARGE;
  icons.clearSize = icon::CLOUDY_LARGE_SIZE;
  return icons;
}

static inline void drawClimatePanel(const Climate& c, TempUnit tempUnit) {
  epaper.setTextDatum(MC_DATUM);

  if (!c.ok) {
    drawIconCentered(icon::THERMOMETER, ui::ICON_X, ui::TEMP_Y, icon::THERMOMETER_SIZE);
    drawIconCentered(icon::DROPLET, ui::ICON_X, ui::HUM_Y, icon::DROPLET_SIZE);
    epaper.setTextDatum(ML_DATUM);
    epaper.setFreeFont(&FreeSansBold24pt7b);
    epaper.drawString("--", ui::NUM_X, ui::TEMP_Y);
    epaper.drawString("--", ui::NUM_X, ui::HUM_Y);
    epaper.setTextDatum(TL_DATUM);
    return;
  }

  drawIconCentered(icon::THERMOMETER, ui::ICON_X, ui::TEMP_Y, icon::THERMOMETER_SIZE);
  epaper.setTextDatum(ML_DATUM);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  char tnum[8];
  formatTemperature(tnum, sizeof(tnum), tempUnit, c.tempC);
  epaper.drawString(tnum, ui::NUM_X, ui::TEMP_Y);
  int w = epaper.textWidth(tnum);
  int dx = ui::NUM_X + w + 10, dy = ui::TEMP_Y - 14;
  epaper.drawCircle(dx, dy, 5, TFT_BLACK);
  epaper.drawCircle(dx, dy, 4, TFT_BLACK);
  epaper.drawString(climateUnit(tempUnit), dx + 12, ui::TEMP_Y);

  drawIconCentered(icon::DROPLET, ui::ICON_X, ui::HUM_Y, icon::DROPLET_SIZE);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  char hnum[8];
  formatHumidity(hnum, sizeof(hnum), c.hum);
  epaper.drawString(hnum, ui::NUM_X, ui::HUM_Y);

  epaper.setTextDatum(TL_DATUM);
}

static inline void drawFrameHeader(int batt) {
  drawIcon(icon::PLANE, ui::HDR_ICON_X, ui::HDR_ICON_Y, icon::PLANE_SIZE);
  epaper.setFreeFont(&FreeSansBold9pt7b);
  epaper.drawString("SKY OVERHEAD", ui::HDR_TEXT_X, ui::HDR_TEXT_Y);
  batteryGlyph(ui::BATT_X, ui::BATT_Y, batt);
}

static inline void drawFrameFooter(const String& refreshedText, const String& sourceText = "") {
  epaper.setFreeFont(&FreeSansBold12pt7b);
  epaper.setTextDatum(MC_DATUM);
  String refreshed = frameFooterRefreshedText(refreshedText);
  if (!textHasLength(sourceText)) {
    epaper.drawString(fit(refreshed, ui::SCREEN_W - ui::MARGIN * 2), ui::SCREEN_W / 2, ui::FOOTER_Y);
    epaper.setTextDatum(TL_DATUM);
    return;
  }

  const int maxW = ui::SCREEN_W - ui::MARGIN * 2;
  const int sepW = 24;
  int refreshedW = epaper.textWidth(refreshed);
  String source = fit(frameFooterSourceText(sourceText), maxW - refreshedW - sepW);
  int sourceW = epaper.textWidth(source);
  int totalW = refreshedW + sepW + sourceW;
  if (totalW > maxW) {
    refreshed = fit(refreshed, maxW - sepW - sourceW);
    refreshedW = epaper.textWidth(refreshed);
    totalW = refreshedW + sepW + sourceW;
  }

  int x = ui::SCREEN_W / 2 - totalW / 2;
  epaper.drawString(refreshed, x + refreshedW / 2, ui::FOOTER_Y);
  drawIconCentered(icon::DOT, x + refreshedW + sepW / 2, ui::FOOTER_Y, icon::DOT_SIZE);
  epaper.setFreeFont(&FreeSansBold12pt7b);
  epaper.setTextDatum(MC_DATUM);
  epaper.drawString(source, x + refreshedW + sepW + sourceW / 2, ui::FOOTER_Y);
  epaper.setTextDatum(TL_DATUM);
}

static inline bool hasRoute(const LeftColumnView& v) {
  return v.routeFrom.length() || v.routeTo.length();
}

static inline int leftTextRowCount(const LeftColumnView& v) {
  int count = 0;
  if (v.line1.length()) count++;
  if (v.line2.length()) count++;
  if (hasRoute(v)) count++;
  if (v.position.length()) count++;
  return count;
}

static inline int leftRowHeight(const LeftColumnView& v, uint8_t row) {
  uint8_t visible = 0;
  if (v.line1.length() && visible++ == row) return 36;
  if (v.line2.length() && visible++ == row) return 36;
  if (hasRoute(v) && visible++ == row) return 54;
  if (v.position.length() && visible++ == row) return 36;
  return 0;
}

static inline int leftStackHeight(const LeftColumnView& v) {
  int h = v.glyphSize + 18 + 42;
  for (uint8_t row = 0; row < leftTextRowCount(v); row++) h += leftRowHeight(v, row);
  return h;
}

static inline void drawLeftText(int cx, int y, const String& text, const GFXfont* font) {
  epaper.setFreeFont(font);
  epaper.setTextDatum(MC_DATUM);
  epaper.drawString(fit(text, ui::LEFT_TEXT_W), cx, y);
}

static inline void drawLeftTitle(int cx, int y, const LeftColumnView& v) {
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.setTextDatum(MC_DATUM);
  String title = v.title;
  if (textHasLength(v.titleFallback) && epaper.textWidth(title) > ui::LEFT_TEXT_W) {
    title = v.titleFallback;
  }
  epaper.drawString(fit(title, ui::LEFT_TEXT_W), cx, y);
}

static inline void drawPositionText(int cx, int y, const String& text) {
  const String sep = "  ...  ";
  String part[3];
  int count = 0;
  int start = 0;
  while (count < 3) {
    int split = text.indexOf(sep, start);
    part[count++] = split < 0 ? text.substring(start) : text.substring(start, split);
    if (split < 0) break;
    start = split + sep.length();
  }
  if (count <= 1) {
    drawLeftText(cx, y, text, &FreeSans18pt7b);
    return;
  }

  epaper.setFreeFont(&FreeSans18pt7b);
  int sepW = 22;
  const GFXfont* motionFont = &FreeSans18pt7b;
  int width[3];
  int totalW = 0;
  for (int i = 0; i < count; i++) {
    width[i] = epaper.textWidth(part[i]);
    totalW += width[i];
  }
  totalW += sepW * (count - 1);
  if (totalW > ui::LEFT_TEXT_W) {
    epaper.setFreeFont(&FreeSans12pt7b);
    motionFont = &FreeSans12pt7b;
    sepW = 18;
    totalW = 0;
    for (int i = 0; i < count; i++) {
      width[i] = epaper.textWidth(part[i]);
      totalW += width[i];
    }
    totalW += sepW * (count - 1);
    if (totalW > ui::LEFT_TEXT_W) {
      String compact;
      for (int i = 0; i < count; i++) {
        if (compact.length()) compact += " ";
        compact += part[i];
      }
      drawLeftText(cx, y, compact, &FreeSans12pt7b);
      return;
    }
  }

  int x = cx - totalW / 2;
  epaper.setTextDatum(MC_DATUM);
  epaper.setFreeFont(motionFont);
  for (int i = 0; i < count; i++) {
    epaper.drawString(part[i], x + width[i] / 2, y);
    x += width[i];
    if (i < count - 1) {
      drawIconCentered(icon::DOT, x + sepW / 2, y, icon::DOT_SIZE);
      epaper.setFreeFont(motionFont);
      epaper.setTextDatum(MC_DATUM);
      x += sepW;
    }
  }
}

static inline void drawLeftRoute(const LeftColumnView& v, int cx, int y) {
  String fc = v.routeFrom.length() ? v.routeFrom : "???";
  String tc = v.routeTo.length()   ? v.routeTo   : "???";
  drawRouteCodes(fc, tc, cx, y);
  epaper.setTextDatum(MC_DATUM);
}

static inline void drawLeftColumn(LeftColumnView v) {
  int cx = ui::LEFT_CX;
  int top = (ui::CONTENT_TOP_Y + ui::FOOTER_Y - leftStackHeight(v)) / 2;
  if (top < ui::CONTENT_TOP_Y + 6) top = ui::CONTENT_TOP_Y + 6;

  int y = top;
  drawIconCentered(v.glyph, cx, y + v.glyphSize / 2, v.glyphSize);
  y += v.glyphSize + 18;

  drawLeftTitle(cx, y + 21, v);
  y += 42;

  if (v.line1.length()) {
    drawLeftText(cx, y + 18, v.line1, &FreeSans18pt7b);
    y += 36;
  }
  if (v.line2.length()) {
    drawLeftText(cx, y + 18, v.line2, &FreeSans18pt7b);
    y += 36;
  }
  if (hasRoute(v)) {
    drawLeftRoute(v, cx, y + 27);
    y += 54;
  }
  if (v.position.length()) {
    drawPositionText(cx, y + 18, v.position);
  }
  epaper.setTextDatum(TL_DATUM);
}

static inline void drawNightSleep(uint16_t wakeMinute, int batt, const String& refreshedText) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  epaper.setTextDatum(TL_DATUM);
  drawFrameHeader(batt);

  epaper.setTextDatum(MC_DATUM);
  drawIconCentered(icon::MOON_STAR, ui::SLEEP_CX, ui::LEFT_ICON_Y, icon::MOON_STAR_SIZE);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString("Sleeping", ui::SLEEP_CX, ui::LEFT_TITLE_Y);
  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.drawString("Aircraft checks paused", ui::SLEEP_CX, ui::LEFT_SUBTITLE_Y);
  char wake[32];
  snprintf(wake, sizeof(wake), "until %02u:%02u", wakeMinute / 60, wakeMinute % 60);
  epaper.setFreeFont(&FreeSans12pt7b);
  epaper.drawString(wake, ui::SLEEP_CX, ui::SLEEP_WAKE_Y);

  drawFrameFooter(refreshedText);
}

static inline void drawLive(
  const Plane& p,
  int batt,
  const Climate& clim,
  TempUnit tempUnit,
  HeightUnit height,
  SpeedUnit speed,
  const RetainedAircraftView& retained,
  const String& refreshedText,
  const String& sourceText = ""
) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextDatum(TL_DATUM);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  drawFrameHeader(batt);
  if (p.found) drawLeftColumn(makeLiveAircraftView(p, height, speed, displayIcons()));
  else drawLeftColumn(makeRetainedAircraftView(retained, displayIcons()));

  drawClimatePanel(clim, tempUnit);

  drawFrameFooter(refreshedText, sourceText);
}
