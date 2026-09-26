# DECISIONS

This is the single append-only accepted decision log.

Use this for non-secret accepted project decisions that should remain traceable.

Read this file only when the current task needs prior decisions, decision rationale, or supersession history.

Do not split decisions into current and old files. If a decision is superseded, append a new entry and mark the older entry as superseded or superseded-by.

Do not store credentials, private infrastructure details, personal data, private remote URLs, or private-only business context here. Put private decisions in the sibling private repository when one is used.

## Template

### YYYY-MM-DD: <decision title>

- Status: Accepted | Superseded
- Context:
- Decision:
- Consequences:
- Supersedes:

### 2026-09-26: lowstruct v0.1 — format name, extension, scope, license

- Status: Accepted
- Context: The Lowini drafts (a config format in Lowent surface syntax) became a project of their own.
- Decision: Name `lowstruct`, extension `.lows`, MIT license. Version 0.1 covers the format only; using a Lowent
  `struct` as a schema moves to the next version. All three implementations (C on proven_c_lib, Node.js, Python)
  are held to one conformance suite and a three-way differential fuzz.
- Consequences: Diagnostic codes are `E-LOWS-*`. The canonical dump (spec annex C) is the comparison format,
  because float-to-text differs between languages.
- Supersedes:

### 2026-09-26: Literals are Lowent's, unchanged

- Status: Accepted
- Context: The first Lowini draft trimmed literals (no char literals, no `\0`, no high `\x`); the owner asked for all of them.
- Decision: Every Lowent literal is a lowstruct literal with the same value, checked against `lowentc` (tests/lowent).
  Windows paths are written with `\\` in one-line strings; `text` heredocs are for multi-line or raw text.
- Consequences: A bare `"..."` is a byte string that may be non-UTF-8 or hold NUL; readers that need text check UTF-8.
- Supersedes:

### 2026-09-26: Nesting limit 64

- Status: Accepted
- Context: Without a limit, deeply nested `do` blocks overflow the C stack and raise RecursionError in Python.
- Decision: At most 64 nested blocks; deeper is `E-LOWS-DEPTH` in all three implementations.
- Consequences: None for hand-written files.
- Supersedes:

### 2026-09-26: lowstruct and Lowent share syntax but are separate projects

- Status: Accepted (owner instruction)
- Context: lowstruct borrows Lowent's surface syntax and literals, which invites treating it as part of Lowent.
- Decision: lowstruct is a separate project with its own spec, versions, diagnostics and decisions. Literals are
  pinned to Lowent spec 6.1.4 / A.6 as of 2026-09-26 (language revision 1.3); later Lowent changes are adopted only
  through a new lowstruct spec version. No library, test or conformance case depends on Lowent. Work here never edits
  the Lowent repositories; Lowent defects found here go to Lowent's intake.
- Consequences: The rule lives in `AGENTS.md` ("lowstruct and Lowent") and spec annex A.
- Supersedes:
