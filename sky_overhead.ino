/*
 * Sky Overhead — reTerminal E1001 (ESP32-S3)  ·  wall-appliance build
 * ============================================================================
 * Shows the nearest aircraft (airline, route, type, altitude, speed)
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

#include "IconFont.h"

// Runtime config — loaded from /config.txt on the SD card at boot.
// If the card is missing or the file can't be parsed the device boots but
// won't connect to Wi-Fi; it will retry on the next timer wake.
String wifiSSID, wifiPass, tzInfo;
double myLat = 0.0, myLon = 0.0, myAltM = 0.0;

// Unit preferences — also loaded from config.txt.
// Defaults: aviation height (ft/FL), km/h, Celsius.
enum SpeedUnit  { SPD_KPH = 0, SPD_MPH = 1, SPD_KTS = 2 };
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

  constexpr int HDR_TEXT_Y = 14;
  constexpr int BATT_X = 740, BATT_Y = 12, BATT_W = 40, BATT_H = 18;
  constexpr int CONTENT_TOP_Y = 40;
  constexpr int DIVIDER_X = 466;
  constexpr int LEFT_TEXT_W = DIVIDER_X - 48;

  // shared left-column vertical anchors
  constexpr int LEFT_CX = DIVIDER_X / 2;
  constexpr int LEFT_ICON_Y = 128;
  constexpr int LEFT_TITLE_Y = 226;
  constexpr int LEFT_SUBTITLE_Y = 282;
  constexpr int LEFT_ROUTE_Y = 344;
  constexpr int LEFT_DETAIL_GAP = 30;
  constexpr int LEFT_DETAIL_1_Y = 380;
  constexpr int LEFT_DETAIL_2_Y = LEFT_DETAIL_1_Y + LEFT_DETAIL_GAP;
  constexpr int LEFT_DETAIL_3_Y = LEFT_DETAIL_2_Y + LEFT_DETAIL_GAP;

  // shared route-row geometry
  constexpr int ROUTE_ARROW_LEN = 48;
  constexpr int ROUTE_GAP = 14;
  constexpr int ROUTE_TEXT_HALF_H = 18;

  // full-screen sleep view: intentional exception to the two-column content grid
  constexpr int SLEEP_CX = SCREEN_W / 2;
  constexpr int SLEEP_TEXT_W = SCREEN_W - 2 * MARGIN;
  constexpr int SLEEP_WAKE_Y = 326;

  // shared frame geometry
  constexpr int HDR_ICON_X = MARGIN - 2;
  constexpr int HDR_ICON_Y = HDR_TEXT_Y - 8;
  constexpr int HDR_TEXT_X = MARGIN + 26;

  // right panel = indoor climate (thermometer + droplet)
  constexpr int PANEL_CX  = 632;     // panel centre
  constexpr int ICON_X    = 548;     // icon centre
  constexpr int NUM_X     = 588;     // big number left edge
  constexpr int TEMP_Y    = 205;     // vertical centre of the temperature row
  constexpr int HUM_Y     = 325;     // vertical centre of the humidity row

  constexpr int FOOTER_Y = 452;

}

// All settings — loaded from config.txt on the SD card.
struct Settings {
  SpeedUnit  speed   = SPD_KPH;
  HeightUnit height  = HGT_FTFL;
  TempUnit   temp    = TEMP_C;
  uint16_t   radius  = 30;  // km, any value
  bool       night   = false;
  uint16_t   nightStart = 0;  // minutes after midnight
  uint16_t   nightEnd   = 0;
  uint16_t   busy    = 60;  // seconds, any value
  bool       demo    = false;
};

// State that survives deep sleep but not a power cycle.
RTC_DATA_ATTR char     rtcSig[320]     = "";   // signature of what's on screen
RTC_DATA_ATTR char     rtcLastSeen[96] = "";   // airline/callsign of last plane
RTC_DATA_ATTR char     rtcLastFrom[8]  = "";   // origin IATA/ICAO code
RTC_DATA_ATTR char     rtcLastTo[8]    = "";   // destination IATA/ICAO code
RTC_DATA_ATTR char     rtcLastCities[96] = ""; // origin/destination city text
RTC_DATA_ATTR char     rtcLastType[64] = "";   // aircraft type/description
RTC_DATA_ATTR char     rtcLastReg[16]  = "";   // tail number / registration
RTC_DATA_ATTR char     rtcLastMotion[64] = ""; // altitude/trend/speed text
RTC_DATA_ATTR time_t   rtcLastEpoch    = 0;    // when it was last overhead
RTC_DATA_ATTR uint16_t rtcRedraws      = 0;    // for periodic ghost-clean
RTC_DATA_ATTR uint8_t  rtcDemoStep     = 0;    // rotates demo screens

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

static void goSleep(uint32_t seconds);

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

static String jsonText(JsonVariant v) {
  if (v.isNull()) return "";
  String s = v.as<String>();
  s.trim();
  return (s == "null") ? "" : s;
}

// ============================ TIME HELPERS ==========================
static bool haveClock() {
  struct tm t;
  return getLocalTime(&t, 800);
}

static bool syncClock() {
  configTzTime(tzInfo.c_str(), "pool.ntp.org", "time.nist.gov");
  struct tm t;
  bool ok = getLocalTime(&t, 5000);
  LOG("[time] sync %s\n", ok ? "ok" : "timeout");
  return ok;
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

static int curMinuteOfDay() {
  struct tm t;
  if (!getLocalTime(&t, 800)) return -1;
  return t.tm_hour * 60 + t.tm_min;
}

static uint32_t secsUntilMinuteOfDay(uint16_t minuteOfDay) {
  struct tm t;
  if (!getLocalTime(&t, 800)) return timing::RETRY_SECONDS;
  int now  = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  int diff = (int)minuteOfDay * 60 - now;
  if (diff <= 0) diff += 86400;
  return diff;
}

static bool isNightNow() {
  if (!cfg.night) return false;
  int now = curMinuteOfDay();
  if (now < 0) return false;                       // no clock -> never "night"
  if (cfg.nightStart == cfg.nightEnd) return true;
  if (cfg.nightStart < cfg.nightEnd)
    return now >= cfg.nightStart && now < cfg.nightEnd;
  return now >= cfg.nightStart || now < cfg.nightEnd;
}

static bool parseHHMM(const String& value, uint16_t& minuteOfDay) {
  int colon = value.indexOf(':');
  if (colon < 0) return false;
  if (value.indexOf(':', colon + 1) >= 0) return false;
  if (colon == 0 || colon > 2) return false;
  int minuteDigits = value.length() - colon - 1;
  if (minuteDigits != 2) return false;
  for (int i = 0; i < value.length(); i++) {
    if (i == colon) continue;
    if (!isDigit(value[i])) return false;
  }
  int h = value.substring(0, colon).toInt();
  int m = value.substring(colon + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  minuteOfDay = (uint16_t)(h * 60 + m);
  return true;
}

static bool parseNightMode(const String& value) {
  String range = value;
  range.trim();
  if (!range.length()) return false;

  int dash = range.indexOf('-');
  if (dash < 0 || range.indexOf('-', dash + 1) >= 0) return false;

  String startText = range.substring(0, dash);
  String endText = range.substring(dash + 1);
  startText.trim();
  endText.trim();

  uint16_t start = 0, end = 0;
  if (!parseHHMM(startText, start) || !parseHHMM(endText, end)) return false;

  cfg.nightStart = start;
  cfg.nightEnd = end;
  return true;
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
    else if (key == "HEIGHT") cfg.height = (val == "metric") ? HGT_METRIC : HGT_FTFL;
    else if (key == "TEMP")   cfg.temp   = (val == "f")      ? TEMP_F     : TEMP_C;
    else if (key == "RADIUS") cfg.radius = (uint16_t)constrain(val.toInt(), 1, 500);
    else if (key == "NIGHT_MODE") cfg.night = parseNightMode(val);
    else if (key == "BUSY")   cfg.busy   = (uint16_t)constrain(val.toInt(), 15, 600);
    else if (key == "DEMO")   cfg.demo   = (val == "1" || val == "true" || val == "on");
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
    best.hex       = jsonText(a["hex"]);
    best.callsign  = jsonText(a["flight"]);
    best.altFt     = altFt;
    best.typeDesc  = jsonText(a["desc"]);
    if (!best.typeDesc.length()) best.typeDesc = jsonText(a["t"]);
    best.reg       = jsonText(a["r"]);
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

  p.airline = jsonText(fr["airline"]["name"]);
  auto city = [](JsonObject ap) -> String {
    if (ap.isNull()) return "";
    String s = jsonText(ap["municipality"]);
    return s.length() ? s : jsonText(ap["name"]);
  };
  auto code = [](JsonObject ap) -> String {
    if (ap.isNull()) return "";
    String s = jsonText(ap["iata_code"]);
    return s.length() ? s : jsonText(ap["icao_code"]);
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

static void drawIcon(char glyph, int x, int y, uint8_t nativeSize) {
  epaper.setFreeFont(&SkyIcon24);
  uint8_t baseline = nativeSize > 127 ? 127 : nativeSize;
  epaper.drawChar(x, y + baseline, glyph, TFT_BLACK, TFT_WHITE, 1);
}

static void drawIconCentered(char glyph, int cx, int cy, uint8_t nativeSize) {
  drawIcon(glyph, cx - nativeSize / 2, cy - nativeSize / 2, nativeSize);
}

static void batteryGlyph(int x, int y, int pct) {
  if (pct < 0) return;
  char glyph = icon::BATTERY_EMPTY;
  if (pct >= 85) glyph = icon::BATTERY_FULL;
  else if (pct >= 45) glyph = icon::BATTERY_MEDIUM;
  else if (pct >= 15) glyph = icon::BATTERY_LOW;
  drawIcon(glyph, x, y - 6, icon::BATTERY_EMPTY_SIZE);
}

static void drawRouteArrow(int x, int y, int len) {
  drawIconCentered(icon::ARROW_RIGHT, x + len / 2, y, icon::ARROW_RIGHT_SIZE);
}

static void drawRouteCodes(const String& fromCode, const String& toCode, int cx, int cy) {
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.setTextDatum(TL_DATUM);
  int fromW = epaper.textWidth(fromCode);
  int totalW = fromW + ui::ROUTE_GAP + ui::ROUTE_ARROW_LEN + ui::ROUTE_GAP + epaper.textWidth(toCode);
  int x = cx - totalW / 2;
  int y = cy - ui::ROUTE_TEXT_HALF_H;
  epaper.drawString(fromCode, x, y);
  drawRouteArrow(x + fromW + ui::ROUTE_GAP, cy, ui::ROUTE_ARROW_LEN);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString(toCode, x + fromW + ui::ROUTE_GAP + ui::ROUTE_ARROW_LEN + ui::ROUTE_GAP, y);
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
  return "level flight";
}

static String aircraftMeta(const Plane& p) {
  String s = p.typeDesc.length() ? p.typeDesc : "Type unknown";
  if (p.reg.length()) {
    s += " · ";
    s += p.reg;
  }
  return s;
}

static String routeCities(const Plane& p) {
  String cities;
  if (p.fromCity.length()) cities = p.fromCity;
  if (p.toCity.length()) {
    if (cities.length()) cities += " to ";
    cities += p.toCity;
  }
  return cities;
}

static String motionText(const Plane& p) {
  return altStr(p.altFt) + " (" + trendWord(p.vrateFpm) + ") · " + speedStr(p.gsKt);
}

static Plane demoPlane() {
  Plane p;
  p.found = true;
  p.callsign = "DLH4JA";
  p.hex = "3C65C2";
  p.airline = "Lufthansa";
  p.fromCode = "MUC";
  p.toCode = "BUD";
  p.fromCity = "Munich";
  p.toCity = "Budapest";
  p.typeDesc = "Airbus A320neo";
  p.reg = "D-AINZ";
  p.altFt = 33000;
  p.slantKm = 8.2;
  p.gsKt = 421;
  p.vrateFpm = 850;
  return p;
}

static void rememberLastSeen(const Plane& p) {
  String s = p.airline.length() ? p.airline : p.callsign;
  s.toCharArray(rtcLastSeen, sizeof(rtcLastSeen));
  p.fromCode.toCharArray(rtcLastFrom, sizeof(rtcLastFrom));
  p.toCode.toCharArray(rtcLastTo, sizeof(rtcLastTo));
  routeCities(p).toCharArray(rtcLastCities, sizeof(rtcLastCities));
  p.typeDesc.toCharArray(rtcLastType, sizeof(rtcLastType));
  p.reg.toCharArray(rtcLastReg, sizeof(rtcLastReg));
  motionText(p).toCharArray(rtcLastMotion, sizeof(rtcLastMotion));
  if (haveClock()) rtcLastEpoch = time(nullptr);
}

// The right-hand indoor-climate panel: a thermometer with the temperature and
// a droplet with the humidity. Big and icon-led.
static void drawClimatePanel(const Climate& c) {
  epaper.setTextDatum(MC_DATUM);

  if (!c.ok) {                                  // sensor not ready
    drawIconCentered(icon::THERMOMETER, ui::ICON_X, ui::TEMP_Y, icon::THERMOMETER_SIZE);
    drawIconCentered(icon::DROPLET, ui::ICON_X, ui::HUM_Y, icon::DROPLET_SIZE);
    epaper.setTextDatum(ML_DATUM);
    epaper.setFreeFont(&FreeSansBold24pt7b);
    epaper.drawString("--", ui::NUM_X, ui::TEMP_Y);
    epaper.drawString("--", ui::NUM_X, ui::HUM_Y);
    epaper.setTextDatum(TL_DATUM);
    return;
  }

  // ---- temperature ----
  drawIconCentered(icon::THERMOMETER, ui::ICON_X, ui::TEMP_Y, icon::THERMOMETER_SIZE);
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
  drawIconCentered(icon::DROPLET, ui::ICON_X, ui::HUM_Y, icon::DROPLET_SIZE);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  char hnum[8];
  snprintf(hnum, sizeof(hnum), "%.0f%%", c.hum);
  epaper.drawString(hnum, ui::NUM_X, ui::HUM_Y);

  epaper.setTextDatum(TL_DATUM);
}

// ============================ LIVE SCREEN ===========================
static void drawFrameHeader(int batt) {
  drawIcon(icon::PLANE, ui::HDR_ICON_X, ui::HDR_ICON_Y, icon::PLANE_SIZE);
  epaper.setFreeFont(&FreeSansBold9pt7b);
  epaper.drawString("SKY OVERHEAD", ui::HDR_TEXT_X, ui::HDR_TEXT_Y);
  batteryGlyph(ui::BATT_X, ui::BATT_Y, batt);
}

static void drawFrameFooter() {
  epaper.setFreeFont(&FreeSans9pt7b);
  epaper.setTextDatum(TR_DATUM);
  epaper.drawString("updated " + hhmm(), ui::SCREEN_W - ui::MARGIN, ui::FOOTER_Y);
  epaper.setTextDatum(TL_DATUM);
}

static void drawStateHero(char glyph, uint8_t glyphSize, int cx, const String& title,
                          const String& subtitle, int textW = ui::LEFT_TEXT_W) {
  epaper.setTextDatum(MC_DATUM);
  drawIconCentered(glyph, cx, ui::LEFT_ICON_Y, glyphSize);
  epaper.setFreeFont(&FreeSansBold24pt7b);
  epaper.drawString(fit(title, textW), cx, ui::LEFT_TITLE_Y);
  if (subtitle.length()) {
    epaper.setFreeFont(&FreeSans18pt7b);
    epaper.drawString(fit(subtitle, textW), cx, ui::LEFT_SUBTITLE_Y);
  }
}

static int infoRowY(uint8_t row) {
  if (row == 0) return ui::LEFT_ROUTE_Y;
  return ui::LEFT_DETAIL_1_Y + (row - 1) * ui::LEFT_DETAIL_GAP;
}

static void drawInfoLine(int cx, uint8_t& row, const String& text) {
  if (!text.length() || row >= 4) return;
  epaper.setFreeFont(&FreeSans12pt7b);
  epaper.setTextDatum(MC_DATUM);
  epaper.drawString(fit(text, ui::LEFT_TEXT_W), cx, infoRowY(row));
  row++;
}

static void drawPlane(const Plane& p) {
  int cx = ui::LEFT_CX;

  String hero = p.callsign.length() ? p.callsign : p.hex;
  if (!hero.length()) hero = "Aircraft";
  drawStateHero(icon::PLANE_LARGE, icon::PLANE_LARGE_SIZE, cx, hero, p.airline);

  String meta = aircraftMeta(p);
  String motion = motionText(p);
  bool routeKnown = p.fromCode.length() || p.toCode.length();

  if (routeKnown) {
    String fc = p.fromCode.length() ? p.fromCode : "???";
    String tc = p.toCode.length()   ? p.toCode   : "???";
    drawRouteCodes(fc, tc, cx, ui::LEFT_ROUTE_Y);
  }

  epaper.setTextDatum(MC_DATUM);
  uint8_t row = routeKnown ? 1 : 0;
  drawInfoLine(cx, row, routeKnown ? routeCities(p) : "");
  drawInfoLine(cx, row, meta);
  drawInfoLine(cx, row, motion);
  epaper.setTextDatum(TL_DATUM);
}

static void drawEmpty() {
  int cx = ui::LEFT_CX;
  epaper.setTextDatum(MC_DATUM);

  if (!strlen(rtcLastSeen)) {
    drawStateHero(icon::SUN, icon::SUN_SIZE, cx, "Clear skies", "Nothing overhead");
    epaper.setTextDatum(TL_DATUM);
    return;
  }

  drawStateHero(icon::SUN, icon::SUN_SIZE, cx, "Clear skies", String(rtcLastSeen));
  {
    String meta;
    if (strlen(rtcLastType)) meta = String(rtcLastType);
    if (strlen(rtcLastReg)) {
      if (meta.length()) meta += " · ";
      meta += String(rtcLastReg);
    }

    bool hasRoute = strlen(rtcLastFrom) || strlen(rtcLastTo);
    if (hasRoute) {
      String fc = strlen(rtcLastFrom) ? String(rtcLastFrom) : "???";
      String tc = strlen(rtcLastTo)   ? String(rtcLastTo)   : "???";
      drawRouteCodes(fc, tc, cx, ui::LEFT_ROUTE_Y);
    }
    uint8_t row = hasRoute ? 1 : 0;
    drawInfoLine(cx, row, hasRoute ? String(rtcLastCities) : "");
    drawInfoLine(cx, row, meta);
    drawInfoLine(cx, row, String(rtcLastMotion));
  }
  epaper.setTextDatum(TL_DATUM);
}

static void drawNightSleep(uint16_t wakeMinute, int batt) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  epaper.setTextDatum(TL_DATUM);
  drawFrameHeader(batt);

  drawStateHero(icon::MOON_STAR, icon::MOON_STAR_SIZE, ui::SLEEP_CX, "Sleeping",
                "Aircraft checks paused", ui::SLEEP_TEXT_W);
  char wake[32];
  snprintf(wake, sizeof(wake), "until %02u:%02u", wakeMinute / 60, wakeMinute % 60);
  epaper.setFreeFont(&FreeSans12pt7b);
  epaper.drawString(wake, ui::SLEEP_CX, ui::SLEEP_WAKE_Y);

  drawFrameFooter();
}

static void drawLive(const Plane& p, int batt, const Climate& clim) {
  epaper.fillScreen(TFT_WHITE);
  epaper.setTextDatum(TL_DATUM);
  epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  drawFrameHeader(batt);
  if (p.found) drawPlane(p); else drawEmpty();

  drawClimatePanel(clim);

  drawFrameFooter();
}

static void runDemoMode() {
  int batt = batteryPct();
  Climate clim = readClimate();
  Plane p = demoPlane();
  rememberLastSeen(p);

  uint8_t step = rtcDemoStep % 3;
  if (step == 0) {
    drawLive(p, batt, clim);
  } else if (step == 1) {
    Plane empty;
    drawLive(empty, batt, clim);
  } else {
    drawNightSleep(cfg.nightEnd, batt);
  }

  epaper.update();
  rtcRedraws++;
  rtcDemoStep = (step + 1) % 3;
  snprintf(rtcSig, sizeof(rtcSig), "D|%u", step);
  LOG("[demo] drew step %u\n", step);
  uint32_t demoSleep = cfg.busy < 30 ? cfg.busy : 30;
  if (demoSleep < 15) demoSleep = 15;
  goSleep(demoSleep);
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

  if (cfg.demo) runDemoMode();

  bool netOk = connectWiFi();
  if (netOk) syncClock();

  // Quiet hours: show a sleep screen, skip the fetch, then sleep until morning.
  if (isNightNow()) {
    int batt = batteryPct();
    int lowBucket = (batt >= 0 && batt < 15) ? 1 : 0;
    char sig[32];
    snprintf(sig, sizeof(sig), "N|%u|%d", cfg.nightEnd, lowBucket);
    if (strcmp(sig, rtcSig) != 0) {
      drawNightSleep(cfg.nightEnd, batt);
      epaper.update();
      rtcRedraws++;
      strncpy(rtcSig, sig, sizeof(rtcSig));
      LOG("[draw] night screen (%s)\n", sig);
    } else {
      LOG("[draw] night unchanged, skipped\n");
    }
    goSleep(secsUntilMinuteOfDay(cfg.nightEnd));
  }

  // Network glitch: leave the last good screen up and retry soon.
  if (!netOk) {
    goSleep(timing::RETRY_SECONDS);
  }

  Plane p;
  bool got = fetchOverhead(p);
  if (got) fetchRoute(p);

  if (got) {                                       // remember for the empty-sky screen
    rememberLastSeen(p);
  }

  int batt = batteryPct();
  Climate clim = readClimate();

  // No-flash: repaint on identity, static metadata, coarse position, or display-state
  // changes. Fast-moving motion and climate can wait; each repaint uses fresh data.
  char sig[320];
  int lowBucket = (batt >= 0 && batt < 15) ? 1 : 0;
  if (got)
    snprintf(sig, sizeof(sig), "F|%s|%s|%s|%s|%s|%s|%ld|%ld|%d",
             p.hex.c_str(), p.airline.c_str(), p.fromCode.c_str(), p.toCode.c_str(),
             p.typeDesc.c_str(), p.reg.c_str(), lround(p.altFt / 500.0),
             lround(p.slantKm / 5.0), lowBucket);
  else
    snprintf(sig, sizeof(sig), "E|%d|%s|%s|%s|%s|%s|%s|%s",
             lowBucket, rtcLastSeen, rtcLastFrom, rtcLastTo, rtcLastCities,
             rtcLastType, rtcLastReg, rtcLastMotion);

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
