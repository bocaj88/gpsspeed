"""Generate gpsspeed.kicad_pcb (placement, outline, keepouts, pours, RF
pre-routes) from design.PARTS. Autorouting and outputs happen in build.py.

Run with KiCad's bundled Python (it provides the pcbnew module).

Board: 72 x 46 mm, 2 layers, 1.6 mm FR4.
  Left edge  : J1 screw terminal, wires exit off the left edge
  Top-left   : 12V input protection + LMR38010 buck
  Top-middle : ESP32-C3, PCB antenna flush with the top edge over a keepout
  Right      : ATGM336H GPS, u.FL at the right edge (~2 mm RF trace)
  Bottom     : USB-C at the bottom edge, status LEDs, output stage near J1
"""
import os
import sys
import uuid

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design  # noqa: E402
from symlib import PROJ, PROJECT_FOOTPRINTS, STOCK_FOOTPRINTS, Symbol  # noqa: E402
from gen_sch import ROOT, uid  # noqa: E402

W, H = 72.0, 46.0
MM = pcbnew.FromMM

# ref -> (x, y, rotation_deg).  Rotation follows KiCad: +90 is CCW on screen.
PLACE = {
    # boat connector: rot 270 puts pin 1 (+12V) on top and the wire entry
    # (the silkscreen arrows) facing off the left edge
    "J1": (4.4, 24.0, 270),

    # --- 12V input chain (rev 1.1): fuse -> TVS -> diode, stacked by J1 ---
    "F1": (11.3, 16.0, 90),        # 1812 PTC, pad 1 (+12V_IN) at the bottom by J1.1
    "D3": (11.5, 8.4, 90),         # SMCJ36CA, pad 1 (+12V_F) at the bottom
    "D1": (16.3, 14.15, 0),        # SS310: anode left (from fuse), cathode right
    "D2": (17.9, 10.6, 0),         # SS310 USB OR: VBUS left, VIN_BUCK right
    "C16": (18.2, 20.2, 270),      # 22uF/100V bulk, pad 1 (+) on top
    # --- LMR38010 buck: VIN/GND pins face down onto the input caps ---
    "U1": (25.8, 16.6, 0),
    "C3": (25.2, 22.0, 180),       # 100nF 100V directly under VIN(3)/GND(1): ~2mm loop
    "C1": (25.2, 24.3, 180),
    "C2": (25.2, 26.5, 180),
    "C14": (25.2, 28.7, 180),
    "C17": (25.2, 30.9, 180),      # rev 1.2: 4th input ceramic
    "R16": (28.7, 21.3, 270),      # RT, right at pin 4
    "C4": (25.17, 11.0, 90),       # BOOT cap, straight above pin 7
    "L1": (21.4, 6.6, 0),          # SW pad directly above pin 8
    "C5": (16.3, 3.2, 180),
    "C6": (16.3, 5.4, 180),
    "C15": (16.3, 7.6, 180),
    "R18": (27.7, 11.0, 90),       # FB divider on the quiet side, away from SW/L1
    "R17": (29.3, 11.0, 270),
    # battery-voltage divider, tapped after D1
    "R1": (31.2, 17.0, 0),
    "R2": (33.2, 18.4, 90),
    "C7": (34.6, 18.4, 90),

    # --- ESP32-C3, antenna flush with the top edge ---
    "U2": (46.0, 11.3, 0),
    "C8": (35.9, 8.3, 270),
    "C9": (37.7, 8.3, 270),
    "R6": (37.7, 11.2, 90),        # IO2 strap
    "R5": (37.7, 14.0, 90),        # EN pull-up
    "C10": (36.3, 14.0, 90),       # EN delay cap
    "R7": (49.2, 19.0, 90),        # IO8 strap
    "R8": (50.8, 19.0, 90),        # IO9 / BOOT pull-up
    "SW1": (40.2, 23.0, 0),        # RESET
    "SW2": (51.4, 23.0, 0),        # BOOT (clear of the GPS pads)

    # --- USB-C at the bottom edge ---
    "J3": (46.0, 40.9, 0),
    "U4": (46.0, 34.6, 0),
    "R3": (42.4, 36.2, 90),
    "R4": (49.6, 36.2, 90),

    # --- GPS on the right, RF pins facing the right edge ---
    "U3": (62.0, 27.0, 90),
    "J2": (69.6, 30.3, 270),       # u.FL, signal pad faces the GPS
    "L2": (68.4, 28.4, 90),        # LQW18AN bias choke, pad 1 (VCC_RF) on top
    "FB1": (53.2, 33.6, 0),
    "C11": (53.6, 31.0, 90),
    "C12": (55.6, 30.3, 90),       # right at GPS VCC
    "R12": (54.6, 26.2, 0),        # ON/OFF pull-up

    # --- paddlewheel output stage, next to J1 pin 3 ---
    "D5": (11.4, 29.0, 0),         # series Schottky: anode at the terminal
    "Q1": (16.0, 29.0, 0),         # drain faces D5, gate/source face right
    "D4": (14.6, 32.0, 0),         # drain clamp: cathode toward the drain node
    "R13": (19.6, 30.0, 180),      # gate end toward Q1
    "R14": (18.4, 32.4, 90),
    "R15": (10.2, 32.6, 270),      # optional pull-up, on the terminal side of D5
    "JP1": (10.4, 35.8, 0),

    # --- status LEDs along the bottom (visible through clear potting) ---
    "R9": (24.5, 40.8, 0),  "LED1": (24.5, 43.0, 0),
    "R10": (29.0, 40.8, 0), "LED2": (29.0, 43.0, 0),
    "R11": (33.5, 40.8, 0), "LED3": (33.5, 43.0, 0),

    # --- test points ---
    "TP1": (36.8, 28.0, 0),  "TP2": (36.8, 31.0, 0),
    "TP3": (21.8, 32.6, 0),  "TP4": (54.4, 37.0, 0),
    "TP5": (45.2, 28.0, 0),  "TP6": (48.2, 28.0, 0),

    # --- mounting ---
    "H1": (3.6, 3.6, 0), "H2": (68.4, 3.6, 0),
    "H3": (3.6, 42.4, 0), "H4": (68.4, 42.4, 0),
}

