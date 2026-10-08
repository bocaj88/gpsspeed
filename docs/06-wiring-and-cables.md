# 6. Wiring, cables and adapters

Everything you need besides the board itself.

> **⚠ To be completed by Jake:** the boat-side connectors and adapters we used on the X2 (rows
> marked **TBD**). Each one needs the exact part and where to buy it.

## Shopping list

| # | Item | What to get | Notes |
|---|---|---|---|
| 1 | GPS antenna | An **active** GPS antenna (3.3 V) with a **u.FL / IPEX1** plug. The antenna that comes with the MakerFocus GT-U7 kit works: <https://www.amazon.com/dp/B07P8YMVNT> | The board powers the antenna through the u.FL. Mount it flat with a clear view of the sky. Fiberglass, plastic and canvas overhead are fine; metal is not |
| 2 | SMA adapter (only if your antenna has an SMA plug) | **SMA female → u.FL (IPEX1) pigtail**, ~10–15 cm | Most marine and "puck" GPS antennas are SMA |
| 3 | Boat power + ground | **TBD**: connector/adapter we used on the X2 | Use **switched** +12 V so the board powers on with the boat. The input is protected for 12/24 V systems, 32 V continuous max |
| 4 | Speed signal | **TBD**: the plug that mates with the paddlewheel / PerfectPass speed connector, so the harness doesn't have to be cut | SIGNAL replaces the paddlewheel's signal wire into PerfectPass. Leave the paddlewheel's own signal wire disconnected |
| 5 | Wire | 18–20 AWG marine (tinned) wire, three colors | The screw terminal (KF301, 5.0 mm pitch) takes 14–22 AWG |
| 6 | USB-C cable | Any **data-capable** USB-C cable | Only needed for the first flash or recovery. After that, updates go over WiFi |

## Screw terminal J1

Looking at the board with the terminal on the left, top to bottom:

| Pin | Label | Connect to |
|---|---|---|
| 1 | **+12V** | Switched +12 V (12/24 V nominal, 32 V max) |
| 2 | **GND** | Boat ground |
| 3 | **SIG** | The PerfectPass paddlewheel/speed input |

## Before you power it on the boat

1. **Meter it.** In beep mode, +12V to GND must not beep continuously (a brief chirp is the
   capacitors charging).
2. **Power it from USB first**, and check the app sees a GPS fix.
3. **Wire it**, power up, and check the app: GPS fix, `vin` around your battery voltage.
4. **Self-test** (in the app) steps through 0 / 10 / 15 / 20 / 22 / 24 / 26 / 30 MPH. At each step,
   type in what the dash reads and save it. That calibrates the board to your boat.

## The JP1 pull-up

A real paddlewheel is open-collector: the dash or PerfectPass supplies the pull-up. JP1 ships
**open**. Only bridge it if your dash doesn't pull SIGNAL up on its own (the app would show pulses
but the dash would read 0). **Never bridge JP1 on a 24 V boat.**

## Wiring mistakes the board survives

- +12 V and GND swapped (reverse-polarity diode; D5 keeps the dash input safe).
- SIGNAL wired to +12 V (the output switch current-limits and cycles).
- Transients up to about ±58 V at full surge current (1.5 kW TVS).

None of these is a reason to skip the meter check.
