# lowstruct Format Specification — Version 0.0.1

**English** · [한국어](lowstruct.ko.md)

- Format name: **lowstruct**. File extension: `.lows`. Media type (unregistered): `text/x-lowstruct`.
- lowstruct shares its lexical syntax with the Lowent language but is a **separate format** (Annex A).
  A `.lows` file is not a Lowent program.
- Status: version 0.0.1, draft. No compatibility is promised yet.
- This document is normative. The three implementations (`c/`, `js/`, `python/`) and the conformance suite
  (`conformance/`) follow it; where they disagree with it, this document is right and they are fixed.
- This English text is the canonical version; the Korean text is its translation.
- The key words MUST, MUST NOT, SHOULD and MAY are to be read as described in RFC 2119.
  Paragraphs marked *(Note)* are informative.

## 1. Overview

lowstruct is a configuration file format for files that people edit by hand. It borrows the surface
syntax of the Lowent language.

```lowstruct
rem server settings
title "my-app" .

server do
  host "0.0.0.0" .
  ports 80 443 .
  tls do
    cert "a.pem" .
    key  "a.key" .
  end
end

windir "C:\\Windows" .
```

- A statement is **names (the path) + literals (the values) + `.`**. There is no `=`, no `[ ]`, no `,`.
- `do … end` groups a common prefix.
- Several values are a list.
- A backslash means only what the closed escape set says; raw multi-line text goes in a `text` heredoc.

## 2. The File

(1) A file is UTF-8. A processor MUST reject invalid UTF-8 (`E-LOWS-UTF8`). Validity follows RFC 3629:
    overlong encodings, surrogates (U+D800–U+DFFF) and values above U+10FFFF are invalid.

(2) A processor MUST reject a byte order mark (EF BB BF) at the start of the file (`E-LOWS-BOM`).

(3) A line ends with LF or CRLF. A processor replaces every CRLF with LF before reading. Any CR that remains
    after that MUST be rejected (`E-LOWS-CR`). Consequently no CRLF survives into a `text` heredoc value.

(4) Non-ASCII characters MAY appear only inside comments, string and character literals, and heredoc bodies.
    Anywhere else, a character that belongs to none of the tokens in section 3 MUST be rejected
    (`E-LOWS-CHARSET`). This includes NUL and `[`, `=`, `#`, `;`, `,`.

*(Note)* An INI or TOML file passed by mistake therefore stops at its first statement.

## 3. Lexical Structure

### 3.1 White Space and Comments

(1) White space is space (U+0020), tab (U+0009) and line feed (U+000A). It separates tokens and has no
    other meaning.

(2) From the word `rem` to the end of its line is a comment.

(3) A block comment starts with `note TAG` and ends at a line holding only the same `TAG`, under the same
    rules as a heredoc (3.5). It produces no value.

### 3.2 Names and Reserved Words

(1) A name is `[A-Za-z_][A-Za-z0-9_]*`. Names are case-sensitive and contain no dot.

(2) These seven words are reserved and are never names: `rem note text do end true false`.
    Because the lexer already turns them into a comment, a heredoc, a block or a boolean, there is no separate
    "reserved word used as a name" diagnostic; the grammar breaks at that point instead
    (for example, `server text 1 .` is `E-LOWS-TAG`).

(3) A name immediately followed by a quote is a prefix (3.4). Any prefix other than `u` and `U` MUST be rejected
    (`E-LOWS-PREFIX`). With a space in between it is not a prefix: `u "x"` is the name `u` and the string `"x"`.

### 3.3 Numbers

(1) Numbers have the following shapes. A processor tries the alternatives in the order written and uses the
    first that matches.

```ebnf
number    = [ "+" | "-" ] , ( hexfloat | decfloat | hex | bin | dec ) ;
dec       = digit , { [ "_" ] , digit } ;
hex       = "0" , ( "x" | "X" ) , hexdigit , { [ "_" ] , hexdigit } ;
bin       = "0" , ( "b" | "B" ) , bindigit , { [ "_" ] , bindigit } ;
decfloat  = dec , "." , dec , [ dexp ] | dec , dexp ;
hexfloat  = hex , [ "." , hexdigit , { [ "_" ] , hexdigit } ] , ( "p" | "P" ) , [ "+" | "-" ] , dec ;
dexp      = ( "e" | "E" ) , [ "+" | "-" ] , dec ;
```

