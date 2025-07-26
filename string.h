#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include "gc.h"

/**
 * Duplicate string, die on failure.
 */
char *gc_strdup(const char *s);

/**
 * Allocate and format string (like asprintf).
 */
char *gc_asprintf(const char *fmt, ...);

/**
 * String builder for efficient string concatenation
 */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    gc_state *gc;  // Store gc pointer for realloc operations
} string_builder_t;

void string_builder_init(string_builder_t *sb, gc_state *gc, size_t initial_capacity);
void string_builder_append(string_builder_t *sb, const char *data, size_t len);
void string_builder_append_str(string_builder_t *sb, const char *str);
void string_builder_append_fmt(string_builder_t *sb, const char *fmt, ...);
char *string_builder_finalize(string_builder_t *sb);

#endif /* STRING_H */