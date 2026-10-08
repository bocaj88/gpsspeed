/**
 * @file v2.ino
 * @author Jake Michalski
 * @brief GPS -> paddlewheel pulse emulator for PerfectPass, consolidated build.
 * @version 2.0
 * @date 2026-05-21
 *
 * @copyright Copyright MIT License (c) 2025-2026
 *
 * WHAT THIS IS
 * ------------
 * Reads boat speed from a GPS module and emits a square wave on the MOSFET pin.
 * PerfectPass reads that square wave as if it came from the paddlewheel speed
 * sensor (frequency is proportional to speed). So GPS speed -> fake paddlewheel.
 *
 * GPS module: MakerFocus GT-U7 (NEO-6M compatible).
 *   https://www.amazon.com/dp/B07P8YMVNT
 *   LED: single red PPS LED. Blinking (~1 Hz) = fix acquired. No blink = no
 *   fix. There is NO green LED on this module (earlier v1 comment was wrong).
 *   Needs sky view to GET a fix; can come back inside once locked.
 *   HAS battery-backed RAM, so UBX config commands PERSIST across power
 *   cycles. To revert custom config, use GPS_FACTORY_RESET (see below).
 *
 * THE ONE-SERIAL-PORT PROBLEM (why there used to be many copies of this file)
 * --------------------------------------------------------------------------
 * The Uno has a single hardware UART. The GPS and the USB serial monitor both
 * want it. Two ways to share it, selected below with GPS_USE_SOFTSERIAL:
 *   - Hardware Serial for GPS  -> handles high speeds, but no serial monitor.
 *   - SoftwareSerial for GPS   -> frees the monitor for debug, but the CPU hit
 *                                 means it can't keep up much past ~18 MPH.
 * Instead of keeping a separate .ino per combination, everything is now one
 * file. Pick a mode + transport with the #defines at the top. No more copies.
 *
 * WHAT'S NEW IN v2
 * ----------------
 *  1. Holds a fixed NO_FIX_MPH while acquiring GPS lock. An earlier build swept
 *     the needle 0->30 to show it was still searching, but PerfectPass treats
 *     that as real speed and chases it with throttle, which upsets RPM at idle.
 *     A steady low value keeps the dash honest without provoking the PID.
 *  2. SELF-TEST mode that steps through known speeds (30 s each) so you can
 *     verify the dash reads each one correctly, on the bench or on the water.
 *  3. Software lag compensation (predictor) + 10 Hz GPS, to fight the
 *     PerfectPass oscillation. See the PERFECTPASS LAG note below.
 */

#include <TinyGPS++.h>

/* ======================= CONFIG: pick your setup ========================= */

// --- Run mode: what drives the commanded speed ---------------------------
#define MODE_GPS       0   // Real use: speed comes from GPS (holds NO_FIX_MPH until lock)
#define MODE_SERIAL    1   // Bench: type a speed into the serial monitor + Enter
#define MODE_SELFTEST  2   // Verify: auto-step through TEST_SPEEDS, 30 s each
#define RUN_MODE       MODE_GPS

// --- GPS transport (only matters in MODE_GPS) ----------------------------
//   0 = hardware Serial (high speed, NO serial monitor)
//   1 = SoftwareSerial on pins 4/3 (frees the monitor, but <~18 MPH)
// Hardware Serial means the GPS must be on the Uno UART pins:
//   GPS TX -> Arduino D0/RX, GPS RX -> Arduino D1/TX, common GND.
// If the GPS is still plugged into the SoftwareSerial header, this will
// compile and run, but TinyGPS++ will see zero bytes and the dash will sit at NO_FIX_MPH.
#define GPS_USE_SOFTSERIAL 1

// --- One-shot factory reset of GPS config ---------------------------------
// The GT-U7's battery-backed RAM preserves UBX config across power cycles.
// FULL NUKE: clears ALL config sections across every permanent device
// (BBR + Flash + EEPROM + SpiFlash) and reloads defaults, WITHOUT re-saving
// the current (broken) RAM state. Wipes almanac/ephemeris too -> next first
// fix is a cold start (~30 s with sky view). Flip back to 0 once chars > 0
// is restored, so subsequent boots don't keep cold-starting.
#define GPS_FACTORY_RESET 0

