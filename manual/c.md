# C Library

**English** · [한국어](../manual-ko/c.md)

C23 on [proven_c_lib](../c/vendor/proven/LICENSE) v0.1.1, which is vendored under `c/vendor/proven/`.
The API is `c/include/lowstruct.h`. Errors are returned as values, every allocation goes through the
`proven_allocator_t` you pass in, and a parsed document owns all of its memory.

## Prebuilt libraries

Each [release](https://github.com/rubidus-api/lowstruct/releases) carries the C library for two platforms:

| Archive | Contents |
|---|---|
| `lowstruct-VERSION-linux-x86_64.tar.gz` | `include/`, `lib/liblowstruct.a`, `lib/liblowstruct.so.VERSION` (+ `.so.0`, `.so` links) |
| `lowstruct-VERSION-windows-x86_64.zip` | `include/`, `lib/liblowstruct.a`, `lib/liblowstruct.dll.a`, `lib/lowstruct.def`, `bin/lowstruct.dll` |

`include/` holds `lowstruct.h` and the four proven_c_lib headers it includes. The shared libraries export only the
`lows_*` API. The Linux `.so` needs glibc 2.14 or later; the Windows DLL (MinGW-w64, UCRT) needs only the Universal
C Runtime of Windows 10 and later.

## Linking

| How | Compile | Link |
|---|---|---|
| static | `-Iinclude` | `lib/liblowstruct.a -lm` (Linux) · `lib/liblowstruct.a` (Windows) |
| shared | `-Iinclude -DLOWS_SHARED` | `-Llib -llowstruct` (Linux) · `lib/liblowstruct.dll.a` (Windows, MinGW-w64), and ship the `.so`/`.dll` |

`LOWS_SHARED` selects `__declspec(dllimport)` on Windows. The header compiles as C23, C17 and C11 (not as C++).
With Microsoft's tools, `lib /def:lowstruct.def /machine:x64 /out:lowstruct.lib` makes an import library from the
shipped `.def`; that route has not been tested.

## Building from source

```sh
cd c
cc -o ../build/nob nob.c    # once; nob rebuilds itself when nob.c changes
../build/nob                # static + shared library in ../build/c/, conformance suite against both
../build/nob lib            # the libraries only
../build/nob windows        # cross-build for Windows (needs x86_64-w64-mingw32-gcc)
```

The Windows target writes `../build/c/windows-x86_64/`: the static library, `lowstruct.dll` with its import library
and `.def`, and two test programs (static and DLL) to run on Windows with the conformance files as arguments.
`tools/package.sh` turns both builds into the release archives. The compiler must accept `-std=c23`; the suite is
tested with GCC 14 and Clang 19 on Linux, and the Windows build (MinGW-w64 GCC 16) is tested on Windows 11.

## Read a file

```c
#include <stdio.h>
#include <string.h>

#include "lowstruct.h"

int main(void) {
    const char *src = "server do\n  host \"0.0.0.0\" .\n  port 8080 .\nend\n";
    lows_doc_t *doc;
    lows_error_t e;
    if (lows_parse(lows_default_allocator(), (const proven_byte_t *)src, strlen(src), &doc, &e) != PROVEN_OK) {
        fprintf(stderr, "%u:%u %s: %s\n", e.line, e.col, e.code, e.message);
        return 1;
    }
    const lows_node_t *root = lows_doc_root(doc);
    proven_i64 port;
    const proven_byte_t *host;
    proven_size_t host_len;
    if (lows_get_i64(lows_lookup(root, "server port"), &port) == PROVEN_OK &&
        lows_get_bytes(lows_lookup(root, "server host"), &host, &host_len) == PROVEN_OK) {
        printf("%.*s:%lld\n", (int)host_len, (const char *)host, (long long)port);
    }
    lows_doc_free(doc);
    return 0;
}
```

## Parsing

```c
proven_err_t lows_parse(proven_allocator_t alloc, const proven_byte_t *src, proven_size_t len,
                        lows_doc_t **out, lows_error_t *err);
void lows_doc_free(lows_doc_t *doc);
proven_allocator_t lows_default_allocator(void);
```

`lows_default_allocator()` is the general-purpose heap allocator. Pass your own `proven_allocator_t` to control
where the memory comes from.

| Return | Meaning |
|---|---|
| `PROVEN_OK` | `*out` is a document; free it with `lows_doc_free` |
| `PROVEN_ERR_INVALID_FORMAT` | not lowstruct; `*err` holds `line`, `col` (code points), `code`, `message` |
| `PROVEN_ERR_NOMEM` | the allocator failed; nothing is leaked |
| `PROVEN_ERR_INVALID_ARG` | `out` is `NULL`, `src` is `NULL` with a length, or the allocator is incomplete |

`err->code` and `err->message` point to static strings. `err` may be `NULL`. The input does not need a
terminating NUL, and `src` may be `NULL` when `len` is 0.

## The tree

| Function | Returns |
|---|---|
| `lows_doc_root(doc)` | the root branch |
| `lows_lookup(node, "a b c")` | the node at that path (names separated by spaces or tabs), or `NULL` |
| `lows_node_is_leaf(node)` · `lows_node_name(node)` | leaf or branch; its name (`""` for the root) |
| `lows_branch_count(node)` · `lows_branch_child(node, i)` | children, in source order |
| `lows_leaf_kind(node)` · `lows_leaf_count(node)` · `lows_leaf_value(node, i)` | a leaf's kind and values |
| `lows_kind_name(kind)` | `"int"`, `"u_str"`, `"empty"`, … |

A value is a `lows_value_t`; the member that is live depends on the leaf's kind:

| Kind | Member |
|---|---|
| `LOWS_KIND_INT` | `i.magnitude`, `i.negative` (the value is −magnitude when negative) |
| `LOWS_KIND_FLOAT` | `f` |
| `LOWS_KIND_BOOL` | `b` |
| `LOWS_KIND_STR` · `STR16` · `STR32` | `s.ptr`, `s.len` — bytes, `proven_u16` units, `proven_u32` code points; `len` counts elements |
| `LOWS_KIND_CHAR` · `CHAR16` · `CHAR32` | `ch` |
| `LOWS_KIND_EMPTY` | a leaf with no values |

## Single-value accessors

`lows_get_i64`, `lows_get_u64`, `lows_get_f64`, `lows_get_bool` and `lows_get_bytes` read a leaf that has
exactly one value:

| Return | Meaning |
|---|---|
| `PROVEN_OK` | the value was stored |
| `PROVEN_ERR_NOT_FOUND` | `NULL`, a branch, or a leaf without exactly one value |
| `PROVEN_ERR_INVALID_ARG` | a leaf of another kind |
| `PROVEN_ERR_OVERFLOW` | the integer does not fit (`2⁶⁴−1` in `i64`, a negative in `u64`) |

A `str` value is a byte string with a length. It may contain NUL and is not guaranteed to be UTF-8; never
treat it as a C string.

## Memory

Two pools sit on top of your allocator: the document's (names, nodes, values; released by `lows_doc_free`) and
a scratch pool for tokens, released before `lows_parse` returns. Nesting is limited to 64 blocks, so recursion
depth is bounded. The test suite runs clean under AddressSanitizer, LeakSanitizer and UBSan.

## Other functions

`lows_dump_canonical(doc, alloc, &text, &len)` writes the canonical dump (specification, Annex C); release the
text with `alloc.free_fn(alloc.ctx, text)`. `LOWSTRUCT_VERSION_STRING` and `LOWS_MAX_DEPTH` are in the header.
