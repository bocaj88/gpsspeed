/**
 * @file v3.ino
 * @author Jake Michalski
 * @brief GPS -> paddlewheel pulse emulator for PerfectPass, hardware-timed.
 * @version 3.0
 * @date 2026-09-26
 *
 * @copyright Copyright MIT License (c) 2025-2026
 *
 * ###########################################################################
 * #  !!! HARDWARE CHANGE REQUIRED -- THIS SKETCH WILL NOT WORK ON THE OLD  !!!
 * #  !!! WIRING. THE MOSFET MUST MOVE FROM PIN 6 TO PIN 9.                 !!!
 * ###########################################################################
 *
 * WHAT TO DO
 * ----------
 * The hat doesn't break out pin 9, so this is a single external jumper:
 *
 *      MOSFET GATE  ---->  Arduino PIN 9      (was pin 6)
 *
 * Run one wire from the MOSFET gate (or the hat pad that feeds it) to the
 * pin 9 header on the Uno. That's the whole modification.
 *
 * ---------------------------------------------------------------------------
 * SAFETY: YOU CAN LEAVE THE OLD PIN 6 WIRE CONNECTED.
 * ---------------------------------------------------------------------------
 * The danger with a jumper like this is having TWO OUTPUTS DRIVING ONE GATE.
 * If the hat still ties the gate to pin 6, and the sketch also drove pin 6,
 * then any time pin 6 and pin 9 disagreed you'd have one pin sourcing current
 * straight into another pin sinking it -- a dead short through two output
 * drivers. That is exactly the class of fault that has already cost this
 * project a GPS module and an Uno's USB chip.
 *
 * So this sketch NEVER drives pin 6. It explicitly configures pin 6 as a
 * high-impedance INPUT at startup (see PIN6_LEGACY below), which means it
 * cannot contend with the pin 9 jumper no matter what the hat is wired to.
 *
 *   -> Leaving the old pin 6 wire in place is SAFE with this firmware.
 *   -> Lifting it is tidier, but NOT required.
 *   -> If you ever port this back to a sketch that drives pin 6, lift the
 *      jumper FIRST. Do not run both.
 *
 * WHY PIN 9 SPECIFICALLY
 * ----------------------
 * Pin 9 is OC1A -- the hardware output of Timer1. It's not a preference, it's
 * silicon: Timer1 can only drive pin 9 (OC1A) or pin 10 (OC1B). Pin 6 is OC0A,
 * which belongs to Timer0, and Timer0 is what runs millis()/micros()/delay().
 * Taking Timer0 would break every timing call in this file.
 *
 * WHAT THIS FIXES (the 21 MPH cliff)
 * ----------------------------------
 * v1/v2 generated the square wave by polling micros() in loop() and flipping
 * the pin in software. That works until something else hogs the CPU --
 * and SoftwareSerial does exactly that, because it bit-bangs bytes with
 * interrupts DISABLED. Past ~21 MPH the toggles landed late, pulses came out
 * too wide, and the dash jumped to ~30 MPH.
 *
 * v3 hands the square wave to Timer1 in CTC toggle mode. The timer flips
 * pin 9 in hardware, with zero CPU involvement. SoftwareSerial can disable
 * interrupts all it likes -- the peripheral doesn't care. The speed ceiling
 * is gone, and it's gone structurally, not by tuning.
 *
 * The happy side effect: because pulse timing no longer competes with the
 * GPS, you can run the GPS on SoftwareSerial AND keep the USB serial monitor
 * AND hold accurate pulses at any speed -- all three at once, which was never
 * possible before. The remaining tradeoff is only about GPS update RATE:
 *
 *   SoftwareSerial + 1 Hz GPS  -> monitor works, pulses perfect.       SAFE
 *   SoftwareSerial + 10 Hz GPS -> ~75-85% of a bit-banged 9600 link.   RISKY
 *                                 Expect dropped sentences.
 *   Hardware Serial + 10 Hz    -> best data, no monitor (LED instead). BEST DATA
 *
 * Everything else (modes, predictor, status LED, calibration) carries over
 * from v2 unchanged. frequencyMile is still 4.38 so your dash calibration
 * transfers directly.
 */

