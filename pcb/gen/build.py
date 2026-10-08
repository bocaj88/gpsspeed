"""One-shot board build:  generate -> autoroute -> pour -> stitch -> DRC.

    ~/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 build.py

Needs Freerouting (~/Applications/freerouting) and a Java >= 25
(/opt/homebrew/opt/openjdk). Leaves the routed board in pcb/gpsspeed.kicad_pcb.
"""
import os
import subprocess
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_pcb  # noqa: E402
from gen_pcb import MM, H, W, _blocked, add_via  # noqa: E402

PROJ = os.path.dirname(HERE)
PCB = os.path.join(PROJ, "gpsspeed.kicad_pcb")
ROUTE = os.path.join(PROJ, "route")
JAVA = "/opt/homebrew/opt/openjdk/bin/java"
FREEROUTING = os.path.expanduser("~/Applications/freerouting/freerouting-2.4.1.jar")
KICAD_CLI = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")


def autoroute(board):
    os.makedirs(ROUTE, exist_ok=True)
    dsn, ses = os.path.join(ROUTE, "gpsspeed.dsn"), os.path.join(ROUTE, "gpsspeed.ses")
    for f in (ses,):
        if os.path.exists(f):
            os.remove(f)
    # Define SignalOut on the in-memory board. The project file copy is not
    # enough: gen_pcb ran in this same process, so KiCad serves its cached
    # (class-less) project instead of re-reading the file.
    ns = board.GetDesignSettings().m_NetSettings
    d = ns.GetDefaultNetclass()
    nc = pcbnew.NETCLASS("SignalOut")
    nc.SetClearance(d.GetClearance()); nc.SetTrackWidth(MM(0.5))
    nc.SetViaDiameter(d.GetViaDiameter()); nc.SetViaDrill(d.GetViaDrill())
    ns.SetNetclass("SignalOut", nc)
    ns.SetNetclassPatternAssignment("*SIGNAL_*", "SignalOut")
    board.SynchronizeNetsAndNetClasses(False)
    assert pcbnew.ExportSpecctraDSN(board, dsn)
    assert "(class SignalOut" in open(dsn).read(), "SignalOut class missing from DSN"
    with open(os.path.join(ROUTE, "freerouting.log"), "w") as log:
        subprocess.run([JAVA, "-Djava.awt.headless=true", "-jar", FREEROUTING,
                        "--gui.enabled=false", "-de", dsn, "-do", ses, "-mp", "60", "-mt", "4"],
                       stdout=log, stderr=subprocess.STDOUT, timeout=1800, check=True)
    summary = [l for l in open(os.path.join(ROUTE, "freerouting.log"))
               if "Auto-routing stage completed" in l or "unrouted connection" in l]
    for l in summary:
        print("  freerouting:", l.strip().split("] ", 1)[-1][:160])
    assert pcbnew.ImportSpecctraSES(board, ses)


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def stitch(board, pitch=3.2):
    """Drop GND vias wherever BOTH layers are poured GND, tying every top-side
    pour island to the solid bottom plane."""
    gnd = board.FindNet("GND")
    zones = {z.GetLayer(): z for z in board.Zones() if z.GetNetname() == "GND" and not z.GetIsRuleArea()}
    ux, uy = gen_pcb.PLACE["U2"][0], gen_pcb.PLACE["U2"][1]
    keepouts = [(ux - 9.0, -1.0, ux + 9.0, uy - 5.55)]
    vias = [(pcbnew.ToMM(v.GetPosition().x), pcbnew.ToMM(v.GetPosition().y))
            for v in board.GetTracks() if v.GetClass() == "PCB_VIA"]
    added = 0
    y = 1.6
    while y < H - 1.2:
        x = 1.6
        while x < W - 1.2:
            p = pcbnew.VECTOR2I(MM(x), MM(y))
            inside = all(zones[l].GetFilledPolysList(l).Contains(p) for l in (pcbnew.F_Cu, pcbnew.B_Cu))
            # stay well inside the pour so the via is fully surrounded by copper
            if inside and not _blocked(board, x, y, 0.3 + 0.35, "GND", vias, keepouts):
                add_via(board, gnd, x, y)
                vias.append((x, y))
                added += 1
            x += pitch
        y += pitch
    return added


