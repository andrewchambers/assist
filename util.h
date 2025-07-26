#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include "gc.h"

/**
 * Print error message to stderr and exit with status 1.
 */
void die(const char *fmt, ...);

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
 * Returns 1 if binary, 0 if text, -1 on error.
 * If error is not NULL and an error occurs, *error will be set to an error message.
 * Binary detection: contains null bytes or has more than 10% non-printable characters.
 */
int is_binary_file(const char *path, char **error);

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
extern char g_executable_path[PATH_MAX * 2];

/**
 * Initialize the executable path with argv[0] from main.
 * This should be called early in main() to store the executable path.
 * The function will attempt to resolve argv[0] to an absolute path,
 * combining it with the current working directory if necessary.
 */
void self_exec_path_init(const char *argv0);

#endif /* UTIL_H */