(2) A sign is a sign only when attached: `- 5` is not a number.

(3) `_` appears only between digits. A leading zero does not mean octal: `0755` is 755. There is no octal notation.

(4) A float needs digits on both sides of its point. `80.` is the integer 80 followed by the statement end;
    `.5` is not a number.

(5) A number immediately followed by `[A-Za-z0-9_]` is malformed and MUST be rejected (`E-LOWS-NUMBER`).
    This rejects `0x` and `0b` (a base marker with no digit), `0o7`, `1__0`, `0x_1`, `1_` and `0x1p`
    (no exponent digits).

(6) An integer MUST lie between −2⁶³ and 2⁶⁴−1 inclusive (`E-LOWS-RANGE`). `-0` is 0.

(7) A float is an IEEE 754 binary64 value, rounded to nearest with ties to even, for decimal and hexadecimal
    notation alike. A result that is not finite MUST be rejected (`E-LOWS-RANGE`). There is no way to write
    `inf` or `nan`.

### 3.4 Strings and Characters

(1) A string is `"…"`; a character is `'…'`. Either MAY carry the prefix `u` or `U`.

(2) A literal MUST close on its own line (`E-LOWS-UNCLOSED`).

(3) An escape starts with a backslash. Exactly these fourteen exist; any other MUST be rejected (`E-LOWS-ESCAPE`).

| Written | Value |
|---|---|
| `\\` `\"` `\'` | 0x5C · 0x22 · 0x27 |
| `\a` `\b` `\f` `\n` `\r` `\t` `\v` `\0` | 0x07 · 0x08 · 0x0C · 0x0A · 0x0D · 0x09 · 0x0B · 0x00 |
| `\xNN` | one byte (exactly two hex digits) |
| `\uXXXX` · `\UXXXXXXXX` | a code point (exactly four or eight hex digits); surrogates and values above U+10FFFF are rejected |

(4) **A value is built in two steps.**
    1. The literal first becomes a byte string. A written character becomes its UTF-8 bytes, `\xNN` becomes
       that byte, and `\u`/`\U` become the UTF-8 bytes of the code point.
    2. The prefix chooses the element. Without a prefix, the bytes are the elements. With `u`, the bytes are
       decoded as UTF-8 and counted as UTF-16 code units; with `U`, as code points. Bytes that do not decode
       (for example `u"\xE9"`) MUST be rejected (`E-LOWS-ESCAPE`).

(5) A character literal MUST yield exactly one element under (4): none is `E-LOWS-CHAR-EMPTY`, more than one is
    `E-LOWS-CHAR-WIDTH`. `'é'` is two bytes and is rejected; `u'é'` is accepted.

(6) A string without a prefix is a **byte string with no UTF-8 guarantee** (`"\xE9"`, `"a\0b"`). A reader that
    needs text SHOULD check UTF-8. A C reader MUST NOT look for the end at a NUL byte.

### 3.5 Heredocs

(1) A heredoc starts with `text [u|U] TAG` and ends at the first line that holds only `TAG` once leading and
    trailing spaces and tabs are removed. `TAG` is a name. Anything else on the opening line is `E-LOWS-TAG`;
    a name other than `u` or `U` in the prefix position is `E-LOWS-PREFIX`; a missing closing line is
    `E-LOWS-UNCLOSED`.

(2) The body runs from the line after the opening line to the line before the closing line, joined with LF.
    No LF is added after the last line. Escapes are not interpreted. The value then goes through step 2 of 3.4 (4).

### 3.6 Windows Paths

(1) In a one-line value, a backslash is written `\\`: `windir "C:\\Windows" .`

(2) *(Note)* `"C:\temp\new"` is not rejected: `\t` and `\n` are in the escape set and become a tab and a line
    feed. Following (1) avoids this.

## 4. Statements and the Data Model

### 4.1 Grammar

```ebnf
file   = { stmt } ;
stmt   = path , ( "do" , stmt , { stmt } , "end" | { value } , "." ) ;
path   = NAME , { NAME } ;
value  = NUMBER | "true" | "false" | STRING | CHAR | TEXT ;
```

