#include "./str.h"

void str_ensure_capacity(Arena *arena, String *str, size_t cap) {
    if (str->capacity >= cap) {
        return;
    }
    str->items = arena_realloc(arena, str->items, str->capacity, cap);
    str->capacity = cap;
}

bool str_eq(String *a, String *b) {
    if (a->count != b->count) {
        return false;
    }
    return memcmp(a->items, b->items, a->count) == 0;
}

bool str_eq_cstr(String *a, const char *b) {
    size_t b_count = strlen(b);
    if (a->count != b_count) {
        return false;
    }
    return memcmp(a->items, b, a->count) == 0;
}

void str_append_vfmt(Arena *arena, String *str, const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int str_size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    char temp[(str_size + 1) * sizeof(char)];
    vsnprintf(temp, str_size + 1, fmt, args);
    arena_da_append_many(arena, str, temp, str_size);
}

void str_append_fmt(Arena *arena, String *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_append_vfmt(arena, str, fmt, args);
    va_end(args);
}
