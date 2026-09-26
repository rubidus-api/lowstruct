"""Shared conformance suite (../../conformance) plus the hand-written JSON views."""
import json
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
import lowstruct  # noqa: E402

CASES = ROOT / "conformance"


def json_view(node):
    """The JSON view the cases/*.json files were written in (Lowini drafts, T0038-T0040)."""
    if isinstance(node, lowstruct.Leaf):
        return [value_json(node.kind, v) for v in node.values]
    return {k: json_view(v) for k, v in node.items()}


def value_json(kind, v):
    if kind == "str":
        try:
            return v.decode("utf-8")
        except UnicodeDecodeError:
            return {"bytes": v.hex()}
    if kind in ("u_str", "U_str"):
        return {kind[0]: v}
    if kind.endswith("char"):
        return {kind: v}
    return v


class Conformance(unittest.TestCase):
    def test_accept_dump(self):
        files = sorted((CASES / "accept").glob("*.lows"))
        self.assertGreater(len(files), 0)
        for f in files:
            with self.subTest(case=f.name):
                doc = lowstruct.loads(f.read_bytes())
                want = f.with_suffix(".dump").read_text(encoding="utf-8")
                self.assertEqual(lowstruct.dumps_canonical(doc), want)

    def test_accept_json_view(self):
        # Only the cases whose JSON was written by hand; a JSON made by this parser would prove nothing.
        for f in sorted(p.with_suffix(".lows") for p in (CASES / "accept").glob("*.json")):
            with self.subTest(case=f.name):
                doc = lowstruct.loads(f.read_bytes())
                want = json.loads(f.with_suffix(".json").read_text(encoding="utf-8"))
                got = json_view(doc)
                self.assertEqual(got, want)
                self.assertEqual(json.dumps(got), json.dumps(want))  # 1 and 1.0 differ here

    def test_reject(self):
        files = sorted((CASES / "reject").glob("*.lows"))
        self.assertGreater(len(files), 0)
        for f in files:
            data = f.read_bytes()
            want = re.search(rb"expect (E-LOWS-[A-Z0-9-]+)", data).group(1).decode()
            with self.subTest(case=f.name):
                with self.assertRaises(lowstruct.LowsError) as cm:
                    lowstruct.loads(data)
                self.assertEqual(cm.exception.code, want)


class Api(unittest.TestCase):
    def test_get_one_text(self):
        doc = lowstruct.loads('server do\n  port 8080 .\n  name "é" .\n  u u"é" .\nend\n')
        self.assertEqual(doc.get("server port").one(), 8080)
        self.assertEqual(doc.get(("server", "name")).text(), "é")
        self.assertEqual(doc.get("server u").text(), "é")
        self.assertIsNone(doc.get("server nope"))
        self.assertEqual(list(doc["server"]), ["port", "name", "u"])

    def test_one_requires_exactly_one(self):
        doc = lowstruct.loads("a 1 2 .\nb .\n")
        with self.assertRaises(LookupError):
            doc.get("a").one()
        self.assertEqual(doc.get("b").kind, "empty")

    def test_error_position(self):
        with self.assertRaises(lowstruct.LowsError) as cm:
            lowstruct.loads("a 1 .\nb 1 \"x\" .\n")
        e = cm.exception
        self.assertEqual((e.line, e.col, e.code), (2, 1, "E-LOWS-MIX"))

    def test_invalid_utf8(self):
        with self.assertRaises(lowstruct.LowsError) as cm:
            lowstruct.loads(b'a "\xff" .\n')
        self.assertEqual((cm.exception.code, cm.exception.line, cm.exception.col), ("E-LOWS-UTF8", 1, 4))

    def test_non_ascii_digit_is_not_a_number(self):
        with self.assertRaises(lowstruct.LowsError) as cm:
            lowstruct.loads("a ٣ .\n")
        self.assertEqual(cm.exception.code, "E-LOWS-CHARSET")


if __name__ == "__main__":
    unittest.main()
