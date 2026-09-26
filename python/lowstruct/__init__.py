"""lowstruct — a strict configuration format in Lowent surface syntax.

    >>> doc = loads('server do\\n  port 8080 .\\nend\\n')
    >>> doc.get("server port").one()
    8080

The format is defined by spec/lowstruct.md. Values keep their literal kind:

    int      Python int (-2**63 .. 2**64-1)
    float    Python float (finite)
    bool     Python bool
    str      bytes  (a "..." literal is a byte string; use Leaf.text() for UTF-8 text)
    u_str    list of UTF-16 code units
    U_str    list of code points
    char / u_char / U_char   int
"""

import re
import struct

__all__ = ["loads", "load", "dumps_canonical", "LowsError", "Branch", "Leaf", "KINDS"]
__version__ = "0.1.0"

KINDS = ("int", "float", "bool", "str", "u_str", "U_str", "char", "u_char", "U_char")

# rem note text do end true false are reserved by the lexer: they never become names.
_ESCAPES = {"\\": 0x5C, '"': 0x22, "'": 0x27, "a": 0x07, "b": 0x08, "f": 0x0C,
            "n": 0x0A, "r": 0x0D, "t": 0x09, "v": 0x0B, "0": 0x00}
_NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_D, _H = r"[0-9](?:_?[0-9])*", r"[0-9A-Fa-f](?:_?[0-9A-Fa-f])*"
# Lowent annex A.6; a float is tried before an integer.
_NUM_RE = re.compile(
    rf"[+-]?(?:(?P<hexfloat>0[xX]{_H}(?:\.{_H})?[pP][+-]?{_D})"
    rf"|(?P<decfloat>{_D}(?:\.{_D}(?:[eE][+-]?{_D})?|[eE][+-]?{_D}))"
    rf"|(?P<hex>0[xX]{_H})|(?P<bin>0[bB][01](?:_?[01])*)|(?P<dec>{_D}))")
_INT_MIN, _INT_MAX = -(1 << 63), (1 << 64) - 1
_STR_KIND = {"": "str", "u": "u_str", "U": "U_str"}
_CHAR_KIND = {"": "char", "u": "u_char", "U": "U_char"}
_VALUE_KINDS = frozenset(KINDS)
MAX_DEPTH = 64  # nested `do` blocks; deeper is E-LOWS-DEPTH (spec 4.3)


def _is_digit(c):
    return "0" <= c <= "9"


def _is_word(c):
    return c == "_" or "0" <= c <= "9" or "a" <= c <= "z" or "A" <= c <= "Z"


class LowsError(ValueError):
    """A document that is not lowstruct. `code` is the stable diagnostic (E-LOWS-...)."""

    def __init__(self, line, col, code, msg):
        super().__init__(f"{line}:{col} {code}: {msg}")
        self.line, self.col, self.code, self.msg = line, col, code, msg


class Leaf:
    """A key that holds values: all of one kind, possibly none."""

    __slots__ = ("kind", "values")

    def __init__(self, kind, values):
        self.kind, self.values = kind, values

    def one(self):
        """The single value; LookupError unless there is exactly one."""
        if len(self.values) != 1:
            raise LookupError(f"expected exactly one value, found {len(self.values)}")
        return self.values[0]

    def text(self):
        """The single string value as Python text (str: strict UTF-8, u_str/U_str: decoded)."""
        v = self.one()
        if self.kind == "str":
            return v.decode("utf-8")
        if self.kind == "U_str":
            return "".join(map(chr, v))
        if self.kind == "u_str":
            return struct.pack(f"<{len(v)}H", *v).decode("utf-16-le")
        raise TypeError(f"not a string: {self.kind}")

    def __repr__(self):
        return f"Leaf({self.kind!r}, {self.values!r})"


class Branch(dict):
    """A key that holds keys. Iteration follows source order."""

    def get(self, path, default=None):
        """Look up a path: 'a b c', ('a', 'b', 'c') or a single name."""
        node = self
        for name in (path.split() if isinstance(path, str) else path):
            if not isinstance(node, Branch) or name not in node:
                return default
            node = dict.__getitem__(node, name)
        return node


