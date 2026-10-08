// WiFi "boot window": an access point that exists for a couple of minutes
// after power-on, then turns itself off so the radio is quiet on the water.
//
//   SSID  GPSSpeed-XXXX   open by default (a password can be set from the page)
//   Open  http://gpsspeed.local  or  http://192.168.4.1
//
// Captive portal, two steps: a phone's "is there internet?" probe first gets
// redirected to our page, so the sign-in sheet pops up showing the app. The
// page's JavaScript then calls /api/hello -- only a real, visible page runs JS,
// iOS's background prober doesn't -- and from then on that phone's probes get
// the "online" answer its OS expects. The page reloads once so the sheet
// re-checks and flips Cancel -> Done, and iOS saves the network.
// Only known OS probe URLs ever get "Success"; anything else typed gets the app.
//
// While a phone is connected the window keeps extending, so it won't drop
// out from under you mid-calibration. "Keep WiFi on" holds it until reboot.
// To get back in later: cycle power.
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_mac.h>
#include <Update.h>
#include "app.h"
#include "page.h"
#include "safety.h"

static WebServer server(80);
static DNSServer dns;
static bool     wifiOn = false;
static bool     wifiKeep = false;
static uint32_t wifiUntil = 0;
static bool     otaOk = false;
static String   otaErr;

// Phones that have loaded the page (by IP). Reset each time the AP starts.
static IPAddress portalSeen[6];
static uint8_t   portalSeenN = 0;

static bool portalWasSeen(IPAddress ip) {
  for (uint8_t i = 0; i < portalSeenN; i++) if (portalSeen[i] == ip) return true;
  return false;
}
static void portalMarkSeen(IPAddress ip) {
  if (portalWasSeen(ip)) return;
  if (portalSeenN < sizeof(portalSeen) / sizeof(portalSeen[0])) portalSeen[portalSeenN++] = ip;
  else portalSeen[portalSeenN - 1] = ip;
}

static String reqHost() {
  String h = server.hostHeader();
  h.toLowerCase();
  int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  while (h.endsWith(".")) h.remove(h.length() - 1);
  return h;
}

static bool isOurHost() {
  String h = reqHost();
  return h.length() == 0 || h == WiFi.softAPIP().toString() || h.endsWith(".local") || h.indexOf('.') < 0;
}

static void webLog(const char *what) {
  Serial.printf("web %s %s%s -> %s\n", server.client().remoteIP().toString().c_str(),
                reqHost().c_str(), server.uri().c_str(), what);
}

// OS "is there internet?" checks: Apple, Android, Windows, Firefox.
static bool isProbe() {
  String h = reqHost(), u = server.uri();
  static const char *hosts[] = {"captive.apple.com", "www.apple.com", "www.appleiphonecell.com",
    "www.itools.info", "www.ibook.info", "www.airport.us", "www.thinkdifferent.us",
    "connectivitycheck.gstatic.com", "connectivitycheck.android.com", "clients3.google.com",
    "www.msftconnecttest.com", "www.msftncsi.com", "detectportal.firefox.com"};
  for (const char *x : hosts) if (h == x) return true;
  return u == "/hotspot-detect.html" || u == "/library/test/success.html" || u == "/generate_204" ||
         u == "/gen_204" || u == "/connecttest.txt" || u == "/ncsi.txt" || u == "/success.txt" ||
         u == "/canonical.html";
}

