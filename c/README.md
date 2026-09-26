# lowstruct for C

C23 parser for **lowstruct** (`.lows`) on proven_c_lib (vendored under `vendor/proven/`, MIT).
API: `include/lowstruct.h`.

```sh
cc -o ../build/nob nob.c    # once
../build/nob                # static + shared library in ../build/c/, conformance suite against both
../build/nob lib            # the libraries only
../build/nob windows        # Windows cross-build: static library, lowstruct.dll, import library, .def
```

Manual: [`../manual/c.md`](../manual/c.md) · Specification: [`../spec/lowstruct.md`](../spec/lowstruct.md)
