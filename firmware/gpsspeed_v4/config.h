// GPSSpeed v4 -- pin map and defaults for the ESP32-C3 board (pcb/).
//
// The pin map must match pcb/gen/design.py (the GPIO dict). If you change
// one, change the other.
#pragma once

#define FW_VERSION "4.0.8"   // 4.0.8: tidier open/tips card; 4.0.7: Open-in-browser; 4.0.6: open WiFi; 4.0.5: pop-up app + Done; 4.0.4: stable SSID; 4.0.3: gpsspeed.local

// ---------------------------------------------------------------- pins ----
#define PIN_GPS_RX      20   // ESP RX  <- GPS TXD
#define PIN_GPS_TX      21   // ESP TX  -> GPS RXD
#define PIN_GPS_NRST     0   // GPS reset, active low (left as input = released)
#define PIN_GPS_PPS      1   // GPS 1PPS, rising edge on the UTC second
#define PIN_VSENSE       3   // boat battery through 220k/10k divider (x23), after D1
#define PIN_LED_FIX      4   // green, "GPS"
#define PIN_LED_WIFI     5   // green, "WIFI"
#define PIN_PULSE       10   // -> 100R -> Q1 (ZXMS6004FF) input. HIGH = line pulled low.
#define PIN_BOOT         9   // BOOT button (also the strapping pin)

#define VSENSE_RATIO  23.0f  // (220k + 10k) / 10k  (board rev 1.2; rev 1.1 was 16)
// The divider is tapped after the SS310 reverse-polarity diode, so on boat
// power it reads one diode drop low. Added back when running above USB levels.
#define VSENSE_DIODE_V 0.35f

// -------------------------------------------------------------- defaults --
// All of these are editable from the web UI and persist in NVS.

// Output frequency per commanded MPH.
//
// The Uno builds used 4.38, but that number was tuned against a pulse
// generator that FLOORED every half-period to a multiple of 128 us. That
// rounding made the Uno's real output run ~1-2% fast, so its *effective*
// constant across 15-26 MPH was ~4.42 Hz/MPH. v4 generates the frequency
// exactly (hardware LEDC off the 40 MHz crystal), so to make the dash read
// what it read before, the default starts at the Uno's effective value.
// Run the self-test once on the boat and fine-tune from the web UI.
#define DEF_HZ_PER_MPH     4.42f

#define DEF_NO_FIX_MPH     5.0f    // steady value until GPS lock (never swept)
#define DEF_MIN_MPH        0.6f    // below this we stop pulsing (dash shows 0)
#define DEF_LEAD_SEC       0.30f   // predictor look-ahead
#define DEF_ACCEL_ALPHA    0.40f   // predictor smoothing (0..1)
#define DEF_GPS_RATE_HZ    10
#define DEF_BOOT_WINDOW_S  120     // WiFi AP stays up this long after power-on
#define DEF_AP_PASS        ""          // open network; set one from the page if wanted

#define FIX_TIMEOUT_MS     3000    // no fresh fix this long => NO_FIX_MPH
#define CLIENT_GRACE_MS    60000   // keep WiFi up this long after last client leaves
#define MAX_MPH            60.0f
#define CAL_POINTS         8

// Self-test sequence (MPH). Each step holds for SELFTEST_STEP_MS unless you
// advance it from the web UI / serial.
static const float SELFTEST_SPEEDS[] = {0, 10, 15, 20, 22, 24, 26, 30};
#define SELFTEST_STEP_MS   15000
