/*
 * lowstruct.c — the C implementation of spec/lowstruct.md.
 *
 * It mirrors the Python and Node.js implementations step for step; the shared
 * conformance suite (conformance/) holds all three to the same codes and values.
 * Memory: two chunk pools over the caller's allocator — the document's (names,
 * nodes, values; freed by lows_doc_free) and a scratch pool for tokens (freed
 * when lows_parse returns).
 */
#include "lowstruct.h"

#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "proven/float_parse.h"
#include "proven/heap.h"
#include "proven/u8str.h"

/* ---- pools -------------------------------------------------------------- */

typedef struct chunk {
    struct chunk *next;
    proven_size_t cap, used;
    alignas(max_align_t) unsigned char data[];
} chunk_t;

typedef struct {
    proven_allocator_t alloc;
    chunk_t *head;
} pool_t;

static void *pool_alloc(pool_t *p, proven_size_t size) {
    proven_size_t need = (size + 15u) & ~(proven_size_t)15u;
    if (need < size) return NULL;
    if (!p->head || p->head->cap - p->head->used < need) {
        proven_size_t cap = need > 65536u ? need : 65536u;
        if (cap > (proven_size_t)-1 - sizeof(chunk_t)) return NULL;
        proven_result_mem_mut_t r = p->alloc.alloc_fn(p->alloc.ctx, sizeof(chunk_t) + cap, alignof(max_align_t));
        if (r.err != PROVEN_OK) return NULL;
        chunk_t *c = (chunk_t *)r.value.ptr;
        c->next = p->head;
        c->cap = cap;
        c->used = 0;
        p->head = c;
    }
    void *out = p->head->data + p->head->used;
    p->head->used += need;
    return out;
}

static void pool_free(pool_t *p) {
    chunk_t *c = p->head;
    while (c) {
        chunk_t *next = c->next;
        p->alloc.free_fn(p->alloc.ctx, c);
        c = next;
    }
    p->head = NULL;
}

/* A growable array in a pool; the old block is simply left behind. */
typedef struct {
    void *ptr;
    proven_size_t len, cap, elem;
} vec_t;

static bool vec_push(pool_t *p, vec_t *v, const void *item) {
    if (v->len == v->cap) {
        proven_size_t cap = v->cap ? v->cap * 2 : 8;
        if (cap < v->cap || cap > (proven_size_t)-1 / v->elem) return false;
        void *n = pool_alloc(p, cap * v->elem);
        if (!n) return false;
        if (v->len) memcpy(n, v->ptr, v->len * v->elem);
        v->ptr = n;
        v->cap = cap;
    }
    memcpy((unsigned char *)v->ptr + v->len * v->elem, item, v->elem);
    v->len++;
    return true;
}

static bool vec_push_byte(pool_t *p, vec_t *v, proven_byte_t b) { return vec_push(p, v, &b); }

/* ---- document ------------------------------------------------------------ */

struct lows_node {
    const char *name;
    bool leaf;
    proven_u32 block;      /* id of the `do` block that owns this branch; 0 = none */
    vec_t kids;            /* lows_node_t* */
    lows_kind_t kind;
    lows_value_t *vals;
    proven_size_t nvals;
};

struct lows_doc {
    pool_t pool;
    lows_node_t *root;
};

/* ---- errors --------------------------------------------------------------- */

typedef struct {
    const char *code, *message;
} diag_t;

#define D(name, c, m) static const diag_t name = { c, m };
D(DG_UTF8, "E-LOWS-UTF8", "the file is not valid UTF-8")
D(DG_BOM, "E-LOWS-BOM", "byte order mark is not allowed; the file is UTF-8 without BOM")
D(DG_CR, "E-LOWS-CR", "a lone carriage return is not a line end")
D(DG_CHARSET, "E-LOWS-CHARSET", "unexpected character")
D(DG_UNCLOSED_LIT, "E-LOWS-UNCLOSED", "literal is not closed on its line")
D(DG_UNCLOSED_TAGLINE, "E-LOWS-UNCLOSED", "`note`/`text` needs a tag and a body")
D(DG_UNCLOSED_BODY, "E-LOWS-UNCLOSED", "no line holding only the tag closes this block")
D(DG_UNCLOSED_STMT, "E-LOWS-UNCLOSED", "statement is not closed with `.`")
D(DG_UNCLOSED_DO, "E-LOWS-UNCLOSED", "`do` is not closed with `end`")
D(DG_ESCAPE, "E-LOWS-ESCAPE", "escape is not in the closed set")
D(DG_ESCAPE_CP, "E-LOWS-ESCAPE", "not a code point")
D(DG_ESCAPE_CUT, "E-LOWS-ESCAPE", "a multi-byte character is cut short in this literal")
D(DG_PREFIX, "E-LOWS-PREFIX", "not a prefix; the prefixes are `u` and `U`")
D(DG_TAG, "E-LOWS-TAG", "`note`/`text` must be followed by one tag name on the same line")
D(DG_CHAR_EMPTY, "E-LOWS-CHAR-EMPTY", "an empty character literal")
D(DG_CHAR_WIDTH, "E-LOWS-CHAR-WIDTH", "this character does not fit one element of its prefix")
D(DG_NUMBER, "E-LOWS-NUMBER", "malformed number (a base marker needs a digit, there is no octal, and `_` goes between digits)")
D(DG_RANGE_INT, "E-LOWS-RANGE", "integer is outside -2^63 .. 2^64-1")
D(DG_RANGE_FLOAT, "E-LOWS-RANGE", "float is out of range")
D(DG_END, "E-LOWS-END", "`end` without `do`")
D(DG_PATH, "E-LOWS-PATH", "a statement starts with a key name")
D(DG_ORDER, "E-LOWS-ORDER", "a key name cannot follow a value; values end the path")
D(DG_MIX, "E-LOWS-MIX", "one statement holds one kind of value")
D(DG_SHAPE_VALUES, "E-LOWS-SHAPE", "this path holds values; it cannot also hold keys")
D(DG_SHAPE_KEYS, "E-LOWS-SHAPE", "this path holds keys; it cannot also hold values")
D(DG_SEALED, "E-LOWS-SEALED", "this path was written as a block; add keys inside it")
D(DG_SEALED_EXISTS, "E-LOWS-SEALED", "this path already exists; a block must be its only writer")
D(DG_DUP, "E-LOWS-DUP", "this path is already set")
D(DG_EMPTY, "E-LOWS-EMPTY", "empty block")
D(DG_DEPTH, "E-LOWS-DEPTH", "blocks nest deeper than 64")
#undef D

