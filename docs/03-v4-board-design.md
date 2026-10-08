# 3. The v4 board (ESP32-C3)

The full hardware write-up, including the bring-up checklist, is [pcb/README.md](../pcb/README.md).
This page covers the main decisions and the reasons behind them.

| | |
|---|---|
| Size | 72 × 46 mm, 2 layers, 4 × M3 holes |
| MCU | ESP32-C3-MINI-1-H4 (WiFi, native USB, 105 °C rated) |
| GPS | ATGM336H-5N31 (GPS + BeiDou, 10 Hz), u.FL for an external **active** antenna |
| Power in | 12/24 V nominal, **32 V continuous max**; USB-C on the bench |
| Output | ZXMS6004FF protected low-side switch behind a series Schottky. Open-drain, like the paddlewheel |
| Connector | J1 screw terminal: **1 = +12 V, 2 = GND, 3 = SIGNAL** |
| LEDs | PWR (red), GPS (green), WIFI (green) |

## Decisions

**ESP32-C3 instead of another AVR or an RP2040.** WiFi comes free on the chip, and iOS can't use Web
Bluetooth, so a web page served by the board beats BLE plus a custom app. The C3 also has a
separate hardware UART for the GPS and native USB for flashing and logs. That solves the Uno's
one-serial-port problem outright.

**Hardware pulse generation.** The output comes from the LEDC peripheral, clocked from the 40 MHz
crystal. The Arduino `ledc` API only accepts whole hertz (0.23 MPH steps), so the firmware writes
the fractional clock divider directly (`div = 625000 / f`). Host tests show **≤ 0.02 % frequency
error** from 1 to 60 MPH.

**GPS module soldered to the board.** The ATGM336H is configured with CASIC `$PCAS` commands (it
doesn't speak u-blox UBX). The firmware auto-detects the baud rate and switches the module to
115200 baud and 10 Hz. That was confirmed on the first board.

**Power: built for a hostile boat harness.**
60 V polyfuse → 1.5 kW bidirectional TVS (SMCJ36CA, ≈ ±58 V clamp) → 100 V reverse-polarity Schottky
→ 22 µF/100 V bulk + 4 × 2.2 µF/100 V → TI LMR38010 buck (80 V operating, 85 V abs max) → 3.3 V.
USB-C also feeds the buck through a second diode, so the board runs from either source.

**Output stage: survives miswiring.**
- A **ZXMS6004FF IntelliFET** (60 V, current limit 0.7–1.7 A, thermal shutdown, 60–70 V active
  clamp) pulls SIGNAL low. If SIGNAL is wired to +12 V by mistake, it current-limits and cycles
  instead of burning out.
- A **series Schottky (D5)** stops a swapped +12 V/GND from back-driving the dash input through the
  switch. It adds about 0.3 V to the low level, much like a real Hall-effect sensor.
- A **33 V TVS** clamps the drain, so a 24 V pull-up is fine.
- An optional on-board **4.7 k pull-up** (JP1, ships open) covers dashes that don't supply one.

**WiFi boot window.** The access point runs for 2 minutes after power-up, extends while a phone is
connected, then shuts off. Firmware updates over WiFi verify the image, and a build that
crash-loops rolls back automatically. Brownout resets from engine cranking don't count as crashes.

**Potting.** Keep resin off the ESP32 antenna end (top edge), or pot only the lower two-thirds:
resin detunes the PCB antenna.

## How the board was generated

The schematic and PCB come from Python scripts. `pcb/gen/design.py` is the single netlist source.
Footprints and 3D models are pulled from JLC's own library (easyeda2kicad), routing is done by
Freerouting, then ground pours and stitching are added. Every build must pass KiCad DRC, ERC and
schematic parity, and drill spacing is a hard error. See *Regenerating* in
[pcb/README.md](../pcb/README.md).
