#ifndef _WIN32
#    define _POSIX_C_SOURCE 200809L
#    include <unistd.h>
#else
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "./str.h"

#define ARENA_IMPLEMENTATION
#include "./arena.h"

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "./nob.h"

typedef int Errno;

// NOTE(nic): your string will be overwritten
Errno canal_read_entire_file(Arena *arena, String *str, const char *filepath) {
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
    CANAL_ACTION_STAR,
    CANAL_ACTION_PLUS,
    CANAL_ACTION_BANG,
    CANAL_ACTION_RUN,
    CANAL_ACTION_COUNT,
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
    String_View prefix = sv_from_cstr("//");
    while (source.count > 0) {
        String_View line = sv_chop_by_delim(&source, '\n');
        if (sv_starts_with(line, prefix)) {
            sv_chop_left(&line, prefix.count);
            line = sv_trim(line);

            String_View action = sv_chop_by_predicate(&line, not_isspace);
            String_View arguments = line;

            Canal_Directive directive = {0};
            directive.arguments = arguments;

            static_assert(CANAL_ACTION_COUNT == 4, "Number of actions change, update code here!");
            if (sv_eq(action, sv_from_cstr("*"))) {
                directive.action = CANAL_ACTION_STAR;
            } else if (sv_eq(action, sv_from_cstr("+"))) {
                directive.action = CANAL_ACTION_PLUS;
            } else if (sv_eq(action, sv_from_cstr("!"))) {
                directive.action = CANAL_ACTION_BANG;
            } else if (sv_eq(action, sv_from_cstr("R"))) {
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

typedef struct {
    bool err;
    String error_message;
    String final_command;
} Canal_Result;

typedef struct {
    Canal_Result *items;
    size_t count;
    size_t capacity;
} Canal_Results;

bool canal_run_command(Arena *arena, Nob_Cmd *cmd, String *file_data, Canal_Result *check_result) {
    bool result = true;

    // TODO(nic): maybe find a way of creating temp file without fisically creating it
    const char *temp_out_filepath = "temp.out";
    const char *temp_err_filepath = "temp.err";

    Nob_Fd fdout = nob_fd_open_for_write(temp_out_filepath);
    assert(fdout != NOB_INVALID_FD);

    Nob_Fd fderr = nob_fd_open_for_write(temp_err_filepath);
    assert(fderr != NOB_INVALID_FD);

    Nob_Cmd_Redirect cmd_redirect = {
        .fdout = &fdout,
        .fderr = &fderr,
    };

    if (!nob_cmd_run_sync_redirect_and_reset(cmd, cmd_redirect)) {
        check_result->err = true;
        canal_read_entire_file(arena, &check_result->error_message, temp_err_filepath);
        if (check_result->error_message.count <= 0) {
            str_append_fmt(arena, &check_result->error_message, "<command failed with no message>\n");
        }
        return_defer(false);
    }
    canal_read_entire_file(arena, file_data, temp_out_filepath);

defer:
    nob_delete_file(temp_out_filepath);
    nob_delete_file(temp_err_filepath);
    return result;
}

typedef struct {
    String_View last_line;
    String_View content;
    size_t line;
} Source;

String_View canal_source_next_line(Source *source) {
    source->content = sv_trim_left(source->content);
    if (source->content.count <= 0) {
        return (String_View) {0};
    }
    String_View line = sv_chop_by_delim(&source->content, '\n');
    source->line += 1;
    source->last_line = line;
    return line;
}

typedef bool (*Canal_Action_Func)(Arena *arena, Source *source, String_View arguments, Canal_Result *result);

bool canal_lines_match_ignore_whitespace(String_View a, String_View b) {
    while (a.count > 0 && b.count > 0) {
        a = sv_trim_left(a);
        b = sv_trim_left(b);
        String_View a_word = sv_chop_by_predicate(&a, not_isspace);
        String_View b_word = sv_chop_by_predicate(&b, not_isspace);
        if (!sv_eq(a_word, b_word)) {
            return false;
        }
    }
    return true;
}

bool canal_handle_action_star(Arena *arena, Source *source, String_View arguments, Canal_Result *result) {
    while (true) {
        if (source->content.count <= 0) {
            result->err = true;
            str_append_fmt(arena, &result->error_message, "%zu: Reached end of input, expected '"SV_Fmt"'\n", source->line, SV_Arg(arguments));
            return false;
        }

        String_View line = canal_source_next_line(source);
        if (canal_lines_match_ignore_whitespace(line, arguments)) {
            return true;
        }
    }
}

bool canal_handle_action_plus(Arena *arena, Source *source, String_View arguments, Canal_Result *result) {
    if (source->content.count <= 0) {
        result->err = true;
        str_append_fmt(arena, &result->error_message, "Unexpected end of input\n");
        return false;
    }
    String_View line = canal_source_next_line(source);
    if (!canal_lines_match_ignore_whitespace(line, arguments)) {
        result->err = true;
        str_append_fmt(arena, &result->error_message, "%zu: Found '"SV_Fmt"', expected '"SV_Fmt"'\n", source->line, SV_Arg(line), SV_Arg(arguments));
        return false;
    }
    return true;
}

bool canal_handle_action_bang(Arena *arena, Source *source, String_View arguments, Canal_Result *result) {
    String_View line = source->last_line;
    if (canal_lines_match_ignore_whitespace(line, arguments)) {
        result->err = true;
        str_append_fmt(arena, &result->error_message, "%zu: Found unexpected '"SV_Fmt"'\n", source->line, SV_Arg(line), SV_Arg(arguments));
        return false;
    }
    return true;
}

static_assert(CANAL_ACTION_COUNT == 4, "Number of actions change, update code here!");
Canal_Action_Func canal_action_funcs[] = {
    [CANAL_ACTION_STAR] = canal_handle_action_star,
    [CANAL_ACTION_PLUS] = canal_handle_action_plus,
    [CANAL_ACTION_BANG] = canal_handle_action_bang,
    [CANAL_ACTION_RUN] = NULL,
};

void canal_match(Arena *arena, Source source, Canal_Directives *directives, Canal_Result *result) {
    for (size_t i = 0; i < directives->count; ++i) {
        Canal_Directive *directive = &directives->items[i];
        assert(directive->action != CANAL_ACTION_RUN);

        Canal_Action_Func action_func = canal_action_funcs[directive->action];
        if (!action_func(arena, &source, directive->arguments, result)) {
            return;
        }
    }
}

Canal_Results canal_check(Arena *arena, Nob_Cmd *cmd, Canal_Check *check, const char *filepath) {
    Canal_Results results = {0};

    for (size_t i = 0; i < check->r_directives.count; ++i) {
        Canal_Directive *r_directive = &check->r_directives.items[i];
        assert(r_directive->action == CANAL_ACTION_RUN);

        arena_da_append(arena, &results, (Canal_Result) {0});
        Canal_Result *result = &results.items[results.count - 1];

        String_View args = r_directive->arguments;
        while (args.count > 0) {
            args = sv_trim_left(args);
            String_View arg_fmt = sv_chop_by_predicate(&args, not_isspace);
            if (arg_fmt.count == 0) break;

            // TODO(nic): implement proper formatting
            String arg = {0};
            if (sv_eq(arg_fmt, sv_from_cstr("%s"))) {
                str_append_cstr(arena, &arg, filepath);
                str_append_null(arena, &arg);
            } else {
                str_append_sv(arena, &arg, arg_fmt);
                str_append_null(arena, &arg);
            }
            nob_cmd_append(cmd, arg.items);
        }
        // NOTE(nic): Nob_String_Builder and String are the same thing
        nob_cmd_render(*cmd, (Nob_String_Builder*)&result->final_command);

        String file_data = {0};
        if (!canal_run_command(arena, cmd, &file_data, result)) {
            continue;
        }

        Source source = {0};
        source.content = sv_from_parts(file_data.items, file_data.count);
        canal_match(arena, source, &check->directives, result);
    }

    return results;
}

int main(int argc, const char **argv) {
    nob_minimal_log_level = NOB_NO_LOGS;

    if (argc <= 1) {
        fprintf(stderr, "Error: expected filepath\n");
        exit(1);
    }
    const char *filepath = argv[1];

    Arena arena = {0};
    String file_data = {0};

    Errno err = canal_read_entire_file(&arena, &file_data, filepath);
    if (err) {
        fprintf(stderr, "Error: could not read file '%s': %s\n", filepath, strerror(err));
        exit(1);
    }

    String_View source = sv_from_parts(file_data.items, file_data.count);
    Canal_Check check = {0};
    canal_collect_directives(&arena, &check, source);

    Nob_Cmd cmd = {0};
    Canal_Results results = canal_check(&arena, &cmd, &check, filepath);
    nob_cmd_free(cmd);

    for (size_t i = 0; i < results.count; ++i) {
        Canal_Result *result = &results.items[i];
        printf("[Check %zu] ("STR_FMT"):\n", i + 1, STR_ARG(&result->final_command));

        if (result->err) {
            fprintf(stderr, STR_FMT, STR_ARG(&result->error_message));
        } else {
            printf("Passed!\n");
        }

        if (i < results.count - 1) {
            printf("\n");
        }
    }

    arena_free(&arena);
    return 0;
}
