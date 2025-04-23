#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "./src/nob.h"

#define CFLAGS "-Wall", "-Wextra", "-pedantic", "-ggdb", "-std=c11"

#ifdef _WIN32
#    define CC "clang"
#else
#    define CC "cc"
#endif

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = {0};
    cmd_append(&cmd, CC, CFLAGS, "-o", "canal", "src/main.c", "src/str.c");
    if (!cmd_run_sync_and_reset(&cmd)) {
        return 1;
    }
    return 0;
}
