#!/usr/bin/env python3
"""Differential test (R2): every literal must mean the same in lowstruct and in Lowent.

For each literal the Python implementation gives (kind, elements); lowentc gives
the same thing by running a tiny program on its VM. Rejected literals must be
rejected by both.

usage: difftest.py [path/to/lowentc]
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python"))
from lowstruct import loads, LowsError  # noqa: E402

# The Lowent compiler: argument, or $LOWENTC, or a sibling checkout.
LOWENTC = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("LOWENTC") or os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "lowent_lang", "impl", "build", "lowentc")

ACCEPT = [
    # integers
    "0", "42", "0755", "1_000_000", "-5", "+5", "0x2A", "0X1F", "0xFF_FF", "0b101010", "0B11",
    "-9223372036854775808", "18446744073709551615",
    # floats (the last two lines: conformance case 11)
    "0.1", "0.3", "1e23", "-0.0", "1.7976931348623157e308", "2.2250738585072014e-308", "4.9e-324",
    "2.4703282292062327e-324", "2.4703282292062328e-324", "3.14159265358979323846264338327950288419716939937510",
    "0x1.00000000000008p0", "0x1.00000000000018p0", "0x1.fffffffffffff8p0", "0x1p-1074", "0x1.8p-1074", "0x1p-1075",
    "0x1.0000000000001p-1075", "0x1.ffffffffffffffffffffp-1", "0x1.fffffffffffffp1023", "0x0.8p1", "0x10.8p0",
    "1.5", "1e3", "1E3", "1.5e-3", "1_000.5", "-1.5", "0x1p3", "0X1P3", "0x1.8p1", "0x1p-2", "0x1_0p0",
    # strings
    '"hello"', '""', '"c:\\\\windows"', '"é\\u00e9"', '"\\xE9"', '"a\\0b"', '"\\\\ \\" \\\' \\a\\b\\f\\n\\r\\t\\v"',
    '"\\U0001F600"', 'u"A😀"', 'u"\\xC3\\xA9"', 'U"한\\U0001F600"', 'U"\\xC3\\xA9é"', 'u"a\\0b"',
    # characters
    "'a'", "'\\x27'", "'\\''", "'\\xE9'", "'\\t'", "u'é'", "u'\\u00e9'", "U'😀'", "U'\\U0001F600'", "U'한'",
    # heredocs
    "text END\nline one\nline two\nEND\n", "text RAW\na\\nb\nRAW\n", "text u END\nAB\nEND\n", "text U END\n한글\nEND\n",
]
REJECT = [
    '"\\q"', '"\\101"', '"\\uD800"', '"\\U00110000"', 'u"\\xE9"', 'u8"ab"', "''", "'ab'", "'é'",
    "u'\\U0001F600'", "'\\uD800'", "0x", "0b", "0X", "1__0", "0x_1", "1_", "0o7", "18446744073709551616", "0x1p",
]

# Where lowentc disagrees with its own spec, Lowini follows the spec and the case is listed here.
# The three below were fixed in lowentc by X-0066 (WO-0223, 2026-09-26); kept so an older lowentc still reports them.
KNOWN = {
    "0x": "lowentc reads a bare base marker as 0: low_lex.c low_scan_digits accepts zero digits; annex A.6 needs one",
    "0b": "same as 0x",
    "0X": "same as 0x",
}

ELEM = {"str": "u8", "u_str": "u16", "U_str": "u32"}


def lowini(lit):
    """(kind, elements) or ('reject', code)."""
    try:
        leaf = loads(f"v {lit} .")["v"]
    except LowsError as e:
        return ("reject", e.code)
    kind, v = leaf.kind, leaf.values[0]
    return (kind, list(v) if kind in ELEM else v)


def program(lit, kind):
    lit = lit.rstrip("\n") + ("\n" if lit.startswith("text") else "")
    if kind in ELEM:
        el = ELEM[kind]
        return f"""module d .

fn n output u64 . do
  let s slice {el} be {lit} .
  return len s .
end

fn h output u64 . do
  let s slice {el} be {lit} .
  var acc u64 be 0 .
  var i u64 be 1 .
  for x s do
    set acc (add acc (mul (widen u64 x) i)) .
    set i (add i 1) .
  end
  return acc .
end
"""
    if kind.endswith("char"):
        return f"module d .\n\nfn f output u32 . do return widen u32 {lit} . end\n"
    if kind == "float":
        # lowentc prints f64 with six significant digits, so compare the bits instead.
        return f"module d .\n\nfn f output u64 . do\n  let x f64 be {lit} .\n  return bit_cast u64 x .\nend\n"
    ty = "i64" if lit.startswith("-") else "u64"
    return f"module d .\n\nfn f output {ty} . do return {lit} . end\n"


def run(src, fn, check=False):
    with tempfile.NamedTemporaryFile("w", suffix=".low", delete=False, encoding="utf-8") as f:
        f.write(src)
    try:
        args = [LOWENTC, "--check", f.name] if check else [LOWENTC, "--run", fn, f.name]
        out = subprocess.run(args, capture_output=True, text=True)
    finally:
        os.unlink(f.name)
    if check:
        return out.returncode, re.findall(r"E-[A-Z0-9-]+", out.stdout + out.stderr)
    m = re.search(rf"{fn}\(\) = (\S+)", out.stdout)
    return m.group(1) if m else "ERR " + " ".join(re.findall(r"E-[A-Z0-9-]+", out.stdout + out.stderr)[:2])


def main():
    fails = 0
    for lit in ACCEPT:
        kind, want = lowini(lit)
        if kind == "reject":
            print(f"FAIL {lit!r}: Lowini rejected it ({want})")
            fails += 1
            continue
        src = program(lit, kind)
        if kind in ELEM:
            got = (run(src, "n"), run(src, "h"))
            exp = (str(len(want)), str(sum(x * (k + 1) for k, x in enumerate(want))))
        elif kind == "float":
            r = run(src, "f")
            import struct
            bits = struct.unpack(">Q", struct.pack(">d", want))[0]
            got, exp = r, bits
            ok = not r.startswith("ERR") and int(r) == bits
            print(("PASS" if ok else "FAIL"), f"{kind:7} {lit!r} lowentc={r} lowini={want!r}")
            fails += not ok
            continue
        else:
            got, exp = run(src, "f"), str(int(want))
        ok = got == exp
        fails += not ok
        print(("PASS" if ok else "FAIL"), f"{kind:7} {lit!r} lowentc={got} lowini={exp}")
    for lit in REJECT:
        mine = lowini(lit)
        rc, codes = run(f"module d .\n\nfn f output u64 . do\n  let x u64 be 0 .\n  let v be {lit} .\n  return x .\nend\n", "", check=True)
        ok = mine[0] == "reject" and rc != 0
        tag = "PASS" if ok else "FAIL"
        if not ok and lit in KNOWN and mine[0] == "reject" and rc == 0:
            tag, ok = "KNOWN", True
        fails += not ok
        print(tag, f"reject  {lit!r} lowentc={codes[:2] if rc else 'accepted'} lowini={mine[1] if mine[0] == 'reject' else 'accepted'}"
              + (f"  [{KNOWN[lit]}]" if tag == "KNOWN" else ""))
    total = len(ACCEPT) + len(REJECT)
    print(f"{total - fails}/{total} agree or are known lowentc divergences")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
