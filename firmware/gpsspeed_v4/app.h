// What speed are we sending the dash, and why. Shared by the main loop,
// the web UI and the serial console.
#pragma once
#include "config.h"
#include "settings.h"
#include "pulse.h"
#include "gps.h"

enum Mode { MODE_NORMAL, MODE_MANUAL, MODE_SELFTEST };

static Mode     mode = MODE_NORMAL;   // never persisted: boot is always NORMAL
static float    manualMph = 0;
static int      stIdx = -1;
static uint32_t stStartMs = 0;
static float    outMph = 0;           // what the dash is being told
static float    rawMph = 0;           // what the GPS said

// predictor state
static float    prevRawMph = 0;
static uint32_t prevRawMs = 0;
static float    accelEma = 0;

static const char *modeName() {
  switch (mode) {
    case MODE_MANUAL: return "manual";
    case MODE_SELFTEST: return "self-test";
    default: return "normal";
  }
}

static void setModeNormal() { mode = MODE_NORMAL; }
static void setModeManual(float mph) {
  mode = MODE_MANUAL;
  manualMph = constrain(mph, 0.0f, MAX_MPH);
}
static void selftestStart() { mode = MODE_SELFTEST; stIdx = -1; }
static void selftestNext() {
  if (mode != MODE_SELFTEST) { selftestStart(); return; }
  stIdx = (stIdx + 1) % (int)(sizeof(SELFTEST_SPEEDS) / sizeof(SELFTEST_SPEEDS[0]));
  stStartMs = millis();
}

// Lag compensation: PerfectPass is the PID; if our number lags reality it
// corrects late and hunts. Project speed forward by the smoothed GPS
// acceleration so we report where the boat is about to be.
static float conditionSpeed(float raw) {
  if (!cfg.predictor) return raw;
  uint32_t now = millis();
  if (prevRawMs) {
    float dt = (now - prevRawMs) / 1000.0f;
    if (dt > 0.0f && dt < 2.0f) {
      float a = (raw - prevRawMph) / dt;
      accelEma += cfg.accelAlpha * (a - accelEma);
    }
  }
  prevRawMph = raw;
  prevRawMs = now;
  float p = raw + accelEma * cfg.leadSec;
  return p < 0 ? 0 : p;
}

// Decide the commanded speed for this pass of loop() and push it to the
// pulse generator only when it actually changes.
static void updateOutput(bool freshFix) {
  float target;
  if (freshFix) rawMph = gps.speed.mph();
  switch (mode) {
    case MODE_MANUAL:
      target = manualMph;
      break;
    case MODE_SELFTEST: {
      const int n = sizeof(SELFTEST_SPEEDS) / sizeof(SELFTEST_SPEEDS[0]);
      if (stIdx < 0 || millis() - stStartMs >= SELFTEST_STEP_MS) {
        stIdx = (stIdx + 1) % n;
        stStartMs = millis();
      }
      target = SELFTEST_SPEEDS[stIdx];
      break;
    }
    default:
      if (!gpsHaveFix()) {
        // Hold steady. Never sweep: PerfectPass would chase the motion.
        target = cfg.noFixMph;
        prevRawMs = 0;
        accelEma = 0;
      } else if (freshFix) {
        target = conditionSpeed(rawMph);
      } else {
        target = outMph;          // between fixes, keep the last value
      }
  }
  if (fabsf(target - outMph) > 0.005f || (target >= cfg.minMph) != pulseRunning) {
    outMph = target;
    pulseSetMph(outMph);
  }
}

static float batteryVolts() {
  float v = analogReadMilliVolts(PIN_VSENSE) * VSENSE_RATIO / 1000.0f;
  return v > 6.0f ? v + VSENSE_DIODE_V : v;   // >6V = on boat power, behind D1
}