// --- Send a config burst at boot to run the GPS at 10 Hz, RMC-only -------
// 1 Hz GPS is a big chunk of the PerfectPass lag. 10 Hz cuts latency ~10x.
// At 10 Hz we also trim to RMC-only so the NEO-6M doesn't drop sentences.
// KEEP OFF until the plotter shows valid GPS speed at 1 Hz first; turn on
// only after that known-working baseline is confirmed.
#define GPS_CONFIG_10HZ 0

/* ======================= CONFIG: tuning knobs ============================ */

// Pulse calibration: output frequency (Hz) per 1 MPH commanded.
// Tuned on the boat. (MSWorking 4.75 -> v1 4.22 -> v2 4.38 via self-test
// calibration sweep, gives +/-0.1 MPH across 10-30 MPH.)
const float frequencyMile = 4.38;

int mosfetPin = 6; // Digital pin driving the MOSFET / paddlewheel line

// --- Lock acquisition (MODE_GPS, before we have a fix) -------------------
// Reported speed while we have no fix. Held steady on purpose: PerfectPass
// reads whatever we report as real boat speed, so anything that moves gets
// chased by its throttle PID. Keep this low and constant.
const float        NO_FIX_MPH      = 5.0;    // steady value shown until lock
const unsigned long FIX_TIMEOUT_MS  = 3000;  // no fresh fix for this long => NO_FIX_MPH

// Built-in LED status for MODE_GPS, useful when hardware Serial owns the UART:
//   solid on  = valid speed/fix is driving the output
//   2 blinks  = GPS bytes are arriving, but no valid fix/speed yet
//   1 blink   = no GPS bytes are arriving (wiring / connector / baud)
#define ENABLE_STATUS_LED 1
const int STATUS_LED_PIN = LED_BUILTIN;

// --- Self-test sequence (MODE_SELFTEST) ----------------------------------
// Wakeboard/ski range. Edit freely. Each value is held for TEST_STEP_MS,
// OR tap any character in the serial monitor to advance immediately.
const float        TEST_SPEEDS[]   = {0, 10, 15, 20, 22, 24, 26, 30};
const unsigned long TEST_STEP_MS    = 15000; // 15 s per step (tap to skip)

// --- Software lag compensation (the SW lever against oscillation) --------
// PERFECTPASS LAG note:
//   PerfectPass is the PID. We only REPORT speed to it. If our reported speed
//   lags reality, PerfectPass corrects late -> overshoot -> the hunting you
//   feel. We can't tune its PID, but we can stop feeding it stale data:
//     (a) 10 Hz GPS (above) removes most of the fixed latency.
//     (b) The predictor below estimates acceleration from successive GPS
//         samples and projects speed forward by SPEED_LEAD_SECONDS, so we
//         report where the boat is ABOUT to be, cancelling pipeline delay.
//   GPS-derived acceleration is noisy, so SPEED_SMOOTH_ALPHA filters it.
//   "Do I need an accelerometer?" -> (a)+(b) get you most of the way and are
//   free. A real accelerometer gives clean, high-rate acceleration for a much
//   stronger lead term, which is the robust fix if oscillation persists after
//   tuning these. Try SW first; reach for the IMU only if it's not enough.
#define ENABLE_PREDICTOR 1
const float SPEED_LEAD_SECONDS = 0.30; // how far ahead to project (s). 0 = off
const float SPEED_SMOOTH_ALPHA = 0.40; // 0..1 EMA on accel; lower = smoother

/* ========================================================================= */
/* Below here is wiring; you normally only touch the config blocks above.    */
/* ========================================================================= */

TinyGPSPlus gps;

#if GPS_USE_SOFTSERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial ss(4, 3); // RX, TX
  Stream& gpsPort = ss;
