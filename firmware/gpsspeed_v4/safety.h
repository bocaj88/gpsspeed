// Crash-loop protection for OTA updates -- the board is meant to be potted,
// so a bad firmware must never brick it.
//
// The ESP32 keeps two app slots. OTA writes the new image to the idle slot
// and the Update library verifies its checksum before switching, so a
// corrupted upload can't be booted at all. This covers the remaining case:
// an image that is valid but crashes. If the app panics or trips a
// watchdog 3 times without ever reaching STABLE_MS of uptime, we switch the
// boot slot back to the previous firmware.
//
// Only crash resets count. Brownouts and power-on resets are ignored, which
// matters on a boat: cranking the engine can sag the supply and reset the
// board several times in a row, and that must NOT look like a bad update.
#pragma once
#include <esp_ota_ops.h>
#include <esp_system.h>

#define CRASH_MAGIC  0x47505334UL   // "GPS4"
#define CRASH_LIMIT  3
#define STABLE_MS    30000

RTC_NOINIT_ATTR static uint32_t crashMagic;
RTC_NOINIT_ATTR static uint32_t crashCount;
static bool markedStable = false;

static bool resetWasCrash(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

// Call first thing in setup().
static void safetyBoot() {
  esp_reset_reason_t why = esp_reset_reason();
  if (crashMagic != CRASH_MAGIC) { crashMagic = CRASH_MAGIC; crashCount = 0; }
  if (resetWasCrash(why)) crashCount++;
  else crashCount = 0;

  if (crashCount >= CRASH_LIMIT) {
    crashCount = 0;
    const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
    esp_app_desc_t desc;
    if (other && esp_ota_get_partition_description(other, &desc) == ESP_OK) {
      esp_ota_set_boot_partition(other);
      esp_restart();                    // boots the previous firmware
    }
  }
}

// Call from loop(); marks this firmware good once it has run long enough.
static void safetyTick() {
  if (!markedStable && millis() > STABLE_MS) {
    markedStable = true;
    crashCount = 0;
    esp_ota_mark_app_valid_cancel_rollback();   // harmless if rollback is off
  }
}

static const char *resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    default: return "other";
  }
}
