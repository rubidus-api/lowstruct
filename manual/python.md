# Python Library

**English** · [한국어](../manual-ko/python.md)

Pure Python, no dependencies, Python 3.10 or later. The package is the directory `python/lowstruct/`.

## Install

From a [release](https://github.com/rubidus-api/lowstruct/releases): unzip `lowstruct-VERSION-python.zip`, then `pip install ./lowstruct-VERSION-python`
(or put that folder on `PYTHONPATH`). The package is not on PyPI yet.

From a checkout:

```sh
pip install ./python
```

or put `python/` on `PYTHONPATH`.

## Read a file

```python
import lowstruct

doc = lowstruct.loads(b'''
server do
  host "0.0.0.0" .
  port 8080 .
  tags "a" "b" .
end
''')

port = doc.get("server port").one()          # 8080
host = doc.get(("server", "host")).text()     # "0.0.0.0"
tags = [t.decode() for t in doc.get("server tags").values]   # ["a", "b"]
print(port, host, tags)
```

`loads()` takes `str` or UTF-8 `bytes`; `load(fp)` reads a file object opened in either mode.
Pass bytes when you can: then invalid UTF-8 is reported with its position as `E-LOWS-UTF8`.

## The tree

- A **branch** is a `lowstruct.Branch`, a `dict` subclass: keys are names, in source order.
  `branch.get(path, default=None)` walks a path given as `"a b c"`, `("a", "b", "c")` or one name.
- A **leaf** is a `lowstruct.Leaf` with `kind` and `values`:

| `kind` | Python values |
|---|---|
| `int` | `int` |
| `float` | `float` |
| `bool` | `bool` |
| `str` | `bytes` (a byte string; may be non-UTF-8) |
| `u_str` · `U_str` | `list` of UTF-16 code units · code points |
| `char` · `u_char` · `U_char` | `int` |
| `empty` | no values (`key .`) |

- `Leaf.one()` returns the single value and raises `LookupError` unless there is exactly one.
- `Leaf.text()` returns the single string value as `str`: UTF-8 for `str` (strict), decoded units for `u_str`
  and `U_str`.

```python
import lowstruct

doc = lowstruct.loads('remote origin do\n  url "https://example.invalid/r.git" .\nend\n')
for name, remote in doc["remote"].items():
    print(name, remote.get("url").text())
```

## Errors

A document that is not lowstruct raises `lowstruct.LowsError`, a `ValueError` with `line`, `col` (in code
points), `code` (`E-LOWS-…`, stable) and `msg` (may change).

```python
import lowstruct

try:
    lowstruct.loads('port 80 "x" .\n')
except lowstruct.LowsError as e:
    print(e.line, e.col, e.code)   # 1 1 E-LOWS-MIX
```

## Other names

- `lowstruct.dumps_canonical(doc)` — the canonical dump (specification, Annex C).
- `lowstruct.KINDS`, `lowstruct.MAX_DEPTH` (64), `lowstruct.__version__`.

## Tests

From the repository root: `python3 -m unittest discover -s python/tests`
