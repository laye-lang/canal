#include "./utils.h"

String str_with_cap(Arena *arena, size_t cap) {
    String str = {0};
    str.items = arena_alloc(arena, cap * sizeof(char));
    str.capacity = cap;
    return str;
}

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

int64_t sv_to_int64(String_View sv) {
    char int64_string[sv.count + 1];
    memcpy(int64_string, sv.data, sv.count);
    int64_string[sv.count] = '\0';
    return atoll(int64_string);
}

double sv_to_decimal(String_View sv) {
    char decimal_string[sv.count + 1];
    memcpy(decimal_string, sv.data, sv.count);
    decimal_string[sv.count] = '\0';
    return strtod(decimal_string, NULL);
}

String_View sv_from_parts(const char *data, size_t count) {
    return (String_View) {
        .data = data,
        .count = count,
    };
}

bool sv_starts_with(String_View sv, const char *prefix) {
    size_t prefix_size = strlen(prefix);
    if (prefix_size > sv.count) {
        return false;
    }
    return memcmp(sv.data, prefix, prefix_size) == 0;
}

bool sv_find(String_View sv, char ch, size_t *index) {
    for (size_t i = 0; i < sv.count; ++i) {
        if (sv.data[i] == ch) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool sv_find_rev(String_View sv, char ch, size_t *index) {
    for (int i = sv.count; i >= 0; --i) {
        if (sv.data[i] == ch) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool sv_eq(String_View a, String_View b) {
    if (a.count != b.count) {
        return false;
    }
    return memcmp(a.data, b.data, a.count) == 0;
}

String_View sv_chop(String_View *sv, size_t count) {
    assert(sv->count >= count && "String_View too short");
    String_View prefix = sv_from_parts(sv->data, count);
    sv->data += count;
    sv->count -= count;
    return prefix;
}

String_View sv_chop_predicate(String_View *sv, int (*predicate)(int)) {
    size_t i = 0;
    while (i < sv->count && predicate(sv->data[i])) {
        i += 1;
    }

    String_View result = sv_from_parts(sv->data, i);
    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }
    return result;
}

String_View sv_chop_until(String_View *sv, char ch) {
    size_t i = 0;
    while (i < sv->count && sv->data[i] != ch) {
        i += 1;
    }

    String_View result = sv_from_parts(sv->data, i);
    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }
    return result;
}

String_View sv_trim_left(String_View sv) {
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i])) {
        i += 1;
    }
    return sv_from_parts(sv.data + i, sv.count - i);
}

String_View sv_trim_right(String_View sv) {
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
        i += 1;
    }
    return sv_from_parts(sv.data, sv.count - i);
}

String_View sv_trim(String_View sv) {
    return sv_trim_right(sv_trim_left(sv));
}
