// ATGM336H GPS: hardware UART, autobaud, CASIC config, fix tracking.
//
// The ATGM336H speaks NMEA plus ZhongKe's CASIC "$PCASxx" commands -- NOT
// u-blox UBX, so the UBX config bursts from v2/v3 don't apply here.
// Settings are sent on every boot and not saved in the module, so a power
// cycle always returns it to a known 9600-baud / 1 Hz state.
#pragma once
#include <TinyGPS++.h>
#include "config.h"
#include "settings.h"

static HardwareSerial GPSSerial(1);
static TinyGPSPlus gps;
// Antenna status arrives as $GPTXT/$GNTXT,...,ANTENNA OK|OPEN|SHORT
static TinyGPSCustom antGP(gps, "GPTXT", 4);
static TinyGPSCustom antGN(gps, "GNTXT", 4);

static uint32_t gpsBaud = 0;          // 0 = nothing heard yet
static uint8_t  gpsRate = 1;          // Hz we believe the module is running
static uint32_t gpsLastByteMs = 0;
static uint32_t gpsLastFixMs = 0;
static bool     gpsEverFix = false;
static char     gpsAntenna[16] = "unknown";
static volatile uint32_t ppsCount = 0;

static void IRAM_ATTR onPps() { ppsCount = ppsCount + 1; }

static void casSend(const char *body) {
  uint8_t cs = 0;
  for (const char *p = body; *p; p++) cs ^= (uint8_t)*p;
  char buf[96];
  snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
  GPSSerial.print(buf);
  GPSSerial.flush();
}

// Listen at `baud` for `ms` and report whether real NMEA came through
// (checksums passing, not just noise).
static bool gpsProbe(uint32_t baud, uint32_t ms) {
  GPSSerial.updateBaudRate(baud);
  while (GPSSerial.available()) GPSSerial.read();
  uint32_t before = gps.passedChecksum();
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    while (GPSSerial.available()) gps.encode(GPSSerial.read());
    if (gps.passedChecksum() >= before + 2) return true;
    delay(2);
  }
  return false;
}

static void gpsReset() {
  pinMode(PIN_GPS_NRST, OUTPUT);
  digitalWrite(PIN_GPS_NRST, LOW);
  delay(20);
  pinMode(PIN_GPS_NRST, INPUT);        // release; module has its own pull-up
}

static void gpsBegin() {
  pinMode(PIN_GPS_NRST, INPUT);
  pinMode(PIN_GPS_PPS, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_GPS_PPS), onPps, RISING);
  GPSSerial.setRxBufferSize(2048);
  GPSSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  // The module may still be at 115200 if only the ESP32 rebooted.
  if (gpsProbe(9600, 1500))        gpsBaud = 9600;
  else if (gpsProbe(115200, 1000)) gpsBaud = 115200;
  else { gpsReset(); delay(300); if (gpsProbe(9600, 2000)) gpsBaud = 9600; }
  if (!gpsBaud) { GPSSerial.updateBaudRate(9600); gpsRate = 1; return; }   // keep listening

  // 10 Hz of GGA+RMC is ~1.5 kB/s -- more than 9600 baud carries, so go to
  // 115200 first. PCAS01 baud codes: 1=9600 .. 5=115200.
  if (gpsBaud == 9600 && cfg.gpsRateHz > 1) {
    casSend("PCAS01,5");
    delay(150);
    if (gpsProbe(115200, 1500)) gpsBaud = 115200;
    else gpsProbe(9600, 500);            // stayed at 9600
  }

  // Sentence mask: GGA (satellites/HDOP) + RMC (speed) every fix, antenna
  // status every 10th fix, everything else off.
  // Order: GGA,GLL,GSA,GSV,RMC,VTG,ZDA,ANT,DHV,LPS,res,res,UTC,GST,res,res,res,TIM
  casSend("PCAS03,1,0,0,0,1,0,0,10,0,0,,,0,0,,,,0");
  delay(50);

  uint8_t want = cfg.gpsRateHz;
  if (gpsBaud == 9600 && want > 5) want = 5;   // RMC+GGA at 5 Hz still fits 9600
  const char *rate = want >= 10 ? "PCAS02,100" : want >= 5 ? "PCAS02,200" : "PCAS02,1000";
  casSend(rate);
  gpsRate = want;
}

// Pump bytes; returns true when a fresh speed-bearing fix was parsed.
static bool gpsPoll() {
  bool fresh = false;
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    gpsLastByteMs = millis();
    if (gps.encode(c)) {
      if (gps.speed.isUpdated() && gps.speed.isValid() && gps.location.isValid()) {
        gpsLastFixMs = millis();
        gpsEverFix = true;
        fresh = true;
      }
      if (antGP.isUpdated() || antGN.isUpdated()) {
        const char *v = antGP.isUpdated() ? antGP.value() : antGN.value();
        if (strstr(v, "OK")) strlcpy(gpsAntenna, "OK", sizeof(gpsAntenna));
        else if (strstr(v, "OPEN")) strlcpy(gpsAntenna, "OPEN", sizeof(gpsAntenna));
        else if (strstr(v, "SHORT")) strlcpy(gpsAntenna, "SHORT", sizeof(gpsAntenna));
      }
    }
  }
  return fresh;
}

static bool gpsHaveFix() {
  return gpsEverFix && (millis() - gpsLastFixMs < FIX_TIMEOUT_MS);
}

static bool gpsTalking() {
  return gpsLastByteMs && (millis() - gpsLastByteMs < FIX_TIMEOUT_MS);
}
