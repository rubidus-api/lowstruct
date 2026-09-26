# lowstruct for C

C23 parser for **lowstruct** (`.lows`) on proven_c_lib (vendored under `vendor/proven/`, MIT).
API: `include/lowstruct.h`.

```sh
cc -o ../build/nob nob.c    # once
../build/nob                # ../build/c/liblowstruct.a + conformance suite
../build/nob lib            # the library only
```

Manual: [`../manual/c.md`](../manual/c.md) · Specification: [`../spec/lowstruct.md`](../spec/lowstruct.md)
