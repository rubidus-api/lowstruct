# For Implementers

**English** · [한국어](../manual-ko/implementers.md)

This page is for people who change one of the three implementations, or write a fourth.

## The conformance suite

`conformance/` holds the cases every implementation must pass (specification, Annex B).

| Path | Rule |
|---|---|
| `conformance/accept/NAME.lows` | must be accepted |
| `conformance/accept/NAME.dump` | its canonical dump, byte for byte |
| `conformance/accept/NAME.json` | for the first ten cases only: a hand-written view, checked by the Python tests as an independent oracle |
| `conformance/reject/NAME.lows` | must be rejected with the code on its first line: `rem expect E-LOWS-…` |

The cases are byte-exact — some contain CRLF, a lone CR, a BOM, NUL or invalid UTF-8 on purpose.
`.gitattributes` keeps Git from converting them; do not edit them with a tool that normalises line ends.

## The canonical dump

Implementations are compared through a line-oriented dump, because float-to-text conversion differs between
languages:

```text
lowstruct-dump 1
server.port	int	8080
ratio	float	3fe0000000000000
name	str	x:6869
units	u_str	[65,66]
flag	empty	
```

One line per leaf, depth-first in source order: the dotted path, a tab, the kind, a tab, and the values
separated by spaces. Floats are their binary64 bits in hex; byte strings are `x:` and hex. The full rules are
in Annex C of the specification.

## Running everything

```sh
tools/test-all.sh           # Python, Node.js and C against the conformance suite
tools/test-all.sh --fuzz    # plus the differential fuzz below
```

## The differential fuzz

`tools/difffuzz.py [iterations] [seed]` mutates the conformance cases and generates token soup, then feeds every
input to all three implementations. They must print the same dump, or the same error **code, line and column**.
Disagreements are saved under `build/fuzz/` for replay. It needs `cc` and `node` on the path.

## Making a release

1. Set the version in `c/include/lowstruct.h`, `js/package.json`, `js/src/index.js`, `python/pyproject.toml`,
   `python/lowstruct/__init__.py` and the specification, and move the `CHANGELOG.md` entries under it.
2. Build natively (`cd c && ../build/nob`) and for Windows (`../build/nob windows` where MinGW-w64 is installed), and
   run both Windows test programs on Windows with the conformance files.
3. `tools/site/build-site.sh` regenerates the web manual in `docs/` and the two manual PDFs in `build/site/`
   (needs Typst and the fonts named in the script); the PDFs are attached to the release.
4. `tools/package.sh` writes the C archives, the Node.js and Python install packs (zip) and `SHA256SUMS` to
   `build/dist/`. Nothing is published to a package registry.

## Adding a rule or fixing a bug

1. Add a case to `conformance/` first: an accept case with its `.dump`, or a reject case with its
   `rem expect` line.
2. Make the three implementations pass `tools/test-all.sh`. The implementations deliberately follow the same
   algorithm (file checks, then the whole file's tokens, then statements), so the change usually looks alike in
   all three.
3. Run `tools/test-all.sh --fuzz` and, for C, a build with `-fsanitize=address,undefined`.
4. Update the specification in English, then the Korean translation.

To make an expected dump, write it by hand or produce it with one implementation and check it line by line; a
dump produced by the code under test proves nothing on its own.

## Writing a fourth implementation

Read the specification, then run your parser over `conformance/`. The details that most often go wrong:

- the order of the number alternatives (hex float, decimal float, hex, binary, decimal) and the rule that a number
  must not be followed by a letter, digit or `_`;
- strings are built as bytes first, and only then does the prefix choose bytes, UTF-16 units or code points;
- hexadecimal floats must be rounded to nearest-even, subnormals included;
- columns count code points, not bytes or UTF-16 units;
- lexical errors anywhere in the file win over structural errors earlier in the file;
- a leading BOM is an error — some decoders drop it silently;
- keys such as `__proto__` are ordinary names.
