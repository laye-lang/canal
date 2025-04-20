#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "./utils.h"
#define ARENA_IMPLEMENTATION
#include "./arena.h"
#define NOB_IMPLEMENTATION
#include "./nob.h"

typedef int Errno;
#define return_defer(val) do { result = (val); goto defer; } while (0)

// NOTE(nic): your string will be overwritten
Errno read_entire_file(Arena *arena, String *str, const char *filepath) {
    Errno result = 0;

    FILE *file = fopen(filepath, "r");
    if (file == NULL) return_defer(errno);

    if (fseek(file, 0L, SEEK_END) != 0) return_defer(errno);
    long file_size = ftell(file);
    if (file_size < 0) return_defer(errno);
    rewind(file);

    str_ensure_capacity(arena, str, file_size);
    fread(str->items, file_size, sizeof(char), file);
    str->count = file_size;

defer:
    if (file != NULL) fclose(file);
    return result;
}

typedef enum {
    CANAL_ACTION_ASTERISK,
    CANAL_ACTION_PLUS,
    CANAL_ACTION_RUN,
} Canal_Action;

typedef struct {
    Canal_Action action;
    String_View arguments;
} Canal_Directive;

typedef struct {
    Canal_Directive *items;
    size_t count;
    size_t capacity;
} Canal_Directives;

typedef struct {
    Canal_Directives r_directives;
    Canal_Directives directives;
} Canal_Check;

int not_isspace(int ch) {
    return !isspace(ch);
}

void canal_collect_directives(Arena *arena, Canal_Check *check, String_View source) {
    // TODO(nic): make the prefix customizable
    const char *prefix = "//";
    while (source.count > 0) {
        String_View line = sv_chop_until(&source, '\n');
        if (sv_starts_with(line, prefix)) {
            sv_chop(&line, strlen(prefix));
            line = sv_trim(line);

            String_View action = sv_chop_predicate(&line, not_isspace);
            String_View arguments = line;

            Canal_Directive directive = {0};
            directive.arguments = arguments;
            if (sv_eq(action, SV("*"))) {
                directive.action = CANAL_ACTION_ASTERISK;
            } else if (sv_eq(action, SV("+"))) {
                directive.action = CANAL_ACTION_PLUS;
            } else if (sv_eq(action, SV("R"))) {
                directive.action = CANAL_ACTION_RUN;
            } else {
                continue;
            }

            if (directive.action == CANAL_ACTION_RUN) {
                arena_da_append(arena, &check->r_directives, directive);
            } else {
                arena_da_append(arena, &check->directives, directive);
            }
        }
    }
}

void canal_check(Arena *arena, Nob_Cmd *cmd, Canal_Check *check, const char *filepath) {
    for (size_t i = 0; i < check->r_directives.count; ++i) {
        Canal_Directive *r_directive = &check->r_directives.items[i];
        assert(r_directive->action == CANAL_ACTION_RUN);

        String_View args = r_directive->arguments;
        while (args.count > 0) {
            args = sv_trim_left(args);
            String_View arg_fmt = sv_chop_predicate(&args, not_isspace);
            if (arg_fmt.count == 0) break;

            // TODO(nic): implement proper foramtting
            String arg = {0};
            if (sv_eq(arg_fmt, SV("%s"))) {
                str_append_cstr(arena, &arg, filepath);
                str_append_null(arena, &arg);
            } else {
                str_append_sv(arena, &arg, arg_fmt);
                str_append_null(arena, &arg);
            }
            nob_cmd_append(cmd, arg.items);
        }

        // TODO(nic): maybe find a way of creating temp file without fisically creating it
        const char *temp_filepath = "./temp";
        Nob_Fd fdout = nob_fd_open_for_write(temp_filepath);
        Nob_Cmd_Redirect cmd_redirect = {
            .fdout = &fdout,
        };
        // TODO(nic): check for errors in all these calls
        nob_cmd_run_sync_redirect_and_reset(cmd, cmd_redirect);
        String file_data = {0};
        read_entire_file(arena, &file_data, temp_filepath);
        nob_fd_close(fdout);

        nob_cmd_append(cmd, "rm", temp_filepath);
        nob_cmd_run_sync_and_reset(cmd);

        printf(STR_FMT"\n", STR_ARG(&file_data));
    }
}

int main(int argc, const char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Error: expected filepath\n");
        exit(1);
    }
    const char *filepath = argv[1];

    Arena arena = {0};
    String file_data = {0};

    Errno err = read_entire_file(&arena, &file_data, filepath);
    if (err) {
        fprintf(stderr, "Error: could not read file '%s': %s\n", filepath, strerror(err));
        exit(1);
    }

    String_View source = sv_from_parts(file_data.items, file_data.count);
    Canal_Check check = {0};
    canal_collect_directives(&arena, &check, source);

    Nob_Cmd cmd = {0};
    canal_check(&arena, &cmd, &check, filepath);

    nob_cmd_free(cmd);
    arena_free(&arena);
    return 0;
}
