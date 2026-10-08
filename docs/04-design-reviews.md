# 4. Design reviews

The board went through three external electrical-engineering reviews before it was ordered. Each
round's findings and what changed are below; the per-revision tables are also in
[pcb/README.md](../pcb/README.md).

## Round 1: rev 1.0 → 1.1

Reviewer's verdict: fine on a bench supply, but not ready for an unsuppressed marine electrical
system.

| Finding | Change |
|---|---|
| The "12–24 V" label was wrong: the old SMBJ18A TVS starts conducting around 20 V | Input re-rated; silkscreen now **12/24V DC 32V MAX** |
| The TVS and PTC sat too close to the AP63203 buck's 35 V absolute max | Buck → **LMR38010** (80 V); TVS → **SMCJ36CA** 1.5 kW bidirectional; 60 V PTC; 100 V diodes and input ceramics; bulk capacitor added |
| L2 (the GPS antenna bias choke) self-resonated at 1.2 GHz, below GPS L1 (1.575 GHz) | Murata **LQW18AN27NG** wirewound, self-resonant at 3.7 GHz |
| Swapped +12 V/GND could back-drive the dash through the MOSFET's body diode | **Series Schottky D5** on SIGNAL |
| Open via-in-pad under the ESP32 | Removed |
| Buck loop and u.FL grounding | Hand-routed buck input loop; 8 stitching vias around the u.FL |

## Round 2: rev 1.1 → 1.2 (reviewer had the native KiCad files)

| Finding | Change |
|---|---|
| C16 rated only 63 V against a 58 V clamp | **22 µF / 100 V** (same can size) |
| The battery-sense ADC pin reached ~3.6 V at the clamp | 150 k → **220 k** (×23 divider) |
| Input capacitance borderline after DC-bias derating | **Fourth 2.2 µF / 100 V** |
| A 24 V pull-up would forward-bias D5 and make D4 clamp continuously | Q1 → **60 V** part, D4 → **SMF33A** |
| One full-size paste opening on the buck's exposed pad (risk of floating the part) | Paste split into **4 windows (60 %)**, 2 tented ground vias beside the pad, pad tied solid into the ground pour |
| *Found by us while re-checking* | Two stitching and fanout vias overlapped near the u.FL. The placement script was fixed, and drill spacing now fails the build |

Not changed: L1 stays where it is. The reviewer called moving it closer "desirable, not
essential", and doing so meant relocating D2 in the crowded input corner.

## Round 3: rev 1.2 → 1.3 (independent reviewer, fresh eyes)

| Finding | Change |
|---|---|
| **Stop-ship:** if SIGNAL is wired to +12 V, the first time Q1 turns on it shorts the supply through D5 and a bare 2N7002K | Q1 → **ZXMS6004FF** protected low-side switch (current limit, thermal shutdown). SIGNAL traces widened to 0.5 mm for the fault current |
| D4/Q1 clamp margin thin (53 V clamp vs 60 V switch) | The ZXMS6004FF has its own 60–70 V active clamp rated for 90 mJ |
| Bench-test item: USB-only startup at ~4.6 V into the buck | **Passed** on the first board, including WiFi transmitting |
| Bench-test item: the output-low level with D5 in series against the real PerfectPass input | Not yet tested |
| Operational: JP1 pull-up on 24 V boats overheats R15 | Documented: leave JP1 open on 24 V |

The reviewer reported no reversed pinouts, power-topology errors, ESP32 strapping mistakes, or
GPS/USB wiring problems.

## Placement check at JLC

JLC's engineers asked us to confirm the polarity of the SS310 diodes (D1, D2, D5). The question came
up because that part's library footprint puts the pin-1 dot on the anode, while most diodes put it
on the cathode. Our cathode bands matched the circuit. JLC also revised the GPS module's (U3) entry in
their pick-and-place data. They didn't say how, but the final placement matched the design.
