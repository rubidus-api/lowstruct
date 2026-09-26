# C Library

**English** · [한국어](../manual-ko/c.md)

C23 on [proven_c_lib](../c/vendor/proven/LICENSE) v0.1.1, which is vendored under `c/vendor/proven/`.
The API is `c/include/lowstruct.h`. Errors are returned as values, every allocation goes through the
`proven_allocator_t` you pass in, and a parsed document owns all of its memory.

## Build

```sh
cd c
cc -o ../build/nob nob.c    # once; nob rebuilds itself when nob.c changes
../build/nob                # builds ../build/c/liblowstruct.a and runs the conformance suite
../build/nob lib            # the library only
```

To use the library, add `c/include`, `c/vendor/proven/include` and `c/vendor/proven/platform` to the include
path and link `build/c/liblowstruct.a` with `-lm`. The compiler must accept `-std=c23`; the suite is tested with
GCC 14 and Clang 19 on Linux. A Windows build has not been verified yet.

## Read a file

```c
#include <stdio.h>
#include <string.h>

#include "lowstruct.h"
#include "proven/heap.h"

int main(void) {
    const char *src = "server do\n  host \"0.0.0.0\" .\n  port 8080 .\nend\n";
    lows_doc_t *doc;
    lows_error_t e;
    if (lows_parse(proven_heap_allocator(), (const proven_byte_t *)src, strlen(src), &doc, &e) != PROVEN_OK) {
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
```

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
