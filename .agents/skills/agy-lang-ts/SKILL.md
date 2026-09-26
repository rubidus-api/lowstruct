---
name: agy-lang-ts
description: Use when changing, debugging, or verifying TypeScript or JavaScript in this project — including type errors, test failures, build/bundling problems, and questions about which package manager or script to run.
---

# TypeScript work

## Package manager

Determine it from evidence — the lockfile (`package-lock.json`, `pnpm-lock.yaml`,
`yarn.lock`, `bun.lockb`) and the project's own docs. Do not switch between npm,
pnpm, yarn, and bun. Do not change dependency versions or regenerate a lockfile
unless the task requires it.

## Verification

Run whichever of these the project actually defines: type check, tests, lint,
build. A successful transpile does not prove runtime correctness — `tsc` passing
is `V0`, not `V1`.

## Types

Do not introduce `any` to silence a type error unless the project deliberately
permits it and the semantics genuinely require it. A type error is usually
evidence about the code, not an obstacle to remove.