/* ---- UTF-8 ----------------------------------------------------------------- */

/* Index of the first byte that starts an invalid sequence (RFC 3629), or len. */
static proven_size_t utf8_first_invalid(const proven_byte_t *b, proven_size_t len) {
    proven_size_t i = 0;
    while (i < len) {
        proven_byte_t c = b[i];
        proven_u32 need, min, cp;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) { need = 1; min = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { need = 2; min = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { need = 3; min = 0x10000; }
        else return i;
        if (len - i <= need) return i;
        cp = c & (0x3Fu >> need);
        for (proven_u32 k = 1; k <= need; k++) {
            if ((b[i + k] & 0xC0) != 0x80) return i;
            cp = (cp << 6) | (b[i + k] & 0x3Fu);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return i;
        i += need + 1;
    }
    return len;
}

static proven_size_t utf8_encode(proven_u32 cp, proven_byte_t out[4]) {
    if (cp < 0x80) { out[0] = (proven_byte_t)cp; return 1; }
    if (cp < 0x800) { out[0] = (proven_byte_t)(0xC0 | (cp >> 6)); out[1] = (proven_byte_t)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        out[0] = (proven_byte_t)(0xE0 | (cp >> 12)); out[1] = (proven_byte_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (proven_byte_t)(0x80 | (cp & 0x3F)); return 3;
    }
    out[0] = (proven_byte_t)(0xF0 | (cp >> 18)); out[1] = (proven_byte_t)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (proven_byte_t)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (proven_byte_t)(0x80 | (cp & 0x3F)); return 4;
}

/* Decode valid UTF-8 (checked by the caller) one code point at *i. */
static proven_u32 utf8_next(const proven_byte_t *b, proven_size_t *i) {
    proven_byte_t c = b[*i];
    proven_u32 need = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    proven_u32 cp = need ? (c & (0x3Fu >> need)) : c;
    for (proven_u32 k = 1; k <= need; k++) cp = (cp << 6) | (b[*i + k] & 0x3Fu);
    *i += need + 1;
    return cp;
}

/* ---- lexer ------------------------------------------------------------------ */

typedef enum { T_NAME, T_DOT, T_DO, T_END, T_VALUE } tok_kind_t;

typedef struct {
    tok_kind_t k;
    lows_kind_t vk;
    proven_u32 line;
    proven_size_t off, lstart;
    lows_value_t v;
    const char *name;
} tok_t;

typedef struct {
    pool_t *doc, *scratch;
    const proven_byte_t *s;
    proven_size_t n;
    vec_t toks;
    /* failure */
    bool nomem;
    const diag_t *diag;
    proven_u32 eline;
    proven_size_t eoff, elstart;
} lexer_t;

static bool fail_at(lexer_t *L, const diag_t *d, proven_u32 line, proven_size_t off, proven_size_t lstart) {
    L->diag = d;
    L->eline = line;
    L->eoff = off;
    L->elstart = lstart;
    return false;
}

static bool oom(lexer_t *L) {
    L->nomem = true;
    return false;
}

static bool is_digit(proven_byte_t c) { return c >= '0' && c <= '9'; }
static bool is_hex(proven_byte_t c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_bin(proven_byte_t c) { return c == '0' || c == '1'; }
static bool is_alpha_(proven_byte_t c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_word(proven_byte_t c) { return is_alpha_(c) || is_digit(c); }
static int hexval(proven_byte_t c) { return is_digit(c) ? c - '0' : (c | 0x20) - 'a' + 10; }

static bool is_name(const proven_byte_t *p, proven_size_t len) {
    if (len == 0 || !is_alpha_(p[0])) return false;
    for (proven_size_t k = 1; k < len; k++) if (!is_word(p[k])) return false;
    return true;
}

static bool word_is(const proven_byte_t *p, proven_size_t len, const char *w) {
    return strlen(w) == len && memcmp(p, w, len) == 0;
}

/* The digit run `d (_? d)*` starting at p: its end, or p when there is no digit at p. */
static proven_size_t scan_run(const proven_byte_t *s, proven_size_t n, proven_size_t p, bool (*isd)(proven_byte_t)) {
    if (p >= n || !isd(s[p])) return p;
    p++;
    for (;;) {
        if (p < n && isd(s[p])) { p++; continue; }
        if (p + 1 < n && s[p] == '_' && isd(s[p + 1])) { p += 2; continue; }
        return p;
    }
}

/* Raw literal bytes -> values of the prefix's element (spec 5): bytes, UTF-16 units, code points. */
static bool elements(lexer_t *L, const proven_byte_t *raw, proven_size_t len, char prefix,
                     lows_value_t *out, proven_size_t *count, proven_u32 line, proven_size_t off, proven_size_t lstart) {
    if (prefix == 0) {
        proven_byte_t *p = pool_alloc(L->doc, len ? len : 1);
        if (!p) return oom(L);
        if (len) memcpy(p, raw, len);
        out->s.ptr = p;
        out->s.len = len;
        *count = len;
        return true;
    }
    if (utf8_first_invalid(raw, len) != len) return fail_at(L, &DG_ESCAPE_CUT, line, off, lstart);
    proven_size_t ncp = 0;
    for (proven_size_t i = 0; i < len;) { (void)utf8_next(raw, &i); ncp++; }
    if (prefix == 'U') {
        proven_u32 *p = pool_alloc(L->doc, (ncp ? ncp : 1) * sizeof(proven_u32));
        if (!p) return oom(L);
        proven_size_t k = 0;
        for (proven_size_t i = 0; i < len;) p[k++] = utf8_next(raw, &i);
        out->s.ptr = p;
        out->s.len = ncp;
        *count = ncp;
        return true;
    }
    proven_size_t nun = 0;
    for (proven_size_t i = 0; i < len;) nun += utf8_next(raw, &i) >= 0x10000 ? 2 : 1;
    proven_u16 *p = pool_alloc(L->doc, (nun ? nun : 1) * sizeof(proven_u16));
    if (!p) return oom(L);
    proven_size_t k = 0;
    for (proven_size_t i = 0; i < len;) {
        proven_u32 cp = utf8_next(raw, &i);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            p[k++] = (proven_u16)(0xD800 | (cp >> 10));
            p[k++] = (proven_u16)(0xDC00 | (cp & 0x3FF));
        } else {
            p[k++] = (proven_u16)cp;
        }
    }
    out->s.ptr = p;
    out->s.len = nun;
    *count = nun;
    return true;
}

static lows_kind_t str_kind(char prefix) { return prefix == 'u' ? LOWS_KIND_STR16 : prefix == 'U' ? LOWS_KIND_STR32 : LOWS_KIND_STR; }
static lows_kind_t char_kind(char prefix) { return prefix == 'u' ? LOWS_KIND_CHAR16 : prefix == 'U' ? LOWS_KIND_CHAR32 : LOWS_KIND_CHAR; }

static bool push_value(lexer_t *L, lows_kind_t vk, lows_value_t v, proven_u32 line, proven_size_t off, proven_size_t lstart) {
    tok_t t = { .k = T_VALUE, .vk = vk, .line = line, .off = off, .lstart = lstart, .v = v };
    return vec_push(L->scratch, &L->toks, &t) || oom(L);
}

/* A "string" or 'c' whose quote is at `at`; *end receives the index after it. */
static bool lex_quoted(lexer_t *L, proven_size_t at, char prefix, proven_u32 line, proven_size_t lstart, proven_size_t *end) {
    const proven_byte_t *s = L->s;
    proven_size_t n = L->n, start = at - (prefix ? 1 : 0), j = at + 1;
    proven_byte_t q = s[at];
    vec_t raw = { .elem = 1 };
    for (;;) {
        if (j >= n || s[j] == '\n') return fail_at(L, &DG_UNCLOSED_LIT, line, start, lstart);
        proven_byte_t c = s[j];
        if (c == q) { j++; break; }
        if (c == '\\') {
            proven_byte_t e = j + 1 < n ? s[j + 1] : 0;
            int simple = -1;
            switch (e) {
            case '\\': simple = 0x5C; break;
            case '"': simple = 0x22; break;
            case '\'': simple = 0x27; break;
            case 'a': simple = 0x07; break;
            case 'b': simple = 0x08; break;
            case 'f': simple = 0x0C; break;
            case 'n': simple = 0x0A; break;
            case 'r': simple = 0x0D; break;
            case 't': simple = 0x09; break;
            case 'v': simple = 0x0B; break;
            case '0': simple = 0x00; break;
            default: break;
            }
            if (simple >= 0) {
                if (!vec_push_byte(L->scratch, &raw, (proven_byte_t)simple)) return oom(L);
                j += 2;
                continue;
            }
            proven_size_t width = e == 'x' ? 2 : e == 'u' ? 4 : e == 'U' ? 8 : 0;
            if (!width || n - (j + 2) < width) return fail_at(L, &DG_ESCAPE, line, j, lstart);
            proven_u32 v = 0;
            for (proven_size_t k = 0; k < width; k++) {
                if (!is_hex(s[j + 2 + k])) return fail_at(L, &DG_ESCAPE, line, j, lstart);
                v = (v << 4) | (proven_u32)hexval(s[j + 2 + k]);
            }
            if (e == 'x') {
                if (!vec_push_byte(L->scratch, &raw, (proven_byte_t)v)) return oom(L);
            } else {
                if ((v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF) return fail_at(L, &DG_ESCAPE_CP, line, j, lstart);
                proven_byte_t buf[4];
                proven_size_t m = utf8_encode(v, buf);
                for (proven_size_t k = 0; k < m; k++)
                    if (!vec_push_byte(L->scratch, &raw, buf[k])) return oom(L);
            }
            j += 2 + width;
            continue;
        }
        if (!vec_push_byte(L->scratch, &raw, c)) return oom(L);
        j++;
    }
    lows_value_t v = { 0 };
    proven_size_t count = 0;
    if (!elements(L, raw.ptr, raw.len, prefix, &v, &count, line, start, lstart)) return false;
    *end = j;
    if (q == '"') return push_value(L, str_kind(prefix), v, line, start, lstart);
    if (count == 0) return fail_at(L, &DG_CHAR_EMPTY, line, start, lstart);
    if (count > 1) return fail_at(L, &DG_CHAR_WIDTH, line, start, lstart);
    lows_value_t c = { 0 };
    c.ch = prefix == 0 ? ((const proven_byte_t *)v.s.ptr)[0]
         : prefix == 'u' ? ((const proven_u16 *)v.s.ptr)[0]
         : ((const proven_u32 *)v.s.ptr)[0];
    return push_value(L, char_kind(prefix), c, line, start, lstart);
}

static unsigned bitlen64(proven_u64 x) {
    unsigned b = 0;
    while (x) { b++; x >>= 1; }
    return b;
}

/* Round M * 2^E (+ sticky bits below M) to nearest-even binary64. false on overflow. */
static bool hex_to_double(proven_u64 m, proven_i64 e, bool sticky, double *out) {
    if (m == 0) { *out = 0.0; return true; }
    proven_i64 len = bitlen64(m);
    proven_i64 s = len - 53 > -1074 - e ? len - 53 : -1074 - e;
    proven_u64 q;
    if (s <= 0) {
        q = m << -s; /* len - 53 <= 0 bounds the shift: q < 2^53 */
    } else {
        proven_u64 rem, half;
        if (s > 64) { q = 0; rem = m; half = 0; /* m < 2^64 <= half: rounds to 0 */ }
        else if (s == 64) { q = 0; rem = m; half = (proven_u64)1 << 63; }
        else { q = m >> s; rem = m & (((proven_u64)1 << s) - 1); half = (proven_u64)1 << (s - 1); }
        if (s <= 64 && (rem > half || (rem == half && (sticky || (q & 1))))) q++;
        if (q == (proven_u64)1 << 53) { q = (proven_u64)1 << 52; s++; }
    }
    proven_i64 exp2 = e + s;
    if (q == 0) { *out = 0.0; return true; }
    if ((proven_i64)bitlen64(q) + exp2 > 1024) return false;
    *out = ldexp((double)q, (int)exp2);
    return true;
}

static bool lex_number(lexer_t *L, proven_size_t i, proven_u32 line, proven_size_t lstart, proven_size_t *endp) {
    const proven_byte_t *s = L->s;
    proven_size_t n = L->n, p = i, end = 0;
    bool neg = false;
    if (s[p] == '+' || s[p] == '-') { neg = s[p] == '-'; p++; }
    enum { K_NONE, K_HEXFLOAT, K_DECFLOAT, K_HEX, K_BIN, K_DEC } kind = K_NONE;
    bool hex_marker = p + 1 < n && s[p] == '0' && (s[p + 1] == 'x' || s[p + 1] == 'X');
    bool bin_marker = p + 1 < n && s[p] == '0' && (s[p + 1] == 'b' || s[p + 1] == 'B');
    /* The alternatives in the order of annex A.6's regular reading: hex float, decimal float, hex, bin, dec. */
    if (hex_marker) {
        proven_size_t q = scan_run(s, n, p + 2, is_hex);
        if (q > p + 2) {
            proven_size_t r = q;
            if (r < n && s[r] == '.') {
                proven_size_t q2 = scan_run(s, n, r + 1, is_hex);
                if (q2 > r + 1) r = q2;
            }
            if (r < n && (s[r] == 'p' || s[r] == 'P')) {
                proven_size_t t = r + 1;
                if (t < n && (s[t] == '+' || s[t] == '-')) t++;
                proven_size_t q3 = scan_run(s, n, t, is_digit);
                if (q3 > t) { kind = K_HEXFLOAT; end = q3; }
            }
        }
    }
    if (kind == K_NONE) {
        proven_size_t q = scan_run(s, n, p, is_digit);
        if (q > p) {
            proven_size_t r = q;
            bool isf = false;
            if (r < n && s[r] == '.') {
                proven_size_t q2 = scan_run(s, n, r + 1, is_digit);
                if (q2 > r + 1) {
                    r = q2;
                    isf = true;
                    if (r < n && (s[r] == 'e' || s[r] == 'E')) {
                        proven_size_t t = r + 1;
                        if (t < n && (s[t] == '+' || s[t] == '-')) t++;
                        proven_size_t q3 = scan_run(s, n, t, is_digit);
                        if (q3 > t) r = q3;
                    }
                }
            }
            if (!isf && r < n && (s[r] == 'e' || s[r] == 'E')) {
                proven_size_t t = r + 1;
                if (t < n && (s[t] == '+' || s[t] == '-')) t++;
                proven_size_t q3 = scan_run(s, n, t, is_digit);
                if (q3 > t) { r = q3; isf = true; }
            }
            if (isf) { kind = K_DECFLOAT; end = r; }
        }
    }
    if (kind == K_NONE && hex_marker) {
        proven_size_t q = scan_run(s, n, p + 2, is_hex);
        if (q > p + 2) { kind = K_HEX; end = q; }
    }
    if (kind == K_NONE && bin_marker) {
        proven_size_t q = scan_run(s, n, p + 2, is_bin);
        if (q > p + 2) { kind = K_BIN; end = q; }
    }
    if (kind == K_NONE) { kind = K_DEC; end = scan_run(s, n, p, is_digit); }
    if (end < n && is_word(s[end])) return fail_at(L, &DG_NUMBER, line, i, lstart);
    *endp = end;

    lows_value_t v = { 0 };
    if (kind == K_DECFLOAT) {
        char buf[512];
        vec_t big = { .elem = 1 };
        char *text = buf;
        proven_size_t tl = 0, cap = sizeof buf;
        if (end - i >= cap) {
            for (proven_size_t k = i; k < end; k++)
                if (s[k] != '_' && !vec_push_byte(L->scratch, &big, s[k])) return oom(L);
            text = big.ptr;
            tl = big.len;
        } else {
            for (proven_size_t k = i; k < end; k++) if (s[k] != '_') buf[tl++] = (char)s[k];
        }
        proven_parse_f64_result_t r = proven_parse_f64_ascii((proven_u8str_view_t){ (const proven_byte_t *)text, tl });
        /* The token already matched the grammar, so a failure here can only be overflow. */
        if (r.err != PROVEN_OK || r.consumed != tl || isinf(r.val)) return fail_at(L, &DG_RANGE_FLOAT, line, i, lstart);
        v.f = r.val;
        return push_value(L, LOWS_KIND_FLOAT, v, line, i, lstart);
    }
    if (kind == K_HEXFLOAT) {
        proven_u64 m = 0;
        proven_i64 e = 0;
        bool sticky = false, frac = false;
        proven_size_t k = p + 2;
        for (; k < end && s[k] != 'p' && s[k] != 'P'; k++) {
            if (s[k] == '_') continue;
            if (s[k] == '.') { frac = true; continue; }
            int d = hexval(s[k]);
            if (m < ((proven_u64)1 << 60)) {
                m = m * 16 + (proven_u64)d;
                if (frac) e -= 4;
            } else {
                sticky |= d != 0;
                if (!frac) e += 4;
            }
        }
        k++; /* p */
        bool eneg = false;
        if (s[k] == '+' || s[k] == '-') { eneg = s[k] == '-'; k++; }
        proven_i64 be = 0;
        for (; k < end; k++) {
            if (s[k] == '_') continue;
            if (be < 100000000) be = be * 10 + (s[k] - '0'); /* beyond this the answer is 0 or overflow anyway */
        }
        double d;
        if (!hex_to_double(m, e + (eneg ? -be : be), sticky, &d)) return fail_at(L, &DG_RANGE_FLOAT, line, i, lstart);
        v.f = neg ? -d : d;
        return push_value(L, LOWS_KIND_FLOAT, v, line, i, lstart);
    }
    proven_u64 base = kind == K_HEX ? 16 : kind == K_BIN ? 2 : 10, mag = 0;
    for (proven_size_t k = (kind == K_DEC ? p : p + 2); k < end; k++) {
        if (s[k] == '_') continue;
        proven_u64 d = (proven_u64)hexval(s[k]);
        if (mag > (UINT64_MAX - d) / base) return fail_at(L, &DG_RANGE_INT, line, i, lstart);
        mag = mag * base + d;
    }
    if (neg && mag > ((proven_u64)1 << 63)) return fail_at(L, &DG_RANGE_INT, line, i, lstart);
    v.i.magnitude = mag;
    v.i.negative = neg && mag != 0;
    return push_value(L, LOWS_KIND_INT, v, line, i, lstart);
}

static proven_size_t find_nl(const proven_byte_t *s, proven_size_t n, proven_size_t from) {
    const void *p = from < n ? memchr(s + from, '\n', n - from) : NULL;
    return p ? (proven_size_t)((const proven_byte_t *)p - s) : n;
}

static bool lex(lexer_t *L) {
    const proven_byte_t *s = L->s;
    proven_size_t n = L->n, i = 0, lstart = 0;
    proven_u32 line = 1;
    while (i < n) {
        proven_byte_t ch = s[i];
        if (ch == '\n') { i++; line++; lstart = i; continue; }
        if (ch == ' ' || ch == '\t') { i++; continue; }
        if (ch == '.') {
            tok_t t = { .k = T_DOT, .line = line, .off = i, .lstart = lstart };
            if (!vec_push(L->scratch, &L->toks, &t)) return oom(L);
            i++;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            if (!lex_quoted(L, i, 0, line, lstart, &i)) return false;
            continue;
        }
        if (is_alpha_(ch)) {
            proven_size_t w = i + 1;
            while (w < n && is_word(s[w])) w++;
            const proven_byte_t *word = s + i;
            proven_size_t wl = w - i;
            if (w < n && (s[w] == '"' || s[w] == '\'')) {
                if (!(wl == 1 && (word[0] == 'u' || word[0] == 'U'))) return fail_at(L, &DG_PREFIX, line, i, lstart);
                if (!lex_quoted(L, w, (char)word[0], line, lstart, &i)) return false;
                continue;
            }
            if (word_is(word, wl, "rem")) { i = find_nl(s, n, i); continue; }
            bool is_text = word_is(word, wl, "text");
            if (is_text || word_is(word, wl, "note")) {
                proven_size_t start = i, rest_end = find_nl(s, n, w);
                if (rest_end >= n) return fail_at(L, &DG_UNCLOSED_TAGLINE, line, start, lstart);
                /* Split the rest of the line on [ \t]+ (at most three pieces matter). */
                proven_size_t ps[3], pl[3], cnt = 0;
                for (proven_size_t k = w; k < rest_end;) {
                    while (k < rest_end && (s[k] == ' ' || s[k] == '\t')) k++;
                    if (k >= rest_end) break;
                    proven_size_t b = k;
                    while (k < rest_end && s[k] != ' ' && s[k] != '\t') k++;
                    if (cnt < 3) { ps[cnt] = b; pl[cnt] = k - b; }
                    cnt++;
                }
                char prefix = 0;
                proven_size_t tag = 0, tagl = 0;
                bool tag_ok = false;
                if (is_text && cnt == 2 && is_name(s + ps[0], pl[0]) && is_name(s + ps[1], pl[1])) {
                    if (!(pl[0] == 1 && (s[ps[0]] == 'u' || s[ps[0]] == 'U'))) return fail_at(L, &DG_PREFIX, line, start, lstart);
                    prefix = (char)s[ps[0]];
                    tag = ps[1]; tagl = pl[1]; tag_ok = true;
                } else if (cnt == 1 && is_name(s + ps[0], pl[0])) {
                    tag = ps[0]; tagl = pl[0]; tag_ok = true;
                }
                if (!tag_ok) return fail_at(L, &DG_TAG, line, start, lstart);
                vec_t raw = { .elem = 1 };
                proven_size_t j = rest_end + 1, k;
                proven_u32 bline = line + 1;
                bool first = true;
                for (;;) {
                    if (j >= n) return fail_at(L, &DG_UNCLOSED_BODY, line, start, lstart);
                    k = find_nl(s, n, j);
                    proven_size_t a = j, b = k;
                    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
                    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
                    if (b - a == tagl && memcmp(s + a, s + tag, tagl) == 0) break;
                    if (is_text) {
                        if (!first && !vec_push_byte(L->scratch, &raw, '\n')) return oom(L);
                        for (proven_size_t c = j; c < k; c++)
                            if (!vec_push_byte(L->scratch, &raw, s[c])) return oom(L);
                    }
                    first = false;
                    j = k + 1;
                    bline++;
                }
                if (is_text) {
                    lows_value_t v = { 0 };
                    proven_size_t count;
                    if (!elements(L, raw.ptr, raw.len, prefix, &v, &count, line, start, lstart)) return false;
                    if (!push_value(L, str_kind(prefix), v, line, start, lstart)) return false;
                }
                i = k;
                line = bline;
                lstart = j;
                continue;
            }
            tok_t t = { .line = line, .off = i, .lstart = lstart };
            if (word_is(word, wl, "true") || word_is(word, wl, "false")) {
                t.k = T_VALUE;
                t.vk = LOWS_KIND_BOOL;
                t.v.b = word[0] == 't';
            } else if (word_is(word, wl, "do")) {
                t.k = T_DO;
            } else if (word_is(word, wl, "end")) {
                t.k = T_END;
            } else {
                t.k = T_NAME;
                char *nm = pool_alloc(L->doc, wl + 1);
                if (!nm) return oom(L);
                memcpy(nm, word, wl);
                nm[wl] = 0;
                t.name = nm;
            }
            if (!vec_push(L->scratch, &L->toks, &t)) return oom(L);
            i = w;
            continue;
        }
        if (is_digit(ch) || ((ch == '+' || ch == '-') && i + 1 < n && is_digit(s[i + 1]))) {
            if (!lex_number(L, i, line, lstart, &i)) return false;
            continue;
        }
        return fail_at(L, &DG_CHARSET, line, i, lstart);
    }
    return true;
}

/* ---- parser ----------------------------------------------------------------- */

typedef struct {
    lexer_t *L;
    tok_t *t;
    proven_size_t nt, i;
    proven_u32 open[LOWS_MAX_DEPTH];
    proven_size_t nopen;
    proven_u32 next_block;
} parser_t;

static bool perr(parser_t *P, const diag_t *d, const tok_t *t) {
    return fail_at(P->L, d, t->line, t->off, t->lstart);
}

static lows_node_t *find_child(const lows_node_t *node, const char *name) {
    lows_node_t **k = node->kids.ptr;
    for (proven_size_t i = 0; i < node->kids.len; i++)
        if (strcmp(k[i]->name, name) == 0) return k[i];
    return NULL;
}

static bool is_open(const parser_t *P, proven_u32 id) {
    for (proven_size_t k = 0; k < P->nopen; k++) if (P->open[k] == id) return true;
    return false;
}

static lows_node_t *new_node(parser_t *P, const char *name, bool leaf) {
    lows_node_t *nd = pool_alloc(P->L->doc, sizeof *nd);
    if (!nd) { oom(P->L); return NULL; }
    memset(nd, 0, sizeof *nd);
    nd->name = name;
    nd->leaf = leaf;
    nd->kids.elem = sizeof(lows_node_t *);
    return nd;
}

static bool add_child(parser_t *P, lows_node_t *parent, lows_node_t *child) {
    return vec_push(P->L->doc, &parent->kids, &child) || oom(P->L);
}

/* Walk `path` from `base`. For a leaf, *parent receives the node to hold it (the last name is new).
 * For a block, *parent receives the (possibly new) branch itself. */
static bool walk(parser_t *P, lows_node_t *base, const tok_t *path, proven_size_t np, bool want_leaf, lows_node_t **parent) {
    lows_node_t *node = base;
    for (proven_size_t k = 0; k < np; k++) {
        if (node->leaf) return perr(P, &DG_SHAPE_VALUES, &path[0]);
        if (node->block && !is_open(P, node->block)) return perr(P, &DG_SEALED, &path[0]);
        lows_node_t *child = find_child(node, path[k].name);
        bool last = k + 1 == np;
        if (child && last) {
            if (!want_leaf) return perr(P, &DG_SEALED_EXISTS, &path[0]);
            if (child->leaf) return perr(P, &DG_DUP, &path[0]);
            return perr(P, &DG_SHAPE_KEYS, &path[0]);
        }
        if (!child) {
            if (last && want_leaf) { *parent = node; return true; }
            child = new_node(P, path[k].name, false);
            if (!child || !add_child(P, node, child)) return false;
        }
        node = child;
    }
    *parent = node;
    return true;
}

static bool statements(parser_t *P, lows_node_t *base, bool closing) {
    while (P->i < P->nt) {
        tok_t *t = &P->t[P->i];
        if (t->k == T_END) {
            if (!closing) return perr(P, &DG_END, t);
            P->i++;
            return true;
        }
        if (t->k != T_NAME) return perr(P, &DG_PATH, t);
        proven_size_t p0 = P->i;
        while (P->i < P->nt && P->t[P->i].k == T_NAME) P->i++;
        proven_size_t np = P->i - p0;
        if (P->i >= P->nt) return perr(P, &DG_UNCLOSED_STMT, t);
        if (P->t[P->i].k == T_DO) {
            tok_t *do_tok = &P->t[P->i];
            if (P->nopen >= LOWS_MAX_DEPTH) return perr(P, &DG_DEPTH, do_tok);
            lows_node_t *node;
            if (!walk(P, base, &P->t[p0], np, false, &node)) return false;
            node->block = ++P->next_block;
            P->open[P->nopen++] = node->block;
            P->i++;
            if (!statements(P, node, true)) return false;
            P->nopen--;
            if (node->kids.len == 0) return perr(P, &DG_EMPTY, do_tok);
            continue;
        }
        proven_size_t v0 = P->i;
        lows_kind_t kind = LOWS_KIND_EMPTY;
        bool mixed = false;
        while (P->i < P->nt && P->t[P->i].k == T_VALUE) {
            if (P->i > v0 && P->t[P->i].vk != kind) mixed = true;
            kind = P->t[P->i].vk;
            P->i++;
        }
        if (P->i >= P->nt || P->t[P->i].k != T_DOT) {
            if (P->i < P->nt && P->t[P->i].k == T_NAME) return perr(P, &DG_ORDER, &P->t[P->i]);
            return perr(P, &DG_UNCLOSED_STMT, P->i < P->nt ? &P->t[P->i] : t);
        }
        if (mixed) return perr(P, &DG_MIX, t);
        P->i++;
        lows_node_t *parent;
        if (!walk(P, base, &P->t[p0], np, true, &parent)) return false;
        lows_node_t *leaf = new_node(P, P->t[p0 + np - 1].name, true);
        if (!leaf) return false;
        leaf->kind = kind;
        leaf->nvals = P->i - 1 - v0;
        if (leaf->nvals) {
            leaf->vals = pool_alloc(P->L->doc, leaf->nvals * sizeof(lows_value_t));
            if (!leaf->vals) return oom(P->L);
            for (proven_size_t k = 0; k < leaf->nvals; k++) leaf->vals[k] = P->t[v0 + k].v;
        }
        if (!add_child(P, parent, leaf)) return false;
    }
    if (closing) return perr(P, &DG_UNCLOSED_DO, &P->t[P->nt - 1]);
    return true;
}

/* ---- entry points ------------------------------------------------------------ */

static proven_u32 cols(const proven_byte_t *s, proven_size_t from, proven_size_t to) {
    proven_u32 c = 1;
    for (proven_size_t k = from; k < to; k++) if ((s[k] & 0xC0) != 0x80) c++;
    return c;
}

static proven_err_t report(lows_error_t *err, const diag_t *d, proven_u32 line, proven_u32 col) {
    if (err) { err->line = line; err->col = col; err->code = d->code; err->message = d->message; }
    return PROVEN_ERR_INVALID_FORMAT;
}

proven_err_t lows_parse(proven_allocator_t alloc, const proven_byte_t *src, proven_size_t len,
                        lows_doc_t **out, lows_error_t *err) {
    if (!out || (!src && len) || !proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    *out = NULL;
    if (!src) src = (const proven_byte_t *)"";

    /* Order as in the other implementations: UTF-8, BOM, CRLF, lone CR. */
    proven_size_t bad = utf8_first_invalid(src, len);
    if (bad != len) {
        proven_size_t ls = 0;
        proven_u32 line = 1;
        for (proven_size_t k = 0; k < bad; k++) if (src[k] == '\n') { line++; ls = k + 1; }
        return report(err, &DG_UTF8, line, cols(src, ls, bad));
    }
    if (len >= 3 && src[0] == 0xEF && src[1] == 0xBB && src[2] == 0xBF) return report(err, &DG_BOM, 1, 1);

    pool_t docpool = { .alloc = alloc }, scratch = { .alloc = alloc };
    lows_doc_t *doc = pool_alloc(&docpool, sizeof *doc);
    proven_byte_t *s = pool_alloc(&scratch, len ? len : 1);
    if (!doc || !s) { pool_free(&docpool); pool_free(&scratch); return PROVEN_ERR_NOMEM; }
    proven_size_t n = 0;
    for (proven_size_t k = 0; k < len; k++) {
        if (src[k] == '\r' && k + 1 < len && src[k + 1] == '\n') continue;
        s[n++] = src[k];
    }
    for (proven_size_t k = 0, ls = 0, line = 1; k < n; k++) {
        if (s[k] == '\n') { line++; ls = k + 1; }
        if (s[k] == '\r') {
            proven_u32 col = cols(s, ls, k); /* before the pool that holds `s` is gone */
            pool_free(&docpool);
            pool_free(&scratch);
            return report(err, &DG_CR, (proven_u32)line, col);
        }
    }

    lexer_t L = { .doc = &docpool, .scratch = &scratch, .s = s, .n = n, .toks = { .elem = sizeof(tok_t) } };
    parser_t P = { .L = &L };
    lows_node_t *root = NULL;
    bool ok = lex(&L);
    if (ok) {
        P.t = L.toks.ptr;
        P.nt = L.toks.len;
        root = new_node(&P, "", false);
        ok = root && statements(&P, root, false);
    }
    proven_err_t rc = PROVEN_OK;
    if (!ok) {
        rc = L.nomem ? PROVEN_ERR_NOMEM : report(err, L.diag, L.eline, cols(s, L.elstart, L.eoff));
    }
    pool_free(&scratch);
    if (!ok) { pool_free(&docpool); return rc; }
    doc->pool = docpool;
    doc->root = root;
    *out = doc;
    return PROVEN_OK;
}

void lows_doc_free(lows_doc_t *doc) {
    if (!doc) return;
    pool_t p = doc->pool; /* the doc lives inside its own pool */
    pool_free(&p);
}

const lows_node_t *lows_doc_root(const lows_doc_t *doc) { return doc ? doc->root : NULL; }

proven_allocator_t lows_default_allocator(void) { return proven_heap_allocator(); }

const lows_node_t *lows_lookup(const lows_node_t *node, const char *path) {
    if (!node || !path) return NULL;
    const char *p = path;
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) return node;
        const char *b = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (node->leaf) return NULL;
        proven_size_t wl = (proven_size_t)(p - b);
        const lows_node_t *next = NULL;
        lows_node_t *const *k = node->kids.ptr;
        for (proven_size_t i = 0; i < node->kids.len; i++)
            if (strlen(k[i]->name) == wl && memcmp(k[i]->name, b, wl) == 0) { next = k[i]; break; }
        if (!next) return NULL;
        node = next;
    }
}

bool lows_node_is_leaf(const lows_node_t *node) { return node && node->leaf; }
const char *lows_node_name(const lows_node_t *node) { return node ? node->name : ""; }
proven_size_t lows_branch_count(const lows_node_t *node) { return node && !node->leaf ? node->kids.len : 0; }
const lows_node_t *lows_branch_child(const lows_node_t *node, proven_size_t i) {
    return node && !node->leaf && i < node->kids.len ? ((lows_node_t *const *)node->kids.ptr)[i] : NULL;
}
lows_kind_t lows_leaf_kind(const lows_node_t *node) { return node && node->leaf ? node->kind : LOWS_KIND_EMPTY; }
proven_size_t lows_leaf_count(const lows_node_t *node) { return node && node->leaf ? node->nvals : 0; }
const lows_value_t *lows_leaf_value(const lows_node_t *node, proven_size_t i) {
    return node && node->leaf && i < node->nvals ? &node->vals[i] : NULL;
}

const char *lows_kind_name(lows_kind_t kind) {
    switch (kind) {
    case LOWS_KIND_INT: return "int";
    case LOWS_KIND_FLOAT: return "float";
    case LOWS_KIND_BOOL: return "bool";
    case LOWS_KIND_STR: return "str";
    case LOWS_KIND_STR16: return "u_str";
    case LOWS_KIND_STR32: return "U_str";
    case LOWS_KIND_CHAR: return "char";
    case LOWS_KIND_CHAR16: return "u_char";
    case LOWS_KIND_CHAR32: return "U_char";
    case LOWS_KIND_EMPTY: break;
    }
    return "empty";
}

static proven_err_t one(const lows_node_t *node, lows_kind_t kind, const lows_value_t **v) {
    if (!node || !node->leaf || node->nvals != 1) return PROVEN_ERR_NOT_FOUND;
    if (node->kind != kind) return PROVEN_ERR_INVALID_ARG;
    *v = &node->vals[0];
    return PROVEN_OK;
}

proven_err_t lows_get_i64(const lows_node_t *node, proven_i64 *out) {
    const lows_value_t *v;
    proven_err_t e = one(node, LOWS_KIND_INT, &v);
    if (e != PROVEN_OK) return e;
    if (v->i.negative) {
        if (v->i.magnitude == (proven_u64)1 << 63) { *out = INT64_MIN; return PROVEN_OK; }
        *out = -(proven_i64)v->i.magnitude;
        return PROVEN_OK;
    }
    if (v->i.magnitude > INT64_MAX) return PROVEN_ERR_OVERFLOW;
    *out = (proven_i64)v->i.magnitude;
    return PROVEN_OK;
}

proven_err_t lows_get_u64(const lows_node_t *node, proven_u64 *out) {
    const lows_value_t *v;
    proven_err_t e = one(node, LOWS_KIND_INT, &v);
    if (e != PROVEN_OK) return e;
    if (v->i.negative) return PROVEN_ERR_OVERFLOW;
    *out = v->i.magnitude;
    return PROVEN_OK;
}

proven_err_t lows_get_f64(const lows_node_t *node, double *out) {
    const lows_value_t *v;
    proven_err_t e = one(node, LOWS_KIND_FLOAT, &v);
    if (e == PROVEN_OK) *out = v->f;
    return e;
}

proven_err_t lows_get_bool(const lows_node_t *node, bool *out) {
    const lows_value_t *v;
    proven_err_t e = one(node, LOWS_KIND_BOOL, &v);
    if (e == PROVEN_OK) *out = v->b;
    return e;
}

proven_err_t lows_get_bytes(const lows_node_t *node, const proven_byte_t **ptr, proven_size_t *len) {
    const lows_value_t *v;
    proven_err_t e = one(node, LOWS_KIND_STR, &v);
    if (e == PROVEN_OK) { *ptr = v->s.ptr; *len = v->s.len; }
    return e;
}

/* ---- canonical dump (spec annex C) --------------------------------------------- */

typedef struct {
    proven_allocator_t a;
    char *p;
    proven_size_t len, cap;
    bool nomem;
} sbuf_t;

static void sb_put(sbuf_t *b, const char *s, proven_size_t n) {
    if (b->nomem) return;
    if (b->len + n + 1 > b->cap) {
        proven_size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + n + 1) cap *= 2;
        proven_result_mem_mut_t r = b->p
            ? b->a.realloc_fn(b->a.ctx, b->p, b->cap, cap, 1)
            : b->a.alloc_fn(b->a.ctx, cap, 1);
        if (r.err != PROVEN_OK) { b->nomem = true; return; }
        b->p = (char *)r.value.ptr;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

static void sb_str(sbuf_t *b, const char *s) { sb_put(b, s, strlen(s)); }

static void sb_u64(sbuf_t *b, proven_u64 v) {
    char t[24];
    int k = 23;
    t[k] = 0;
    do { t[--k] = (char)('0' + v % 10); v /= 10; } while (v);
    sb_str(b, t + k);
}

static void sb_hex(sbuf_t *b, const proven_byte_t *p, proven_size_t n) {
    static const char H[] = "0123456789abcdef";
    for (proven_size_t k = 0; k < n; k++) { char t[2] = { H[p[k] >> 4], H[p[k] & 15] }; sb_put(b, t, 2); }
}

static void dump_value(sbuf_t *b, lows_kind_t kind, const lows_value_t *x) {
    switch (kind) {
    case LOWS_KIND_INT:
        if (x->i.negative) sb_str(b, "-");
        sb_u64(b, x->i.magnitude);
        break;
    case LOWS_KIND_FLOAT: {
        proven_u64 bits;
        memcpy(&bits, &x->f, 8);
        proven_byte_t be[8];
        for (int q = 0; q < 8; q++) be[q] = (proven_byte_t)(bits >> (56 - 8 * q));
        sb_hex(b, be, 8);
        break;
    }
    case LOWS_KIND_BOOL: sb_str(b, x->b ? "true" : "false"); break;
    case LOWS_KIND_STR: sb_str(b, "x:"); sb_hex(b, x->s.ptr, x->s.len); break;
    case LOWS_KIND_STR16:
    case LOWS_KIND_STR32:
        sb_str(b, "[");
        for (proven_size_t e = 0; e < x->s.len; e++) {
            if (e) sb_str(b, ",");
            sb_u64(b, kind == LOWS_KIND_STR16 ? ((const proven_u16 *)x->s.ptr)[e] : ((const proven_u32 *)x->s.ptr)[e]);
        }
        sb_str(b, "]");
        break;
    case LOWS_KIND_CHAR:
    case LOWS_KIND_CHAR16:
    case LOWS_KIND_CHAR32: sb_u64(b, x->ch); break;
    case LOWS_KIND_EMPTY: break;
    }
}

/* `path` holds the dotted path of `node`; each child appends its name and cuts it off again. */
static void dump_node(sbuf_t *b, sbuf_t *path, const lows_node_t *node) {
    lows_node_t *const *k = node->kids.ptr;
    for (proven_size_t i = 0; i < node->kids.len && !b->nomem && !path->nomem; i++) {
        const lows_node_t *c = k[i];
        proven_size_t old = path->len;
        if (old) sb_str(path, ".");
        sb_str(path, c->name);
        if (path->nomem) { b->nomem = true; return; }
        if (!c->leaf) {
            dump_node(b, path, c);
        } else {
            sb_put(b, path->p, path->len);
            sb_str(b, "\t");
            sb_str(b, lows_kind_name(c->kind));
            sb_str(b, "\t");
            for (proven_size_t v = 0; v < c->nvals; v++) {
                if (v) sb_str(b, " ");
                dump_value(b, c->kind, &c->vals[v]);
            }
            sb_str(b, "\n");
        }
        path->len = old;
        path->p[old] = 0;
    }
    if (path->nomem) b->nomem = true;
}

proven_err_t lows_dump_canonical(const lows_doc_t *doc, proven_allocator_t alloc, char **out, proven_size_t *len) {
    if (!doc || !out || !proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    sbuf_t b = { .a = alloc };
    sbuf_t path = { .a = alloc };
    sb_str(&b, "lowstruct-dump 1\n");
    sb_str(&path, "");
    dump_node(&b, &path, doc->root);
    if (path.p) alloc.free_fn(alloc.ctx, path.p);
    if (b.nomem || path.nomem) {
        if (b.p) alloc.free_fn(alloc.ctx, b.p);
        return PROVEN_ERR_NOMEM;
    }
    *out = b.p;
    if (len) *len = b.len;
    return PROVEN_OK;
}
