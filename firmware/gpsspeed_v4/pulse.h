// Paddlewheel pulse generator: hardware LEDC, clocked from the 40 MHz crystal.
//
// This is the whole reason v4 exists. The Uno toggled the pin in software,
// so anything else that grabbed the CPU (SoftwareSerial, mostly) stretched
// the pulses and the dash jumped around past ~21 MPH. Here the LEDC
// peripheral generates the square wave on its own. GPS parsing, WiFi, the
// web server -- none of it can touch the timing.
//
// Frequency is set through the timer's fractional divider directly, because
// ledc_set_freq() only accepts whole Hz (1 Hz ~ 0.23 MPH -- too coarse).
//
//   f = XTAL / (div / 256) / 2^14   ->   div = 40e6 * 256 / (f * 16384)
//
// div is 18 bits with 8 fractional, so f spans 2.38 Hz .. 2441 Hz and the
// error at 30 MPH is ~0.01%.
#pragma once
#include <driver/ledc.h>
#include <hal/ledc_ll.h>
#include <soc/ledc_struct.h>
#include "config.h"
#include "settings.h"

#define PULSE_MODE     LEDC_LOW_SPEED_MODE
#define PULSE_TIMER    LEDC_TIMER_0
#define PULSE_CHANNEL  LEDC_CHANNEL_0
#define PULSE_RES_BITS 14
#define PULSE_XTAL_HZ  40000000.0f
#define PULSE_DIV_MIN  256UL          // divider of 1.0
#define PULSE_DIV_MAX  ((1UL << 18) - 1)

static float    pulseHz = 0;          // what we are actually outputting (0 = idle)
static float    pulseMph = 0;         // commanded speed that produced it
static bool     pulseRunning = false;

// Correction factor at a given speed, linearly interpolated from the
// calibration table (flat beyond its ends). 1.0 with no table.
static float calFactor(float mph) {
  if (cfg.calCount == 0) return 1.0f;
  if (mph <= cfg.cal[0].mph) return cfg.cal[0].factor;
  for (int i = 1; i < cfg.calCount; i++) {
    if (mph <= cfg.cal[i].mph) {
      const CalPoint &a = cfg.cal[i - 1], &b = cfg.cal[i];
      float t = (mph - a.mph) / (b.mph - a.mph);
      return a.factor + t * (b.factor - a.factor);
    }
  }
  return cfg.cal[cfg.calCount - 1].factor;
}

static void calSort() {
  for (int i = 1; i < cfg.calCount; i++)
    for (int j = i; j > 0 && cfg.cal[j].mph < cfg.cal[j - 1].mph; j--) {
      CalPoint t = cfg.cal[j]; cfg.cal[j] = cfg.cal[j - 1]; cfg.cal[j - 1] = t;
    }
}

// Record a calibration point: at commanded `mph` the dash showed `dashMph`.
// Scaling the output by mph/dashMph makes the dash read `mph` there.
static bool calAddPoint(float mph, float dashMph) {
  if (mph < 2.0f || dashMph < 0.5f) return false;
  float f = calFactor(mph) * (mph / dashMph);
  if (f < 0.5f || f > 2.0f) return false;          // way off => refuse
  for (int i = 0; i < cfg.calCount; i++) {
    if (fabsf(cfg.cal[i].mph - mph) < 0.25f) { cfg.cal[i].factor = f; return true; }
  }
  if (cfg.calCount >= CAL_POINTS) return false;
  cfg.cal[cfg.calCount++] = {mph, f};
  calSort();
  return true;
}

static void pulseInit() {
  ledc_timer_config_t t = {};
  t.speed_mode = PULSE_MODE;
  t.duty_resolution = (ledc_timer_bit_t)PULSE_RES_BITS;
  t.timer_num = PULSE_TIMER;
  t.freq_hz = 22;                    // placeholder; replaced via the divider
  t.clk_cfg = LEDC_USE_XTAL_CLK;     // crystal, not the +-5% RC oscillator
  ESP_ERROR_CHECK(ledc_timer_config(&t));

  ledc_channel_config_t c = {};
  c.gpio_num = PIN_PULSE;
  c.speed_mode = PULSE_MODE;
  c.channel = PULSE_CHANNEL;
  c.timer_sel = PULSE_TIMER;
  c.duty = 0;                        // start idle: MOSFET off, line released
  c.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&c));
  pulseRunning = false;
}

static void pulseStop() {
  ledc_stop(PULSE_MODE, PULSE_CHANNEL, 0);   // idle LOW = MOSFET off
  pulseRunning = false;
  pulseHz = 0;
}

static void pulseSetHz(float hz) {
  float divf = PULSE_XTAL_HZ * 256.0f / (hz * (float)(1UL << PULSE_RES_BITS));
  uint32_t div = (uint32_t)(divf + 0.5f);
  if (div < PULSE_DIV_MIN) div = PULSE_DIV_MIN;
  if (div > PULSE_DIV_MAX) { pulseStop(); return; }   // below 2.38 Hz: idle
  ledc_ll_set_clock_divider(&LEDC, PULSE_MODE, PULSE_TIMER, div);
  ledc_ll_ls_timer_update(&LEDC, PULSE_MODE, PULSE_TIMER);
  if (!pulseRunning) {
    ledc_set_duty(PULSE_MODE, PULSE_CHANNEL, 1UL << (PULSE_RES_BITS - 1));   // 50%
    ledc_update_duty(PULSE_MODE, PULSE_CHANNEL);
    pulseRunning = true;
  }
  pulseHz = PULSE_XTAL_HZ * 256.0f / ((float)div * (float)(1UL << PULSE_RES_BITS));
}

// The one entry point the rest of the firmware uses.
static void pulseSetMph(float mph) {
  if (mph > MAX_MPH) mph = MAX_MPH;
  pulseMph = mph;
  if (mph < cfg.minMph) { pulseStop(); return; }
  pulseSetHz(mph * cfg.hzPerMph * calFactor(mph));
}
