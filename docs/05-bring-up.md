# 5. Bring-up log

## 2026-10-08: board 1 (rev 1.3), USB power

| Check | Result |
|---|---|
| USB-C to a Mac | Enumerates as Espressif "USB JTAG_serial debug unit" (VID 0x303A) → `/dev/cu.usbmodem*` |
| `esptool flash_id` | ESP32-C3, 40 MHz crystal, 4 MB flash |
| Flash firmware over USB | OK |
| Boot | `GPSSpeed v4.0.x … GPS:115200 baud @ 10 Hz`, so the CASIC configuration commands work |
| WiFi on USB power alone (the review's "USB-only startup" item) | **OK**: the access point comes up and serves pages on USB power |
| Battery sense on USB | `vin: 4.8` (USB 5 V minus the diode drop). The 220 k / 10 k divider reads correctly |
| No antenna | `ant:OPEN`; output holds 5 MPH (22.1 Hz) |
| Active antenna on the u.FL | `ant:OK`. **Fix indoors in seconds, 13–16 satellites** |
| At rest with a fix | Output drops to 0 Hz below 0.6 MPH (the dash would read 0) |
| Phone (iPhone) | Joins the open `GPSSpeed-XXXX` network, the app pops up, **Done** saves the network, `gpsspeed.local` works |
| **Over-the-air update from the iPhone** | **OK**: uploaded 4.0.7, verified, rebooted into it |

### Firmware fixes found during bring-up

- **4.0.4:** the WiFi name changed every boot (`GPSSpeed-0060`, then `-0000`). `WiFi.macAddress()`
  returns junk before the radio starts, so phones never found a saved network again. The name now
  comes from the factory MAC in eFuse, so it's permanent per board.
- **4.0.3 / 4.0.5:** the iOS captive-portal flow. At first the board redirected every iOS "is there
  internet?" check to the app, so iOS never finished joining and never saved the network.
  Answering "online" after the page loads fixed the joining. But iOS's *background* check follows
  the redirect too, and doesn't run JavaScript, so the pop-up then showed "Success". The board now
  waits for the page's own JavaScript to check in (`/api/hello`) before answering "online". Only
  known OS check URLs ever get "Success".
- **4.0.6:** the WiFi is open by default; a password can be set from the app.
- **4.0.3:** `http://gpsspeed.local` via mDNS.

## Still to do

See [TODO.md](TODO.md).
