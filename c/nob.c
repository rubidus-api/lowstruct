// Build script for the lowstruct C library.
//
//   cc -o ../build/nob nob.c    once; nob rebuilds itself when this file changes
//   ../build/nob                native build (static + shared library) and the conformance suite against both
//   ../build/nob lib            native libraries only
//   ../build/nob windows        cross-build for 64-bit Windows with x86_64-w64-mingw32-gcc:
//                               static library, DLL + import library + .def, and two test programs
//
// Output: ../build/c/ (native) and ../build/c/windows-x86_64/ (Windows).
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "vendor/proven/nob.h"

#define OUT "../build/c/"
#define WOUT OUT "windows-x86_64/"
#define SOVERSION "0"

static const char *LIB_SRC[] = {
    "src/lowstruct.c",
    // proven_c_lib: only what lowstruct links against
    "vendor/proven/src/proven/float_parse.c",
    "vendor/proven/src/proven/float_decimal.c",
    "vendor/proven/src/proven/heap.c",
    "vendor/proven/platform/proven_sys_mem.c",
};

static const char *version(void) {
    // LOWSTRUCT_VERSION_STRING from the header is the one place the version is written.
    String_Builder sb = { 0 };
    if (!read_entire_file("include/lowstruct.h", &sb)) return "0.0.0";
    sb_append_null(&sb);
    const char *p = strstr(sb.items, "LOWSTRUCT_VERSION_STRING \"");
    if (!p) return "0.0.0";
    p += strlen("LOWSTRUCT_VERSION_STRING \"");
    const char *q = strchr(p, '"');
    return temp_sprintf("%.*s", (int)(q - p), p);
}

static void cflags(Cmd *cmd) {
    cmd_append(cmd, "-std=c23", "-Wall", "-Wextra", "-Wshadow", "-O2", "-g");
    cmd_append(cmd, "-Iinclude", "-Ivendor/proven/include", "-Ivendor/proven/platform");
}

static bool mkdirs(const char *const *dirs, size_t n) {
    for (size_t i = 0; i < n; i++) if (!mkdir_if_not_exists(dirs[i])) return false;
    return true;
}

// Compile LIB_SRC into objdir with the given compiler and extra flags; the object paths go to objs.
static bool compile(Cmd *cmd, const char *cc, const char *objdir, const char **extra, size_t nextra, File_Paths *objs) {
    for (size_t i = 0; i < ARRAY_LEN(LIB_SRC); i++) {
        const char *base = path_name(LIB_SRC[i]);
        const char *obj = temp_sprintf("%s%.*s.o", objdir, (int)(strlen(base) - 2), base);
        da_append(objs, obj);
        const char *deps[] = { LIB_SRC[i], "include/lowstruct.h", "nob.c" };
        if (!needs_rebuild(obj, deps, ARRAY_LEN(deps))) continue;
        cmd_append(cmd, cc);
        cflags(cmd);
        da_append_many(cmd, extra, nextra);
        cmd_append(cmd, "-c", LIB_SRC[i], "-o", obj);
        if (!cmd_run(cmd)) return false;
    }
    return true;
}

static bool archive(Cmd *cmd, const char *ar, const char *lib, File_Paths *objs) {
    if (file_exists(lib) == 1) delete_file(lib); // `ar rcs` would keep members that are no longer built
    cmd_append(cmd, ar, "rcs", lib);
    da_append_many(cmd, objs->items, objs->count);
    return cmd_run(cmd);
}

