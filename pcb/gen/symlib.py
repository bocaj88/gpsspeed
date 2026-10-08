"""Load symbols from the project library and KiCad's stock libraries."""
import copy
import os

from sexp import Q, find, first, parse

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
PROJECT_SYMLIB = os.path.join(PROJ, "lib", "gpsspeed.kicad_sym")
KICAD = os.path.expanduser("~/Applications/KiCad/KiCad.app/Contents/SharedSupport")
STOCK_SYMBOLS = os.path.join(KICAD, "symbols")
STOCK_FOOTPRINTS = os.path.join(KICAD, "footprints")
PROJECT_FOOTPRINTS = os.path.join(PROJ, "lib", "gpsspeed.pretty")

_cache = {}


def _load(path):
    if path not in _cache:
        tree = parse(open(path, encoding="utf-8").read())
        _cache[path] = {s[1]: s for s in find(tree, "symbol")}
    return _cache[path]


class Symbol:
    def __init__(self, lib_id):
        if ":" in lib_id:
            lib, name = lib_id.split(":", 1)
            path = os.path.join(STOCK_SYMBOLS, lib + ".kicad_sym")
        else:
            lib, name = "gpsspeed", lib_id
            path = PROJECT_SYMLIB
        syms = _load(path)
        if name not in syms:
            raise KeyError("symbol %s not found in %s" % (name, path))
        self.lib, self.name = lib, name
        self.lib_id = "%s:%s" % (lib, name)
        self.tree = syms[name]
        self.props = {p[1]: p[2] for p in find(self.tree, "property")}
        self.pins = []
        for sub in find(self.tree, "symbol"):
            # sub-symbol names are NAME_<unit>_<style>; skip De Morgan style 2
            if not sub[1].endswith("_1") and not sub[1].endswith("_0"):
                continue
            for p in find(sub, "pin"):
                at = first(p, "at")
                self.pins.append(dict(
                    type=p[1],
                    name=str(first(p, "name")[1]),
                    number=str(first(p, "number")[1]),
                    x=float(at[1]), y=float(at[2]),
                    angle=float(at[3]) if len(at) > 3 else 0.0,
                ))

    @property
    def footprint(self):
        return str(self.props.get("Footprint", ""))

    def embedded(self):
        """The symbol definition as it must appear inside a schematic's
        lib_symbols block: top-level name gets the library prefix."""
        t = copy.deepcopy(self.tree)
        t[1] = Q(self.lib_id)
        return t

    def resolve(self, pinmap):
        """pin name/number -> net  ==>  list of (pin, net-or-None)."""
        numbers = {p["number"] for p in self.pins}
        out = []
        for p in self.pins:
            net = None
            if p["number"] in pinmap and p["number"] in numbers:
                net = pinmap[p["number"]]
            elif p["name"] in pinmap:
                net = pinmap[p["name"]]
            out.append((p, net))
        used = {k for k in pinmap}
        known = numbers | {p["name"] for p in self.pins}
        missing = used - known
        if missing:
            raise KeyError("%s: pins %s not on symbol" % (self.lib_id, sorted(missing)))
        return out
