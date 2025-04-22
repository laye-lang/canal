#ifndef STR_H_
#define STR_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>

#include "./arena.h"

#define STR_FMT "%.*s"
#define STR_ARG(str) (int) (str)->count, (str)->items

#define str_append_char(a, str, ch) arena_da_append((a), (str), ch)
#define str_append_sv(a, str, sv) arena_da_append_many((a), (str), (sv).data, (sv).count)
#define str_append_cstr(a, str, cstr) arena_da_append_many((a), (str), (cstr), strlen(cstr))
#define str_append_null(a, str) arena_da_append((a), (str), '\0')

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String;

void str_ensure_capacity(Arena *arena, String *str, size_t cap);
bool str_eq(String *a, String *b);
bool str_eq_cstr(String *a, const char *b);
void str_append_vfmt(Arena *arena, String *str, const char *fmt, va_list args);
void str_append_fmt(Arena *arena, String *str, const char *fmt, ...);

#endif // STR_H_
