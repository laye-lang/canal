#ifndef UTILS_H_
#define UTILS_H_

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

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int) (sv).count, (sv).data

#define SV(cstr) ((String_View) { .data = (cstr), .count = strlen(cstr) })
#define SV_STATIC(cstr) { .data = (cstr), .count = sizeof(cstr) - 1 }

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
String str_with_cap(Arena *arena, size_t cap);
bool str_eq(String *a, String *b);
bool str_eq_cstr(String *a, const char *b);
void str_append_vfmt(Arena *arena, String *str, const char *fmt, va_list args);
void str_append_fmt(Arena *arena, String *str, const char *fmt, ...);

typedef struct {
    const char *data;
    size_t count;
} String_View;

int64_t sv_to_int64(String_View sv);
double sv_to_decimal(String_View sv);

String_View sv_from_parts(const char *data, size_t count);

bool sv_eq(String_View a, String_View b);
bool sv_starts_with(String_View sv, const char *prefix);
bool sv_find(String_View sv, char ch, size_t *index);
bool sv_find_rev(String_View sv, char ch, size_t *index);

String_View sv_chop(String_View *sv, size_t count);
String_View sv_chop_predicate(String_View *sv, int (*predicate)(int));
String_View sv_chop_until(String_View *sv, char ch);

String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
String_View sv_trim(String_View sv);

#endif // UTILS_H_
