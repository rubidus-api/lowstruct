# lowstruct for Node.js

Parser for **lowstruct** (`.lows`), a small, strict configuration file format. ES module, no dependencies,
Node.js 20+.

```js
import { readFileSync } from "node:fs";
import { parse } from "lowstruct";

const doc = parse(readFileSync("app.lows"));
const port = doc.lookup("server port").one();   // BigInt
const name = doc.lookup("server name").text();  // string (strict UTF-8)
```

Errors throw `LowsError` with `line`, `col`, and a stable `code` such as `E-LOWS-DUP`.

- Manual: [https://github.com/rubidus-api/lowstruct/blob/main/manual/javascript.md](https://github.com/rubidus-api/lowstruct/blob/main/manual/javascript.md)
- Format specification: [https://github.com/rubidus-api/lowstruct/blob/main/spec/lowstruct.md](https://github.com/rubidus-api/lowstruct/blob/main/spec/lowstruct.md)
- License: MIT
