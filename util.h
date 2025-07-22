#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

/**
 * Print error message to stderr and exit with status 1.
 */
void die(const char *fmt, ...);

/**
 * Allocate memory, die on failure.
 */
void *gc_malloc(size_t size);

/**
 * Reallocate memory, die on failure.
 */
void *gc_realloc(void *ptr, size_t size);

/**
 * Free memory allocated by gc_malloc.
 */
void gc_free(void *ptr);

/**
 * Duplicate string, die on failure.
 */
char *gc_strdup(const char *s);

/**
 * Allocate and format string (like asprintf).
 */
char *gc_asprintf(const char *fmt, ...);

/**
 * Check if file exists.
 * Returns: 1 if file exists, 0 if not exists, -1 on error (check errno)
 */
int file_exists(const char *path);

/**
 * Read entire file into a string.
 * Returns allocated string on success, NULL on failure.
 * On failure, *error is set to allocated error message.
 */
char *file_to_string(const char *path, char **error);

/**
 * Check if a file appears to be binary.
 * Returns true if file contains null bytes or has more than 10% non-printable characters.
 */
bool is_binary_file(const char *path);

/**
 * String builder for efficient string concatenation
 */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} gc_string_builder_t;

void gc_string_builder_init(gc_string_builder_t *sb, size_t initial_capacity);
void gc_string_builder_append(gc_string_builder_t *sb, const char *data, size_t len);
void gc_string_builder_append_str(gc_string_builder_t *sb, const char *str);
void gc_string_builder_append_fmt(gc_string_builder_t *sb, const char *fmt, ...);
char *gc_string_builder_finalize(gc_string_builder_t *sb);

/**
 * Execute a command and capture its output.
 * Returns exit code on success, -1 on failure.
 * If out_output is provided, captures stdout and stderr.
 * If forward_output is true, also forwards output to parent terminal in real-time.
 */
int exec_command(const char *command, char **out_output, int forward_output);

/**
 * Start the spinner animation on stderr with an optional message.
 * This function is idempotent - calling it multiple times has no effect
 * if the spinner is already running.
 * The spinner will not start if stderr is not a TTY.
 * @param message Optional message to display after the spinner (can be NULL)
 */
void start_spinner(const char *message);

/**
 * Stop the spinner animation.
 * This function is idempotent - calling it multiple times has no effect
 * if the spinner is already stopped.
 */
void stop_spinner(void);

/**
 * Recursively remove a directory and its contents.
 * Returns 0 on success, -1 on error.
 */
int remove_directory(const char *path);

/**
 * Glob expansion result structure.
 * Uses our garbage collector for memory management.
 */
typedef struct {
    size_t we_wordc;  // Number of words
    char **we_wordv;  // Array of words (NULL-terminated)
} expand_globs_t;

/**
 * Expand glob patterns and split words from input string.
 * Supports:
 * - Space-separated words
 * - Single and double quoted strings (quotes are removed)
 * - Glob patterns (*, ?, [...]) - patterns that don't match are returned as-is
 * - Tilde expansion (~)
 * 
 * All unquoted words are processed through glob() with GLOB_NOCHECK flag,
 * ensuring non-matching patterns are returned unchanged.
 * 
 * Returns 0 on success, -1 on error.
 * No need to free result - memory is managed by garbage collector.
 */
int expand_globs(const char *words, expand_globs_t *result);

/**
 * Global buffer containing the executable path.
 * Initialized by self_exec_path_init().
 */
extern char g_executable_path[PATH_MAX];

/**
 * Initialize the executable path with argv[0] from main.
 * This should be called early in main() to store the executable path.
 * The function will attempt to resolve argv[0] to an absolute path,
 * combining it with the current working directory if necessary.
 */
void self_exec_path_init(const char *argv0);

#endif /* UTIL_H */