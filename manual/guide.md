# Writing lowstruct Files

**English** · [한국어](../manual-ko/guide.md)

This page teaches the format by example. The rules are defined in the [specification](../spec/lowstruct.md).

## Statements

A statement is a **path** (one or more names), then its **values** (literals), then a full stop.

```lowstruct
title "my-app" .
server port 8080 .
server debug false .
```

The path ends at the first literal, so no `=` is needed. The two `server …` statements build a `server`
branch holding `port` and `debug`.

Names are ASCII letters, digits and `_`, starting with a letter or `_`, and are case-sensitive.
Seven words are reserved: `rem note text do end true false`.

## Blocks

`do … end` writes a common prefix once. These two files build the same tree:

```lowstruct
server do
  host "0.0.0.0" .
  tls do
    cert "a.pem" .
  end
end
```

```lowstruct
server host "0.0.0.0" .
server tls cert "a.pem" .
```

A path opened as a block belongs to that block. You cannot open it again later, or add to it with a flat
statement outside it (`E-LOWS-SEALED`) — everything about `server` is in one place. Blocks nest up to 64 deep.

Several things of the same sort get names of their own:

```lowstruct
remote origin do
  url "https://example.invalid/r.git" .
end
remote backup do
  url "https://example.invalid/b.git" .
end
```

## Lists

Several values are a list. There are no brackets and no commas; a long list may span lines, and the
full stop ends it.

```lowstruct
ports 80 443 8443 .
allow
  "10.0.0.0/8"
  "192.168.0.0/16"
  .
```

All values of one statement are of one kind: `1 2.0` mixes an integer and a float and is rejected.
A statement with no values (`feature_x .`) is an empty list — it records that the key is present.

A key is written once. Repeating a statement is an error (`E-LOWS-DUP`), not a way to build a list.

## Comments

```lowstruct
rem a comment runs to the end of the line
port 8080 .   rem also after a statement

note END
A block comment.
Anything goes here until a line holding only END.
END
```

## Numbers

| Write | Means |
|---|---|
| `42` `-5` `+5` `1_000_000` | decimal integers; `_` separates digits |
| `0x2A` `0b101010` | hexadecimal and binary integers |
| `0755` | 755 — there is no octal |
| `1.5` `1e3` `1.5e-3` | decimal floats; a point needs digits on both sides |
| `0x1.8p1` | a hexadecimal float (3.0) |

Integers run from −2⁶³ to 2⁶⁴−1. Floats are IEEE 754 doubles; there is no `inf` or `nan`.
`true` and `false` are booleans.

## Strings

`"…"` is a string. It must close on its line. A backslash starts one of a closed set of escapes:

| Escape | Value |
|---|---|
| `\\` `\"` `\'` | backslash, double quote, single quote |
| `\n` `\t` `\r` `\0` `\a` `\b` `\f` `\v` | control characters |
| `\xNN` | one byte |
| `\uXXXX` `\UXXXXXXXX` | a Unicode code point |

Any other backslash sequence is an error (`E-LOWS-ESCAPE`); a stray backslash never silently becomes
something else.

### Windows paths

Write every backslash twice:

```lowstruct
windir "C:\\Windows" .
share "\\\\server\\share" .
```

`"C:\temp"` is not an error — `\t` is the tab escape — but it is not the path you meant. Always double the
backslash.

### Multi-line and raw text

A `text` heredoc keeps its body exactly as written, with no escapes. It ends at a line holding only its tag.

```lowstruct
banner text END
Welcome.
  Indentation and \backslashes stay as they are.
END
.
```

The line break after the last body line is not part of the value. The heredoc is a value like any other,
so the statement still ends with a full stop.

### Byte strings and prefixes

A plain `"…"` holds **bytes**: `"\xE9"` is one byte that is not valid UTF-8. The libraries give you the
bytes and a helper that decodes UTF-8 text. Two prefixes change what a string holds:

| Literal | Elements |
|---|---|
| `"é"` | bytes (2) |
| `u"é"` | UTF-16 code units (1) |
| `U"é"` | code points (1) |

Character literals such as `'a'`, `u'é'` and `U'😀'` hold exactly one element of the same kind.

## Errors you will meet

Every rejection names a line, a column (counted in characters) and a code.

| Code | Typical cause | Fix |
|---|---|---|
| `E-LOWS-UNCLOSED` | a missing `.`, `end`, closing quote, or heredoc tag | add it |
| `E-LOWS-CHARSET` | `=`, `[`, `#`, `,` or a non-ASCII letter outside a string | lowstruct uses none of these |
| `E-LOWS-ESCAPE` | `"\q"`, `"\101"` (no octal), a bad `\u` | use one of the escapes above |
| `E-LOWS-NUMBER` | `0x` with no digits, `0o7`, `1__0` | write a complete number |
| `E-LOWS-MIX` | `1 "one" .` or `1 2.0 .` | one kind per statement |
| `E-LOWS-ORDER` | `a 1 b 2 .` | one statement per path |
| `E-LOWS-DUP` | the same key twice | write it once |
| `E-LOWS-SHAPE` | `a 1 .` and also `a b 2 .` | a key holds values or keys, not both |
| `E-LOWS-SEALED` | reopening a block, or adding to it from outside | keep a block's keys inside it |
| `E-LOWS-TAG` | `text` or `note` used as a key, or a bad heredoc opening | rename the key |
| `E-LOWS-BOM` · `E-LOWS-CR` · `E-LOWS-UTF8` | the file's encoding | save as UTF-8 without BOM, LF or CRLF |

The complete list, with where each one points, is in the specification, section 5.
