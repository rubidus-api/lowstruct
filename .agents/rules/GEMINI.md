# agy rules — lowstruct

Project-specific facts for agy. The general operating policy lives in the
machine-global `~/.gemini/config/GEMINI.md`; do not restate it here.

## Context economy

Read first: `docs/spec/lowstruct.md` (search it by section), `SPEC.md`, `conformance/`.
Never read whole: `c/vendor/proven/`, `build/`, `conformance/reject/depth_huge.lows`.
Search with `rg` rooted at the project root; do not scan the workspace.

## Tool discipline

- Build: `cd c && cc -o ../build/nob nob.c && ../build/nob lib` (Python and JS need no build)
- Test (targeted): `python3 -m unittest discover -s python/tests` · `cd js && node --test` · `cd c && ../build/nob`
- Test (full, about a minute): `scripts/test-all.sh --fuzz`
- Gate: `scripts/gate.sh` runs the minimum tier for what changed; use it before reporting.
- Run builds and tests only through the commands above.
- Do not re-read a file already in context.

## Stop conditions

Stop and report when the same check fails three times for the same reason, or
when a step produces no new evidence.

## Effort and invocation

Default `--effort medium`. Raise only for float rounding (hex floats) and the C memory pools.
Headless (`--print`) auto-denies tools that need approval; for this project a
batch run needs: interactive only.

## Project facts

- Language/build: C23 (gcc/clang, nob), Node.js 20+ ESM, Python 3.10+
- Output goes to: `build/` (C objects, library, test binaries, fuzz leftovers)
- Cross-build or remote machine required: no (Windows build not verified yet)
- Slow paths: `tests/fuzz/difffuzz.py`, `tests/lowent/difftest.py` (needs a Lowent compiler)
- Do not touch: `c/vendor/proven/` (vendored, unmodified), `conformance/*/*.dump` by hand (regenerate and review), `../lowstruct_private/`

## Prohibited

See `AGENTS.md` and `LESSONS.md` in this project. Do not restate their rules here;
read them when a decision depends on them.