#include <TinyGPS++.h>

/* ======================= CONFIG: pick your setup ========================= */

// --- Run mode: what drives the commanded speed ---------------------------
#define MODE_GPS       0   // Real use: speed comes from GPS
#define MODE_SERIAL    1   // Bench: type a speed into the serial monitor + Enter
#define MODE_SELFTEST  2   // Verify: auto-step through TEST_SPEEDS
#define RUN_MODE       MODE_GPS

// --- GPS transport (only matters in MODE_GPS) ----------------------------
//   0 = hardware Serial (best data rate, NO serial monitor)
//   1 = SoftwareSerial on pins 4/3 (keeps the monitor; see the rate note above)
// Unlike v1/v2, this choice NO LONGER affects pulse accuracy.
#define GPS_USE_SOFTSERIAL 1

// --- One-shot factory reset of GPS config ---------------------------------
// The GT-U7's battery-backed RAM preserves UBX config across power cycles.
// Set to 1 once to wipe a bad config, then back to 0.
#define GPS_FACTORY_RESET 0

// --- Send a config burst at boot to run the GPS at 10 Hz, RMC-only -------
// Cuts GPS latency ~10x, which is half the answer to PerfectPass hunting.
// Leave OFF while on SoftwareSerial unless you've confirmed it keeps up.
#define GPS_CONFIG_10HZ 0

/* ======================= CONFIG: tuning knobs ============================ */

// Pulse calibration: output frequency (Hz) per 1 MPH commanded.
// Tuned on the boat. Theoretical for a 20,000 pulse/nm paddlewheel is
// 4.83 Hz/MPH; the gap is the boat's own calibration absorbing paddlewheel
// slip, so keep the measured value rather than the textbook one.
const float frequencyMile = 4.38;

// Pulse output pin. MUST be 9 (OC1A) or 10 (OC1B) -- Timer1's only outputs.
const int PULSE_PIN = 9;

// The v1/v2 output pin. This sketch must NEVER drive it -- see the SAFETY
// block in the header. If the hat still wires the gate to pin 6 and we drove
// it too, pin 6 and pin 9 would fight over the same gate and short their
// output drivers together. Held as a high-impedance INPUT instead, so the old
// wire can stay connected harmlessly.
// Set to 0 ONLY if you have physically lifted the pin 6 wire.
#define PIN6_LEGACY 1
const int LEGACY_PULSE_PIN = 6;

// --- Lock acquisition (MODE_GPS, before we have a fix) -------------------
// Reported speed while we have no fix. Held steady on purpose: PerfectPass
// reads whatever we report as real boat speed, so anything that moves gets
// chased by its throttle PID. (An earlier build swept 0->30 here and it
// upset RPM at idle.)
const float        NO_FIX_MPH      = 5.0;
const unsigned long FIX_TIMEOUT_MS  = 3000;  // no fresh fix this long => NO_FIX_MPH

// Built-in LED status for MODE_GPS, useful when hardware Serial owns the UART:
//   solid on  = valid speed/fix is driving the output
//   2 blinks  = GPS bytes are arriving, but no valid fix/speed yet
//   1 blink   = no GPS bytes are arriving (wiring / connector / baud)
#define ENABLE_STATUS_LED 1
const int STATUS_LED_PIN = LED_BUILTIN;

// --- Self-test sequence (MODE_SELFTEST) ----------------------------------
const float        TEST_SPEEDS[]   = {0, 10, 15, 20, 22, 24, 26, 30};
const unsigned long TEST_STEP_MS    = 15000; // tap any key to skip

// --- Software lag compensation -------------------------------------------
#define ENABLE_PREDICTOR 1
const float SPEED_LEAD_SECONDS = 0.30; // how far ahead to project (s). 0 = off
const float SPEED_SMOOTH_ALPHA = 0.40; // 0..1 EMA on accel; lower = smoother

// Clamp range for the commanded speed.
const float MIN_SPEED_MPH = 0.1;
const float MAX_SPEED_MPH = 60.0;

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

float speedGlobal = 0.0;        // Final commanded speed
float lastAppliedSpeed = -1.0;  // Last speed actually programmed into Timer1