#else
  Stream& gpsPort = Serial;
#endif

// Serial monitor is only usable when the GPS isn't squatting on hardware UART.
#if (RUN_MODE == MODE_GPS) && (GPS_USE_SOFTSERIAL == 0)
  #define SERIAL_FREE 0
#else
  #define SERIAL_FREE 1
#endif

float speedGlobal = 0.0;            // Final commanded speed sent to the pulse gen
unsigned long nextMosfetPulse = 0;  // Next scheduled pin toggle (micros)

String inputString = "";            // MODE_SERIAL line buffer

// Predictor / lock-tracking state
unsigned long lastFixMs = 0;        // millis() of last valid GPS location update
unsigned long lastGpsByteMs = 0;    // millis() of last received byte from GPS
bool   everHadFix = false;          // true once we've seen any valid fix
float  prevRawMph = 0.0;
unsigned long prevRawMs = 0;
float  accelEma = 0.0;              // smoothed acceleration estimate (mph/s)

void setup() {
  pinMode(mosfetPin, OUTPUT);
#if (RUN_MODE == MODE_GPS) && ENABLE_STATUS_LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
#endif

  Serial.begin(9600);   // GPS link AND/OR monitor, depending on mode

#if GPS_USE_SOFTSERIAL
  ss.begin(9600);
#endif

#if (RUN_MODE == MODE_GPS) && GPS_FACTORY_RESET
  factoryResetGps();   // clear any custom config persisted in BBR
#endif

#if (RUN_MODE == MODE_GPS) && GPS_CONFIG_10HZ
  configureGps10Hz();
#endif

#if SERIAL_FREE
  Serial.println(F("\nGPSSpeed v2 ready"));
  #if RUN_MODE == MODE_SELFTEST
    Serial.println(F("MODE: SELF-TEST (stepping speeds, 30s each)"));
  #elif RUN_MODE == MODE_SERIAL
    Serial.println(F("MODE: SERIAL (type an MPH value + Enter)"));
  #else
    Serial.println(F("MODE: GPS (holding NO_FIX_MPH until lock)"));
  #endif
#endif
}

void loop() {
  unsigned long nowMicros = micros();

  updateCommandedSpeed();

  if (nowMicros >= nextMosfetPulse) {
    modulateOutput(speedGlobal);
  }
}

// Decide what speed we should be commanding right now, based on the mode.
void updateCommandedSpeed() {
#if RUN_MODE == MODE_SERIAL
  updateSpeedSerial();
#elif RUN_MODE == MODE_SELFTEST
  updateSpeedSelfTest();
#else
  updateSpeedGPS();
#endif
}

// ---- MODE_SERIAL: read a speed typed into the monitor -------------------
void updateSpeedSerial() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      speedGlobal = inputString.toFloat();
      Serial.print(F("Speed set to: "));
      Serial.println(speedGlobal);
      inputString = "";
    } else if (inChar != '\r') {
      inputString += inChar;
    }
  }
}

// ---- MODE_SELFTEST: step through TEST_SPEEDS; auto every TEST_STEP_MS,
//      or immediately when any character arrives on Serial. ---------------
void updateSpeedSelfTest() {
  const int count = sizeof(TEST_SPEEDS) / sizeof(TEST_SPEEDS[0]);
  static int idx = -1;             // -1 = uninitialized; first call starts step 0
  static unsigned long stepStartMs = 0;

  // Drain anything the user typed; any char triggers an advance.
  bool tapAdvance = false;
  while (Serial.available()) {
    Serial.read();
    tapAdvance = true;
  }

  bool timeAdvance = (idx >= 0) && (millis() - stepStartMs >= TEST_STEP_MS);
  if (idx < 0 || timeAdvance || tapAdvance) {
    idx = (idx + 1) % count;
    stepStartMs = millis();
    speedGlobal = TEST_SPEEDS[idx];
#if SERIAL_FREE
    Serial.print(F("[self-test] step "));
    Serial.print(idx + 1);
    Serial.print(F("/"));
    Serial.print(count);
    Serial.print(F(" -> "));
    Serial.print(speedGlobal);
    Serial.println(F(" MPH (tap any key to skip)"));
#endif
  }
}