(1) The path is the names; the values are the literals. The path ends at the first literal. A name after a value
    MUST be rejected (`E-LOWS-ORDER`).

(2) A statement starts with a name (`E-LOWS-PATH`). An `end` without a `do` is `E-LOWS-END`; an unclosed
    statement or block is `E-LOWS-UNCLOSED`.

(3) `a b c 1 .` and `a do b do c 1 . end end` build the same tree.

### 4.2 The Tree

(1) A document is one tree. A **branch** maps names to children, in source order. A **leaf** holds a list of values.

(2) All values of one leaf are of one kind (`E-LOWS-MIX`). There are nine kinds.

| Kind | Literals | Element |
|---|---|---|
| `int` | `42` `-5` `0xFF` | −2⁶³ … 2⁶⁴−1 |
| `float` | `1.5` `0x1p3` | binary64, finite |
| `bool` | `true` `false` | |
| `str` | `"…"`, `text TAG` | byte |
| `u_str` | `u"…"`, `text u TAG` | UTF-16 code unit |
| `U_str` | `U"…"`, `text U TAG` | code point |
| `char` · `u_char` · `U_char` | `'a'` · `u'é'` · `U'😀'` | one element |

`int` and `float` are different kinds: `1 2.0` is mixed.

(3) A leaf with one value is a scalar. A leaf with no values (`flag .`) is an empty list and marks that the key
    is present. Implementations report its kind as `empty`.

(4) There is no `null` and no date type. There are no lists of lists and no lists of tables; several things of
    one sort are written as named blocks.

```lowstruct
remote origin do
  url "https://example.invalid/r.git" .
end
remote backup do
  url "https://example.invalid/b.git" .
end
```

### 4.3 One Writer per Path

| Rule | Diagnostic |
|---|---|
| A leaf MUST NOT be written twice | `E-LOWS-DUP` |
| A path MUST NOT be both a leaf and a branch (`a 1 .` and `a b 2 .`) | `E-LOWS-SHAPE` |
| A path opened as a block is written only by that block; it MUST NOT be reopened or extended by a flat statement outside it | `E-LOWS-SEALED` |
| An existing path MUST NOT be opened as a block | `E-LOWS-SEALED` |
| A block MUST NOT be empty | `E-LOWS-EMPTY` |
| Blocks nest at most 64 deep; the 65th `do` MUST be rejected | `E-LOWS-DEPTH` |

Flat statements MAY share a prefix (`package name …` / `package version …`).

## 5. Diagnostics

(1) A rejection is reported as one diagnostic: line (from 1), column (from 1, counted in **code points**),
    code, and message. The code is a stable name; the message MAY change between versions.

(2) A processor reads in this order and reports the **first** fault it meets:
    1. whole-file checks: UTF-8, then BOM, then CR (section 2);
    2. the lexical structure of the whole file (section 3) — so a lexical fault is reported before a structural
       fault that appears earlier in the file;
    3. statements and the tree (section 4), in source order.

(3) The reported position is:

| Code | Position |
|---|---|
| `E-LOWS-UTF8` · `E-LOWS-CR` · `E-LOWS-CHARSET` | that byte or character |
| `E-LOWS-BOM` | 1:1 |
| `E-LOWS-ESCAPE` (escape shape, code point) | the backslash |
| `E-LOWS-ESCAPE` (does not decode as UTF-8) · `E-LOWS-UNCLOSED` (literal) · `E-LOWS-CHAR-*` · `E-LOWS-PREFIX` (literal) · `E-LOWS-NUMBER` · `E-LOWS-RANGE` | the first character of the literal or number, prefix and sign included |
| `E-LOWS-TAG` · `E-LOWS-PREFIX` (heredoc) · `E-LOWS-UNCLOSED` (heredoc) · `E-LOWS-ESCAPE` in a heredoc value | the word `note` or `text` |
| `E-LOWS-ORDER` | the name after the value |
| `E-LOWS-UNCLOSED` (statement) | the token where the `.` should be; at end of file, the statement's first name |
| `E-LOWS-UNCLOSED` (block) | the last token of the file |
| `E-LOWS-EMPTY` · `E-LOWS-DEPTH` | that `do` |
| `E-LOWS-PATH` · `E-LOWS-END` | that token |
| `E-LOWS-MIX` · `E-LOWS-DUP` · `E-LOWS-SHAPE` · `E-LOWS-SEALED` | the statement's first name |

