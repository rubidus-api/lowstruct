# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The specification, the three libraries and the
release archives share one version number.

## [Unreleased]

## [0.0.1] - 2026-09-26

First release.

### Added

- Format specification 0.0.1 in English (normative) and Korean (`spec/`).
- Python, Node.js and C (on proven_c_lib 0.1.1) parsers that pass one conformance suite
  (13 accept cases with canonical dumps, 54 reject cases with expected codes).
- C library builds for Linux x86_64 (static `.a`, shared `.so`) and Windows x86_64 (static `.a`, `lowstruct.dll`
  with import library and `.def`); the shared libraries export only the `lows_*` API.
- `lows_default_allocator()`, so that users of the shared library have an allocator to pass to `lows_parse`.
- Manual in English and Korean (`manual/`, `manual-ko/`): writing files, the three libraries, notes for implementers.
- `tools/test-all.sh`, a three-way differential fuzz (`tools/difffuzz.py`) that compares dumps, error codes and
  error positions, and `tools/package.sh` for the release archives.

[Unreleased]: https://github.com/rubidus-api/lowstruct/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/rubidus-api/lowstruct/releases/tag/v0.0.1
