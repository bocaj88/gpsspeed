"""JLCPCB fabrication + assembly outputs.

    python3 gen_outputs.py      (plain Python; shells out to kicad-cli)

Writes pcb/out/jlcpcb/:
    gerbers.zip            upload as the PCB
    bom_jlcpcb.csv         upload as the BOM         (Comment,Designator,Footprint,LCSC Part #)
    cpl_jlcpcb.csv         upload as the CPL / pick-and-place
All coordinates use the drill/place origin at the board's bottom-left corner,
so the Gerbers and the CPL share one origin.
"""
import csv
import os
import shutil
import subprocess
import sys
import zipfile
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design  # noqa: E402
from symlib import PROJ, Symbol  # noqa: E402

CLI = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
PCB = os.path.join(PROJ, "gpsspeed.kicad_pcb")
SCH = os.path.join(PROJ, "gpsspeed.kicad_sch")
OUT = os.path.join(PROJ, "out", "jlcpcb")
GERB = os.path.join(OUT, "gerbers")

LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts"


def run(*args):
    r = subprocess.run([CLI, *args], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr)
        raise SystemExit("kicad-cli failed: %s" % " ".join(args[:3]))
    return r.stdout


def gerbers():
    shutil.rmtree(GERB, ignore_errors=True)
    os.makedirs(GERB)
    run("pcb", "export", "gerbers", "-l", LAYERS, "--use-drill-file-origin",
        "--subtract-soldermask", "-o", GERB + "/", PCB)
    run("pcb", "export", "drill", "--format", "excellon", "--drill-origin", "plot",
        "--excellon-units", "mm", "--excellon-zeros-format", "decimal",
        "--excellon-oval-format", "alternate", "--excellon-separate-th", "-o", GERB + "/", PCB)
    zpath = os.path.join(OUT, "gerbers.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(GERB)):
            z.write(os.path.join(GERB, f), f)
    return zpath, sorted(os.listdir(GERB))


def bom():
    groups = OrderedDict()
    for p in design.PARTS:
        if not p.get("lcsc"):
            continue
        fp = (p.get("fp") or Symbol(p["sym"]).footprint).split(":")[-1]
        g = groups.setdefault(p["lcsc"], dict(comment=p["value"], fp=fp, refs=[]))
        g["refs"].append(p["ref"])
    path = os.path.join(OUT, "bom_jlcpcb.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for lcsc, g in groups.items():
            w.writerow([g["comment"], ",".join(g["refs"]), g["fp"], lcsc])
    return path, groups


def cpl():
    raw = os.path.join(OUT, "pos_raw.csv")
    run("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both",
        "--use-drill-file-origin", "--exclude-dnp", "-o", raw, PCB)
    assembled = {p["ref"] for p in design.PARTS if p.get("lcsc")}
    rows = []
    with open(raw) as f:
        for r in csv.DictReader(f):
            if r["Ref"] not in assembled:
                continue
            rows.append([r["Ref"], "%.4fmm" % float(r["PosX"]), "%.4fmm" % float(r["PosY"]),
                         "Top" if r["Side"].lower() == "top" else "Bottom",
                         "%g" % (float(r["Rot"]) % 360)])
    os.remove(raw)
    path = os.path.join(OUT, "cpl_jlcpcb.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(rows, key=lambda r: (r[0].rstrip("0123456789"), int("".join(c for c in r[0] if c.isdigit()) or 0))))
    missing = assembled - {r[0] for r in rows}
    return path, len(rows), missing


def docs():
    run("sch", "export", "pdf", "-o", os.path.join(PROJ, "out", "gpsspeed_schematic.pdf"), SCH)
    run("pcb", "export", "pdf", "-l", "F.Cu,F.Silkscreen,F.Fab,Edge.Cuts", "--mode-single",
        "-o", os.path.join(PROJ, "out", "gpsspeed_top_assembly.pdf"), PCB)
    run("pcb", "export", "step", "--subst-models", "-f", "-o", os.path.join(PROJ, "out", "gpsspeed.step"), PCB)


def main():
    os.makedirs(OUT, exist_ok=True)
    z, files = gerbers()
    print("gerbers:", z)
    for f in files:
        print("   ", f)
    b, groups = bom()
    print("bom:", b, "(%d unique parts, %d placements)" % (len(groups), sum(len(g["refs"]) for g in groups.values())))
    c, n, missing = cpl()
    print("cpl:", c, "(%d placements)" % n, "MISSING: %s" % sorted(missing) if missing else "")
    docs()
    print("docs: out/gpsspeed_schematic.pdf, out/gpsspeed_top_assembly.pdf, out/gpsspeed.step")


if __name__ == "__main__":
    main()
