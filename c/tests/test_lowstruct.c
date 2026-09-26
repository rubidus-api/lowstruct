/*
 * test_lowstruct — the shared conformance suite plus API checks for the C library.
 *
 * usage: test_lowstruct <case.lows>...
 *   A path containing "accept" must parse and dump exactly as its sibling .dump file.
 *   Any other path must be rejected with the code named on its `rem expect E-LOWS-...` line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lowstruct.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static unsigned char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 4096, n = 0;
    unsigned char *buf = malloc(cap);
    for (;;) {
        if (n == cap) buf = realloc(buf, cap *= 2);
        size_t r = fread(buf + n, 1, cap - n, f);
        if (r == 0) break;
        n += r;
    }
    fclose(f);
    *len = n;
    return buf;
}

static void run_case(const char *path) {
    size_t len;
    unsigned char *src = read_file(path, &len);
    CHECK(src != NULL, "%s: cannot read", path);
    if (!src) return;
    proven_allocator_t heap = lows_default_allocator();
    lows_doc_t *doc = NULL;
    lows_error_t e = { 0 };
    proven_err_t rc = lows_parse(heap, src, len, &doc, &e);
    if (strstr(path, "accept")) {
        CHECK(rc == PROVEN_OK, "%s: rejected: %u:%u %s: %s", path, e.line, e.col, e.code, e.message);
        if (rc == PROVEN_OK) {
            char dump_path[4096];
            snprintf(dump_path, sizeof dump_path, "%.*s.dump", (int)(strlen(path) - 5), path);
            size_t wl;
            unsigned char *want = read_file(dump_path, &wl);
            char *got = NULL;
            size_t gl = 0;
            CHECK(want != NULL, "%s: missing", dump_path);
            CHECK(lows_dump_canonical(doc, heap, &got, &gl) == PROVEN_OK, "%s: dump failed", path);
            if (want && got) {
                bool same = gl == wl && memcmp(got, want, gl) == 0;
                CHECK(same, "%s: dump differs\n--- got\n%s--- want\n%.*s", path, got, (int)wl, (const char *)want);
            }
            if (got) heap.free_fn(heap.ctx, got);
            free(want);
            lows_doc_free(doc);
        }
    } else {
        const char *m = strstr((const char *)src, "expect ");
        char want[32] = { 0 };
        if (m) sscanf(m + 7, "%31[A-Z0-9-]", want);
        CHECK(want[0], "%s: no `expect` line", path);
        CHECK(rc == PROVEN_ERR_INVALID_FORMAT, "%s: accepted (want %s)", path, want);
        if (rc == PROVEN_ERR_INVALID_FORMAT) CHECK(strcmp(e.code, want) == 0, "%s: got %s, want %s (%s)", path, e.code, want, e.message);
        if (rc == PROVEN_OK) lows_doc_free(doc);
    }
    free(src);
}

static lows_doc_t *parse_str(const char *s, lows_error_t *e) {
    lows_doc_t *doc = NULL;
    lows_error_t tmp;
    if (lows_parse(lows_default_allocator(), (const proven_byte_t *)s, strlen(s), &doc, e ? e : &tmp) != PROVEN_OK) return NULL;
    return doc;
}

static void api_tests(void) {
    lows_doc_t *doc = parse_str("server do\n  port 8080 .\n  name \"\xc3\xa9\" .\n  big 18446744073709551615 .\n  neg -9223372036854775808 .\n  on true .\n  r 0.5 .\nend\n", NULL);
    CHECK(doc != NULL, "api doc parses");
    if (!doc) return;
    const lows_node_t *root = lows_doc_root(doc);
    proven_i64 i;
    proven_u64 u;
    double f;
    bool b;
    const proven_byte_t *p;
    proven_size_t n;
    CHECK(lows_get_i64(lows_lookup(root, "server port"), &i) == PROVEN_OK && i == 8080, "port");
    CHECK(lows_get_u64(lows_lookup(root, " server\tbig "), &u) == PROVEN_OK && u == UINT64_MAX, "big as u64");
    CHECK(lows_get_i64(lows_lookup(root, "server big"), &i) == PROVEN_ERR_OVERFLOW, "big does not fit i64");
    CHECK(lows_get_i64(lows_lookup(root, "server neg"), &i) == PROVEN_OK && i == INT64_MIN, "i64 min");
    CHECK(lows_get_u64(lows_lookup(root, "server neg"), &u) == PROVEN_ERR_OVERFLOW, "negative is not u64");
    CHECK(lows_get_bool(lows_lookup(root, "server on"), &b) == PROVEN_OK && b, "bool");
    CHECK(lows_get_f64(lows_lookup(root, "server r"), &f) == PROVEN_OK && f == 0.5, "float");
    CHECK(lows_get_bytes(lows_lookup(root, "server name"), &p, &n) == PROVEN_OK && n == 2 && p[0] == 0xC3, "bytes");
    CHECK(lows_get_i64(lows_lookup(root, "server name"), &i) == PROVEN_ERR_INVALID_ARG, "wrong kind");
    CHECK(lows_lookup(root, "server nope") == NULL, "absent");
    CHECK(lows_get_i64(lows_lookup(root, "server"), &i) == PROVEN_ERR_NOT_FOUND, "branch is not a value");
    const lows_node_t *srv = lows_lookup(root, "server");
    CHECK(lows_branch_count(srv) == 6 && strcmp(lows_node_name(lows_branch_child(srv, 1)), "name") == 0, "source order");
    CHECK(strcmp(lows_kind_name(lows_leaf_kind(lows_lookup(root, "server name"))), "str") == 0, "kind name");
    lows_doc_free(doc);

    lows_error_t e;
    CHECK(parse_str("a 1 .\nb 1 \"x\" .\n", &e) == NULL && e.line == 2 && e.col == 1 && strcmp(e.code, "E-LOWS-MIX") == 0, "mix position");
    CHECK(parse_str("a \"\xff\" .\n", &e) == NULL && e.line == 1 && e.col == 4 && strcmp(e.code, "E-LOWS-UTF8") == 0, "utf8 position");
    CHECK(parse_str("a \"\xf0\x9f\x98\x80\" \xd9\xa3 .", &e) == NULL && e.col == 7 && strcmp(e.code, "E-LOWS-CHARSET") == 0, "column counts code points");

    lows_doc_t *out = (lows_doc_t *)1;
    CHECK(lows_parse(lows_default_allocator(), NULL, 0, &out, NULL) == PROVEN_OK && out != NULL, "empty input is an empty document");
    if (out) lows_doc_free(out);
    CHECK(lows_parse(lows_default_allocator(), (const proven_byte_t *)"a", 1, NULL, NULL) == PROVEN_ERR_INVALID_ARG, "NULL out");
}

int main(int argc, char **argv) {
    for (int k = 1; k < argc; k++) run_case(argv[k]);
    api_tests();
    printf("test_lowstruct: %d checks, %d cases, %d failures\n", checks, argc - 1, failures);
    return failures ? 1 : 0;
}