String inputString = "";        // MODE_SERIAL line buffer

// Predictor / lock-tracking state
unsigned long lastFixMs = 0;
unsigned long lastGpsByteMs = 0;
bool   everHadFix = false;
float  prevRawMph = 0.0;
unsigned long prevRawMs = 0;
float  accelEma = 0.0;

void setup() {
  setupPulseTimer();

#if PIN6_LEGACY
  // SAFETY -- do not change this to OUTPUT. Pin 6 is the old pulse pin and may
  // still be wired to the MOSFET gate on the hat. Driving it while pin 9 also
  // drives that gate would short two output drivers together whenever they
  // disagree. INPUT = high-impedance = the old wire is harmless.
  pinMode(LEGACY_PULSE_PIN, INPUT);
#endif

#if (RUN_MODE == MODE_GPS) && ENABLE_STATUS_LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
#endif

  Serial.begin(9600);

#if GPS_USE_SOFTSERIAL
  ss.begin(9600);
#endif

#if (RUN_MODE == MODE_GPS) && GPS_FACTORY_RESET
  factoryResetGps();
#endif

#if (RUN_MODE == MODE_GPS) && GPS_CONFIG_10HZ
  configureGps10Hz();
#endif

#if SERIAL_FREE
  Serial.println(F("\nGPSSpeed v3 ready (Timer1 hardware pulse on pin 9)"));
  #if RUN_MODE == MODE_SELFTEST
    Serial.println(F("MODE: SELF-TEST (stepping speeds)"));
  #elif RUN_MODE == MODE_SERIAL
    Serial.println(F("MODE: SERIAL (type an MPH value + Enter)"));
  #else
    Serial.println(F("MODE: GPS (holding NO_FIX_MPH until lock)"));
  #endif
#endif
}

void loop() {
  updateCommandedSpeed();

  // Only reprogram the timer when the speed actually moved. Rewriting OCR1A
  // thousands of times a second would be pointless work and would nudge the
  // counter around; the wave keeps running untouched in between.
  if (fabs(speedGlobal - lastAppliedSpeed) > 0.01) {
    applySpeed(speedGlobal);
    lastAppliedSpeed = speedGlobal;
  }
}

/* ------------------------- Timer1 pulse generator ------------------------
 * CTC mode with OC1A toggling on compare match. Output frequency is
 *
 *      f = F_CPU / (2 * N * (1 + OCR1A))
 *
 * so the value we need is
 *
 *      OCR1A = F_CPU / (2 * N * f) - 1
 *
 * N (the prescaler) is picked as the SMALLEST one that keeps OCR1A inside
 * 16 bits, because a bigger OCR1A means finer frequency resolution. At 30 MPH
 * that lands around OCR1A = 60881 with no prescaler -- roughly 0.002% steps,
 * far finer than the calibration itself.
 *
 * Once set, the timer drives pin 9 by itself. No ISR, no CPU, nothing for
 * SoftwareSerial to interfere with. That is the entire point of v3.
 * ------------------------------------------------------------------------ */

const uint16_t PRESCALER_VALUE[] = {1, 8, 64, 256, 1024};
const uint8_t  PRESCALER_BITS[]  = {
  _BV(CS10),                 // /1
  _BV(CS11),                 // /8
  _BV(CS11) | _BV(CS10),     // /64
  _BV(CS12),                 // /256
  _BV(CS12) | _BV(CS10)      // /1024
};
const uint8_t PRESCALER_MASK = _BV(CS12) | _BV(CS11) | _BV(CS10);

void setupPulseTimer() {
  pinMode(PULSE_PIN, OUTPUT);

  noInterrupts();
  TCCR1A = _BV(COM1A0);   // toggle OC1A on compare match
  TCCR1B = _BV(WGM12);    // CTC, TOP = OCR1A. No clock bits yet = stopped.
  TCNT1  = 0;
  OCR1A  = 0xFFFF;
  interrupts();
}