SILK = [
    # (text, x, y, size, layer)
    ("+12V", 10.2, 19.0, 1.0, "F"), ("GND", 10.2, 24.0, 1.0, "F"), ("SIG", 10.2, 26.6, 1.0, "F"),
    ("PWR", 24.5, 45.1, 0.8, "F"), ("GPS", 29.0, 45.1, 0.8, "F"), ("WIFI", 33.5, 45.1, 0.8, "F"),
    ("RST", 40.2, 26.6, 0.8, "F"), ("BOOT", 51.4, 26.6, 0.8, "F"),
    ("GPS ANT", 64.2, 34.4, 0.8, "F"), ("PULLUP", 10.4, 37.6, 0.8, "F"),
    ("GPSSpeed v4", 60.0, 40.0, 1.2, "F"), ("rev 1.3  2026-09", 60.0, 41.8, 0.8, "F"),
    ("12/24V DC  32V MAX", 13.5, 41.5, 0.8, "F"),
    ("GPSSpeed v4 - ESP32-C3 + ATGM336H", 36.0, 30.0, 1.0, "B"),
    ("JP1 ships OPEN: bridge only if dash has no pull-up (12V boats)", 36.0, 32.0, 0.8, "B"),
    ("JLCJLCJLCJLC", 36.0, 34.5, 0.8, "B"),
]


def fp_source(part):
    fp = part.get("fp") or Symbol(part["sym"]).footprint
    lib, name = fp.split(":", 1)
    path = PROJECT_FOOTPRINTS if lib == "gpsspeed" else os.path.join(STOCK_FOOTPRINTS, lib + ".pretty")
    return path, name, fp


def add_outline(board):
    r = 2.0
    def seg(a, b):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(MM(a[0]), MM(a[1]))); s.SetEnd(pcbnew.VECTOR2I(MM(b[0]), MM(b[1])))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); board.Add(s)
    def arc(c, start, end):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        s.SetCenter(pcbnew.VECTOR2I(MM(c[0]), MM(c[1])))
        s.SetStart(pcbnew.VECTOR2I(MM(start[0]), MM(start[1])))
        s.SetEnd(pcbnew.VECTOR2I(MM(end[0]), MM(end[1])))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); board.Add(s)
    seg((r, 0), (W - r, 0)); seg((W, r), (W, H - r)); seg((W - r, H), (r, H)); seg((0, H - r), (0, r))
    arc((W - r, r), (W - r, 0), (W, r))
    arc((W - r, H - r), (W, H - r), (W - r, H))
    arc((r, H - r), (r, H), (0, H - r))
    arc((r, r), (0, r), (r, 0))


