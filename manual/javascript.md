# Node.js Library

**English** · [한국어](../manual-ko/javascript.md)

An ES module with no dependencies, for Node.js 20 or later. The package is the directory `js/`.

## Install

From a [release](https://github.com/rubidus-api/lowstruct/releases): unzip `lowstruct-VERSION-node.zip`, then `npm install ./lowstruct-VERSION-node`.
The package is not on npm yet.

From a checkout:

```sh
npm install ./js
```

## Read a file

```js
import { parse } from "lowstruct";

const doc = parse(Buffer.from(`
server do
  host "0.0.0.0" .
  port 8080 .
  tags "a" "b" .
end
`));

const port = doc.lookup("server port").one();           // 8080n
const host = doc.lookup(["server", "host"]).text();     // "0.0.0.0"
const tags = doc.lookup("server tags").values.map((b) => new TextDecoder().decode(b));
console.log(port, host, tags);
```

`parse()` takes a string or UTF-8 bytes (`Uint8Array`, `Buffer`). Pass bytes when you can: then invalid UTF-8
is reported with its position as `E-LOWS-UTF8`.

## The tree

- A **branch** is a `Branch`, a `Map` subclass: keys are names, in source order.
  `branch.lookup(path)` walks a path given as `"a b c"` or `["a", "b", "c"]` and returns `undefined` when absent.
- A **leaf** is a `Leaf` with `kind` and `values`:

| `kind` | JavaScript values |
|---|---|
| `int` | `BigInt` — integers reach 2⁶⁴−1, beyond `Number`'s exact range |
| `float` | `Number` |
| `bool` | `Boolean` |
| `str` | `Uint8Array` (a byte string; may be non-UTF-8) |
| `u_str` · `U_str` | arrays of UTF-16 code units · code points |
| `char` · `u_char` · `U_char` | `Number` |
| `empty` | no values (`key .`) |

- `Leaf.one()` returns the single value and throws `RangeError` unless there is exactly one.
- `Leaf.text()` returns the single string value as a JavaScript string (strict UTF-8 for `str`).

```js
import { parse } from "lowstruct";

const doc = parse('remote origin do\n  url "https://example.invalid/r.git" .\nend\n');
for (const [name, remote] of doc.get("remote")) console.log(name, remote.lookup("url").text());
```

A key may be any lowstruct name, including `__proto__` or `constructor`; branches are `Map`s, so this is safe.

## Errors

A document that is not lowstruct throws `LowsError` with `line`, `col` (in code points), `code`
(`E-LOWS-…`, stable) and `msg` (may change).

```js
import { parse, LowsError } from "lowstruct";

try {
  parse('port 80 "x" .\n');
} catch (e) {
  if (e instanceof LowsError) console.log(e.line, e.col, e.code); // 1 1 E-LOWS-MIX
}
```

## Other exports

- `dumpCanonical(doc)` — the canonical dump (specification, Annex C).
- `KINDS`, `MAX_DEPTH` (64), `VERSION`.

## Tests

`npm test` in `js/`.
