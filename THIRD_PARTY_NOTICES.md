# Third-Party Notices

## proven_c_lib

`c/vendor/proven/` is an unmodified copy of proven_c_lib `v0.1.1` (git `22f964e`), MIT License —
see `c/vendor/proven/LICENSE`. Its own notices (the clean-room float parser, and the vendored
`nob.h` build helper, public domain / MIT) are in `c/vendor/proven/THIRD_PARTY_NOTICES.md`.
lowstruct compiles five of its files: `float_parse.c`, `float_decimal.c`, `heap.c`, `proven_sys_mem.c`,
and uses `nob.h` for its C build script.

## Conformance case taken from Lowent

`conformance/accept/08-lowent-pkg.lows` is the package manifest `pkg.low` of the Lowent language
repository (MIT License, same author), kept unchanged to show that it reads as lowstruct.