def stitch_islands(board):
    """Join every stranded piece of GND pour to the main ground.

    Pour islands on both layers are graph nodes; GND vias and plated GND
    holes are edges. Any component not joined to the main ground (the one
    containing the biggest bottom island) gets a via where its copper
    overlaps main-ground copper on the other layer."""
    gnd = board.FindNet("GND")
    zones = {z.GetLayer(): z for z in board.Zones() if z.GetNetname() == "GND" and not z.GetIsRuleArea()}
    ux, uy = gen_pcb.PLACE["U2"][0], gen_pcb.PLACE["U2"][1]
    keepouts = [(ux - 9.0, -1.0, ux + 9.0, uy - 5.55)]
    polys = {l: zones[l].GetFilledPolysList(l) for l in (pcbnew.F_Cu, pcbnew.B_Cu)}
    nodes = [(l, i) for l in polys for i in range(polys[l].OutlineCount())]
    parent = {n: n for n in nodes}

    def find(n):
        while parent[n] != n:
            parent[n] = parent[parent[n]]
            n = parent[n]
        return n

    def island_at(layer, pt):
        ps = polys[layer]
        for i in range(ps.OutlineCount()):
            if ps.Contains(pt, i):
                return (layer, i)
        return None

    joints = [v.GetPosition() for v in board.GetTracks()
              if v.GetClass() == "PCB_VIA" and v.GetNetname() == "GND"]
    joints += [p.GetPosition() for fp in board.GetFootprints() for p in fp.Pads()
               if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    for pt in joints:
        a_, b_ = island_at(pcbnew.F_Cu, pt), island_at(pcbnew.B_Cu, pt)
        if a_ and b_:
            parent[find(a_)] = find(b_)

    bot = polys[pcbnew.B_Cu]
    main_node = max(((pcbnew.B_Cu, i) for i in range(bot.OutlineCount())),
                    key=lambda n: abs(bot.Outline(n[1]).Area()))
    main = find(main_node)
    vias = [(pcbnew.ToMM(v.x), pcbnew.ToMM(v.y)) for v in joints]
    added, done = 0, set()
    for n in nodes:
        root = find(n)
        if root == main or root in done:
            continue
        layer, idx = n
        other = pcbnew.B_Cu if layer == pcbnew.F_Cu else pcbnew.F_Cu
        bb = polys[layer].Outline(idx).BBox()
        x0, y0 = pcbnew.ToMM(bb.GetX()), pcbnew.ToMM(bb.GetY())
        x1, y1 = x0 + pcbnew.ToMM(bb.GetWidth()), y0 + pcbnew.ToMM(bb.GetHeight())
        y = y0 + 0.35
        placed = False
        while y < y1 and not placed:
            x = x0 + 0.35
            while x < x1 and not placed:
                ring = [pcbnew.VECTOR2I(MM(x + dx), MM(y + dy)) for dx, dy in
                        ((0, 0), (0.33, 0), (-0.33, 0), (0, 0.33), (0, -0.33))]
                o = island_at(other, ring[0])
                if o and find(o) == main and all(polys[layer].Contains(q, idx) for q in ring) \
                        and all(polys[other].Contains(q, o[1]) for q in ring) \
                        and not _blocked(board, x, y, 0.3, "GND", vias, keepouts):
                    add_via(board, gnd, x, y); vias.append((x, y)); added += 1; placed = True
                    parent[root] = main
                    done.add(root)
                x += 0.2
            y += 0.2
        if not placed:
            print("  WARNING: could not bridge GND island at (%.1f, %.1f) on %s"
                  % ((x0 + x1) / 2, (y0 + y1) / 2, pcbnew.LayerName(layer)))
    return added


def set_severities():
    """Board.Save() rewrites the project file with default severities, so
    apply our overrides afterwards. starved_thermal: every GND pad also has a
    dedicated fanout via, so a single thermal spoke is not a real problem."""
    import json
    pro = os.path.join(PROJ, "gpsspeed.kicad_pro")
    d = json.load(open(pro))
    rs = d.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rule_severities", {})
    rs["starved_thermal"] = "warning"
    # Drill spacing is a fab limit, not a nicety: fail the build on it
    # (KiCad's default here is only a warning, which let rev 1.1 ship two
    # overlapping via drills by the u.FL).
    rs["hole_to_hole"] = "error"
    rs["holes_co_located"] = "error"
    json.dump(d, open(pro, "w"), indent=2)


def set_netclasses():
    """gen_pcb writes the project file from an empty board, which drops any
    net classes, so apply ours before the board is reloaded for routing.
    SIGNAL nets carry up to Q1's 1.7A current limit if SIGNAL is miswired to
    +12V, so they get 0.5 mm copper (rev 1.3)."""
    import copy
    import json
    pro = os.path.join(PROJ, "gpsspeed.kicad_pro")
    d = json.load(open(pro))
    ns = d.setdefault("net_settings", {})
    classes = ns.setdefault("classes", [])
    default = next(c for c in classes if c.get("name") == "Default")
    hc = copy.deepcopy(default)
    hc.update(name="SignalOut", track_width=0.5, priority=0)
    ns["classes"] = [c for c in classes if c.get("name") != "SignalOut"] + [hc]
    ns["netclass_patterns"] = [{"netclass": "SignalOut", "pattern": "*SIGNAL_*"}]
    json.dump(d, open(pro, "w"), indent=2)


def drc(path, tag):
    rpt = os.path.join(ROUTE, "drc_%s.rpt" % tag)
    out = subprocess.run([KICAD_CLI, "pcb", "drc", "--severity-error", "--schematic-parity",
                          "-o", rpt, path], capture_output=True, text=True)
    lines = [l for l in out.stdout.splitlines() if "Found" in l]
    print("  DRC:", "; ".join(lines))
    return rpt


def widen_necks(board, min_w=0.15):
    """Freerouting sometimes necks a trace down at a pad entry. Anything under
    the fab minimum gets widened back; DRC then checks the clearance."""
    n = 0
    for t in board.GetTracks():
        if t.GetClass() == "PCB_TRACK" and t.GetWidth() < MM(min_w):
            t.SetWidth(MM(min_w)); n += 1
    return n


def drc_clean(rpt):
    txt = open(rpt).read()
    import re
    v = re.findall(r"\*\* Found (\d+) DRC violations", txt)
    u = re.findall(r"\*\* Found (\d+) unconnected pads", txt)
    return (int(v[0]) if v else -1) == 0 and (int(u[0]) if u else -1) == 0


def main(attempts=4):
    for attempt in range(1, attempts + 1):
        print("=== build attempt %d ===" % attempt)
        if build_once():
            print("  clean on attempt %d" % attempt)
            return
    raise SystemExit("board not DRC-clean after %d attempts" % attempts)


def build_once():
    gen_pcb.main()
    set_netclasses()
    board = pcbnew.LoadBoard(PCB)          # reload so project net classes apply
    print("autorouting ...")
    autoroute(board)
    print("  necked-down tracks widened:", widen_necks(board))
    fill(board)
    n = stitch(board)
    fill(board)
    for _ in range(3):              # islands can split further after refill
        k = stitch_islands(board)
        fill(board)
        n += k
        if not k:
            break
    print("  stitching vias added:", n)
    board.Save(PCB)
    set_severities()
    set_netclasses()
    ntr = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    nv = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print("  tracks %d, vias %d" % (ntr, nv))
    return drc_clean(drc(PCB, "final"))


if __name__ == "__main__":
    main()