def loads(src):
    """Parse a document from str or UTF-8 bytes. Raises LowsError."""
    if isinstance(src, (bytes, bytearray, memoryview)):
        src = _decode(bytes(src))
    return _Parser(_lex(src)).document()


def load(fp):
    """Parse a document from a binary or text file object."""
    return loads(fp.read())


def _decode(data):
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as e:
        head = data[:e.start]
        line = head.count(b"\n") + 1
        col = len(head[head.rfind(b"\n") + 1:].decode("utf-8")) + 1
        raise LowsError(line, col, "E-LOWS-UTF8", "the file is not valid UTF-8") from None


class _Tok:
    __slots__ = ("kind", "val", "line", "col")

    def __init__(self, kind, val, line, col):
        self.kind, self.val, self.line, self.col = kind, val, line, col


def _lex(src):
    if src.startswith("﻿"):
        raise LowsError(1, 1, "E-LOWS-BOM", "byte order mark is not allowed; the file is UTF-8 without BOM")
    src = src.replace("\r\n", "\n")
    if "\r" in src:
        i = src.index("\r")
        line = src.count("\n", 0, i) + 1
        raise LowsError(line, i - (src.rfind("\n", 0, i) + 1) + 1, "E-LOWS-CR", "a lone carriage return is not a line end")
    toks, i, n = [], 0, len(src)
    line, lstart = 1, 0

    def pos(at):
        return line, at - lstart + 1

    def err(at, code, msg):
        raise LowsError(*pos(at), code, msg)

    def elements(raw, prefix, at):
        # Bytes -> the elements the prefix names: bytes, UTF-16 code units, or code points.
        if prefix == "":
            return bytes(raw)
        try:
            text = bytes(raw).decode("utf-8")
        except UnicodeDecodeError:
            err(at, "E-LOWS-ESCAPE", "a multi-byte character is cut short in this literal")
        if prefix == "U":
            return [ord(c) for c in text]
        u16 = text.encode("utf-16-le")
        return [u16[k] | (u16[k + 1] << 8) for k in range(0, len(u16), 2)]

    def quoted(at, prefix):
        # Lex a "string" or 'c' starting at the quote; return the index after it.
        q, start, j, raw = src[at], at - len(prefix), at + 1, bytearray()
        while True:
            if j >= n or src[j] == "\n":
                err(start, "E-LOWS-UNCLOSED", "literal is not closed on its line")
            c = src[j]
            if c == q:
                j += 1
                break
            if c == "\\":
                e = src[j + 1] if j + 1 < n else ""
                if e in _ESCAPES:
                    raw.append(_ESCAPES[e])
                    j += 2
                    continue
                width = {"x": 2, "u": 4, "U": 8}.get(e)
                hexpart = src[j + 2:j + 2 + width] if width else ""
                if not (width and len(hexpart) == width and re.fullmatch(r"[0-9A-Fa-f]+", hexpart)):
                    err(j, "E-LOWS-ESCAPE", f"escape '\\{e}' is not in the closed set")
                v = int(hexpart, 16)
                if e == "x":
                    raw.append(v)
                else:
                    if 0xD800 <= v <= 0xDFFF or v > 0x10FFFF:
                        err(j, "E-LOWS-ESCAPE", "not a code point")
                    raw += chr(v).encode("utf-8")
                j += 2 + width
                continue
            raw += c.encode("utf-8")
            j += 1
        els = elements(raw, prefix, start)
        if q == '"':
            toks.append(_Tok(_STR_KIND[prefix], els, *pos(start)))
            return j
        if len(els) == 0:
            err(start, "E-LOWS-CHAR-EMPTY", "an empty character literal")
        if len(els) > 1:
            unit = {"": "byte", "u": "UTF-16 unit", "U": "code point"}[prefix]
            err(start, "E-LOWS-CHAR-WIDTH", f"this character does not fit one {unit}")
        toks.append(_Tok(_CHAR_KIND[prefix], els[0], *pos(start)))
        return j

    while i < n:
        ch = src[i]
        if ch == "\n":
            i += 1
            line, lstart = line + 1, i
            continue
        if ch in " \t":
            i += 1
            continue
        if ch == ".":
            toks.append(_Tok("dot", ".", *pos(i)))
            i += 1
            continue
        if ch in "\"'":
            i = quoted(i, "")
            continue
        m = _NAME_RE.match(src, i)
        if m:
            word = m.group(0)
            if m.end() < n and src[m.end()] in "\"'":
                if word not in ("u", "U"):
                    err(i, "E-LOWS-PREFIX", f"`{word}` is not a prefix; the prefixes are `u` and `U`")
                i = quoted(m.end(), word)
                continue
            if word == "rem":
                j = src.find("\n", i)
                i = n if j < 0 else j
                continue
            if word in ("note", "text"):
                start = i
                rest_end = src.find("\n", m.end())
                if rest_end < 0:
                    err(start, "E-LOWS-UNCLOSED", f"`{word}` needs a tag and a body")
                head = [h for h in re.split(r"[ \t]+", src[m.end():rest_end]) if h]
                prefix = ""
                if word == "text" and len(head) == 2 and all(_NAME_RE.fullmatch(h) for h in head):
                    if head[0] not in ("u", "U"):
                        err(start, "E-LOWS-PREFIX", f"`{head[0]}` is not a prefix; the prefixes are `u` and `U`")
                    prefix = head.pop(0)
                tag = head[0] if len(head) == 1 else ""
                if not _NAME_RE.fullmatch(tag or "-"):
                    err(start, "E-LOWS-TAG", f"`{word}` must be followed by one tag name on the same line")
                body, j = [], rest_end + 1
                bline = line + 1
                while True:
                    if j >= n:
                        err(start, "E-LOWS-UNCLOSED", f"no line holding only `{tag}` closes this block")
                    k = src.find("\n", j)
                    k = n if k < 0 else k
                    ln = src[j:k]
                    if ln.strip(" \t") == tag:
                        break
                    body.append(ln)
                    j = k + 1
                    bline += 1
                if word == "text":
                    raw = "\n".join(body).encode("utf-8")
                    toks.append(_Tok(_STR_KIND[prefix], elements(raw, prefix, start), *pos(start)))
                i = k
                line, lstart = bline, j
                continue
            kind = {"true": "bool", "false": "bool", "do": "do", "end": "end"}.get(word, "name")
            val = (word == "true") if kind == "bool" else word
            toks.append(_Tok(kind, val, *pos(i)))
            i = m.end()
            continue
        if _is_digit(ch) or (ch in "+-" and i + 1 < n and _is_digit(src[i + 1])):
            m = _NUM_RE.match(src, i)
            end = m.end()
            if end < n and _is_word(src[end]):
                err(i, "E-LOWS-NUMBER", "malformed number (a base marker needs a digit, there is no octal, and `_` goes between digits)")
            clean = m.group(0).replace("_", "")
            if m.group("hexfloat") or m.group("decfloat"):
                try:
                    v = float.fromhex(clean) if m.group("hexfloat") else float(clean)
                except OverflowError:
                    v = float("inf")
                if v in (float("inf"), float("-inf")):
                    err(i, "E-LOWS-RANGE", "float is out of range")
                toks.append(_Tok("float", v, *pos(i)))
            else:
                v = int(clean, 10) if m.group("dec") else int(clean, 0)
                if not _INT_MIN <= v <= _INT_MAX:
                    err(i, "E-LOWS-RANGE", "integer is outside -2^63 .. 2^64-1")
                toks.append(_Tok("int", v, *pos(i)))
            i = end
            continue
        err(i, "E-LOWS-CHARSET", f"unexpected character {ch!r}")
    return toks


