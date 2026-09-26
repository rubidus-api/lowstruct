#!/usr/bin/env python3
"""Differential fuzz: the three implementations must print the same dump or the same error (code and position).

usage: difffuzz.py [iterations] [seed]
Inputs are mutations of the conformance cases plus random token soup. Disagreements are saved under
build/fuzz/ for replay.
"""
import os
import pathlib
import random
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
import lowstruct  # noqa: E402

OUT = ROOT / "build" / "fuzz"
C_CLI = ROOT / "build" / "c" / "lows_cli"
PIECES = [b"a", b"b", b"do", b"end", b".", b" ", b"\n", b"\t", b"\r\n", b"\r", b"rem x", b"note N\nx\nN\n", b"text T\nv\nT\n",
          b"text u T\n\xc3\xa9\nT\n", b"1", b"-5", b"+0x1F", b"0x", b"0b2", b"1.5", b"1e3", b"0x1.8p1", b"0x1p-1075", b"1_0", b"1__0",
          b'"s"', b'"\\x41"', b'"\\u00e9"', b'u"x"', b'U"\\U0001F600"', b"'c'", b"u'\\u00e9'", b"''", b"true", b"false",
          b'"\\q"', b'"', b"'", b"\xc3\xa9", b"\xff", b"\x00", b"\xef\xbb\xbf", b"[", b"=", b"#", b"u8\"x\"", b"18446744073709551616"]


def py(data):
    try:
        return lowstruct.dumps_canonical(lowstruct.loads(data))
    except lowstruct.LowsError as e:
        return f"ERR {e.code} {e.line}:{e.col}\n"
    except Exception as e:  # a crash is a finding too
        return f"CRASH {type(e).__name__}: {e}\n"


def mutate(rng, base):
    b = bytearray(base)
    for _ in range(rng.randint(1, 4)):
        op = rng.random()
        pos = rng.randint(0, len(b))
        if op < 0.35 and b:
            del b[rng.randrange(len(b))]
        elif op < 0.7:
            b[pos:pos] = rng.choice(PIECES)
        elif b:
            b[rng.randrange(len(b))] = rng.randrange(256)
    return bytes(b)


def soup(rng):
    return b" ".join(rng.choice(PIECES) for _ in range(rng.randint(1, 12)))


def main():
    iters = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
    subprocess.run(["cc", "-std=c23", "-O1", "-I", "c/include", "-I", "c/vendor/proven/include", "-I", "c/vendor/proven/platform",
                    "c/tests/lows_cli.c", "build/c/liblowstruct.a", "-lm", "-o", str(C_CLI)], cwd=ROOT, check=True)
    bases = [p.read_bytes() for p in sorted((ROOT / "conformance").glob("*/*.lows")) if p.stat().st_size < 4000]
    work = OUT / "work"
    work.mkdir(parents=True, exist_ok=True)
    inputs = []
    for k in range(iters):
        data = mutate(rng, rng.choice(bases)) if rng.random() < 0.7 else soup(rng)
        p = work / f"{k:06d}.lows"
        p.write_bytes(data)
        inputs.append((p, data))
    bad = 0
    for chunk in range(0, len(inputs), 500):
        part = inputs[chunk:chunk + 500]
        files = [str(p) for p, _ in part]
        c_out = subprocess.run([str(C_CLI), *files], capture_output=True, check=True).stdout.decode("latin1").split("\x1e\n")
        j_out = subprocess.run(["node", str(ROOT / "tools/lows_cli.mjs"), *files], capture_output=True, check=True).stdout.decode("latin1").split("\x1e\n")
        for (p, data), c, j in zip(part, c_out, j_out):
            want = py(data)
            if not (want == c == j) or want.startswith("CRASH"):
                bad += 1
                keep = OUT / f"disagree-{p.name}"
                keep.write_bytes(data)
                if bad <= 5:
                    print(f"DISAGREE {keep.name}\n  py: {want.strip()[:200]}\n  c : {c.strip()[:200]}\n  js: {j.strip()[:200]}")
    for p, _ in inputs:
        p.unlink()
    print(f"difffuzz: {iters} inputs, {bad} disagreements")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
