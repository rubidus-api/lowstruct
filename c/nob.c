// Build script for the lowstruct C library.
//
//   cc -o nob nob.c      (once; nob rebuilds itself afterwards)
//   ./nob                builds build/liblowstruct.a and runs the conformance tests
//   ./nob lib            builds the library only
//
// Output goes to ../build/c/ (the project's build/ directory).
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "vendor/proven/nob.h"

#define OUT "../build/c/"

static const char *LIB_SRC[] = {
    "src/lowstruct.c",
    // proven_c_lib: only what lowstruct links against
    "vendor/proven/src/proven/float_parse.c",
    "vendor/proven/src/proven/float_decimal.c",
    "vendor/proven/src/proven/heap.c",
    "vendor/proven/platform/proven_sys_mem.c",
};

static void cflags(Cmd *cmd) {
    cmd_append(cmd, "-std=c23", "-Wall", "-Wextra", "-Wshadow", "-O2", "-g");
    cmd_append(cmd, "-Iinclude", "-Ivendor/proven/include", "-Ivendor/proven/platform");
}

static bool build_lib(Cmd *cmd, File_Paths *objs) {
    if (!mkdir_if_not_exists("../build") || !mkdir_if_not_exists(OUT) || !mkdir_if_not_exists(OUT "obj")) return false;
    for (size_t i = 0; i < ARRAY_LEN(LIB_SRC); i++) {
        const char *base = path_name(LIB_SRC[i]);
        const char *obj = temp_sprintf(OUT "obj/%.*s.o", (int)(strlen(base) - 2), base);
        da_append(objs, obj);
        if (!needs_rebuild1(obj, LIB_SRC[i]) && !needs_rebuild1(obj, "include/lowstruct.h")) continue;
        cmd_append(cmd, "cc");
        cflags(cmd);
        cmd_append(cmd, "-c", LIB_SRC[i], "-o", obj);
        if (!cmd_run(cmd)) return false;
    }
    cmd_append(cmd, "ar", "rcs", OUT "liblowstruct.a");
    da_append_many(cmd, objs->items, objs->count);
    return cmd_run(cmd);
}

static int by_name(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool collect(const char *dir, Cmd *cmd) {
    File_Paths names = { 0 };
    if (!read_entire_dir(dir, &names)) return false;
    qsort(names.items, names.count, sizeof(*names.items), by_name);
    for (size_t i = 0; i < names.count; i++) {
        size_t n = strlen(names.items[i]);
        if (n > 5 && strcmp(names.items[i] + n - 5, ".lows") == 0) cmd_append(cmd, temp_sprintf("%s/%s", dir, names.items[i]));
    }
    return true;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    File_Paths objs = { 0 };
    if (!build_lib(&cmd, &objs)) return 1;
    if (argc > 1 && strcmp(argv[1], "lib") == 0) return 0;

    cmd_append(&cmd, "cc");
    cflags(&cmd);
    cmd_append(&cmd, "tests/test_lowstruct.c", OUT "liblowstruct.a", "-lm", "-o", OUT "test_lowstruct");
    if (!cmd_run(&cmd)) return 1;

    cmd_append(&cmd, OUT "test_lowstruct");
    if (!collect("../conformance/accept", &cmd) || !collect("../conformance/reject", &cmd)) return 1;
    return cmd_run(&cmd) ? 0 : 1;
}
