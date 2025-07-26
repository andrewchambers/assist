#include "string.h"
#include "util.h"  // for die()
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern gc_state gc;

char *gc_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = gc_malloc(&gc, len);
    if (!dup) {
        die("Memory allocation failed");
    }
    memcpy(dup, s, len);
    return dup;
}

char *gc_asprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return NULL;
    }
    
    char *str = gc_malloc(&gc, len + 1);
    if (!str) {
        va_end(args);
        die("Memory allocation failed");
    }
    
    vsnprintf(str, len + 1, fmt, args);
    va_end(args);
    
    return str;
}

void string_builder_init(string_builder_t *sb, gc_state *gc, size_t initial_capacity) {
    sb->gc = gc;
    sb->data = gc_malloc(gc, initial_capacity);
    sb->data[0] = '\0';
    sb->size = 0;
    sb->capacity = initial_capacity;
}

void string_builder_append(string_builder_t *sb, const char *data, size_t len) {
    size_t new_size = sb->size + len;
    
    if (new_size + 1 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < new_size + 1) {
            new_capacity *= 2;
        }
        sb->data = gc_realloc(sb->gc, sb->data, new_capacity);
        sb->capacity = new_capacity;
    }
    
    memcpy(sb->data + sb->size, data, len);
    sb->size = new_size;
    sb->data[sb->size] = '\0';
}

void string_builder_append_str(string_builder_t *sb, const char *str) {
    string_builder_append(sb, str, strlen(str));
}

void string_builder_append_fmt(string_builder_t *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return;
    }
    
    size_t new_size = sb->size + len;
    
    if (new_size + 1 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < new_size + 1) {
            new_capacity *= 2;
        }
        sb->data = gc_realloc(sb->gc, sb->data, new_capacity);
        sb->capacity = new_capacity;
    }
    
    vsnprintf(sb->data + sb->size, len + 1, fmt, args);
    sb->size = new_size;
    
    va_end(args);
}

char *string_builder_finalize(string_builder_t *sb) {
    return sb->data;
}