// ---- MODE_GPS: real speed once locked, otherwise hold NO_FIX_MPH --------
void updateSpeedGPS() {
  // Pump every available GPS byte through the parser.
  while (gpsPort.available() > 0) {
    gps.encode(gpsPort.read());
    lastGpsByteMs = millis();

    // TinyGPS++ quirk seen on earlier builds: touch lat/lng before speed.
    bool locationUpdated = gps.location.isUpdated();
    if (locationUpdated) {
      gps.location.lat();
      gps.location.lng();
    }

    // RMC-only 10 Hz output updates speed directly, so key off speed updates
    // instead of requiring a fresh location update on the same pass.
    if ((gps.speed.isUpdated() || locationUpdated) &&
        gps.speed.isValid() &&
        gps.location.isValid()) {
      lastFixMs = millis();
      everHadFix = true;

      float rawMph = gps.speed.mph();
      speedGlobal = conditionSpeed(rawMph);
    }
  }

  bool haveFix = everHadFix && (millis() - lastFixMs < FIX_TIMEOUT_MS);

  // No fresh fix? Hold a steady low value. Don't move the needle around --
  // PerfectPass would read the motion as real acceleration and hunt throttle.
  if (!haveFix) {
    speedGlobal = NO_FIX_MPH;
  }

#if ENABLE_STATUS_LED
  updateGpsStatusLed(haveFix);
#endif

#if SERIAL_FREE
  // Heartbeat once a second. Format is label:value pairs so the Arduino IDE
  // Serial Plotter can graph each channel; Serial Monitor still reads fine.
  //   chars=0           -> no bytes reaching the parser (wiring / port / baud)
  //   chars>0, sats=0   -> GPS is talking, hasn't computed a fix yet
  //   fix=1             -> we're tracking real speed (raw + out are valid)
  static unsigned long lastDbgMs = 0;
  if (millis() - lastDbgMs >= 1000) {
    lastDbgMs = millis();
    Serial.print(F("sats:"));
    Serial.print(gps.satellites.value());
    Serial.print(F(" chars:"));
    Serial.print(gps.charsProcessed());
    Serial.print(F(" fix:"));
    Serial.print(haveFix ? 1 : 0);
    Serial.print(F(" age:"));
    Serial.print(lastFixMs == 0 ? 9999 : millis() - lastFixMs);
    Serial.print(F(" raw:"));
    Serial.print(gps.speed.mph());
    Serial.print(F(" out:"));
    Serial.println(speedGlobal);
  }
#endif
}

#if (RUN_MODE == MODE_GPS) && ENABLE_STATUS_LED
void updateGpsStatusLed(bool haveFix) {
  if (haveFix) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    return;
  }

  bool haveRecentBytes = (lastGpsByteMs != 0) && (millis() - lastGpsByteMs < FIX_TIMEOUT_MS);
  byte blinkCount = haveRecentBytes ? 2 : 1;
  unsigned long phase = millis() % 1600;
  bool ledOn = false;

  for (byte i = 0; i < blinkCount; i++) {
    unsigned long blinkStart = i * 250UL;
    if (phase >= blinkStart && phase < blinkStart + 100UL) {
      ledOn = true;
    }
  }

  digitalWrite(STATUS_LED_PIN, ledOn ? HIGH : LOW);
}
#endif

// Lag compensation: project speed forward using GPS-derived acceleration.
float conditionSpeed(float rawMph) {
#if ENABLE_PREDICTOR
  unsigned long now = millis();
  if (prevRawMs != 0) {
    float dt = (now - prevRawMs) / 1000.0;
    if (dt > 0.0) {
      float accelRaw = (rawMph - prevRawMph) / dt;          // mph per second
      accelEma += SPEED_SMOOTH_ALPHA * (accelRaw - accelEma); // tame GPS noise
    }
  }
  prevRawMph = rawMph;
  prevRawMs  = now;

  float predicted = rawMph + accelEma * SPEED_LEAD_SECONDS;
  if (predicted < 0.0) predicted = 0.0;
  return predicted;
#else
  return rawMph;
#endif
}

