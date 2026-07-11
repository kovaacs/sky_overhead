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
 *   local ADS-B feeder — preferred live position source when LOCAL_ADSB_URL is configured
 *   adsb.lol   — public live position fallback
 *   adsb.im    — route lookup using callsign + live position
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

#include "AdsbParser.h"
#include "AdsbFallback.h"
#include "Aircraft.h"
#include "Climate.h"
#include "ClimateSensor.h"
#include "Config.h"
#include "DisplayView.h"
#include "IconFont.h"
#include "RetainedState.h"
#include "RouteParser.h"
#include "TimeRules.h"

// Runtime config — loaded from /config.txt on the SD card at boot.
// If the card is missing or the file can't be parsed the device boots but
// won't connect to Wi-Fi; it will retry on the next timer wake.
RuntimeConfig runtime;

// Unit preferences — also loaded from config.txt.
// Defaults: aviation height (ft/FL), km/h, Celsius.


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
  constexpr uint32_t HTTP_TIMEOUT   = 10000;     // per request (ms)
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

  // full-screen sleep-view vertical anchors
  constexpr int LEFT_CX = DIVIDER_X / 2;
  constexpr int LEFT_ICON_Y = 128;
  constexpr int LEFT_TITLE_Y = 226;
  constexpr int LEFT_SUBTITLE_Y = 282;

  // shared route-row geometry
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
// State that survives deep sleep but not a power cycle.
RTC_DATA_ATTR char     rtcSig[320]     = "";   // signature of what's on screen
RTC_DATA_ATTR char     rtcLastSeen[96] = "";   // airline/callsign of last plane
RTC_DATA_ATTR char     rtcLastFrom[8]  = "";   // origin IATA/ICAO code
RTC_DATA_ATTR char     rtcLastTo[8]    = "";   // destination IATA/ICAO code
RTC_DATA_ATTR char     rtcLastRouteKey[40] = ""; // callsign/hex used for retained route
RTC_DATA_ATTR char     rtcLastCities[96] = ""; // origin/destination city text
RTC_DATA_ATTR char     rtcLastAircraft[80] = ""; // aircraft type label
RTC_DATA_ATTR char     rtcLastIdentity[96] = ""; // callsign + tail number
RTC_DATA_ATTR char     rtcLastAirline[96] = ""; // airline of last plane
RTC_DATA_ATTR char     rtcLastCategory[8] = ""; // ADS-B category for last icon choice
RTC_DATA_ATTR char     rtcLastType[64] = "";   // aircraft type/description
RTC_DATA_ATTR char     rtcLastReg[16]  = "";   // tail number / registration
RTC_DATA_ATTR char     rtcLastMotion[64] = ""; // altitude/trend/speed text
RTC_DATA_ATTR time_t   rtcLastEpoch    = 0;    // when it was last overhead
RTC_DATA_ATTR uint16_t rtcRedraws      = 0;    // for periodic ghost-clean
RTC_DATA_ATTR uint8_t  rtcDemoStep     = 0;    // rotates demo screens

EPaper            epaper;
Settings          cfg;
SensirionI2cSht4x sht4x;

#if DEBUG_LOG
  #define LOG(...) Serial1.printf(__VA_ARGS__)
#else
  #define LOG(...) ((void)0)
#endif

static void goSleep(uint32_t seconds);

// ============================ TIME HELPERS ==========================
static bool haveClock() {
  struct tm t;
  return getLocalTime(&t, 800);
}

static bool syncClock() {
  configTzTime(runtime.tzInfo.c_str(), "pool.ntp.org", "time.nist.gov");
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
  return secondsUntilMinuteOfDay(minuteOfDay, t.tm_hour, t.tm_min, t.tm_sec);
}

static bool isNightNow() {
  return isNightMinute(cfg.night, cfg.nightStart, cfg.nightEnd, curMinuteOfDay());
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
  Climate c = readClimateSensor(sht4x, [](int ms) { delay(ms); });
  if (c.ok) LOG("[sht4x] %.1f C  %.0f%%\n", c.tempC, c.hum);
  else LOG("[sht4x] read failed\n");
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
    digitalWrite(pin::SD_EN, LOW);
    return;
  }

  File f = SD.open("/config.txt");
  if (!f) {
    LOG("[sd] /config.txt not found\n");
    SD.end();
    digitalWrite(pin::SD_EN, LOW);
    return;
  }

  while (f.available()) {
    applyConfigLine(cfg, runtime, f.readStringUntil('\n'));
  }
  f.close();
  SD.end();
  digitalWrite(pin::SD_EN, LOW);   // power off the slot during WiFi+fetch phase
  LOG("[sd] config: SSID=%s LAT=%.5f LON=%.5f ALT=%.1fm TZ=%s\n",
      runtime.wifiSSID.c_str(), runtime.myLat, runtime.myLon, runtime.myAltM, runtime.tzInfo.c_str());
}