def poly(pts):
    chain = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in pts:
        chain.Append(MM(x), MM(y))
    chain.SetClosed(True)
    return chain


def add_zone(board, net, layer, pts, priority=0):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net)
    z.Outline().AddOutline(poly(pts))
    z.SetLocalClearance(MM(0.3))
    z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(MM(0.3))
    z.SetThermalReliefSpokeWidth(MM(0.4))
    z.SetAssignedPriority(priority)
    board.Add(z)
    return z


def add_keepout(board, pts, name):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
    z.SetLayerSet(ls)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowPads(True)
    z.SetDoNotAllowZoneFills(True); z.SetDoNotAllowFootprints(False)
    z.SetZoneName(name)
    z.Outline().AddOutline(poly(pts))
    board.Add(z)


def track(board, net, layer, pts, width):
    for a, b in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(MM(a[0]), MM(a[1]))); t.SetEnd(pcbnew.VECTOR2I(MM(b[0]), MM(b[1])))
        t.SetWidth(MM(width)); t.SetLayer(layer); t.SetNet(net)
        board.Add(t)


def pad_xy(fp, number):
    for p in fp.Pads():
        if p.GetNumber() == number:
            v = p.GetPosition()
            return pcbnew.ToMM(v.x), pcbnew.ToMM(v.y)
    raise KeyError(number)


# --------------------------------------------------------------------------
# GND fanout: give every SMD ground pad its own via to the solid B.Cu plane.
# The autorouter then routes around these vias, so ground never depends on
# the (inevitably fragmented) top-layer pour.
# --------------------------------------------------------------------------
VIA_D, VIA_DRILL, FAN_W, CLR = 0.6, 0.3, 0.4, 0.22


