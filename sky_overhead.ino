/*
 * Sky Overhead — reTerminal E1001 (ESP32-S3)  ·  wall-appliance build
 * ============================================================================
 * Shows the nearest aircraft (airline, route, type, distance, altitude, speed)
 * on the left, with an onboard room temperature/humidity panel on the right.
 * Designed to behave like an appliance: no per-minute flashing, a graceful empty sky,
 * it survives network glitches and stays quiet at night. Config is edited directly
 * on /config.txt on the SD card.
 *
 * Data (both keyless):
 *   adsb.lol   — live position, aircraft type, registration
 *   adsbdb.com — airline + origin/destination airport, looked up by callsign
 *
 * Build settings:
 *   Display : Seeed_GFX   (driver.h must contain: #define BOARD_SCREEN_COMBO 520)
 *   Board   : XIAO_ESP32S3    PSRAM: OPI PSRAM (ON)
 *   Libs    : Seeed_GFX (a TFT_eSPI fork), ArduinoJson v7
 *
 * Hardware notes (verified for the E1001, but worth checking on your unit):
 *   Battery : read on GPIO1 after pulling GPIO21 high; ~2x voltage divider.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "esp_sleep.h"
#include "time.h"

#include <SD.h>
#include <SPI.h>

// Runtime config — loaded from /config.txt on the SD card at boot.
// If the card is missing or the file can't be parsed the device boots but
// won't connect to Wi-Fi; it will retry on the next timer wake.
String wifiSSID, wifiPass, tzInfo;
double myLat = 0.0, myLon = 0.0, myAltM = 0.0;

// Unit preferences — also loaded from config.txt.
// Defaults: aviation height (ft/FL), metric distance, km/h, Celsius.
enum SpeedUnit  { SPD_KPH = 0, SPD_MPH = 1, SPD_KTS = 2 };
enum DistUnit   { DIST_KM  = 0, DIST_MI  = 1 };
enum HeightUnit { HGT_FTFL = 0, HGT_METRIC = 1 };
enum TempUnit   { TEMP_C   = 0, TEMP_F   = 1 };


// ============================ CONSTANTS =============================
static const char* API_HOST   = "api.adsb.lol";   // or "api.airplanes.live"
static const char* USER_AGENT = "reTerminal-E1001-SkyOverhead/3.0";

// Toggle whole subsystems here if one ever misbehaves on your hardware.
#define ENABLE_BATTERY      1
#define DEBUG_LOG           1

namespace pin {
  constexpr int BAT_ADC    = 1;    // battery voltage sense
  constexpr int BAT_EN     = 21;   // battery-sense enable (drive HIGH to read)
  constexpr int I2C_SDA    = 19;   // onboard SHT4x temp/humidity sensor
  constexpr int I2C_SCL    = 20;
  // SD card (HSPI bus, E1001 power-enable pin)
  constexpr int SD_CS      = 14;
  constexpr int SD_SCK     = 7;
  constexpr int SD_MOSI    = 9;
  constexpr int SD_MISO    = 8;
  constexpr int SD_EN      = 16;   // E1001/E1002/E1004 — pull HIGH to power the slot
}

namespace timing {
  constexpr uint32_t RETRY_SECONDS  = 45;          // after a failed fetch or no-clock sleep
  constexpr uint32_t WIFI_TIMEOUT   = 20000;       // connect attempt (ms)
  constexpr uint32_t HTTP_TIMEOUT   = 8000;      // per request (ms)
  constexpr int      GHOST_CLEAN_EVERY = 20;     // full white flush every N redraws
}

// Screen layout. Panel is 800 x 480. Two columns: aircraft left, climate right.
namespace ui {
  constexpr int SCREEN_W = 800, SCREEN_H = 480;
  constexpr int MARGIN   = 24;
  constexpr int CONTENT_W = SCREEN_W - 2 * MARGIN;

  constexpr int HDR_TEXT_Y = 14;
  constexpr int BATT_X = 740, BATT_Y = 12, BATT_W = 40, BATT_H = 18;
  constexpr int RULE_Y = 40;

  // left column = aircraft (rows flow from y=66 in drawPlane)
  constexpr int LEFT_X = MARGIN;
  constexpr int LEFT_W = 410;

  // right panel = indoor climate (thermometer + droplet)
  constexpr int DIVIDER_X = 466;
  constexpr int PANEL_CX  = 632;     // panel centre, for the INDOOR header
  constexpr int ICON_X    = 548;     // icon centre
  constexpr int NUM_X     = 588;     // big number left edge
  constexpr int HEADER_Y  = 84;
  constexpr int TEMP_Y    = 205;     // vertical centre of the temperature row
  constexpr int HUM_Y     = 325;     // vertical centre of the humidity row

  constexpr int FOOTER_Y = 452;

}

// All settings — loaded from config.txt on the SD card.
struct Settings {
  SpeedUnit  speed   = SPD_KPH;
  DistUnit   dist    = DIST_KM;
  HeightUnit height  = HGT_FTFL;
  TempUnit   temp    = TEMP_C;
  uint16_t   radius  = 30;  // km, any value
  uint8_t    night   = 1;   // 0 = off, 1 = 22:00-07:00, 2 = 23:00-06:00
  uint16_t   busy    = 60;  // seconds, any value
};

// State that survives deep sleep but not a power cycle.
RTC_DATA_ATTR char     rtcSig[128]     = "";   // signature of what's on screen
RTC_DATA_ATTR char     rtcLastSeen[96] = "";   // airline/callsign of last plane
RTC_DATA_ATTR char     rtcLastFrom[8]  = "";   // origin IATA/ICAO code
RTC_DATA_ATTR char     rtcLastTo[8]    = "";   // destination IATA/ICAO code
RTC_DATA_ATTR char     rtcLastType[64] = "";   // aircraft type/description
RTC_DATA_ATTR char     rtcLastReg[16]  = "";   // tail number / registration
RTC_DATA_ATTR time_t   rtcLastEpoch    = 0;    // when it was last overhead
RTC_DATA_ATTR uint16_t rtcRedraws      = 0;    // for periodic ghost-clean

EPaper            epaper;
Settings          cfg;
SensirionI2cSht4x sht4x;

struct Climate {
  bool  ok = false;
  float tempC = 0, hum = 0;
};

struct Plane {
  bool   found = false;
  String callsign, hex, typeDesc, reg, airline;
  String fromCity, fromCode, toCity, toCode;
  double altFt = 0;
  double slantKm = 0;                 // true 3D straight-line distance
  double gsKt = 0, vrateFpm = 0;      // ground speed (knots), vertical rate (ft/min)
};

#if DEBUG_LOG
  #define LOG(...) Serial1.printf(__VA_ARGS__)
#else
  #define LOG(...) ((void)0)
#endif

// ============================ MATH HELPERS ==========================
static double toRad(double d) { return d * M_PI / 180.0; }

static double haversineKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  double dLat = toRad(lat2 - lat1), dLon = toRad(lon2 - lon1);
  double h = sin(dLat / 2) * sin(dLat / 2) +
             cos(toRad(lat1)) * cos(toRad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  return R * 2 * atan2(sqrt(h), sqrt(1 - h));
}

static double altFeet(JsonVariant v) {
  if (v.isNull() || v.is<const char*>()) return -1;
  return v.as<double>();
}

// ============================ TIME HELPERS ==========================
static bool haveClock() {
  struct tm t;
  return getLocalTime(&t, 800);
}

static String hhmm() {
  struct tm t;
  if (!getLocalTime(&t, 800)) return "--:--";
  char b[6];
  strftime(b, sizeof(b), "%H:%M", &t);
  return String(b);
}

static String epochHHMM(time_t when) {
  if (when <= 0) return "";
  struct tm lt;
  localtime_r(&when, &lt);
  char b[6];
  strftime(b, sizeof(b), "%H:%M", &lt);
  return String(b);
}

static int curHour() {
  struct tm t;
  if (!getLocalTime(&t, 800)) return -1;
  return t.tm_hour;
}

static uint32_t secsUntilHour(int h) {
  struct tm t;
  if (!getLocalTime(&t, 800)) return timing::RETRY_SECONDS;
  int now  = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  int diff = h * 3600 - now;
  if (diff <= 0) diff += 86400;
  return diff;
}

static bool isNightNow() {
  if (!cfg.night) return false;
  int h = curHour();
  if (h < 0) return false;                       // no clock -> never "night"
  return cfg.night == 1 ? (h >= 22 || h < 7) : (h >= 23 || h < 6);
}

// ============================ BATTERY ===============================
// Returns 0-100, or -1 if disabled. Maps 3.30 V -> 0 %, 4.20 V -> 100 %.
static int batteryPct() {
#if ENABLE_BATTERY
  pinMode(pin::BAT_EN, OUTPUT);
  digitalWrite(pin::BAT_EN, HIGH);
  delay(8);
  analogReadResolution(12);
  analogSetPinAttenuation(pin::BAT_ADC, ADC_11db);
  long mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(pin::BAT_ADC);
  digitalWrite(pin::BAT_EN, LOW);

  double volts = (mv / 8.0 / 1000.0) * 2.0;      // undo the voltage divider
  int pct = (int)lround((volts - 3.30) / (4.20 - 3.30) * 100.0);
  LOG("[batt] %.2f V -> %d%%\n", volts, pct);
  return constrain(pct, 0, 100);
#else
  return -1;
#endif
}

// Read the onboard SHT4x. After a deep-sleep wake the sensor needs a moment,
// so we soft-reset and retry once. Returns ok=false if it can't be read.
static Climate readClimate() {
  Climate c;
  float t = 0, h = 0;
  for (int attempt = 0; attempt < 2; attempt++) {
    sht4x.softReset();
    delay(12);
    if (sht4x.measureHighPrecision(t, h) == 0 && !isnan(t) && !isnan(h)) {
      c.ok = true; c.tempC = t; c.hum = h;
      LOG("[sht4x] %.1f C  %.0f%%\n", t, h);
      return c;
    }
  }
  LOG("[sht4x] read failed\n");
  return c;
}

// ============================ CONFIG ================================
// Reads /config.txt from the SD card. Format: one KEY=VALUE per line.
// '#' lines and blank lines are ignored.
static void loadConfig() {
  pinMode(pin::SD_EN, OUTPUT);
  digitalWrite(pin::SD_EN, HIGH);
  delay(50);
  SPIClass spiSD(HSPI);
  spiSD.begin(pin::SD_SCK, pin::SD_MISO, pin::SD_MOSI, pin::SD_CS);
  if (!SD.begin(pin::SD_CS, spiSD)) {
    LOG("[sd] init failed — no card or wiring issue\n");
    return;
  }

  File f = SD.open("/config.txt");
  if (!f) {
    LOG("[sd] /config.txt not found\n");
    SD.end();
    return;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim(); val.trim();
    if      (key == "SSID")   wifiSSID  = val;
    else if (key == "PASS")   wifiPass  = val;
    else if (key == "LAT")    myLat     = val.toDouble();
    else if (key == "LON")    myLon     = val.toDouble();
    else if (key == "ALT")    myAltM    = val.toDouble();
    else if (key == "TZ")     tzInfo    = val;
    else if (key == "SPEED") {
      if      (val == "mph") cfg.speed = SPD_MPH;
      else if (val == "kts") cfg.speed = SPD_KTS;
      else                   cfg.speed = SPD_KPH;
    }
    else if (key == "DIST")   cfg.dist   = (val == "mi")     ? DIST_MI    : DIST_KM;
    else if (key == "HEIGHT") cfg.height = (val == "metric") ? HGT_METRIC : HGT_FTFL;
    else if (key == "TEMP")   cfg.temp   = (val == "f")      ? TEMP_F     : TEMP_C;
    else if (key == "RADIUS") cfg.radius = (uint16_t)constrain(val.toInt(), 1, 500);
    else if (key == "NIGHT")  cfg.night  = constrain(val.toInt(), 0, 2);
    else if (key == "BUSY")   cfg.busy   = (uint16_t)constrain(val.toInt(), 15, 600);
  }
  f.close();
  SD.end();
  digitalWrite(pin::SD_EN, LOW);   // power off the slot during WiFi+fetch phase
  LOG("[sd] config: SSID=%s LAT=%.5f LON=%.5f ALT=%.1fm TZ=%s\n",
      wifiSSID.c_str(), myLat, myLon, myAltM, tzInfo.c_str());
}

// ============================ NETWORK ===============================
static bool connectWiFi() {
  if (!wifiSSID.length()) { LOG("[wifi] no SSID configured\n"); return false; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timing::WIFI_TIMEOUT) {
    delay(250);
  }
  bool ok = WiFi.status() == WL_CONNECTED;
  LOG("[wifi] %s\n", ok ? "connected" : "FAILED");
  return ok;
}

// One place for the HTTPS-GET-then-parse-JSON dance. Pass a filter to keep
// only the fields you need (smaller, faster parse). Returns true on 200 + parse.
static bool httpGetJson(const String& url, JsonDocument& doc, const JsonDocument* filter = nullptr) {
  WiFiClientSecure client;
  client.setInsecure();                          // no cert pinning (hobby tradeoff)
  HTTPClient http;
  if (!http.begin(client, url)) {
    LOG("[http] begin failed\n");
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.setUserAgent(USER_AGENT);
  http.setConnectTimeout(timing::HTTP_TIMEOUT);
  http.setTimeout(timing::HTTP_TIMEOUT);

  int code = http.GET();
  if (code != 200) {
    LOG("[http] GET %d  %s\n", code, url.c_str());
    http.end();
    return false;
  }
  DeserializationError err = filter
      ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
      : deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    LOG("[http] JSON error: %s\n", err.c_str());
    return false;
  }
  return true;
}

// adsb.lol: pick the nearest aircraft in 3D (minimum slant distance).
static bool fetchOverhead(Plane& best) {
  int radiusNm = constrain((int)ceil(cfg.radius / 1.852), 1, 250);
  char url[160];
  snprintf(url, sizeof(url), "https://%s/v2/point/%.5f/%.5f/%d",
           API_HOST, myLat, myLon, radiusNm);

  JsonDocument filter;
  JsonObject f = filter["ac"].add<JsonObject>();
  for (const char* k : {"hex", "flight", "lat", "lon", "alt_baro", "alt_geom", "t", "desc", "r", "gs", "baro_rate"})
    f[k] = true;

  JsonDocument doc;
  if (!httpGetJson(url, doc, &filter)) return false;

  double bestSlant = 1e9;
  for (JsonObject a : doc["ac"].as<JsonArray>()) {
    if (a["lat"].isNull() || a["lon"].isNull()) continue;
    double altFt = altFeet(a["alt_geom"]);
    if (altFt < 0) altFt = altFeet(a["alt_baro"]);
    if (altFt < 0) continue;                     // on the ground / no altitude

    double lat = a["lat"], lon = a["lon"];
    double groundKm = haversineKm(myLat, myLon, lat, lon);
    double heightKm = (altFt * 0.3048 - myAltM) / 1000.0;          // height above the observer
    double slantKm  = sqrt(groundKm * groundKm + heightKm * heightKm); // closeness in all 3 axes
    if (slantKm >= bestSlant) continue;          // keep the nearest aircraft in 3D

    bestSlant      = slantKm;
    best.found     = true;
    best.hex       = a["hex"].as<String>();
    best.callsign  = a["flight"].as<String>();
    best.callsign.trim();
    best.altFt     = altFt;
    best.typeDesc  = a["desc"].isNull() ? a["t"].as<String>() : a["desc"].as<String>();
    best.reg       = a["r"].isNull() ? "" : a["r"].as<String>();
    best.slantKm   = slantKm;
    best.gsKt      = a["gs"].isNull() ? 0 : a["gs"].as<double>();
    best.vrateFpm  = a["baro_rate"].isNull() ? 0 : a["baro_rate"].as<double>();
  }
  LOG("[adsb] %s @ %.1f km (3D)\n",
      best.found ? best.callsign.c_str() : "nothing", best.found ? best.slantKm : 0.0);
  return best.found;
}

// adsbdb: airline + origin/destination airport, from the callsign.
static void fetchRoute(Plane& p) {
  if (!p.callsign.length()) return;

  JsonDocument doc;
  if (!httpGetJson("https://api.adsbdb.com/v0/callsign/" + p.callsign, doc)) return;

  JsonObject fr = doc["response"]["flightroute"];
  if (fr.isNull()) return;                        // unknown route (GA / military)

  p.airline = fr["airline"]["name"].as<String>();
  auto city = [](JsonObject ap) -> String {
    if (ap.isNull()) return "";
    return ap["municipality"].isNull() ? ap["name"].as<String>()
                                       : ap["municipality"].as<String>();
  };
  auto code = [](JsonObject ap) -> String {
    if (ap.isNull()) return "";
    return ap["iata_code"].isNull() ? ap["icao_code"].as<String>()
                                    : ap["iata_code"].as<String>();
  };
  p.fromCity = city(fr["origin"]);      p.fromCode = code(fr["origin"]);
  p.toCity   = city(fr["destination"]); p.toCode   = code(fr["destination"]);
}

// ============================ DRAW HELPERS ==========================
// Truncate to fit a pixel width using the currently-selected font.
static String fit(const String& s, int maxW) {
  if (epaper.textWidth(s) <= maxW) return s;
  String t = s;
  while (t.length() && epaper.textWidth(t + "…") > maxW) t.remove(t.length() - 1);
  return t + "…";
}

static void batteryGlyph(int x, int y, int pct) {
  if (pct < 0) return;
  epaper.drawRect(x, y, ui::BATT_W, ui::BATT_H, TFT_BLACK);
  epaper.fillRect(x + ui::BATT_W, y + 5, 3, 8, TFT_BLACK);   // nub
  int fillW = (int)lround((ui::BATT_W - 4) * pct / 100.0);
  epaper.fillRect(x + 2, y + 2, fillW, ui::BATT_H - 4, TFT_BLACK);
}

// A small top-view plane, pointing toward headingDeg (0 = north = up).
static void drawPlaneIcon(int cx, int cy, double scale, double headingDeg) {
  double th = toRad(headingDeg), s = sin(th), c = cos(th);
  auto X = [&](double lx, double ly) { return cx + (int)lround(scale * (lx * c - ly * s)); };
  auto Y = [&](double lx, double ly) { return cy + (int)lround(scale * (lx * s + ly * c)); };
  epaper.fillTriangle(X(0,-1.0), Y(0,-1.0), X(-0.13,0.85), Y(-0.13,0.85),
                      X(0.13,0.85), Y(0.13,0.85), TFT_BLACK);                 // fuselage
  epaper.fillTriangle(X(-0.95,0.25), Y(-0.95,0.25), X(0.95,0.25), Y(0.95,0.25),
                      X(0,0.5), Y(0,0.5), TFT_BLACK);                         // main wings
  epaper.fillTriangle(X(-0.4,0.95), Y(-0.4,0.95), X(0.4,0.95), Y(0.4,0.95),
                      X(0,0.6), Y(0,0.6), TFT_BLACK);                         // tailplane
}

// A friendly sun: filled disc with tapered triangular rays and a small gap.
static void drawSun(int cx, int cy, int r) {
  epaper.fillCircle(cx, cy, r, TFT_BLACK);
  const double gap = 5, rayLen = 13, halfW = 3.2;
  for (int a = 0; a < 360; a += 45) {
    double th = toRad(a), dx = cos(th), dy = sin(th), px = -sin(th), py = cos(th);
    double rb = r + gap, rt = r + gap + rayLen;
    int b1x = cx + (int)lround(rb * dx + halfW * px), b1y = cy + (int)lround(rb * dy + halfW * py);
    int b2x = cx + (int)lround(rb * dx - halfW * px), b2y = cy + (int)lround(rb * dy - halfW * py);
    int tx  = cx + (int)lround(rt * dx),              ty  = cy + (int)lround(rt * dy);
    epaper.fillTriangle(b1x, b1y, b2x, b2y, tx, ty, TFT_BLACK);
  }
}

// Thermometer: outlined stem (rounded top) + bulb, mercury filling bulb and the
// lower stem, with a few tick marks. Centred at (cx, cy), total height h.
static void drawThermometer(int cx, int cy, int h) {
  const int sw = 5;            // stem half-width
  const int bulbR = 12;
  int topY  = cy - h / 2;
  int bulbY = cy + h / 2;

  epaper.drawRoundRect(cx - sw, topY, 2 * sw, bulbY - topY, sw, TFT_BLACK);  // stem
  epaper.fillCircle(cx, bulbY, bulbR - 1, TFT_WHITE);     // clear stem bottom out of the bulb
  epaper.drawCircle(cx, bulbY, bulbR, TFT_BLACK);         // bulb outline

  epaper.fillCircle(cx, bulbY, bulbR - 3, TFT_BLACK);     // mercury: bulb
  int mercTop = topY + (int)((bulbY - topY) * 0.42);
  epaper.fillRect(cx - (sw - 1), mercTop, 2 * (sw - 1), bulbY - mercTop, TFT_BLACK); // column

  for (int i = 1; i <= 3; i++) {                          // tick marks
    int ty = topY + i * (bulbY - topY) / 5;
    epaper.drawFastHLine(cx + sw + 2, ty, 4, TFT_BLACK);
  }
}

// Water droplet: round bottom + tangent point on top, with a small highlight.
static void drawDroplet(int cx, int cy, int s) {
  int ccy = cy + (int)lround(s * 0.35);                   // circle centre
  epaper.fillCircle(cx, ccy, s, TFT_BLACK);
  int bx = (int)lround(s * 0.71);
  int by = ccy - bx;
  epaper.fillTriangle(cx - bx, by, cx + bx, by, cx, ccy - 2 * s, TFT_BLACK);
  epaper.fillCircle(cx - s / 3, ccy - s / 4, max(2, s / 6), TFT_WHITE);   // sheen
}

// A solid right-pointing arrow: a thick shaft with a filled triangular head.
static void drawArrow(int x, int y, int len) {
  const int head = 16;     // head length
  const int hh   = 9;      // head half-height
  const int th   = 4;      // shaft thickness
  epaper.fillRect(x, y - th / 2, len - head + 1, th, TFT_BLACK);
  epaper.fillTriangle(x + len - head, y - hh, x + len - head, y + hh, x + len, y, TFT_BLACK);
}

static String altStr(double ft) {
  char b[24];
  if (cfg.height == HGT_METRIC)
    snprintf(b, sizeof(b), "%.0f m", ft * 0.3048);
  else if (ft >= 10000)
    snprintf(b, sizeof(b), "FL%.0f", ft / 100.0);
  else
    snprintf(b, sizeof(b), "%.0f ft", ft);
  return String(b);
}

static String distStr(double km) {
  char b[24];
  if (cfg.dist == DIST_MI) snprintf(b, sizeof(b), "%.1f mi away", km / 1.609);
  else                     snprintf(b, sizeof(b), "%.1f km away", km);
  return String(b);
}

static String speedStr(double kt) {
  char b[24];
  if      (cfg.speed == SPD_MPH) snprintf(b, sizeof(b), "%.0f mph",  kt * 1.151);
  else if (cfg.speed == SPD_KTS) snprintf(b, sizeof(b), "%.0f kts",  kt);
  else                           snprintf(b, sizeof(b), "%.0f km/h", kt * 1.852);
  return String(b);
}

static const char* trendWord(double fpm) {
  if (fpm >  150) return "climbing";
  if (fpm < -150) return "descending";
  return "level";
}

// The right-hand indoor-climate panel: a divider, a header, then a thermometer
// with the temperature and a droplet with the humidity. Big and icon-led.
static void drawClimatePanel(const Climate& c) {
  epaper.drawFastVLine(ui::DIVIDER_X, ui::RULE_Y + 16, 360, TFT_BLACK);

  epaper.setTextDatum(MC_DATUM);
  epaper.setFreeFont(&FreeSansBold12pt7b);
  epaper.drawString("ROOM", ui::PANEL_CX, ui::HEADER_Y);

  if (!c.ok) {                                  // sensor not ready
    epaper.setFreeFont(&FreeSans12pt7b);
    epaper.drawString("sensor warming up", ui::PANEL_CX, ui::TEMP_Y);
    epaper.setTextDatum(TL_DATUM);
    return;
  }

  // ---- temperature ----
  drawThermometer(ui::ICON_X, ui::TEMP_Y, 52);
  epaper.setTextDatum(ML_DATUM);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  char tnum[8];
  float t = (cfg.temp == TEMP_F) ? c.tempC * 9 / 5 + 32 : c.tempC;
  snprintf(tnum, sizeof(tnum), (cfg.temp == TEMP_F) ? "%.0f" : "%.1f", t);
  epaper.drawString(tnum, ui::NUM_X, ui::TEMP_Y);
  int w = epaper.textWidth(tnum);
  int dx = ui::NUM_X + w + 10, dy = ui::TEMP_Y - 14;
  epaper.drawCircle(dx, dy, 5, TFT_BLACK);                 // ° symbol (2 px ring)
  epaper.drawCircle(dx, dy, 4, TFT_BLACK);
  char unit[2] = { (cfg.temp == TEMP_F) ? 'F' : 'C', 0 };
  epaper.drawString(unit, dx + 12, ui::TEMP_Y);

  // ---- humidity ----
  drawDroplet(ui::ICON_X, ui::HUM_Y, 16);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  char hnum[8];
  snprintf(hnum, sizeof(hnum), "%.0f%%", c.hum);
  epaper.drawString(hnum, ui::NUM_X, ui::HUM_Y);

  epaper.setTextDatum(TL_DATUM);
}

// ============================ LIVE SCREEN ===========================
static void drawHeader(const Plane& p, int batt) {
  drawPlaneIcon(ui::MARGIN + 7, ui::HDR_TEXT_Y + 6, 9.0, 45);
  epaper.setFreeFont(&FreeSansBold9pt7b);
  epaper.drawString(p.found ? "OVERHEAD NOW" : "SKY OVERHEAD", ui::MARGIN + 26, ui::HDR_TEXT_Y);
  batteryGlyph(ui::BATT_X, ui::BATT_Y, batt);
  if (batt >= 0 && batt < 15) {
    epaper.setFreeFont(&FreeSansBold9pt7b);
    epaper.drawString("LOW", ui::BATT_X - 45, ui::HDR_TEXT_Y);
  }
  epaper.drawFastHLine(ui::MARGIN, ui::RULE_Y, ui::CONTENT_W, TFT_BLACK);
}

static void drawPlane(const Plane& p) {
  // ---- left column: a flowing layout, so missing fields close up (no gaps) ----
  int y = 66;
  bool airlineKnown = p.airline.length();

  epaper.setTextDatum(TL_DATUM);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  String hero = airlineKnown ? p.airline
              : (p.callsign.length() ? p.callsign : p.hex);
  epaper.drawString(fit(hero, ui::LEFT_W), ui::LEFT_X, y);
  y += 50;

  if (airlineKnown && p.callsign.length()) {         // callsign as a subtitle
    epaper.setFreeFont(&FreeSans12pt7b);
    epaper.drawString(p.callsign, ui::LEFT_X, y);
    y += 30;
  }
  y += 16;

  // route — show codes + cities if known, otherwise a single clean line
  bool routeKnown = p.fromCode.length() || p.toCode.length();
  if (routeKnown) {
    String fc = p.fromCode.length() ? p.fromCode : "???";
    String tc = p.toCode.length()   ? p.toCode   : "???";
    epaper.setFreeFont(&FreeSansBold24pt7b);
    epaper.drawString(fc, ui::LEFT_X, y);
    int ax = ui::LEFT_X + epaper.textWidth(fc) + 20;
    int ay = y + 18;                              // centre on the 24pt codes
    drawArrow(ax, ay, 62);
    int dX = ax + 62 + 22;
    epaper.drawString(tc, dX, y);
    y += 50;
    if (p.fromCity.length() || p.toCity.length()) {
      epaper.setFreeFont(&FreeSans12pt7b);
      epaper.drawString(fit(p.fromCity, dX - ui::LEFT_X - 12), ui::LEFT_X, y);
      epaper.drawString(fit(p.toCity, ui::LEFT_X + ui::LEFT_W - dX), dX, y);
      y += 30;
    }
  } else {
    epaper.setFreeFont(&FreeSans18pt7b);
    epaper.drawString("Route unknown", ui::LEFT_X, y);
    y += 36;
  }
  y += 18;

  // type, distance, altitude, and speed — all in the flight column
  epaper.setFreeFont(&FreeSans18pt7b);
  epaper.drawString(fit(p.typeDesc, ui::LEFT_W), ui::LEFT_X, y); y += 40;
  epaper.drawString(fit(distStr(p.slantKm) + " · " + altStr(p.altFt), ui::LEFT_W), ui::LEFT_X, y); y += 40;
  epaper.drawString(speedStr(p.gsKt) + " · " + trendWord(p.vrateFpm), ui::LEFT_X, y);
}

static void drawEmpty() {
  int cx = ui::DIVIDER_X / 2;                     // centre within the left area
  drawSun(cx, 96, 26);
  epaper.setTextDatum(MC_DATUM);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString("Clear skies", cx, 184);
  epaper.setFreeFont(&FreeSans18pt7b);
  if (strlen(rtcLastSeen)) {
    epaper.drawString("Last seen overhead", cx, 250);

    String meta;
    if (strlen(rtcLastType)) meta = String(rtcLastType);
    if (strlen(rtcLastReg)) {
      if (meta.length()) meta += " · ";
      meta += String(rtcLastReg);
    }

    bool hasRoute = strlen(rtcLastFrom) || strlen(rtcLastTo);
    if (hasRoute) {
      epaper.drawString(fit(String(rtcLastSeen), ui::DIVIDER_X - 48), cx, 288);
      if (meta.length()) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString(fit(meta, ui::DIVIDER_X - 48), cx, 322);
      }

      // Route row: centred arrow between the two codes
      epaper.setFreeFont(&FreeSansBold24pt7b);
      epaper.setTextDatum(TL_DATUM);
      String fc = strlen(rtcLastFrom) ? String(rtcLastFrom) : "???";
      String tc = strlen(rtcLastTo)   ? String(rtcLastTo)   : "???";
      const int arrowLen = 48, gap = 14;
      int fcW = epaper.textWidth(fc);
      int totalW = fcW + gap + arrowLen + gap + epaper.textWidth(tc);
      int rx = cx - totalW / 2;
      int ry = meta.length() ? 350 : 324;
      epaper.drawString(fc, rx, ry);
      drawArrow(rx + fcW + gap, ry + 18, arrowLen);  // +18 = vertical centre of 24pt
      epaper.drawString(tc, rx + fcW + gap + arrowLen + gap, ry);

      epaper.setFreeFont(&FreeSans12pt7b);
      epaper.setTextDatum(MC_DATUM);
      String at = epochHHMM(rtcLastEpoch);
      if (at.length())
        epaper.drawString("at " + at, cx, meta.length() ? 410 : 382);
    } else {
      epaper.drawString(fit(String(rtcLastSeen), ui::DIVIDER_X - 48), cx, 294);
      if (meta.length()) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString(fit(meta, ui::DIVIDER_X - 48), cx, 332);
      }
      String at = epochHHMM(rtcLastEpoch);
      if (at.length()) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString("at " + at, cx, meta.length() ? 372 : 342);
      }
    }
  } else {
    epaper.drawString("Nothing overhead", cx, 250);
  }
  epaper.setTextDatum(TL_DATUM);
}

static void drawLive(const Plane& p, int batt, const Climate& clim) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextDatum(TL_DATUM);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  drawHeader(p, batt);
  if (p.found) drawPlane(p); else drawEmpty();

  drawClimatePanel(clim);

  epaper.setFreeFont(&FreeSans9pt7b);
  String foot;
  if (p.found) {
    if (p.reg.length()) foot = p.reg + "  ·  ";
    foot += "adsb.lol + adsbdb";
  } else {
    foot = "adsb.lol + adsbdb";
  }
  epaper.drawString(foot, ui::MARGIN, ui::FOOTER_Y);
  epaper.setTextDatum(TR_DATUM);
  epaper.drawString("updated " + hhmm(), ui::SCREEN_W - ui::MARGIN, ui::FOOTER_Y);
  epaper.setTextDatum(TL_DATUM);
}

// ============================ SLEEP =================================
// Never returns: it ends the wake cycle by entering deep sleep.
// (No [[noreturn]] attribute — the Arduino IDE auto-generates a prototype
//  without it, which would cause an attribute-mismatch error.)
static void goSleep(uint32_t seconds) {
  LOG("[sleep] %u s\n", seconds);
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);  // false = don't erase credentials from NVS
    WiFi.mode(WIFI_OFF);
  }
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  esp_deep_sleep_start();
  while (true) {}                                 // unreachable; satisfies [[noreturn]]
}

// ============================ MAIN ==================================
void setup() {
  Serial1.begin(115200, SERIAL_8N1, 44, 43);  // hardware UART, works without USB
  loadConfig();
  epaper.begin();
  Wire.begin(pin::I2C_SDA, pin::I2C_SCL);
  sht4x.begin(Wire, 0x44);

  LOG("\n[boot] redraws=%u\n", rtcRedraws);

  bool netOk = connectWiFi();
  if (netOk) configTzTime(tzInfo.c_str(), "pool.ntp.org", "time.nist.gov");

  // Quiet hours: skip the fetch and the refresh entirely; sleep until morning.
  if (isNightNow()) {
    goSleep(secsUntilHour(cfg.night == 1 ? 7 : 6));
  }

  // Network glitch: leave the last good screen up and retry soon.
  if (!netOk) {
    goSleep(timing::RETRY_SECONDS);
  }

  Plane p;
  bool got = fetchOverhead(p);
  if (got) fetchRoute(p);

  if (got) {                                       // remember for the empty-sky screen
    String s = p.airline.length() ? p.airline : p.callsign;
    s.toCharArray(rtcLastSeen, sizeof(rtcLastSeen));
    p.fromCode.toCharArray(rtcLastFrom, sizeof(rtcLastFrom));
    p.toCode.toCharArray(rtcLastTo, sizeof(rtcLastTo));
    p.typeDesc.toCharArray(rtcLastType, sizeof(rtcLastType));
    p.reg.toCharArray(rtcLastReg, sizeof(rtcLastReg));
    if (haveClock()) rtcLastEpoch = time(nullptr);
  }

  int batt = batteryPct();
  Climate clim = readClimate();

  // No-flash: build a signature of the visible content and only repaint on change
  // (or when a settings edit forced it). Climate is excluded from the signature
  // intentionally — temp/humidity updates piggyback on the next plane-triggered repaint.
  char sig[224];
  int lowBucket = (batt >= 0 && batt < 15) ? 1 : 0;
  if (got)
    snprintf(sig, sizeof(sig), "F|%s|%ld|%ld|%d",
             p.hex.c_str(), lround(p.altFt / 500.0), lround(p.slantKm / 5.0), lowBucket);
  else
    snprintf(sig, sizeof(sig), "E|%d|%s|%s|%s|%s|%s", lowBucket, rtcLastSeen, rtcLastFrom, rtcLastTo, rtcLastType, rtcLastReg);

  if (strcmp(sig, rtcSig) != 0) {
    if (rtcRedraws > 0 && rtcRedraws % timing::GHOST_CLEAN_EVERY == 0) {
      epaper.fillScreen(TFT_WHITE);
      epaper.update();                             // clear accumulated ghosting
    }
    drawLive(p, batt, clim);
    epaper.update();
    rtcRedraws++;
    strncpy(rtcSig, sig, sizeof(rtcSig));
    LOG("[draw] repainted (%s)\n", sig);
  } else {
    LOG("[draw] unchanged, skipped\n");
  }

  goSleep(cfg.busy);
}

void loop() {}   // never runs: every cycle ends in deep sleep inside setup()
