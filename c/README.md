# lowstruct (C)

C23 on proven_c_lib (vendored under `vendor/proven`, v0.1.1). Errors are values; all memory comes from the
`proven_allocator_t` you pass, and a document owns everything it allocated. API: `include/lowstruct.h`.

```c
#include "lowstruct.h"
#include "proven/heap.h"

lows_doc_t *doc;
lows_error_t e;
if (lows_parse(proven_heap_allocator(), src, len, &doc, &e) != PROVEN_OK) {
    fprintf(stderr, "%u:%u %s: %s\n", e.line, e.col, e.code, e.message);
} else {
    proven_i64 port;
    if (lows_get_i64(lows_lookup(lows_doc_root(doc), "server port"), &port) == PROVEN_OK) { /* ... */ }
    lows_doc_free(doc);
}
```

Build and test (from this directory):

```sh
cc -o ../build/nob nob.c    # once
../build/nob                # build/c/liblowstruct.a + conformance tests
../build/nob lib            # the library only
```

Link `build/c/liblowstruct.a` with `-lm`, and add `include/` and `vendor/proven/include/` to the include path.
Strings (`str`) are byte strings with a length: they may hold NUL and are not guaranteed to be UTF-8.