// Anything that isn't one of our routes. Probes from a phone that has the page
// open get "online"; everything else is sent to the app.
static void handleForeign() {
  if (!isProbe() || !portalWasSeen(server.client().remoteIP())) {
    webLog(isProbe() ? "redirect (probe, page not shown yet)" : "redirect");
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  webLog("online");
  String u = server.uri();
  if (u == "/generate_204" || u == "/gen_204") { server.send(204, "text/plain", ""); return; }  // Android
  if (u == "/connecttest.txt") { server.send(200, "text/plain", "Microsoft Connect Test"); return; }
  if (u == "/ncsi.txt") { server.send(200, "text/plain", "Microsoft NCSI"); return; }
  if (u == "/success.txt") { server.send(200, "text/plain", "success\n"); return; }      // Firefox
  server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");  // Apple
}

static void sendJson(const String &s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

static float argF(const char *name, float def) {
  if (!server.hasArg(name) || server.arg(name).length() == 0) return def;
  return server.arg(name).toFloat();
}

static void handleStatus() {
  char b[520];
  long age = gpsEverFix ? (long)(millis() - gpsLastFixMs) : -1;
  long left = wifiKeep ? -1 : (long)((int32_t)(wifiUntil - millis()) / 1000);
  snprintf(b, sizeof(b),
    "{\"fw\":\"%s\",\"mode\":\"%s\",\"out\":%.2f,\"cmd\":%.2f,\"raw\":%.2f,\"hz\":%.3f,"
    "\"fix\":%s,\"talking\":%s,\"sats\":%u,\"hdop\":%.2f,\"age\":%ld,\"baud\":%lu,\"rate\":%u,"
    "\"ant\":\"%s\",\"vin\":%.2f,\"up\":%lu,\"reset\":\"%s\",\"wifi_left\":%ld,\"wifi_keep\":%s,"
    "\"chars\":%lu,\"pps\":%lu}",
    FW_VERSION, modeName(), outMph, outMph, rawMph, pulseHz,
    gpsHaveFix() ? "true" : "false", gpsTalking() ? "true" : "false",
    (unsigned)gps.satellites.value(), gps.hdop.isValid() ? gps.hdop.hdop() : 0.0,
    age, (unsigned long)gpsBaud, gpsRate, gpsAntenna, batteryVolts(),
    (unsigned long)(millis() / 1000), resetReasonStr(), left < 0 ? 0 : left,
    wifiKeep ? "true" : "false", (unsigned long)gps.charsProcessed(), (unsigned long)ppsCount);
  sendJson(b);
}

static void handleConfigGet() {
  String s = "{";
  s += "\"k\":" + String(cfg.hzPerMph, 4);
  s += ",\"nofix\":" + String(cfg.noFixMph, 2);
  s += ",\"min\":" + String(cfg.minMph, 2);
  s += ",\"lead\":" + String(cfg.leadSec, 2);
  s += ",\"alpha\":" + String(cfg.accelAlpha, 2);
  s += ",\"pred\":" + String(cfg.predictor ? "true" : "false");
  s += ",\"rate\":" + String(cfg.gpsRateHz);
  s += ",\"win\":" + String(cfg.bootWindowS);
  s += ",\"cal\":[";
  for (int i = 0; i < cfg.calCount; i++) {
    if (i) s += ",";
    s += "{\"mph\":" + String(cfg.cal[i].mph, 2) + ",\"f\":" + String(cfg.cal[i].factor, 5) + "}";
  }
  s += "]}";
  sendJson(s);
}

static void handleConfigPost() {
  float k = argF("k", cfg.hzPerMph);
  if (k < 1.0f || k > 20.0f) { server.send(400, "text/plain", "Hz per MPH must be 1-20"); return; }
  cfg.hzPerMph   = k;
  cfg.noFixMph   = constrain(argF("nofix", cfg.noFixMph), 0.0f, 30.0f);
  cfg.minMph     = constrain(argF("min", cfg.minMph), 0.0f, 5.0f);
  cfg.leadSec    = constrain(argF("lead", cfg.leadSec), 0.0f, 2.0f);
  cfg.accelAlpha = constrain(argF("alpha", cfg.accelAlpha), 0.01f, 1.0f);
  if (server.hasArg("pred")) cfg.predictor = server.arg("pred") == "1";
  int r = (int)argF("rate", cfg.gpsRateHz);
  if (r == 1 || r == 5 || r == 10) cfg.gpsRateHz = r;
  cfg.bootWindowS = constrain((int)argF("win", cfg.bootWindowS), 30, 3600);
  String pw = server.arg("pass");
  if (server.arg("open") == "1") cfg.apPass[0] = 0;  // "Remove password"
  else if (pw.length()) {
    if (pw.length() < 8 || pw.length() > 32) { server.send(400, "text/plain", "Password must be 8-32 characters"); return; }
    strlcpy(cfg.apPass, pw.c_str(), sizeof(cfg.apPass));
  }
  settingsSave();
  pulseSetMph(outMph);           // apply a new k / calibration immediately
  server.send(200, "text/plain", "ok");
}

static void handleMode() {
  String m = server.arg("mode");
  if (m == "gps") setModeNormal();
  else if (m == "manual") setModeManual(argF("mph", 0));
  else if (m == "selftest") selftestStart();
  else if (m == "next") selftestNext();
  else { server.send(400, "text/plain", "unknown mode"); return; }
  server.send(200, "text/plain", "ok");
}

static void handleCal() {
  if (mode == MODE_NORMAL) { server.send(400, "text/plain", "Hold a speed first (self-test or manual)"); return; }
  if (!calAddPoint(outMph, argF("dash", 0))) { server.send(400, "text/plain", "Point rejected (speed under 2 MPH, or more than 2x off)"); return; }
  settingsSave();
  pulseSetMph(outMph);
  server.send(200, "text/plain", "ok");
}

static void handleUpdateDone() {
  if (otaOk) {
    server.send(200, "text/plain", "ok");
    delay(400);
    ESP.restart();
  } else {
    server.send(500, "text/plain", otaErr.length() ? otaErr : String("update failed"));
  }
}

static void handleUpdateChunk() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    otaOk = false; otaErr = "";
    wifiKeep = true;                           // don't time out mid-upload
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) otaErr = Update.errorString();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!otaErr.length() && Update.write(u.buf, u.currentSize) != u.currentSize) otaErr = Update.errorString();
  } else if (u.status == UPLOAD_FILE_END) {
    // end(true) verifies the image checksum before switching boot slots.
    if (!otaErr.length() && Update.end(true)) otaOk = true;
    else if (!otaErr.length()) otaErr = Update.errorString();
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort(); otaErr = "upload aborted";
  }
}

