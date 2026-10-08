"""Generate gpsspeed.kicad_sch from design.PARTS.

Every pin gets a net label at its exact connection point (or a no-connect
flag), so connectivity is unambiguous without drawing wires. Symbols are
grouped by function block with a heading for each block.
"""
import os
import uuid

import design
from sexp import Q, dump
from symlib import PROJ, Symbol

NS = uuid.UUID("6b0f8d3e-4a55-4f1c-9a61-6e3a5d9b0c11")


def uid(*parts):
    return Q(str(uuid.uuid5(NS, "/".join(parts))))


ROOT = uid("root-sheet")
GRID = 1.27

BLOCKS = [
    ("conn", "BOAT CONNECTOR"),
    ("power", "POWER  (polyfuse -> bidirectional TVS -> reverse-polarity -> LMR38010 80V buck to 3.3V; USB diode-OR)  rev 1.3"),
    ("usb", "USB-C  (programming + bench power)"),
    ("mcu", "ESP32-C3  (WiFi boot window + OTA; strapping per Espressif reference)"),
    ("gps", "GPS  (ATGM336H, active antenna bias from VCC_RF per datasheet 2.7.1)"),
    ("out", "PADDLEWHEEL OUTPUT  (ZXMS6004FF protected low-side switch: current limit + thermal shutdown; 33V drain TVS; JP1 pull-up ships OPEN)"),
    ("led", "STATUS LEDs"),
    ("tp", "TEST POINTS (not assembled)"),
    ("mech", "MOUNTING"),
]

# Where each block starts on the sheet and how wide it may grow (mm).
REGIONS = {
    "conn":  (15, 30, 60),
    "power": (95, 30, 305),
    "usb":   (15, 125, 150),
    "mcu":   (175, 125, 225),
    "gps":   (15, 235, 200),
    "out":   (225, 235, 120),
    "led":   (15, 330, 160),
    "tp":    (190, 330, 120),
    "mech":  (320, 330, 80),
}


def snap(v):
    return round(round(v / GRID) * GRID, 4)


def fnt(size=1.27):
    return ["font", ["size", size, size]]


def prop(name, value, x, y, hide=False, size=1.27, justify=None):
    eff = ["effects", fnt(size)]
    if justify:
        eff.append(["justify", justify])
    if hide:
        eff.append("hide")
    return ["property", Q(name), Q(value), ["at", x, y, 0], eff]


def label(net, x, y, angle, key):
    just = {0: "left", 180: "right", 90: "left", 270: "right"}[int(angle)]
    return ["label", Q(net), ["at", x, y, angle], ["fields_autoplaced", "yes"],
            ["effects", fnt(), ["justify", just, "bottom"]], ["uuid", uid("lbl", key)]]


def pin_label_angle(pin_angle):
    # Pin angle says which way the pin runs from its tip INTO the body; the
    # label should point the opposite way, out from the tip.
    return {0: 180, 180: 0, 90: 270, 270: 90}[int(pin_angle) % 360]


def extent(sym, pins):
    xs = [p["x"] for p in sym.pins] or [0]
    ys = [p["y"] for p in sym.pins] or [0]
    longest = max([len(n) for _, n in pins if n] + [4])
    lab = longest * 1.05 + 3
    left = -min(xs) + (lab if any(p["angle"] == 0 for p in sym.pins) else 4)
    right = max(xs) + (lab if any(p["angle"] == 180 for p in sym.pins) else 4)
    top = max(ys) + (lab if any(p["angle"] == 270 for p in sym.pins) else 6)
    bot = -min(ys) + (lab if any(p["angle"] == 90 for p in sym.pins) else 6)
    return left, right, top, bot


def build():
    items, lib_syms, used_libs = [], {}, set()
    placed = {}

    for key, title in BLOCKS:
        x0, y0, width = REGIONS[key]
        items.append(["text", Q(title), ["exclude_from_sim", "no"], ["at", x0, y0 - 8, 0],
                      ["effects", fnt(2.0), ["justify", "left", "bottom"]], ["uuid", uid("title", key)]])
        cx, cy, rowh = x0, y0, 0
        for part in [p for p in design.PARTS if p["grp"] == key]:
            sym = Symbol(part["sym"])
            pins = sym.resolve(part["pins"])
            l, r, t, b = extent(sym, pins)
            if cx + l + r > x0 + width and cx > x0:
                cx, cy, rowh = x0, cy + rowh, 0
            sx, sy = snap(cx + l), snap(cy + t)
            placed[part["ref"]] = (sx, sy)
            cx += l + r + 4
            rowh = max(rowh, t + b + 6)

            lib_syms[sym.lib_id] = sym.embedded()
            fp = part.get("fp") or sym.footprint
            ref = part["ref"]
            su = uid("sym", ref)
            node = ["symbol", ["lib_id", Q(sym.lib_id)], ["at", sx, sy, 0], ["unit", 1],
                    ["exclude_from_sim", "no"],
                    ["in_bom", "yes" if part.get("lcsc") else "no"],
                    ["on_board", "yes"], ["dnp", "no"], ["fields_autoplaced", "yes"],
                    ["uuid", su],
                    prop("Reference", ref, sx, snap(sy - t + 2.5), justify="left"),
                    prop("Value", part["value"], sx, snap(sy - t + 5.0), justify="left"),
                    prop("Footprint", fp, sx, sy, hide=True),
                    prop("Datasheet", str(sym.props.get("Datasheet", "")), sx, sy, hide=True),
                    prop("LCSC", part.get("lcsc") or "", sx, sy, hide=True)]
            for p, _ in pins:
                node.append(["pin", Q(p["number"]), ["uuid", uid("pin", ref, p["number"], p["name"])]])
            node.append(["instances", ["project", Q("gpsspeed"),
                         ["path", Q("/" + ROOT), ["reference", Q(ref)], ["unit", 1]]]])
            items.append(node)

            seen = set()
            for p, net in pins:
                px, py = round(sx + p["x"], 4), round(sy - p["y"], 4)
                k = (px, py)
                if k in seen:        # stacked pins (e.g. ESP32 EPAD) -> one label
                    continue
                seen.add(k)
                if net:
                    items.append(label(net, px, py, pin_label_angle(p["angle"]),
                                       "%s.%s.%s" % (ref, p["number"], p["name"])))
                else:
                    items.append(["no_connect", ["at", px, py],
                                  ["uuid", uid("nc", ref, p["number"])]])

    # PWR_FLAGs so ERC knows these nets are driven by off-sheet sources.
    flag = Symbol("power:PWR_FLAG")
    lib_syms[flag.lib_id] = flag.embedded()
    fx, fy = 330.0, 360.0
    items.append(["text", Q("POWER FLAGS (ERC only)"), ["exclude_from_sim", "no"], ["at", fx, fy - 12, 0],
                  ["effects", fnt(2.0), ["justify", "left", "bottom"]], ["uuid", uid("title", "flags")]])
    for i, net in enumerate([design.GND, design.VIN12, design.VBUS, design.V3V3,
                             design.VBUCK, design.V3V3_GPS, design.VIN_F, design.VCC_RF]):
        x, y = snap(fx + (i % 4) * 16), snap(fy + (i // 4) * 16)
        ref = "#FLG%02d" % (i + 1)
        pin = flag.pins[0]
        items.append(["symbol", ["lib_id", Q(flag.lib_id)], ["at", x, y, 0], ["unit", 1],
                      ["exclude_from_sim", "no"], ["in_bom", "no"], ["on_board", "yes"], ["dnp", "no"],
                      ["uuid", uid("sym", ref)],
                      prop("Reference", ref, x, y, hide=True),
                      prop("Value", "PWR_FLAG", x, snap(y - 4)),
                      prop("Footprint", "", x, y, hide=True),
                      prop("Datasheet", "", x, y, hide=True),
                      ["pin", Q(pin["number"]), ["uuid", uid("pin", ref)]],
                      ["instances", ["project", Q("gpsspeed"),
                       ["path", Q("/" + ROOT), ["reference", Q(ref)], ["unit", 1]]]]])
        items.append(label(net, round(x + pin["x"], 4), round(y - pin["y"], 4), 270, "flag." + ref))

    sch = ["kicad_sch", ["version", 20231120], ["generator", Q("gpsspeed_gen")],
           ["generator_version", Q("8.0")], ["uuid", ROOT], ["paper", Q("A2")],
           ["title_block", ["title", Q("GPSSpeed v4 - GPS paddlewheel emulator")],
            ["date", Q("2026-09-26")], ["rev", Q("1.3")],
            ["company", Q("Jake Michalski")],
            ["comment", 1, Q("ESP32-C3 + ATGM336H GPS + AO3400A open-drain output")],
            ["comment", 2, Q("Generated from pcb/gen/design.py - edit that, then re-run gen_sch.py")]],
           ["lib_symbols"] + list(lib_syms.values())] + items + \
          [["sheet_instances", ["path", Q("/"), ["page", Q("1")]]]]
    return sch, placed


def main():
    sch, placed = build()
    out = os.path.join(PROJ, "gpsspeed.kicad_sch")
    with open(out, "w", encoding="utf-8") as f:
        f.write(dump(sch) + "\n")
    print("wrote", out, "with", len(placed), "symbols")


if __name__ == "__main__":
    main()