def _v(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def _blocked(board, x, y, r, gnd, vias, keepouts, layers=(pcbnew.F_Cu, pcbnew.B_Cu)):
    if x < 0.8 or y < 0.8 or x > W - 0.8 or y > H - 0.8:
        return True
    for kx0, ky0, kx1, ky1 in keepouts:
        if kx0 - r < x < kx1 + r and ky0 - r < y < ky1 + r:
            return True
    for vx, vy in vias:
        if (vx - x) ** 2 + (vy - y) ** 2 < 1.0 ** 2:
            return True
    pt = _v(x, y)
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            # Drill-to-drill spacing applies whatever the net (U1's EP vias,
            # J1/J3 pins): JLC wants >= 0.5 mm between hole edges.
            if pad.HasHole():
                hp = pad.GetPosition()
                d = ((pcbnew.ToMM(hp.x) - x) ** 2 + (pcbnew.ToMM(hp.y) - y) ** 2) ** 0.5
                if d < pcbnew.ToMM(pad.GetDrillSizeX()) / 2 + VIA_DRILL / 2 + 0.55:
                    return True
            same = pad.GetNetname() == gnd and pad.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH
            if same:
                continue
            for layer in layers:
                if pad.IsOnLayer(layer) or pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                    if pad.GetEffectiveShape(layer).Collide(pt, MM(r + CLR)):
                        return True
    for t in board.GetTracks():
        if t.GetNetname() == gnd:
            continue
        for layer in layers:
            if t.IsOnLayer(layer) and t.GetEffectiveShape(layer).Collide(pt, MM(r + CLR)):
                return True
    return False


def _seg_blocked(board, a, b, layer, gnd, own_pad):
    seg = pcbnew.SEG(_v(*a), _v(*b))
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetname() == gnd or not pad.IsOnLayer(layer):
                continue
            if pad.GetEffectiveShape(layer).Collide(seg, MM(FAN_W / 2 + CLR)):
                return True
    for t in board.GetTracks():
        if t.GetNetname() != gnd and t.IsOnLayer(layer) and \
                t.GetEffectiveShape(layer).Collide(seg, MM(FAN_W / 2 + CLR)):
            return True
    return False


def add_via(board, net, x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(_v(x, y)); v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
    v.SetNet(net); v.SetViaType(pcbnew.VIATYPE_THROUGH)
    board.Add(v)


def fanout_gnd(board, gnd_net, keepouts):
    import math
    gnd = gnd_net.GetNetname()
    # Seed with vias already on the board (u.FL stitching ring) so a fanout
    # via can never land on top of one -- rev 1.1 had two overlapping drills.
    existing = [(pcbnew.ToMM(v.GetPosition().x), pcbnew.ToMM(v.GetPosition().y))
                for v in board.GetTracks() if v.GetClass() == "PCB_VIA"]
    vias, missed = [], []
    for fp in board.GetFootprints():
        fc = fp.GetPosition()
        fcx, fcy = pcbnew.ToMM(fc.x), pcbnew.ToMM(fc.y)
        pads = [p for p in fp.Pads() if p.GetNetname() == gnd and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD]
        # Exposed pads get NO via-in-pad (open holes wick paste / tilt the
        # part). ESP32 EPAD reaches ground through the perimeter-pad stubs;
        # the buck EP through a direct track to its GND pin plus the 2 tented
        # vias its footprint carries just past the GND-pin end of the pad (rev
        # 1.2; the FB/RT end is left open -- vias there block the VIN route).
        if fp.GetReference() == "U2":
            pads = [p for p in pads if p.GetNumber() != "49"]
        if fp.GetReference() == "U1":
            pads = [p for p in pads if p.GetNumber() != "9"]
        for p in pads:
            px, py = pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)
            sx, sy = pcbnew.ToMM(p.GetBoundingBox().GetWidth()), pcbnew.ToMM(p.GetBoundingBox().GetHeight())
            out = math.atan2(py - fcy, px - fcx) if (px, py) != (fcx, fcy) else 0.0
            dirs = [out + math.radians(a) for a in (0, 180, 90, -90, 45, -45, 135, -135)]
            done = False
            for dist in (0.55, 0.85, 1.15, 1.5, 1.9):
                for a in dirs:
                    ex = abs(math.cos(a)) * sx / 2 + abs(math.sin(a)) * sy / 2
                    vx = round(px + math.cos(a) * (ex + VIA_D / 2 + dist - 0.3), 3)
                    vy = round(py + math.sin(a) * (ex + VIA_D / 2 + dist - 0.3), 3)
                    if _blocked(board, vx, vy, VIA_D / 2, gnd, vias + existing, keepouts):
                        continue
                    if _seg_blocked(board, (px, py), (vx, vy), p.GetLayer(), gnd, p):
                        continue
                    add_via(board, gnd_net, vx, vy)
                    track(board, gnd_net, p.GetLayer(), [(px, py), (vx, vy)], FAN_W)
                    vias.append((vx, vy)); done = True
                    break
                if done:
                    break
            if not done:
                missed.append("%s.%s" % (fp.GetReference(), p.GetNumber()))
    return vias, missed


def esp_ground_stubs(board, fp, gnd_net):
    """Tie each ESP32 perimeter GND pad inward to the exposed pad. The area
    inside the pad ring is empty, so these can't collide with anything, and
    they give every module ground pin a short path to the EPAD vias."""
    gnd = gnd_net.GetNetname()
    c = fp.GetPosition(); cx, cy = pcbnew.ToMM(c.x), pcbnew.ToMM(c.y)
    epad = [(pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y))
            for p in fp.Pads() if p.GetNumber() == "49"]
    made = 0
    for p in fp.Pads():
        if p.GetNetname() != gnd or p.GetNumber() == "49" or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            continue
        px, py = pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)
        dx, dy = px - cx, py - cy
        if abs(dx) > abs(dy):   # left/right column -> step inward in x
            ix, iy = px - (1.0 if dx > 0 else -1.0), py
        else:                   # top/bottom row -> step inward in y
            ix, iy = px, py - (1.0 if dy > 0 else -1.0)
        ex, ey = min(epad, key=lambda e: (e[0] - ix) ** 2 + (e[1] - iy) ** 2)
        if _seg_blocked(board, (px, py), (ix, iy), pcbnew.F_Cu, gnd, p) or \
                _seg_blocked(board, (ix, iy), (ex, ey), pcbnew.F_Cu, gnd, p):
            continue
        track(board, gnd_net, pcbnew.F_Cu, [(px, py), (ix, iy), (ex, ey)], 0.3)
        made += 1
    return made


