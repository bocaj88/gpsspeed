"""Minimal KiCad S-expression reader/writer.

KiCad files are Lisp-ish trees. Strings are double-quoted with backslash
escapes; everything else is a bare atom. We keep quoted strings as a
distinct type so they round-trip with their quotes intact.
"""


class Q(str):
    """A quoted string atom (so we re-emit it with quotes)."""


def parse(text):
    tokens = _tokenize(text)
    pos = 0

    def read():
        nonlocal pos
        tok = tokens[pos]
        pos += 1
        if tok == "(":
            lst = []
            while tokens[pos] != ")":
                lst.append(read())
            pos += 1
            return lst
        return tok

    out = []
    while pos < len(tokens):
        out.append(read())
    return out[0] if len(out) == 1 else out


def _tokenize(text):
    toks, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            i += 1
        elif c in "()":
            toks.append(c)
            i += 1
        elif c == '"':
            j, buf = i + 1, []
            while text[j] != '"':
                if text[j] == "\\":
                    buf.append(text[j:j + 2])
                    j += 2
                else:
                    buf.append(text[j])
                    j += 1
            toks.append(Q("".join(buf)))
            i = j + 1
        else:
            j = i
            while j < n and text[j] not in ' \t\r\n()"':
                j += 1
            toks.append(text[i:j])
            i = j
    return toks


def dump(node, indent=0):
    """Serialize back to text. Short lists stay on one line."""
    if not isinstance(node, list):
        return '"%s"' % node if isinstance(node, Q) else str(node)
    flat = "(" + " ".join(dump(x) for x in node) + ")"
    if len(flat) < 100 and not any(isinstance(x, list) and len(x) > 3 for x in node):
        return flat
    pad = "  " * (indent + 1)
    head = [dump(x) for x in node[:2] if not isinstance(x, list)]
    rest = node[len(head):]
    s = "(" + " ".join(head)
    for x in rest:
        s += "\n" + pad + dump(x, indent + 1)
    return s + "\n" + "  " * indent + ")"


def find(node, key):
    """All direct children lists whose first atom is `key`."""
    return [x for x in node if isinstance(x, list) and x and x[0] == key]


def first(node, key, default=None):
    r = find(node, key)
    return r[0] if r else default
