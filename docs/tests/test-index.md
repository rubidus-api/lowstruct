# Test Index

Short authoritative TDD catalog.

Read this before implementing or changing behavior. Open detailed case files only when relevant.

| ID | Requirement | Purpose | Command | Detail | Status |
|---|---|---|---|---|---|
| T000 | bootstrap | Confirm test catalog is initialized | manual review | docs/tests/cases/T000-bootstrap.md | done |
| T001 | R1 R3 R5 R6 | Shared conformance suite in Python | `python3 -m unittest discover -s python/tests` | `conformance/` | active |
| T002 | R3 R5 R6 | Shared conformance suite + API in Node.js | `cd js && node --test` | `conformance/` | active |
| T003 | R3 R4 R5 R6 | Shared conformance suite + API in C | `cd c && ../build/nob` | `conformance/` | active |
| T004 | R3 | Three-way differential fuzz (dump, or code+line+col) | `python3 tests/fuzz/difffuzz.py 8000 <seed>` | `tests/fuzz/` | active |
| T005 | R4 | C suite under ASan+UBSan, no leaks | see `LESSONS.md` Checks | — | active |
| T006 | R2 | Literal values agree with `lowentc` | `python3 tests/lowent/difftest.py <lowentc>` | `tests/lowent/` | active |
