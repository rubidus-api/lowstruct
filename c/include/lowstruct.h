/*
 * lowstruct — parser for the lowstruct configuration format (.lows).
 *
 * The format is defined by spec/lowstruct.md. This library is C23 on top of
 * proven_c_lib: errors are values, every allocation goes through the caller's
 * proven_allocator_t, and a parsed document owns all of its memory.
 *
 *     lows_doc_t *doc;
 *     lows_error_t e;
 *     if (lows_parse(lows_default_allocator(), src, len, &doc, &e) != PROVEN_OK) {
 *         fprintf(stderr, "%u:%u %s: %s\n", e.line, e.col, e.code, e.message);
 *         return 1;
 *     }
 *     const lows_node_t *port = lows_lookup(lows_doc_root(doc), "server port");
 *     ...
 *     lows_doc_free(doc);
 */
#ifndef LOWSTRUCT_H
#define LOWSTRUCT_H

#include "proven/types.h"
#include "proven/allocator.h"

/*
 * Linkage. Static linking needs nothing. To use the shared library (lowstruct.dll on Windows,
 * liblowstruct.so elsewhere) define LOWS_SHARED before including this header; on Windows that
 * selects __declspec(dllimport). LOWS_BUILDING is defined only while the library itself is built.
 */
#if defined(_WIN32) && defined(LOWS_SHARED)
#  if defined(LOWS_BUILDING)
#    define LOWS_API __declspec(dllexport)
#  else
#    define LOWS_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(LOWS_BUILDING)
#  define LOWS_API __attribute__((visibility("default")))
#else
#  define LOWS_API
#endif

#define LOWSTRUCT_VERSION_STRING "0.0.1"
#define LOWS_MAX_DEPTH 64 /* nested `do` blocks; deeper is E-LOWS-DEPTH */

typedef enum {
    LOWS_KIND_EMPTY = 0, /* a leaf with no values */
    LOWS_KIND_INT,       /* -2^63 .. 2^64-1 */
    LOWS_KIND_FLOAT,     /* binary64, finite */
    LOWS_KIND_BOOL,
    LOWS_KIND_STR,       /* "..."   bytes */
    LOWS_KIND_STR16,     /* u"..."  UTF-16 code units */
    LOWS_KIND_STR32,     /* U"..."  code points */
    LOWS_KIND_CHAR,      /* '.'     one byte */
    LOWS_KIND_CHAR16,    /* u'.'    one UTF-16 code unit */
    LOWS_KIND_CHAR32     /* U'.'    one code point */
} lows_kind_t;

/* One value. Which member is live is the leaf's kind. */
typedef struct {
    union {
        struct { proven_u64 magnitude; bool negative; } i; /* INT: value = negative ? -magnitude : magnitude */
        double f;                                          /* FLOAT */
        bool b;                                            /* BOOL */
        proven_u32 ch;                                     /* CHAR, CHAR16, CHAR32 */
        struct { const void *ptr; proven_size_t len; } s;  /* STR (bytes), STR16 (u16), STR32 (u32); len counts elements */
    };
} lows_value_t;

typedef struct lows_node lows_node_t;
typedef struct lows_doc lows_doc_t;

typedef struct {
    proven_u32 line;       /* 1-based */
    proven_u32 col;        /* 1-based, in code points */
    const char *code;      /* "E-LOWS-..." — the stable part; static storage */
    const char *message;   /* English sentence; static storage, may change between versions */
} lows_error_t;

/*
 * Parse `len` bytes of UTF-8. On success *out owns the document; free it with lows_doc_free.
 * Returns PROVEN_OK, PROVEN_ERR_INVALID_FORMAT (err is filled), PROVEN_ERR_NOMEM, or
 * PROVEN_ERR_INVALID_ARG. `err` may be NULL.
 */
[[nodiscard]]
LOWS_API proven_err_t lows_parse(proven_allocator_t alloc, const proven_byte_t *src, proven_size_t len,
                        lows_doc_t **out, lows_error_t *err);

LOWS_API void lows_doc_free(lows_doc_t *doc);

/* The general-purpose heap allocator (proven_heap_allocator), exported so that users of the
 * shared library, which does not export proven's own symbols, have one to pass to lows_parse. */
LOWS_API proven_allocator_t lows_default_allocator(void);

LOWS_API const lows_node_t *lows_doc_root(const lows_doc_t *doc);

/* Path of names separated by spaces or tabs ("server tls cert"); NULL when absent. */
LOWS_API const lows_node_t *lows_lookup(const lows_node_t *node, const char *path);

LOWS_API bool lows_node_is_leaf(const lows_node_t *node);
LOWS_API const char *lows_node_name(const lows_node_t *node); /* NUL-terminated ASCII; "" for the root */

/* Branches: children in source order. */
LOWS_API proven_size_t lows_branch_count(const lows_node_t *node);
LOWS_API const lows_node_t *lows_branch_child(const lows_node_t *node, proven_size_t i);

/* Leaves. */
LOWS_API lows_kind_t lows_leaf_kind(const lows_node_t *node);
LOWS_API proven_size_t lows_leaf_count(const lows_node_t *node);
LOWS_API const lows_value_t *lows_leaf_value(const lows_node_t *node, proven_size_t i);

/* "int", "float", "bool", "str", "u_str", "U_str", "char", "u_char", "U_char", "empty". */
LOWS_API const char *lows_kind_name(lows_kind_t kind);

/* Single-value accessors: PROVEN_ERR_NOT_FOUND unless a leaf with exactly one value,
 * PROVEN_ERR_INVALID_ARG on another kind, PROVEN_ERR_OVERFLOW when it does not fit. */
[[nodiscard]] LOWS_API proven_err_t lows_get_i64(const lows_node_t *node, proven_i64 *out);
[[nodiscard]] LOWS_API proven_err_t lows_get_u64(const lows_node_t *node, proven_u64 *out);
[[nodiscard]] LOWS_API proven_err_t lows_get_f64(const lows_node_t *node, double *out);
[[nodiscard]] LOWS_API proven_err_t lows_get_bool(const lows_node_t *node, bool *out);
/* STR only; the bytes are not NUL-terminated and may hold NUL or non-UTF-8 (spec 5). */
[[nodiscard]] LOWS_API proven_err_t lows_get_bytes(const lows_node_t *node, const proven_byte_t **ptr, proven_size_t *len);

/*
 * The canonical dump (spec annex C), used by the conformance suite. The text is
 * allocated with `alloc`; release it with alloc.free_fn(alloc.ctx, *out).
 */
[[nodiscard]]
LOWS_API proven_err_t lows_dump_canonical(const lows_doc_t *doc, proven_allocator_t alloc, char **out, proven_size_t *len);

#endif /* LOWSTRUCT_H */
