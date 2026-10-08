# Arduino Uno builds (v1–v3)

The first versions: an Arduino Uno with a perfboard hat (GPS module, IRLZ44N MOSFET, screw terminal).
Kept for reference; the story and lessons are in [docs/02-history-uno-builds.md](../../docs/02-history-uno-builds.md).

| Folder | What it is |
|---|---|
| `v1-variants/` | First working sketches; one copy per serial wiring (the one-UART problem) |
| `Integration/` | Integration experiments (2024–2025) |
| `v2/` | Consolidated sketch: 10 Hz GPS, predictor, self-test, steady 5 MPH with no fix |
| `v3/` | Timer1 hardware output on pin 9 (needs one jumper on the hat; see the header) |
