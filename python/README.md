# lowstruct (Python)

Pure Python, no dependencies, Python 3.10+. The format is specified in `docs/spec/lowstruct.md`.

```python
import lowstruct

doc = lowstruct.loads(open("app.lows", "rb").read())   # str or UTF-8 bytes
port = doc.get("server port").one()                   # 8080 (int)
name = doc.get(("server", "name")).text()              # "..." as Python str (strict UTF-8)
for key, node in doc["server"].items():                # source order
    ...
```

- `loads(src)` / `load(fp)` return a `Branch` (a `dict` in source order); leaves are `Leaf(kind, values)`.
- Kinds: `int` → `int`, `float` → `float`, `bool` → `bool`, `str` → `bytes`, `u_str`/`U_str` → list of units/code points,
  `char`/`u_char`/`U_char` → `int`, and `empty` for a key with no values.
- `Leaf.one()` raises `LookupError` unless there is exactly one value; `Leaf.text()` decodes a string value.
- Errors raise `LowsError` with `line`, `col` (code points), `code` (`E-LOWS-...`) and `msg`.
- `dumps_canonical(doc)` gives the canonical dump (spec annex C).

Tests: `python3 -m unittest discover -s python/tests` from the repository root.