static void wifiStart() {
  // Read the factory MAC from eFuse: WiFi.macAddress() returns zeros/junk
  // before the radio is started, which made the SSID change between boots
  // (4.0.2 showed GPSSpeed-0060, then -0000), so phones never re-found it.
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "GPSSpeed-%02X%02X", mac[4], mac[5]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, cfg.apPass[0] ? cfg.apPass : nullptr, 6, 0, 2);   // nullptr = open
  // Keep TX power modest: the GPS antenna is a few cm away.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  dns.start(53, "*", WiFi.softAPIP());         // captive portal: every name -> us

  portalSeenN = 0;
  // Bonjour name, so the page is also at http://gpsspeed.local (iOS/macOS resolve it natively).
  if (MDNS.begin("gpsspeed")) MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, [] {
    if (!isOurHost()) { handleForeign(); return; }      // e.g. captive.apple.com/
    webLog("page");
    server.send_P(200, "text/html", PAGE_HTML);
  });
  server.on("/favicon.ico", HTTP_GET, [] { server.send(204, "text/plain", ""); });   // don't redirect it to the page
  server.on("/api/hello", HTTP_POST, [] {              // the page's JS is running on a screen
    IPAddress ip = server.client().remoteIP();
    bool first = !portalWasSeen(ip);
    portalMarkSeen(ip);
    webLog(first ? "hello (first)" : "hello");
    server.send(200, "text/plain", first ? "new" : "seen");
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/cal", HTTP_POST, handleCal);
  server.on("/api/cal/clear", HTTP_POST, [] { cfg.calCount = 0; settingsSave(); pulseSetMph(outMph); server.send(200, "text/plain", "ok"); });
  server.on("/api/wifi/keep", HTTP_POST, [] { wifiKeep = true; server.send(200, "text/plain", "ok"); });
  server.on("/api/reboot", HTTP_POST, [] { server.send(200, "text/plain", "ok"); delay(300); ESP.restart(); });
  server.on("/api/factory", HTTP_POST, [] { settingsFactoryReset(); server.send(200, "text/plain", "ok"); delay(300); ESP.restart(); });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateChunk);
  server.onNotFound(handleForeign);             // captive-portal probes land here
  server.begin();
  wifiOn = true;
  wifiUntil = millis() + (uint32_t)cfg.bootWindowS * 1000UL;
  Serial.printf("WiFi AP %s up for %us -> http://gpsspeed.local (http://192.168.4.1)\n", ssid, cfg.bootWindowS);
}

static void wifiStop() {
  MDNS.end();
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOn = false;
  digitalWrite(PIN_LED_WIFI, LOW);
  Serial.println("WiFi window closed (radio off). Cycle power to reopen.");
}

static void wifiTick() {
  if (!wifiOn) return;
  dns.processNextRequest();
  server.handleClient();
  uint32_t now = millis();
  bool client = WiFi.softAPgetStationNum() > 0;
  if (client && (int32_t)(now + CLIENT_GRACE_MS - wifiUntil) > 0) wifiUntil = now + CLIENT_GRACE_MS;
  // LED: solid while waiting for a phone, blinking while one is connected
  digitalWrite(PIN_LED_WIFI, client ? ((now / 250) & 1) : HIGH);
  if (!wifiKeep && (int32_t)(now - wifiUntil) > 0) wifiStop();
}
