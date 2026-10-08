# 1. Background: PerfectPass and the paddlewheel

## The boat

A **2008 MasterCraft X2** with PerfectPass speed control. Speed comes from the hull's paddlewheel
sensor, part of an Airmar DST800-type triducer (paddlewheel speed + depth + water temperature in one
through-hull).

- Airmar DST800 (now a legacy product): <https://www.airmar.com/Catalog/Legacy-Products/DST800P>
- MasterCraft TeamTalk thread with the triducer wiring diagram:
  <https://teamtalk.mastercraft.com/forum/general-mastercraft-topics/general-discussion/2740901-vdig-touch-screen-replacement>
- DST800 wiring discussion (Sailing Anarchy): <https://forums.sailinganarchy.com/threads/dst800-wiring-to-0183-help.234318/>

## What PerfectPass sees

The paddlewheel output is a pulse train: each pulse is a fixed slice of a turn of the wheel, so
**pulse frequency is proportional to speed**. The output is open-collector: the sensor pulls the
signal line to ground and the gauge or PerfectPass side supplies the pull-up.

The DST800 is specified at about **20,000 pulses per nautical mile**:

```
1 MPH = 0.869 knots = 0.869 nm/hour
0.869 × 20,000 pulses = 17,380 pulses/hour = 4.83 pulses/second
=> about 4.83 Hz per MPH
```

In practice the constant that made our dash read correctly was **about 4.42 Hz per MPH**.
PerfectPass applies its own calibration, and our first emulator's timing also shifted the number
(see [history](02-history-uno-builds.md)). So the firmware treats the constant as a setting:
`Hz/MPH` defaults to 4.42, and the app's calibration table corrects it per boat.

## Why we built this

Our paddlewheel failed, and replacement DST800 sensors aren't sold anymore. Without a speed signal,
PerfectPass doesn't work at all.

## The fix in one sentence

Generate that same open-collector pulse train from GPS speed, on the same signal wire, and
PerfectPass can't tell the difference.

## Requirements we designed to

| Requirement | Why |
|---|---|
| Three screw terminals: +12 V, GND, SIGNAL | Install without new harnesses |
| Survive 12/24 V boat power, transients, reverse wiring, SIGNAL miswired to +12 V | It lives on a boat's electrical system |
| Exact, jitter-free output frequency | PerfectPass integrates it; jitter becomes throttle hunting |
| Hold a steady value with no GPS fix, never sweep | A sweeping speed makes PerfectPass chase it |
| Low lag | PerfectPass is a control loop; a late speed makes it oscillate |
| Configure and update from a phone, no laptop | The board ends up potted in resin behind the dash |
| Radio off on the water | Keep RF away from the GPS and the boat's electronics |
| Under ~$150 for the electronics | It has to beat buying a new paddlewheel |