(4) The complete set of codes: `UTF8 BOM CR CHARSET UNCLOSED ESCAPE PREFIX TAG CHAR-EMPTY CHAR-WIDTH NUMBER RANGE
    PATH END ORDER MIX DUP SHAPE SEALED EMPTY DEPTH`, each prefixed with `E-LOWS-`.

## 6. How the Implementations Present Values *(Note)*

| Kind | C (`lowstruct.h`) | Node.js | Python |
|---|---|---|---|
| `int` | `{magnitude, negative}`; `lows_get_i64`/`lows_get_u64` | `BigInt` | `int` |
| `float` | `double` | `Number` | `float` |
| `bool` | `bool` | `Boolean` | `bool` |
| `str` | byte pointer + length | `Uint8Array` | `bytes` |
| `u_str` · `U_str` | `u16` / `u32` pointer + length | array of numbers | list of ints |
| the three `char` kinds | `u32` | `Number` | `int` |

## Annex A. Relationship to Lowent

- lowstruct is a **separate format that shares Lowent's syntax**. It has its own repository, specification,
  versions, diagnostics and schedule, and this document is its only norm. Lowent documents are a source, not a norm.
- The literals (3.3–3.5) are **pinned** to Lowent's specification as of 2026-09-26 (language revision 1.3),
  §6.1.4 and annex A.6. A later change in Lowent does not change lowstruct; adopting one takes a new lowstruct
  version. In version 0.0.1, the same literal giving different values in the two is a defect.
  *(Note)* A differential check made for version 0.0.1 found that `lowentc` accepted `0x` as 0; that was fixed in Lowent.
- Lowent's package manifest `pkg.low` reads unchanged as lowstruct (case `08-lowent-pkg`).
- The body of Lowent's `make point do x 1 . y 2 . end` has the shape of a lowstruct block; that is the starting
  point for the schema planned for the next version (Annex D).
- A lowstruct file is not a Lowent program.

## Annex B. Conformance

(1) Every `conformance/accept/*.lows` MUST be accepted, and its canonical dump (Annex C) MUST equal the `.dump`
    file of the same name byte for byte.

(2) Every `conformance/reject/*.lows` MUST be rejected with the code on the file's first line, `rem expect E-LOWS-…`.

(3) A conforming implementation passes (1) and (2). Positions (5 (3)) are kept in step across the three
    implementations by a differential fuzz (`tools/difffuzz.py`).

## Annex C. Canonical Dump

A line-oriented form used to compare implementations. Float-to-text conversion differs between languages,
so floats are written as bits.

```text
lowstruct-dump 1
<path>\t<kind>\t<value> <value> …
```

- One line per leaf, walking the tree depth-first in source order. The path joins names with `.`.
- Values: `int` in decimal (negative with `-`); `float` as the 16 lowercase hex digits of its binary64 bits,
  big-endian; `bool` as `true`/`false`; `str` as `x:` followed by its bytes in lowercase hex; `u_str` and `U_str`
  as decimal numbers joined by commas in brackets, like `[65,66]`; the three `char` kinds in decimal.
  A leaf with no values has kind `empty` and an empty value field.
- The dump ends with LF.

## Annex D. Deferred to a Later Version

1. **A Lowent `struct` as the schema.** A `make … do … end` body is a lowstruct block. Every field filled = required
   keys; fields not declared = misspelt keys; field types = value range and kind. The mapping of lists to `slice`,
   named blocks to maps, and empty leaves to `bool` or `option` has to be settled.
2. **Symbols.** `level debug .` is currently a `debug` leaf under a `level` branch. One option marks the boundary with `be`.
3. **Relaxing block sealing (4.3).** Version 0.0.1 takes the strict side.
4. **A canonical formatter.** It would always write one-line backslashes as `\\`.
5. **A warning for path-like strings.** It would flag `\t`, `\n` and the like inside strings that start with a drive
   letter or `\\server`.
