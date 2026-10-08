# 7. Cost and ordering

## What 5 assembled boards cost (JLCPCB, rev 1.3, shipped to New York, Sept 2026)

| Item | USD |
|---|---|
| PCB (5 pcs, 2-layer, lead-free HASL) | 5.20 |
| Standard PCBA (setup, stencil, 35 parts, feeder fees, X-ray, placement check) | 160.58 |
| UPS Express Saver, duties paid by JLC (DDP) | 37.23 |
| US customs duty + fee (China tariff) | 59.02 |
| New-user SMT coupon | −10.00 |
| NY sales tax | 15.44 |
| **Total** | **267.47** |

That's about **$53 per board**, delivered. The components themselves are about $12 a board. Most of
the cost is fixed per order: Standard PCBA setup ($25.75) and $3 extended-part fees ($52.70), which
shrink per board at higher quantities. The tariff is about 35 % of the goods value and applies to any
China-based assembler.

## Why "Standard" PCBA

JLC only places the ESP32-C3-MINI-1 module under Standard assembly ("Standard PCBA only"), which
adds edge rails and a higher setup fee than Economic. The module is ordered as the **-H4** variant
(C2934569, 105 °C rated); same footprint and flash as the -N4.

## Ordering it yourself

1. Upload `pcb/out/jlcpcb/gerbers.zip`. Defaults are fine: 2 layers, 1.6 mm, green. We used
   LeadFree HASL.
2. PCB Assembly: **Standard**, top side, edge rails added by JLC, **Confirm Parts Placement: Yes**.
3. Upload `pcb/out/jlcpcb/bom_jlcpcb.csv` and `cpl_jlcpcb.csv`. All 35 lines should match.
4. Check polarity in the preview: D1–D5, C16 (+), LEDs, U1–U4, Q1. The SS310's pin-1 dot is on its
   **anode**. Its cathode band should be on the side with the silkscreen bar.
5. Customs description: *Research/Education/DIY → Programmable Controller (HS 853890)*.

Expect the parts-placement confirmation request within a few days, and answer it quickly so the
order doesn't sit.