// Convert a commanded speed (MPH) into a Timer1 frequency and program it.
void applySpeed(float speedMph) {
  if (speedMph < MIN_SPEED_MPH) speedMph = MIN_SPEED_MPH;
  if (speedMph > MAX_SPEED_MPH) speedMph = MAX_SPEED_MPH;

  setPulseFrequency(speedMph * frequencyMile);
}

void setPulseFrequency(float freqHz) {
  if (freqHz <= 0.0) return;

  for (uint8_t i = 0; i < 5; i++) {
    float divisor = (float)F_CPU / (2.0 * (float)PRESCALER_VALUE[i] * freqHz);
    uint32_t ticks = (uint32_t)(divisor + 0.5);

    if (ticks >= 2 && ticks <= 65536UL) {
      uint16_t top = (uint16_t)(ticks - 1);

      noInterrupts();
      OCR1A = top;
      // If the counter is already past the new TOP it would run all the way
      // to 0xFFFF before matching, giving one absurdly long pulse. Reset it.
      if (TCNT1 > top) TCNT1 = 0;
      TCCR1B = (TCCR1B & ~PRESCALER_MASK) | PRESCALER_BITS[i];
      interrupts();
      return;
    }
  }
  // Nothing fit (frequency absurdly low) -- stop the clock, hold the pin.
  noInterrupts();
  TCCR1B &= ~PRESCALER_MASK;
  interrupts();
}

/* ------------------------------ speed sources --------------------------- */

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

// ---- MODE_SELFTEST: step through TEST_SPEEDS ----------------------------
void updateSpeedSelfTest() {
  const int count = sizeof(TEST_SPEEDS) / sizeof(TEST_SPEEDS[0]);
  static int idx = -1;
  static unsigned long stepStartMs = 0;

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
    Serial.print(F(" MPH = "));
    Serial.print(speedGlobal * frequencyMile);
    Serial.println(F(" Hz (tap any key to skip)"));
#endif
  }
}

// ---- MODE_GPS: real speed once locked, otherwise hold NO_FIX_MPH --------
void updateSpeedGPS() {
  while (gpsPort.available() > 0) {
    gps.encode(gpsPort.read());
    lastGpsByteMs = millis();

    // TinyGPS++ quirk seen on earlier builds: touch lat/lng before speed.
    bool locationUpdated = gps.location.isUpdated();
    if (locationUpdated) {
      gps.location.lat();
      gps.location.lng();
    }

    if ((gps.speed.isUpdated() || locationUpdated) &&
        gps.speed.isValid() &&
        gps.location.isValid()) {
      lastFixMs = millis();
      everHadFix = true;
      speedGlobal = conditionSpeed(gps.speed.mph());
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
  // Heartbeat once a second. label:value pairs so the Serial Plotter can graph.
  //   chars=0           -> no bytes reaching the parser (wiring / port / baud)
  //   chars>0, sats=0   -> GPS is talking, hasn't computed a fix yet
  //   fix=1             -> tracking real speed
  //   hz                -> what the timer is actually being asked to output
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
    Serial.print(speedGlobal);
    Serial.print(F(" hz:"));
    Serial.println(speedGlobal * frequencyMile);
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
      float accelRaw = (rawMph - prevRawMph) / dt;
      accelEma += SPEED_SMOOTH_ALPHA * (accelRaw - accelEma);
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

/* ------------------------------ GPS config ------------------------------ */

#if (RUN_MODE == MODE_GPS) && (GPS_CONFIG_10HZ || GPS_FACTORY_RESET)
// Frame a UBX packet (sync + body + Fletcher checksum) and write it to the GPS.
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
// UBX-CFG-CFG: clear every config section in every permanent device and load
// defaults, without re-saving the current (broken) RAM state.
void factoryResetGps() {
  uint8_t cfgReset[] = {
    0x06, 0x09, 0x0D, 0x00,
    0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0x00, 0x00,
    0x17
  };
  sendUBX(cfgReset, sizeof(cfgReset));
  delay(1500);
}
#endif

#if (RUN_MODE == MODE_GPS) && GPS_CONFIG_10HZ
// 10 Hz nav rate + RMC-only NMEA output.
void configureGps10Hz() {
  uint8_t cfgRate[] = {0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00};
  sendUBX(cfgRate, sizeof(cfgRate));

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