class _Parser:
    def __init__(self, toks):
        self.toks, self.i = toks, 0
        self.sealed = {}            # id(Branch) -> block id that owns it
        self.open = []              # stack of open block ids
        self.next_block = 0

    def err(self, t, code, msg):
        raise LowsError(t.line, t.col, code, msg)

    def document(self):
        root = Branch()
        self.statements(root, closing=False)
        return root

    def walk(self, base, path, t, want_leaf):
        node = base
        for k, name in enumerate(path):
            if isinstance(node, Leaf):
                self.err(t, "E-LOWS-SHAPE", f"`{' '.join(path[:k])}` holds values; it cannot also hold keys")
            owner = self.sealed.get(id(node))
            if owner is not None and owner not in self.open:
                self.err(t, "E-LOWS-SEALED", f"`{' '.join(path[:k])}` was written as a block; add keys inside it")
            child = dict.get(node, name)
            if child is not None and k == len(path) - 1:
                if not want_leaf:
                    self.err(t, "E-LOWS-SEALED", f"`{' '.join(path)}` already exists; a block must be its only writer")
                if isinstance(child, Leaf):
                    self.err(t, "E-LOWS-DUP", f"`{' '.join(path)}` is already set")
                self.err(t, "E-LOWS-SHAPE", f"`{' '.join(path)}` holds keys; it cannot also hold values")
            if child is None:
                if k == len(path) - 1 and want_leaf:
                    return node, name
                child = Branch()
                dict.__setitem__(node, name, child)
            node = child
        return node, None

    def statements(self, base, closing):
        toks = self.toks
        while self.i < len(toks):
            t = toks[self.i]
            if t.kind == "end":
                if not closing:
                    self.err(t, "E-LOWS-END", "`end` without `do`")
                self.i += 1
                return
            if t.kind != "name":
                self.err(t, "E-LOWS-PATH", "a statement starts with a key name")
            path = []
            while self.i < len(toks) and toks[self.i].kind == "name":
                path.append(toks[self.i].val)
                self.i += 1
            if self.i >= len(toks):
                self.err(t, "E-LOWS-UNCLOSED", "statement is not closed with `.`")
            if toks[self.i].kind == "do":
                if len(self.open) >= MAX_DEPTH:
                    self.err(toks[self.i], "E-LOWS-DEPTH", f"blocks nest deeper than {MAX_DEPTH}")
                node, _ = self.walk(base, path, t, want_leaf=False)
                self.next_block += 1
                self.sealed[id(node)] = self.next_block
                self.open.append(self.next_block)
                do_tok = toks[self.i]
                self.i += 1
                self.statements(node, closing=True)
                self.open.pop()
                if not node:
                    self.err(do_tok, "E-LOWS-EMPTY", "empty block")
                continue
            vals, kinds = [], set()
            while self.i < len(toks) and toks[self.i].kind in _VALUE_KINDS:
                vals.append(toks[self.i].val)
                kinds.add(toks[self.i].kind)
                self.i += 1
            if self.i >= len(toks) or toks[self.i].kind != "dot":
                bad = toks[self.i] if self.i < len(toks) else t
                if self.i < len(toks) and toks[self.i].kind == "name":
                    self.err(bad, "E-LOWS-ORDER", "a key name cannot follow a value; values end the path")
                self.err(bad, "E-LOWS-UNCLOSED", "statement is not closed with `.`")
            if len(kinds) > 1:
                self.err(t, "E-LOWS-MIX", f"one statement holds one kind of value, not {sorted(kinds)}")
            self.i += 1
            parent, name = self.walk(base, path, t, want_leaf=True)
            dict.__setitem__(parent, name, Leaf(kinds.pop() if kinds else "empty", vals))
        if closing:
            last = toks[-1]
            raise LowsError(last.line, last.col, "E-LOWS-UNCLOSED", "`do` is not closed with `end`")


def dumps_canonical(doc):
    """The canonical dump (spec annex C): one line per leaf, identical across implementations."""
    out = ["lowstruct-dump 1"]

    def value(kind, v):
        if kind == "int" or kind.endswith("char"):
            return str(v)
        if kind == "float":
            return struct.pack(">d", v).hex()
        if kind == "bool":
            return "true" if v else "false"
        if kind == "str":
            return "x:" + v.hex()
        return "[" + ",".join(map(str, v)) + "]"

    def walk(node, prefix):
        for name, child in node.items():
            path = prefix + (name,)
            if isinstance(child, Leaf):
                out.append(".".join(path) + "\t" + child.kind + "\t" + " ".join(value(child.kind, v) for v in child.values))
            else:
                walk(child, path)

    walk(doc, ())
    return "\n".join(out) + "\n"