// Toggle the MOSFET pin to make a square wave whose frequency encodes speed.
void modulateOutput(float speed) {
  if (speed < 0.1) speed = 0.1;
  if (speed > 60)  speed = 60;

  float frequency = speed * frequencyMile; // Hz

  // Half period for a 50% duty cycle.
  unsigned long delayTimeMicros = (unsigned long)(1000000.0 / (2 * frequency));

  // Quantize to a multiple of 128 us (keeps timing tidy on the Uno).
  delayTimeMicros = (delayTimeMicros / 128) * 128;

  if (delayTimeMicros < 1)      delayTimeMicros = 1;
  if (delayTimeMicros > 500000) delayTimeMicros = 500000;

  static bool mosfetState = LOW;
  mosfetState = !mosfetState;
  digitalWrite(mosfetPin, mosfetState);
  nextMosfetPulse = micros() + delayTimeMicros;
}

#if (RUN_MODE == MODE_GPS) && (GPS_CONFIG_10HZ || GPS_FACTORY_RESET)
// Frame a UBX packet (sync + body + Fletcher checksum) and write it to the GPS.
// body = {class, id, len_lo, len_hi, payload...}
void sendUBX(uint8_t* body, uint8_t len) {
  uint8_t ckA = 0, ckB = 0;
  for (uint8_t i = 0; i < len; i++) { ckA += body[i]; ckB += ckA; }
  gpsPort.write(0xB5);
  gpsPort.write(0x62);
  gpsPort.write(body, len);
  gpsPort.write(ckA);
  gpsPort.write(ckB);
}
#endif

#if (RUN_MODE == MODE_GPS) && GPS_FACTORY_RESET
// UBX-CFG-CFG: full-nuke revert. Clears every config section in every
// permanent device, loads defaults into RAM, does NOT save current RAM
// (so we can't accidentally re-persist the broken state). Also wipes
// warm-start data (almanac/ephemeris) -> first fix after this is cold.
//   clearMask  = 0x0000FFFF  (all sections)
//   saveMask   = 0x00000000  (don't save current RAM)
//   loadMask   = 0x0000FFFF  (load defaults into RAM)
//   deviceMask = 0x17        (BBR | Flash | EEPROM | SpiFlash)
void factoryResetGps() {
  uint8_t cfgReset[] = {
    0x06, 0x09, 0x0D, 0x00,
    0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0x00, 0x00,
    0x17
  };
  sendUBX(cfgReset, sizeof(cfgReset));
  delay(1500); // give the module time to apply + persist + resume NMEA
}
#endif

#if (RUN_MODE == MODE_GPS) && GPS_CONFIG_10HZ
// Send UBX commands to the NEO-6M: 10 Hz nav rate + RMC-only NMEA output.
void configureGps10Hz() {
  // CFG-RATE: measRate = 100 ms (=10 Hz), navRate 1, timeRef GPS.
  uint8_t cfgRate[] = {0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00};
  sendUBX(cfgRate, sizeof(cfgRate));

  // CFG-MSG (NMEA class 0xF0): keep RMC, silence the rest to avoid drops @10Hz.
  setNmeaRate(0x00, 0); // GGA off
  setNmeaRate(0x01, 0); // GLL off
  setNmeaRate(0x02, 0); // GSA off
  setNmeaRate(0x03, 0); // GSV off
  setNmeaRate(0x04, 1); // RMC on  (this is the one with speed)
  setNmeaRate(0x05, 0); // VTG off
}

void setNmeaRate(uint8_t msgId, uint8_t rate) {
  uint8_t msg[] = {0x06, 0x01, 0x03, 0x00, 0xF0, msgId, rate};
  sendUBX(msg, sizeof(msg));
}
#endif
