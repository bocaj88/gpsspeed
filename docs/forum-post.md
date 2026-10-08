# Forum post (MasterCraft TeamTalk)

**Suggested title:** Open-source GPS replacement for the PerfectPass paddlewheel (2008 X2), 4 spare boards

---

The paddlewheel on our 2008 X2 went bad, and replacement sensors aren't sold anymore (the DST800
it's part of is an Airmar legacy part). No paddlewheel means no PerfectPass. So we built a small
board that reads **GPS speed** and sends PerfectPass the same pulse signal the paddlewheel would.
It goes on the same wire, so nothing else on the boat changes.

**What it is**
- 72 × 46 mm board: ESP32-C3 + 10 Hz GPS, screw terminals for **+12 V / GND / SIGNAL**
- Exact hardware-timed output (≤ 0.02 % error), holds a steady 5 MPH until GPS locks, with lag
  compensation so PerfectPass doesn't hunt
- **Phone app with no app install:** for 2 minutes after power-up it runs its own WiFi. Join it and
  the page pops up with live speed, a self-test, dash calibration, settings, and **firmware updates
  over WiFi**. Then the radio turns off.
- Protected for boat power: 12/24 V, reverse polarity, ±58 V transients, and SIGNAL accidentally
  wired to +12 V
- Three rounds of outside EE review before we ordered

**Status:** 5 boards built (JLCPCB). Bench-tested: GPS locks indoors in seconds, the phone app
works, and over-the-air updates work. **On-water testing in our X2 is next.** We'll post results.

**Everything is open source** (schematic, PCB, firmware, and every dead end along the way):
👉 **GitHub: https://github.com/bocaj88/gpsspeed**
- Wiring + where to get the cables/adapters: https://github.com/bocaj88/gpsspeed/blob/main/docs/06-wiring-and-cables.md
- Full write-up, starting with the problem: https://github.com/bocaj88/gpsspeed#readme

**4 spare boards:** we built 5 and need 1. If you have a PerfectPass boat and want to try one,
reply here or DM me. They come assembled and flashed; you add a GPS antenna and three wires.

Questions or ideas welcome, especially from anyone who knows the PerfectPass input side well.
