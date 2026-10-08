# TODO

## Bench (before the boat)

- [ ] Power board 1 from **12 V** through the screw terminal (bench supply with a ~200 mA current limit, or a battery). Check `vin` and that it boots and the WiFi works.
- [ ] **Check the SIGNAL output**: run Self-test in the app and confirm SIG pulses at the commanded rate (a meter's Hz mode, or the old Uno setup). Check the low level with D5 in series.
- [ ] Flash boards 2–5 over USB (one time each, ~30 s). After that they update over WiFi.
- [ ] Try "Open in browser" from inside the iOS pop-up (does it open Safari, or load inside the pop-up?).

## Boat (2008 MasterCraft X2)

- [ ] Wire +12 V (switched), GND, SIGNAL. Leave the paddlewheel's signal wire disconnected.
- [ ] Mount the antenna flat, with sky view.
- [ ] Idle and low speed first, with a hand on the throttle. Compare the dash against a phone GPS app.
- [ ] Run Self-test and save a calibration point at each step.
- [ ] Tune PerfectPass behavior: predictor lead/smoothing if it hunts.
- [ ] Pot the board (keep resin off the ESP32 antenna end).

## Repo / community

- [ ] Fill in the X2 boat-side connectors and adapters in [06-wiring-and-cables.md](06-wiring-and-cables.md).
- [ ] Add photos of the installed board.
- [ ] Post on **MasterCraft TeamTalk** (primary forum: <https://teamtalk.mastercraft.com>) using [forum-post.md](forum-post.md), then other PerfectPass/wakeboard boat forums.
- [ ] Find homes for the 4 spare boards.
- [ ] Update this repo with on-water results.

## Possible next revision (not needed for the prototypes)

- Move L1 closer to U1 (shorter switch node, less EMI).
- Lower-drop USB OR-ing (bench convenience only).