def main():
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(MM(1.6))
    # drill/place origin at the bottom-left corner: Gerbers and CPL share it
    ds.SetAuxOrigin(pcbnew.VECTOR2I(0, MM(H)))
    ds.SetGridOrigin(pcbnew.VECTOR2I(0, MM(H)))
    # JLCPCB standard capabilities, with margin
    ds.m_TrackMinWidth = MM(0.15)
    ds.m_MinClearance = MM(0.15)
    ds.m_ViasMinSize = MM(0.5)
    ds.m_MinThroughDrill = MM(0.3)
    ds.m_CopperEdgeClearance = MM(0.3)
    ds.m_HoleToHoleMin = MM(0.5)
    ds.m_HoleClearance = MM(0.2)

    nets = {}
    def net(name):
        if name not in nets:
            n = pcbnew.NETINFO_ITEM(board, name)
            board.Add(n)
            nets[name] = n
        return nets[name]

    fps = {}
    for part in design.PARTS:
        ref = part["ref"]
        path, name, fpid = fp_source(part)
        fp = pcbnew.FootprintLoad(path, name)
        if fp is None:
            raise RuntimeError("footprint %s not found" % fpid)
        fp.SetFPIDAsString(fpid)
        fp.SetReference(ref)
        fp.SetValue(part["value"])
        x, y, rot = PLACE[ref]
        fp.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        fp.SetOrientationDegrees(rot)
        # link to the schematic symbol so "Update PCB from Schematic" works
        fp.SetPath(pcbnew.KIID_PATH("/%s/%s" % (ROOT, uid("sym", ref))))
        if part.get("lcsc"):
            f = pcbnew.PCB_FIELD(fp, pcbnew.FIELD_T_USER, "LCSC")
            f.SetText(part["lcsc"]); f.SetVisible(False); f.SetLayer(pcbnew.F_Fab)
            f.SetOrdinal(fp.GetNextFieldOrdinal())
            fp.Add(f)
        else:
            fp.SetExcludedFromBOM(True)
            fp.SetExcludedFromPosFiles(True)
        sym = Symbol(part["sym"])
        pinnet = {p["number"]: n for p, n in sym.resolve(part["pins"])}
        for pad in fp.Pads():
            n = pinnet.get(pad.GetNumber())
            if n:
                pad.SetNet(net(n))
        if ref == "U1":
            # buck thermal pad: solid (not spoked) into the GND pour
            for pad in fp.Pads():
                if pad.GetNumber() == "9":
                    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
        # keep reference text small and out of the way
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(MM(0.8), MM(0.8)))
        fp.Reference().SetTextThickness(MM(0.12))
        board.Add(fp)
        fps[ref] = fp

    add_outline(board)

    # --- ESP32 antenna keepout: no copper under the antenna, both layers ---
    ux, uy = PLACE["U2"][0], PLACE["U2"][1]
    add_keepout(board, [(ux - 9.0, -1.0), (ux + 9.0, -1.0), (ux + 9.0, uy - 5.55),
                        (ux - 9.0, uy - 5.55)], "ESP32 antenna keepout")

    # --- GND pours: full board both sides (keepout carves the antenna) ---
    outline = [(0.3, 0.3), (W - 0.3, 0.3), (W - 0.3, H - 0.3), (0.3, H - 0.3)]
    add_zone(board, net(design.GND), pcbnew.B_Cu, outline, 0)
    add_zone(board, net(design.GND), pcbnew.F_Cu, outline, 0)

    # --- RF pre-route: GPS RF_IN (pad 11) -> u.FL signal pad, ~2 mm ---
    gx, gy = pad_xy(fps["U3"], "11")
    jx, jy = pad_xy(fps["J2"], "1")
    track(board, net(design.RF_ANT), pcbnew.F_Cu, [(gx, gy), (jx, jy)], 0.5)
    lx, ly = pad_xy(fps["L2"], "2")
    track(board, net(design.RF_ANT), pcbnew.F_Cu, [(lx, ly), (lx, jy)], 0.4)
    # VCC_RF (pad 14) -> L2 pad 1
    vx, vy = pad_xy(fps["U3"], "14")
    l1x, l1y = pad_xy(fps["L2"], "1")
    track(board, net(design.VCC_RF), pcbnew.F_Cu, [(vx, vy), (l1x, vy), (l1x, l1y)], 0.3)

    # --- Buck hand-routes (review item: tight input loop, quiet FB) --------
    # Done before autorouting so Freerouting can't meander these. The input
    # HF loop is C3 <-> VIN(3) / GND(1): ~2.5 mm of copper each side.
    u1 = fps["U1"]
    g = net(design.GND); vb = net(design.VBUCK); swn = net(design.SW)
    P = lambda ref, pad: pad_xy(fps[ref], pad)
    track(board, g,  pcbnew.F_Cu, [P("U1", "1"), P("C3", "2")], 0.6)
    track(board, vb, pcbnew.F_Cu, [P("U1", "3"), P("C3", "1")], 0.6)
    track(board, vb, pcbnew.F_Cu, [P("U1", "2"), P("U1", "3")], 0.3)        # EN -> VIN
    # exposed pad -> GND pin (no via-in-pad)
    ex, ey = P("U1", "9"); gx1, gy1 = P("U1", "1")
    track(board, g, pcbnew.F_Cu, [(ex - 1.2, ey + 1.0), (gx1, gy1 - 0.7)], 0.4)
    # SW node: pin 8 straight up into L1's pad, BOOT cap tapped off it
    sx, sy = P("U1", "8"); lx1, ly1 = P("L1", "1")
    track(board, swn, pcbnew.F_Cu, [(sx, sy), (sx, ly1 + 1.2)], 0.8)
    c4b, c4s = P("C4", "1"), P("C4", "2")
    track(board, swn, pcbnew.F_Cu, [c4s, (sx, c4s[1])], 0.4)
    track(board, net(design.BST), pcbnew.F_Cu, [c4b, P("U1", "7")], 0.4)
    # FB: pin 5 -> divider midpoint, kept on the side away from L1/SW
    fb = net(design.BUCK_FB)
    track(board, fb, pcbnew.F_Cu, [P("U1", "5"), P("R18", "1")], 0.25)
    track(board, fb, pcbnew.F_Cu, [P("R18", "1"), P("R17", "2")], 0.25)
    track(board, net(design.BUCK_RT), pcbnew.F_Cu, [P("U1", "4"), P("R16", "1")], 0.25)

    # --- u.FL launch: extra ground stitching around the connector ---------
    jx, jy = pcbnew.ToMM(fps["J2"].GetPosition().x), pcbnew.ToMM(fps["J2"].GetPosition().y)
    ring = []
    import math
    for k in range(16):
        a = 2 * math.pi * k / 16
        for r in (2.3, 2.7):
            x, y = round(jx + r * math.cos(a), 3), round(jy + r * math.sin(a), 3)
            if not _blocked(board, x, y, VIA_D / 2 + 0.1, "GND", ring, []):   # RF rule is 0.25
                add_via(board, g, x, y); ring.append((x, y)); break
    print("u.FL stitching vias:", len(ring))

    # --- GND fanout vias (before autorouting) ---
    keepouts = [(ux - 9.0, -1.0, ux + 9.0, uy - 5.55)]
    print("ESP32 GND stubs to EPAD:", esp_ground_stubs(board, fps["U2"], net(design.GND)))
    vias, missed = fanout_gnd(board, net(design.GND), keepouts)
    print("GND fanout: %d vias, unplaced: %s" % (len(vias), missed or "none"))

    # --- silkscreen ---
    for text, x, y, size, side in SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
        t.SetTextThickness(MM(max(0.12, size * 0.15)))
        t.SetLayer(pcbnew.F_SilkS if side == "F" else pcbnew.B_SilkS)
        if side == "B":
            t.SetMirrored(True)
        board.Add(t)

    out = os.path.join(PROJ, "gpsspeed.kicad_pcb")
    board.Save(out)
    print("wrote", out, "footprints:", len(fps), "nets:", len(nets))
    # report key pad positions so placement can be sanity-checked
    for ref, pads in [("J1", "123"), ("U3", ["2", "3", "8", "11", "14"]), ("J2", "123")]:
        print(" ", ref, {p: tuple(round(v, 2) for v in pad_xy(fps[ref], p)) for p in pads})


if __name__ == "__main__":
    main()
