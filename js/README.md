# lowstruct (Node.js)

ES module, no dependencies, Node.js 20+. The format is specified in `docs/spec/lowstruct.md`.

```js
import { readFileSync } from "node:fs";
import { parse } from "lowstruct";

const doc = parse(readFileSync("app.lows"));        // string or UTF-8 bytes
const port = doc.lookup("server port").one();        // 8080n (BigInt)
const name = doc.lookup(["server", "name"]).text();  // UTF-8 text
for (const [key, node] of doc.get("server")) { }     // source order
```

- `parse(src)` returns a `Branch` (a `Map` in source order; `lookup(path)` walks a path); leaves are `Leaf { kind, values }`.
- Kinds: `int` → `BigInt` (values reach 2^64−1), `float` → `Number`, `bool` → `Boolean`, `str` → `Uint8Array`,
  `u_str`/`U_str` → arrays of units/code points, `char`/`u_char`/`U_char` → `Number`, and `empty`.
- `Leaf.one()` throws `RangeError` unless there is exactly one value; `Leaf.text()` decodes a string value.
- Errors throw `LowsError` with `line`, `col` (code points), `code` (`E-LOWS-...`) and `msg`.
- `dumpCanonical(doc)` gives the canonical dump (spec annex C).

Tests: `npm test` in this directory.
