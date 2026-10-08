/**
 * @file gpsspeed_v4.ino
 * @author Jake Michalski
 * @brief GPS -> paddlewheel pulse emulator for PerfectPass, on the ESP32-C3 board.
 * @version 4.0.0
 * @date 2026-09-26
 *
 * @copyright Copyright MIT License (c) 2025-2026
 *
 * HARDWARE: pcb/ in this repo (ESP32-C3-MINI-1 + ATGM336H + ZXMS6004FF output), board rev 1.2+.
 * Not for the Uno perfboard -- that's v2/ and v3/.
 *
 * WHAT CHANGED FROM THE UNO BUILDS
 * --------------------------------
 *  - The paddlewheel square wave comes from the LEDC hardware peripheral off
 *    the 40 MHz crystal. Nothing in software can stretch it, so the old
 *    ~21 MPH cliff (SoftwareSerial blocking the pulse loop) cannot happen.
 *  - GPS is on a real hardware UART at 115200 baud, 10 Hz.
 *  - No more one-serial-port juggling: the USB-C serial monitor is always
 *    available alongside the GPS.
 *  - Everything tunable lives in flash and is edited from a phone:
 *      power on -> join WiFi "GPSSpeed-XXXX" (open) -> gpsspeed.local
 *    The access point shuts itself off after the boot window (default 2 min,
 *    extended while a phone is connected), so the radio is quiet underway.
 *  - Firmware updates over WiFi, with automatic rollback if a new build
 *    crash-loops -- safe to pot the board in resin.
 *
 * LEDs
 * ----
 *   PWR  red    board has 3.3 V
 *   GPS  green  solid = fix | 2 blinks = GPS talking, no fix | 1 blink = no GPS data
 *               fast flicker = manual / self-test output (NOT following the GPS)
 *   WIFI green  solid = access point up | blinking = phone connected | off = radio off
 *
 * BUILD
 * -----
 *   arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs firmware/gpsspeed_v4
 *   (min_spiffs = two 1.9 MB OTA slots. Keep it: OTA can never change the partition table.)
 *   Needs TinyGPSPlus. First flash over USB-C; after that, OTA from the web UI.
 */

#include "config.h"
#include "safety.h"
#include "settings.h"
#include "pulse.h"
#include "gps.h"
#include "app.h"
#include "web.h"

static void ledFixTick() {
  uint32_t now = millis();
  bool on;
  if (mode != MODE_NORMAL) {
    on = (now / 125) & 1;                     // fast flicker: not on GPS
  } else if (gpsHaveFix()) {
    on = true;
  } else {
    uint8_t blinks = gpsTalking() ? 2 : 1;
    uint32_t ph = now % 1600;
    on = false;
    for (uint8_t i = 0; i < blinks; i++)
      if (ph >= i * 250UL && ph < i * 250UL + 100UL) on = true;
  }
  digitalWrite(PIN_LED_FIX, on);
}

// USB serial console. Type `help`.
static void serialTick() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') { if (line.length() < 64) line += c; continue; }
    line.trim();
    if (!line.length()) continue;
    int sp = line.indexOf(' ');
    String cmd = sp < 0 ? line : line.substring(0, sp);
    float arg = sp < 0 ? NAN : line.substring(sp + 1).toFloat();
    cmd.toLowerCase();
    if (cmd == "speed" && !isnan(arg)) { setModeManual(arg); Serial.printf("manual %.2f MPH\n", manualMph); }
    else if (cmd == "run" || cmd == "gps") { setModeNormal(); Serial.println("normal (GPS)"); }
    else if (cmd == "selftest") { selftestStart(); Serial.println("self-test"); }
    else if (cmd == "next") { selftestNext(); }
    else if (cmd == "dash" && !isnan(arg)) {
      bool ok = mode != MODE_NORMAL && calAddPoint(outMph, arg);
      if (ok) { settingsSave(); pulseSetMph(outMph); }
      Serial.println(ok ? "calibration point saved" : "rejected (hold a speed first)");
    }
    else if (cmd == "k" && !isnan(arg) && arg > 1 && arg < 20) { cfg.hzPerMph = arg; settingsSave(); pulseSetMph(outMph); Serial.printf("Hz/MPH = %.4f\n", cfg.hzPerMph); }
    else if (cmd == "nofix" && !isnan(arg)) { cfg.noFixMph = constrain(arg, 0.0f, 30.0f); settingsSave(); Serial.printf("no-fix = %.2f MPH\n", cfg.noFixMph); }
    else if (cmd == "calclear") { cfg.calCount = 0; settingsSave(); pulseSetMph(outMph); Serial.println("calibration cleared"); }
    else if (cmd == "wifi") { if (!wifiOn) wifiStart(); wifiKeep = true; Serial.println("WiFi on until reboot"); }
    else if (cmd == "reboot") { ESP.restart(); }
    else {
      Serial.println("commands: speed <mph> | run | selftest | next | dash <mph> | k <hz/mph> |");
      Serial.println("          nofix <mph> | calclear | wifi | reboot");
    }
    line = "";
  }
}

// One line a second, label:value pairs (works in the Serial Plotter too).
static void heartbeat() {
  static uint32_t last = 0;
  static int lastStep = -2;
  if (mode == MODE_SELFTEST && stIdx != lastStep) {
    lastStep = stIdx;
    Serial.printf("[self-test] step %d -> %.1f MPH = %.2f Hz  (read the dash; `dash <mph>` to calibrate)\n",
                  stIdx + 1, outMph, pulseHz);
  }
  if (millis() - last < 1000) return;
  last = millis();
  Serial.printf("sats:%u chars:%lu fix:%d age:%ld raw:%.2f out:%.2f hz:%.3f vin:%.1f mode:%s ant:%s\n",
                (unsigned)gps.satellites.value(), (unsigned long)gps.charsProcessed(), gpsHaveFix(),
                gpsEverFix ? (long)(millis() - gpsLastFixMs) : 9999L, rawMph, outMph, pulseHz,
                batteryVolts(), modeName(), gpsAntenna);
}

void setup() {
  safetyBoot();                       // may roll back a crash-looping update
  Serial.begin(115200);

  pinMode(PIN_LED_FIX, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  analogSetPinAttenuation(PIN_VSENSE, ADC_11db);

  settingsLoad();

  // Dash first: steady no-fix speed from the very first moment.
  pulseInit();
  outMph = cfg.noFixMph;
  pulseSetMph(outMph);

  wifiStart();
  gpsBegin();

  Serial.printf("\nGPSSpeed v%s  reset:%s  Hz/MPH:%.4f  no-fix:%.1f  GPS:%lu baud @ %u Hz\n",
                FW_VERSION, resetReasonStr(), cfg.hzPerMph, cfg.noFixMph,
                (unsigned long)gpsBaud, gpsRate);
}

void loop() {
  bool fresh = gpsPoll();
  updateOutput(fresh);
  wifiTick();
  ledFixTick();
  serialTick();
  heartbeat();
  safetyTick();
}
