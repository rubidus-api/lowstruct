# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow the specification's version.

## [Unreleased]

### Added

- Format specification 0.1 in English (normative) and Korean (`spec/`).
- Python, Node.js and C (on proven_c_lib 0.1.1) parsers that pass one conformance suite
  (13 accept cases with canonical dumps, 54 reject cases with expected codes).
- Manual in English and Korean (`manual/`, `manual-ko/`): writing files, the three libraries, notes for implementers.
- `tools/test-all.sh` and a three-way differential fuzz (`tools/difffuzz.py`) that compares dumps, error codes
  and error positions.
