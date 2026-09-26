# lowstruct for Python

Parser for **lowstruct** (`.lows`), a small, strict configuration file format. Pure Python, no dependencies,
Python 3.10+.

```python
import lowstruct

doc = lowstruct.loads(open("app.lows", "rb").read())
port = doc.get("server port").one()          # int
name = doc.get("server name").text()         # str (strict UTF-8)
```

Errors raise `lowstruct.LowsError` with `line`, `col`, and a stable `code` such as `E-LOWS-DUP`.

- Manual: [https://github.com/rubidus-api/lowstruct/blob/main/manual/python.md](https://github.com/rubidus-api/lowstruct/blob/main/manual/python.md)
- Format specification: [https://github.com/rubidus-api/lowstruct/blob/main/spec/lowstruct.md](https://github.com/rubidus-api/lowstruct/blob/main/spec/lowstruct.md)
- License: MIT