static bool build_native(Cmd *cmd) {
    const char *dirs[] = { "../build", OUT, OUT "obj" };
    if (!mkdirs(dirs, ARRAY_LEN(dirs))) return false;
    // One set of position-independent objects serves both libraries. Hidden visibility keeps
    // proven_c_lib's symbols out of the shared library's exports; only LOWS_API functions are public.
    const char *extra[] = { "-fPIC", "-fvisibility=hidden", "-DLOWS_BUILDING" };
    File_Paths objs = { 0 };
    if (!compile(cmd, "cc", OUT "obj/", extra, ARRAY_LEN(extra), &objs)) return false;
    if (!archive(cmd, "ar", OUT "liblowstruct.a", &objs)) return false;

    const char *so = temp_sprintf(OUT "liblowstruct.so.%s", version());
    cmd_append(cmd, "cc", "-shared", "-Wl,-soname,liblowstruct.so." SOVERSION, "-o", so);
    da_append_many(cmd, objs.items, objs.count);
    cmd_append(cmd, "-lm");
    if (!cmd_run(cmd)) return false;
    // liblowstruct.so.0 -> liblowstruct.so.X.Y.Z, liblowstruct.so -> liblowstruct.so.0
    cmd_append(cmd, "ln", "-sf", path_name(so), OUT "liblowstruct.so." SOVERSION);
    if (!cmd_run(cmd)) return false;
    cmd_append(cmd, "ln", "-sf", "liblowstruct.so." SOVERSION, OUT "liblowstruct.so");
    return cmd_run(cmd);
}

static bool build_windows(Cmd *cmd) {
    const char *cc = "x86_64-w64-mingw32-gcc", *ar = "x86_64-w64-mingw32-ar";
    const char *dirs[] = { "../build", OUT, WOUT, WOUT "obj-static", WOUT "obj-dll" };
    if (!mkdirs(dirs, ARRAY_LEN(dirs))) return false;

    File_Paths sobjs = { 0 }, dobjs = { 0 };
    if (!compile(cmd, cc, WOUT "obj-static/", NULL, 0, &sobjs)) return false;
    if (!archive(cmd, ar, WOUT "liblowstruct.a", &sobjs)) return false;

    const char *dll_extra[] = { "-DLOWS_BUILDING", "-DLOWS_SHARED" };
    if (!compile(cmd, cc, WOUT "obj-dll/", dll_extra, ARRAY_LEN(dll_extra), &dobjs)) return false;
    // The DLL depends only on the system UCRT: libgcc is linked in statically.
    cmd_append(cmd, cc, "-shared", "-static-libgcc", "-o", WOUT "lowstruct.dll");
    da_append_many(cmd, dobjs.items, dobjs.count);
    cmd_append(cmd, "-Wl,--out-implib," WOUT "liblowstruct.dll.a", "-Wl,--output-def," WOUT "lowstruct.def");
    if (!cmd_run(cmd)) return false;

    // Test programs: one linked statically, one against the DLL. `-static` keeps the MinGW runtime inside.
    cmd_append(cmd, cc);
    cflags(cmd);
    cmd_append(cmd, "-static", "tests/test_lowstruct.c", WOUT "liblowstruct.a", "-o", WOUT "test_lowstruct.exe");
    if (!cmd_run(cmd)) return false;
    cmd_append(cmd, cc);
    cflags(cmd);
    cmd_append(cmd, "-DLOWS_SHARED", "-static", "tests/test_lowstruct.c", WOUT "liblowstruct.dll.a", "-o", WOUT "test_lowstruct_dll.exe");
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

static bool run_suite(Cmd *cmd, const char *exe) {
    cmd_append(cmd, exe);
    if (!collect("../conformance/accept", cmd) || !collect("../conformance/reject", cmd)) return false;
    return cmd_run(cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = { 0 };
    const char *what = argc > 1 ? argv[1] : "";
    if (strcmp(what, "windows") == 0) return build_windows(&cmd) ? 0 : 1;
    if (!build_native(&cmd)) return 1;
    if (strcmp(what, "lib") == 0) return 0;

    cmd_append(&cmd, "cc");
    cflags(&cmd);
    cmd_append(&cmd, "tests/test_lowstruct.c", OUT "liblowstruct.a", "-lm", "-o", OUT "test_lowstruct");
    if (!cmd_run(&cmd)) return 1;
    cmd_append(&cmd, "cc");
    cflags(&cmd);
    cmd_append(&cmd, "-DLOWS_SHARED", "tests/test_lowstruct.c", "-L" OUT, "-llowstruct", "-Wl,-rpath,$ORIGIN",
               "-o", OUT "test_lowstruct_shared");
    if (!cmd_run(&cmd)) return 1;

    if (!run_suite(&cmd, OUT "test_lowstruct")) return 1;
    return run_suite(&cmd, OUT "test_lowstruct_shared") ? 0 : 1;
}
