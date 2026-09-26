/* lows_cli — print the canonical dump of each file, or "ERR <code> <line>:<col>" (fuzz harness). */
#include <stdio.h>
#include <stdlib.h>
#include "lowstruct.h"
#include "proven/heap.h"

int main(int argc, char **argv) {
    for (int k = 1; k < argc; k++) {
        FILE *f = fopen(argv[k], "rb");
        if (!f) { printf("NOFILE\n"); continue; }
        size_t cap = 1 << 16, n = 0;
        unsigned char *buf = malloc(cap);
        size_t r;
        while ((r = fread(buf + n, 1, cap - n, f)) > 0) { n += r; if (n == cap) buf = realloc(buf, cap *= 2); }
        fclose(f);
        lows_doc_t *doc;
        lows_error_t e;
        proven_err_t rc = lows_parse(proven_heap_allocator(), buf, n, &doc, &e);
        if (rc == PROVEN_ERR_INVALID_FORMAT) printf("ERR %s %u:%u\n", e.code, e.line, e.col);
        else if (rc != PROVEN_OK) printf("FAIL %d\n", (int)rc);
        else {
            char *out; size_t len;
            if (lows_dump_canonical(doc, proven_heap_allocator(), &out, &len) == PROVEN_OK) { fwrite(out, 1, len, stdout); free(out); }
            lows_doc_free(doc);
        }
        printf("\x1e\n");
        free(buf);
    }
    return 0;
}
