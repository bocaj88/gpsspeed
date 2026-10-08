// Persistent settings (NVS via Preferences). Everything the web UI can edit.
#pragma once
#include <Preferences.h>
#include "config.h"

struct CalPoint {
  float mph;      // commanded speed where this point was measured
  float factor;   // output-Hz multiplier that makes the dash read correctly
};

struct Settings {
  float hzPerMph   = DEF_HZ_PER_MPH;
  float noFixMph   = DEF_NO_FIX_MPH;
  float minMph     = DEF_MIN_MPH;
  float leadSec    = DEF_LEAD_SEC;
  float accelAlpha = DEF_ACCEL_ALPHA;
  bool  predictor  = true;
  uint8_t  gpsRateHz  = DEF_GPS_RATE_HZ;
  uint16_t bootWindowS = DEF_BOOT_WINDOW_S;
  char  apPass[33] = DEF_AP_PASS;
  uint8_t  calCount = 0;
  CalPoint cal[CAL_POINTS];
};

static Settings cfg;
static Preferences prefs;

static void settingsLoad() {
  prefs.begin("gpsspeed", true);
  cfg.hzPerMph    = prefs.getFloat("k", DEF_HZ_PER_MPH);
  cfg.noFixMph    = prefs.getFloat("nofix", DEF_NO_FIX_MPH);
  cfg.minMph      = prefs.getFloat("min", DEF_MIN_MPH);
  cfg.leadSec     = prefs.getFloat("lead", DEF_LEAD_SEC);
  cfg.accelAlpha  = prefs.getFloat("alpha", DEF_ACCEL_ALPHA);
  cfg.predictor   = prefs.getBool("pred", true);
  cfg.gpsRateHz   = prefs.getUChar("rate", DEF_GPS_RATE_HZ);
  cfg.bootWindowS = prefs.getUShort("win", DEF_BOOT_WINDOW_S);
  String pw = prefs.getString("pass", DEF_AP_PASS);
  strlcpy(cfg.apPass, pw.c_str(), sizeof(cfg.apPass));
  cfg.calCount = min<uint8_t>(prefs.getUChar("ncal", 0), CAL_POINTS);
  if (cfg.calCount) prefs.getBytes("cal", cfg.cal, sizeof(CalPoint) * cfg.calCount);
  prefs.end();

  // Never trust NVS blindly -- a bad value here drives the dash.
  if (!(cfg.hzPerMph > 1.0f && cfg.hzPerMph < 20.0f)) cfg.hzPerMph = DEF_HZ_PER_MPH;
  if (!(cfg.noFixMph >= 0.0f && cfg.noFixMph <= 30.0f)) cfg.noFixMph = DEF_NO_FIX_MPH;
  if (!(cfg.minMph >= 0.0f && cfg.minMph <= 5.0f)) cfg.minMph = DEF_MIN_MPH;
  if (!(cfg.leadSec >= 0.0f && cfg.leadSec <= 2.0f)) cfg.leadSec = DEF_LEAD_SEC;
  if (!(cfg.accelAlpha > 0.0f && cfg.accelAlpha <= 1.0f)) cfg.accelAlpha = DEF_ACCEL_ALPHA;
  if (cfg.gpsRateHz != 1 && cfg.gpsRateHz != 5 && cfg.gpsRateHz != 10) cfg.gpsRateHz = DEF_GPS_RATE_HZ;
  if (cfg.bootWindowS < 30 || cfg.bootWindowS > 3600) cfg.bootWindowS = DEF_BOOT_WINDOW_S;
  if (strlen(cfg.apPass) < 8) cfg.apPass[0] = 0;   // WPA2 needs 8+; anything shorter = open network
}

static void settingsSave() {
  prefs.begin("gpsspeed", false);
  prefs.putFloat("k", cfg.hzPerMph);
  prefs.putFloat("nofix", cfg.noFixMph);
  prefs.putFloat("min", cfg.minMph);
  prefs.putFloat("lead", cfg.leadSec);
  prefs.putFloat("alpha", cfg.accelAlpha);
  prefs.putBool("pred", cfg.predictor);
  prefs.putUChar("rate", cfg.gpsRateHz);
  prefs.putUShort("win", cfg.bootWindowS);
  prefs.putString("pass", cfg.apPass);
  prefs.putUChar("ncal", cfg.calCount);
  prefs.putBytes("cal", cfg.cal, sizeof(CalPoint) * CAL_POINTS);
  prefs.end();
}

static void settingsFactoryReset() {
  prefs.begin("gpsspeed", false);
  prefs.clear();
  prefs.end();
  cfg = Settings();
}