// ============================ NETWORK ===============================
static bool connectWiFi() {
  if (!runtime.wifiSSID.length()) { LOG("[wifi] no SSID configured\n"); return false; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(runtime.wifiSSID.c_str(), runtime.wifiPass.c_str());
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
  WiFiClient plainClient;
  WiFiClientSecure client;
  client.setInsecure();                          // no cert pinning (hobby tradeoff)
  HTTPClient http;
  bool began = url.startsWith("https://")
      ? http.begin(client, url)
      : http.begin(plainClient, url);
  if (!began) {
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

static bool httpPostJson(const String& url, const String& body, JsonDocument& doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    LOG("[http] begin failed\n");
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.setUserAgent(USER_AGENT);
  http.setConnectTimeout(timing::HTTP_TIMEOUT);
  http.setTimeout(timing::HTTP_TIMEOUT);

  int code = http.POST(body);
  if (code != 200) {
    LOG("[http] POST %d  %s\n", code, url.c_str());
    http.end();
    return false;
  }
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    LOG("[http] JSON error: %s\n", err.c_str());
    return false;
  }
  return true;
}

static JsonDocument aircraftFilter() {
  JsonDocument filter;
  for (const char* arrayName : {"ac", "aircraft"}) {
    JsonObject f = filter[arrayName].add<JsonObject>();
    for (const char* k : {"hex", "flight", "lat", "lon", "alt_baro", "alt_geom", "category", "t", "desc", "r", "gs", "baro_rate"})
      f[k] = true;
  }
  return filter;
}

// adsb.lol: public aircraft fallback.
static FetchResult fetchPublicOverhead(Plane& best) {
  int radiusNm = constrain((int)ceil(cfg.radius / 1.852), 1, 250);
  char url[160];
  snprintf(url, sizeof(url), "https://%s/v2/point/%.5f/%.5f/%d",
           API_HOST, runtime.myLat, runtime.myLon, radiusNm);

  JsonDocument filter = aircraftFilter();
  JsonDocument doc;
  if (!httpGetJson(url, doc, &filter)) return FETCH_ERROR;

  FetchResult result = parseOverheadAircraft(doc, runtime.myLat, runtime.myLon, runtime.myAltM, best);
  LOG("[adsb] public %s @ %.1f km (3D)\n",
      best.found ? best.callsign.c_str() : "nothing", best.found ? best.slantKm : 0.0);
  return result;
}

static FetchResult fetchLocalOverhead(Plane& best) {
  String url = buildLocalAdsbAircraftUrl(runtime.localAdsbBaseUrl);
  if (!url.length()) return FETCH_ERROR;

  JsonDocument filter = aircraftFilter();
  JsonDocument doc;
  if (!httpGetJson(url, doc, &filter)) return FETCH_ERROR;

  FetchResult result = parseOverheadAircraft(doc, runtime.myLat, runtime.myLon, runtime.myAltM, best, cfg.radius);
  LOG("[adsb] local %s @ %.1f km (3D)\n",
      best.found ? best.callsign.c_str() : "nothing", best.found ? best.slantKm : 0.0);
  return result;
}

static FetchResult fetchOverhead(Plane& best, String& source) {
  return fetchWithFallbackSource(
    best,
    [](Plane& out) { return fetchLocalOverhead(out); },
    "local feed",
    [](Plane& out) { return fetchPublicOverhead(out); },
    "adsb.lol",
    source
  );
}

// tar1090 routeset: route lookup by callsign plus live aircraft position.
static void fetchRoute(Plane& p) {
  if (!p.callsign.length()) return;

  JsonDocument bodyDoc;
  JsonObject plane = bodyDoc["planes"].add<JsonObject>();
  plane["callsign"] = p.callsign;
  plane["lat"] = p.lat;
  plane["lng"] = p.lon;
  String body;
  serializeJson(bodyDoc, body);

  JsonDocument doc;
  if (!httpPostJson("https://adsb.im/api/0/routeset", body, doc)) {
    return;
  }

  applyRouteResponse(doc, p);
}

static Plane demoPlane() {
  Plane p;
  p.found = true;
  p.callsign = "DLH4JA";
  p.hex = "3C65C2";
  p.category = "A3";
  p.airline = "Lufthansa";
  p.fromCode = "MUC";
  p.toCode = "BUD";
  p.routeOk = true;
  p.fromCity = "Munich";
  p.toCity = "Budapest";
  p.typeCode = "A20N";
  p.typeDesc = "Airbus A320neo";
  p.reg = "D-AINZ";
  p.altFt = 33000;
  p.slantKm = 8.2;
  p.gsKt = 421;
  p.vrateFpm = 850;
  p.hasGs = true;
  p.hasVrate = true;
  return p;
}

static RetainedAircraftState retainedStateFromRtc() {
  RetainedAircraftState state;
  state.lastSeen = String(rtcLastSeen);
  state.lastFrom = String(rtcLastFrom);
  state.lastTo = String(rtcLastTo);
  state.lastRouteKey = String(rtcLastRouteKey);
  state.lastCities = String(rtcLastCities);
  state.lastAircraft = String(rtcLastAircraft);
  state.lastIdentity = String(rtcLastIdentity);
  state.lastAirline = String(rtcLastAirline);
  state.lastCategory = String(rtcLastCategory);
  state.lastType = String(rtcLastType);
  state.lastReg = String(rtcLastReg);
  state.lastMotion = String(rtcLastMotion);
  state.lastEpoch = rtcLastEpoch;
  return state;
}

static void writeRetainedStateToRtc(const RetainedAircraftState& state) {
  state.lastSeen.toCharArray(rtcLastSeen, sizeof(rtcLastSeen));
  state.lastFrom.toCharArray(rtcLastFrom, sizeof(rtcLastFrom));
  state.lastTo.toCharArray(rtcLastTo, sizeof(rtcLastTo));
  state.lastRouteKey.toCharArray(rtcLastRouteKey, sizeof(rtcLastRouteKey));
  state.lastCities.toCharArray(rtcLastCities, sizeof(rtcLastCities));
  state.lastAircraft.toCharArray(rtcLastAircraft, sizeof(rtcLastAircraft));
  state.lastIdentity.toCharArray(rtcLastIdentity, sizeof(rtcLastIdentity));
  state.lastAirline.toCharArray(rtcLastAirline, sizeof(rtcLastAirline));
  state.lastCategory.toCharArray(rtcLastCategory, sizeof(rtcLastCategory));
  state.lastType.toCharArray(rtcLastType, sizeof(rtcLastType));
  state.lastReg.toCharArray(rtcLastReg, sizeof(rtcLastReg));
  state.lastMotion.toCharArray(rtcLastMotion, sizeof(rtcLastMotion));
  rtcLastEpoch = state.lastEpoch;
}

static void rememberLastSeenRtc(const Plane& p) {
  RetainedAircraftState state = retainedStateFromRtc();
  rememberLastSeen(state, p, cfg.height, cfg.speed, haveClock() ? time(nullptr) : 0);
  writeRetainedStateToRtc(state);
}

static RetainedAircraftView retainedAircraftView() {
  RetainedAircraftState state = retainedStateFromRtc();
  RetainedAircraftView retained;
  retained.lastSeen = state.lastSeen;
  retained.lastFrom = state.lastFrom;
  retained.lastTo = state.lastTo;
  retained.lastAircraft = state.lastAircraft;
  retained.lastIdentity = state.lastIdentity;
  retained.lastAirline = state.lastAirline;
  retained.lastCategory = state.lastCategory;
  retained.lastType = state.lastType;
  retained.lastMotion = state.lastMotion;
  return retained;
}

#include "DisplayRenderer.h"

static void runDemoMode() {
  int batt = batteryPct();
  Climate clim = readClimate();
  Plane p = demoPlane();
  rememberLastSeenRtc(p);

  uint8_t step = rtcDemoStep % 3;
  if (step == 0) {
    drawLive(p, batt, clim, cfg.temp, cfg.height, cfg.speed, retainedAircraftView(), hhmm());
  } else if (step == 1) {
    Plane empty;
    drawLive(empty, batt, clim, cfg.temp, cfg.height, cfg.speed, retainedAircraftView(), hhmm());
  } else {
    drawNightSleep(cfg.nightEnd, batt, hhmm());
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
      drawNightSleep(cfg.nightEnd, batt, hhmm());
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
  String aircraftSource;
  FetchResult fetchResult = fetchOverhead(p, aircraftSource);
  if (fetchResult == FETCH_ERROR) {
    goSleep(timing::RETRY_SECONDS);
  }

  bool got = fetchResult == FETCH_FOUND;
  bool routeSourceUsed = false;
  bool retainedRouteUsed = false;
  if (got) {
    fetchRoute(p);
    routeSourceUsed = p.routeOk;
    retainedRouteUsed = applyRetainedRouteIfSame(p, retainedStateFromRtc());
  }
  String sourceText = dataSourceText(aircraftSource, routeSourceUsed, retainedRouteUsed);

  if (got) {                                       // remember for the empty-sky screen
    rememberLastSeenRtc(p);
  }

  int batt = batteryPct();
  Climate clim = readClimate();

  // No-flash: repaint on identity, static metadata, coarse position, or display-state
  // changes. Fast-moving motion and climate can wait; each repaint uses fresh data.
  int lowBucket = (batt >= 0 && batt < 15) ? 1 : 0;
  String sig = got ? foundRenderSignature(p, lowBucket)
                   : emptyRenderSignature(retainedStateFromRtc(), lowBucket);
  sig += "|S|";
  sig += sourceText;

  if (sig != String(rtcSig)) {
    if (rtcRedraws > 0 && rtcRedraws % timing::GHOST_CLEAN_EVERY == 0) {
      epaper.fillScreen(TFT_WHITE);
      epaper.update();                             // clear accumulated ghosting
    }
    drawLive(p, batt, clim, cfg.temp, cfg.height, cfg.speed, retainedAircraftView(), hhmm(), sourceText);
    epaper.update();
    rtcRedraws++;
    sig.toCharArray(rtcSig, sizeof(rtcSig));
    LOG("[draw] repainted (%s)\n", sig.c_str());
  } else {
    LOG("[draw] unchanged, skipped\n");
  }

  goSleep(cfg.busy);
}

void loop() {}   // never runs: every cycle ends in deep sleep inside setup()
