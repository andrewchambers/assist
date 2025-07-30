#define _GNU_SOURCE
#include "util.h"
#include "gc.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <dirent.h>
#include <glob.h>
#include <limits.h>

extern gc_state gc;

void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}


int file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return 1;  // File exists
    }
    if (errno == ENOENT) {
        return 0;  // File doesn't exist
    }
    return -1;  // Error (permission denied, etc.)
}

char *file_to_string(const char *path, char **error) {
    // Open and read file using stat for size
    struct stat st;
    if (stat(path, &st) != 0) {
        if (error) {
            *error = gc_asprintf(&gc, "Failed to stat file '%s': %s", path, strerror(errno));
        }
        return NULL;
    }
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (error) {
            *error = gc_asprintf(&gc, "Failed to open file '%s': %s", path, strerror(errno));
        }
        return NULL;
    }
    
    // Allocate buffer for entire file
    size_t size = st.st_size;
    char *buffer = gc_malloc(&gc, size + 1);
    
    // Read entire file in one go
    size_t bytes_read = fread(buffer, 1, size, fp);
    fclose(fp);
    
    if (bytes_read != size) {
        if (error) {
            *error = gc_asprintf(&gc, "Failed to read file '%s': expected %zu bytes, got %zu", path, size, bytes_read);
        }
        return NULL;
    }
    
    buffer[size] = '\0';
    return buffer;
}

int is_binary_file(const char *path, char **error) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (error) {
            *error = gc_asprintf(&gc, "Failed to open file '%s': %s", path, strerror(errno));
        }
        return -1;
    }
    
    // Read first 8KB to check for binary content
    unsigned char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), file);
    if (ferror(file)) {
        if (error) {
            *error = gc_asprintf(&gc, "Failed to read file '%s': %s", path, strerror(errno));
        }
        fclose(file);
        return -1;
    }
    fclose(file);
    
    // Check for null bytes
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0) {
            return 1;  // Binary file
        }
    }
    
    // Simple heuristic: if more than 10% of bytes are non-printable, consider binary
    int non_printable = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] < 32 && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
            non_printable++;
        }
    }
    
    return ((non_printable * 10) > (int)n) ? 1 : 0;
}


int remove_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return -1;
    }
    
    struct dirent *entry;
    int ret = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char *full_path = gc_asprintf(&gc, "%s/%s", path, entry->d_name);
        struct stat st;
        
        if (lstat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively remove subdirectory
                if (remove_directory(full_path) != 0) {
                    ret = -1;
                }
            } else {
                // Remove file or symlink
                if (unlink(full_path) != 0) {
                    ret = -1;
                }
            }
        }
    }
    
    closedir(dir);
    
    // Remove the directory itself
    if (rmdir(path) != 0) {
        ret = -1;
    }
    
    return ret;
}

int expand_globs(const char *words, expand_globs_t *result) {
    if (!words || !result) {
        return -1;
    }
    
    // Initialize result
    result->we_wordc = 0;
    result->we_wordv = NULL;
    
    // Work directly with the input string using indices to avoid GC rooting issues
    size_t len = strlen(words);
    char *buffer = gc_malloc(&gc, len + 1);
    strcpy(buffer, words);
    size_t pos = 0;
    
    // Dynamic array for words
    size_t capacity = 16;
    char **wordv = gc_malloc(&gc, capacity * sizeof(char*));
    size_t wordc = 0;
    
    while (pos < len && buffer[pos]) {
        // Skip leading whitespace
        while (pos < len && buffer[pos] && (buffer[pos] == ' ' || buffer[pos] == '\t' || buffer[pos] == '\n')) {
            pos++;
        }
        
        if (pos >= len || !buffer[pos]) break;
        
        size_t word_start = pos;
        char quote_char = 0;
        
        // Handle quoted strings
        if (buffer[pos] == '"' || buffer[pos] == '\'') {
            quote_char = buffer[pos];
            pos++;
            word_start = pos;
            
            // Find matching quote
            while (pos < len && buffer[pos] && buffer[pos] != quote_char) {
                if (buffer[pos] == '\\' && pos + 1 < len && buffer[pos + 1]) {
                    // Skip escaped character
                    pos += 2;
                } else {
                    pos++;
                }
            }
            
            if (pos < len && buffer[pos] == quote_char) {
                buffer[pos] = '\0';
                pos++;
            }
            
            // Add word (no glob expansion for quoted strings)
            if (wordc >= capacity) {
                capacity *= 2;
                wordv = gc_realloc(&gc, wordv, capacity * sizeof(char*));
            }
            wordv[wordc++] = gc_strdup(&gc, &buffer[word_start]);
        } else {
            // Find end of unquoted word
            while (pos < len && buffer[pos] && buffer[pos] != ' ' && buffer[pos] != '\t' && buffer[pos] != '\n') {
                pos++;
            }
            
            // Temporarily null-terminate the word
            char saved = (pos < len) ? buffer[pos] : '\0';
            if (pos < len) {
                buffer[pos] = '\0';
            }
            
            // Always use glob to handle tilde expansion and patterns
            // GLOB_NOCHECK ensures non-matching patterns are returned as-is
            glob_t glob_result;
            int glob_ret = glob(&buffer[word_start], GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
            
            if (glob_ret == 0) {
                // Add all expanded paths
                for (size_t i = 0; i < glob_result.gl_pathc; i++) {
                    if (wordc >= capacity) {
                        capacity *= 2;
                        wordv = gc_realloc(&gc, wordv, capacity * sizeof(char*));
                    }
                    wordv[wordc++] = gc_strdup(&gc, glob_result.gl_pathv[i]);
                }
                globfree(&glob_result);
            } else {
                // On error, just add the word as-is
                if (wordc >= capacity) {
                    capacity *= 2;
                    wordv = gc_realloc(&gc, wordv, capacity * sizeof(char*));
                }
                wordv[wordc++] = gc_strdup(&gc, &buffer[word_start]);
            }
            
            // Restore the character
            if (pos < len) {
                buffer[pos] = saved;
            }
        }
    }
    
    // Add terminating NULL
    if (wordc >= capacity) {
        capacity++;
        wordv = gc_realloc(&gc, wordv, capacity * sizeof(char*));
    }
    wordv[wordc] = NULL;
    
    result->we_wordc = wordc;
    result->we_wordv = wordv;
    
    return 0;
}