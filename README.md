# lowstruct

A small, strict configuration file format (`.lows`) that borrows the surface syntax of the
Lowent language, with three independent parsers — C, Node.js and Python — held to one
conformance suite.

```lowent
rem server settings
title "rubrapack" .

server do
  host "0.0.0.0" .
  ports 80 443 .            rem several values are a list
  tls do
    cert "a.pem" .
  end
end

windir "C:\\Windows" .
```

- A statement is **names (the path) + literals (the values) + `.`** — no `=`, no brackets, no commas.
- `do … end` groups a common prefix; a path written as a block has exactly one writer.
- Backslashes mean only what the closed escape set says; raw multi-line text goes in a `text TAG … TAG` heredoc.
- Literals are exactly Lowent's: `0x2A`, `0b101`, `1_000`, `0x1.8p1`, `'a'`, `u"…"` (UTF-16), `U"…"` (code points).
- No octal, no `inf`/`nan`, no BOM, UTF-8 only. Every rejection has a stable code (`E-LOWS-…`) and a line:column.

## What is here

| Path | What |
|---|---|
| `docs/spec/lowstruct.md` | the specification, version 0.1 (Korean) — normative |
| `conformance/` | accept cases with their canonical dumps, reject cases with their expected codes |
| `c/` | C23 library on proven_c_lib (vendored) |
| `js/` | Node.js ES module, no dependencies |
| `python/` | Python 3.10+ package, no dependencies |

## Status

Version 0.1, experimental — the format and the APIs may change without notice. Schema checking (a Lowent
`struct` as the schema) is planned for the next version.

## Testing

```sh
scripts/test-all.sh          # the conformance suite in all three implementations
scripts/test-all.sh --fuzz   # plus a three-way differential fuzz (same dump or same error and position)
```

## License

MIT — see `LICENSE`. Third-party notices: `THIRD_PARTY_NOTICES.md`.
