#define _GNU_SOURCE
#include "util.h"
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
#include <tgc.h>
#include <dirent.h>

tgc_t gc;

void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void *gc_malloc(size_t size) {
    void *ptr = tgc_alloc(&gc, size);
    if (!ptr) {
        die("Memory allocation failed");
    }
    return ptr;
}

void *gc_realloc(void *ptr, size_t size) {
    void *new_ptr = tgc_realloc(&gc, ptr, size);
    if (!new_ptr && size > 0) {
        die("Memory reallocation failed");
    }
    return new_ptr;
}

void gc_free(void *ptr) {
    if (ptr) {
        tgc_free(&gc, ptr);
    }
}

char *gc_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = gc_malloc(len);
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
    
    char *str = gc_malloc(len + 1);
    
    vsnprintf(str, len + 1, fmt, args);
    va_end(args);
    
    return str;
}

void gc_string_builder_init(gc_string_builder_t *sb, size_t initial_capacity) {
    sb->data = gc_malloc(initial_capacity);
    sb->data[0] = '\0';
    sb->size = 0;
    sb->capacity = initial_capacity;
}

void gc_string_builder_append(gc_string_builder_t *sb, const char *data, size_t len) {
    size_t new_size = sb->size + len;
    
    if (new_size + 1 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < new_size + 1) {
            new_capacity *= 2;
        }
        sb->data = gc_realloc(sb->data, new_capacity);
        sb->capacity = new_capacity;
    }
    
    memcpy(sb->data + sb->size, data, len);
    sb->size = new_size;
    sb->data[sb->size] = '\0';
}

void gc_string_builder_append_str(gc_string_builder_t *sb, const char *str) {
    gc_string_builder_append(sb, str, strlen(str));
}

void gc_string_builder_append_fmt(gc_string_builder_t *sb, const char *fmt, ...) {
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
        sb->data = gc_realloc(sb->data, new_capacity);
        sb->capacity = new_capacity;
    }
    
    vsnprintf(sb->data + sb->size, len + 1, fmt, args);
    sb->size = new_size;
    
    va_end(args);
}

char *gc_string_builder_finalize(gc_string_builder_t *sb) {
    return sb->data;
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
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (error) {
            *error = gc_asprintf("Failed to open file '%s': %s", path, strerror(errno));
        }
        return NULL;
    }
    
    // Get file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        if (error) {
            *error = gc_asprintf("Failed to seek in file '%s': %s", path, strerror(errno));
        }
        fclose(fp);
        return NULL;
    }
    
    long size = ftell(fp);
    if (size < 0) {
        if (error) {
            *error = gc_asprintf("Failed to get file size for '%s': %s", path, strerror(errno));
        }
        fclose(fp);
        return NULL;
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        if (error) {
            *error = gc_asprintf("Failed to seek to beginning of file '%s': %s", path, strerror(errno));
        }
        fclose(fp);
        return NULL;
    }
    
    // Allocate buffer
    char *buffer = gc_malloc(size + 1);
    
    // Read file
    size_t bytes_read = fread(buffer, 1, size, fp);
    if (bytes_read != (size_t)size) {
        if (error) {
            *error = gc_asprintf("Failed to read file '%s': expected %ld bytes, got %zu", path, size, bytes_read);
        }
        fclose(fp);
        return NULL;
    }
    
    buffer[size] = '\0';
    fclose(fp);
    
    return buffer;
}

bool is_binary_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    
    // Read first 8KB to check for binary content
    unsigned char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), file);
    fclose(file);
    
    // Check for null bytes
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0) {
            return true;
        }
    }
    
    // Simple heuristic: if more than 10% of bytes are non-printable, consider binary
    int non_printable = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] < 32 && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
            non_printable++;
        }
    }
    
    return (non_printable * 10) > (int)n;
}

int exec_command(const char *command, char **out_output, int forward_output) {
    if (!command) {
        return 0;  // No command, consider it a success
    }
    
    if (out_output) {
        *out_output = NULL;
    }
    
    // Get shell from environment or use default
    const char *shell = getenv("MINICODER_SHELL");
    if (!shell) {
        shell = "/bin/sh";
    }
    
    // Set up shell arguments
    char *shell_args[4];
    shell_args[0] = (char *)shell;
    shell_args[1] = "-c";
    shell_args[2] = (char *)command;
    shell_args[3] = NULL;
    
    // Create pipe for capturing output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        if (out_output) {
            *out_output = gc_asprintf("Failed to create pipe: %s", strerror(errno));
        }
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        if (out_output) {
            *out_output = gc_asprintf("Failed to fork: %s", strerror(errno));
        }
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close read end
        
        // Redirect stdout and stderr to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            _exit(127);
        }
        close(pipefd[1]);  // Close original write end
        
        // Execute command
        execvp(shell, shell_args);
        // If we get here, execvp failed
        _exit(127);
    }
    
    // Parent process
    close(pipefd[1]);  // Close write end
    
    // Read output from pipe
    gc_string_builder_t sb;
    gc_string_builder_init(&sb, 4096);
    
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        gc_string_builder_append_str(&sb, buffer);
        
        // Forward output to parent terminal if requested
        if (forward_output) {
            printf("%s", buffer);
            fflush(stdout);
        }
    }
    close(pipefd[0]);
    
    // Wait for child process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        if (out_output) {
            *out_output = gc_asprintf("Failed to wait for child process: %s", strerror(errno));
        }
        return -1;
    }
    
    if (out_output) {
        *out_output = gc_string_builder_finalize(&sb);
    }
    
    // Check exit status
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    
    return -1;  // Process terminated abnormally
}

// Spinner implementation
static struct {
    pthread_t thread;
    volatile int running;
    int started;
    pthread_mutex_t mutex;
    char *message;
} spinner_state = {
    .running = 0,
    .started = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .message = NULL
};

// Spinner characters
static const char spinner_chars[] = "|/-\\";
static const int spinner_delay_ms = 100;

// Get the number of characters in the spinner
static int get_spinner_count(void) {
    return strlen(spinner_chars);
}

// Spinner thread function
static void *spinner_thread(void *arg) {
    char *message = (char *)arg;
    
    int spinner_count = get_spinner_count();
    int index = 0;
    
    // Hide cursor
    fprintf(stderr, "\033[?25l");
    fflush(stderr);
    
    while (spinner_state.running) {
        // Move cursor to beginning of line and print spinner with optional message
        if (message && *message) {
            fprintf(stderr, "\r%c %s", spinner_chars[index], message);
        } else {
            fprintf(stderr, "\r%c ", spinner_chars[index]);
        }
        fflush(stderr);
        
        // Move to next spinner character
        index = (index + 1) % spinner_count;
        
        // Sleep for the delay
        usleep(spinner_delay_ms * 1000);
    }
    
    // Clear spinner and show cursor
    // Calculate how much to clear based on message length
    int clear_len = 4; // spinner + space
    if (message && *message) {
        clear_len += strlen(message);
    }
    fprintf(stderr, "\r%*s\r", clear_len, "");
    fprintf(stderr, "\033[?25h");
    fflush(stderr);
    
    // Free the message copy
    if (message) {
        free(message);
    }
    
    return NULL;
}

void start_spinner(const char *message) {
    pthread_mutex_lock(&spinner_state.mutex);
    
    // Check if already started (idempotent)
    if (spinner_state.started) {
        pthread_mutex_unlock(&spinner_state.mutex);
        return;
    }
    
    // Check if stderr is a TTY
    if (!isatty(STDERR_FILENO)) {
        pthread_mutex_unlock(&spinner_state.mutex);
        return;
    }
    
    // Store the message
    // Note: We use regular malloc/free here instead of gc_* functions
    // because the spinner runs in a separate thread and the gc is not thread-safe
    if (spinner_state.message) {
        free(spinner_state.message);
        spinner_state.message = NULL;
    }
    if (message) {
        spinner_state.message = strdup(message);
        if (!spinner_state.message) {
            die("Memory allocation failed for spinner message");
        }
    }
    
    // Start the spinner
    spinner_state.running = 1;
    spinner_state.started = 1;
    
    // Create a malloc'd copy of the message for the thread
    char *message_copy = NULL;
    if (spinner_state.message) {
        message_copy = strdup(spinner_state.message);
        if (!message_copy) {
            die("Memory allocation failed in start_spinner");
        }
    }
    
    // Create spinner thread
    if (pthread_create(&spinner_state.thread, NULL, spinner_thread, message_copy) != 0) {
        // Failed to create thread
        spinner_state.running = 0;
        spinner_state.started = 0;
        if (spinner_state.message) {
            free(spinner_state.message);
            spinner_state.message = NULL;
        }
        if (message_copy) {
            free(message_copy);
        }
    }
    
    pthread_mutex_unlock(&spinner_state.mutex);
}

void stop_spinner(void) {
    pthread_mutex_lock(&spinner_state.mutex);
    
    // Check if not started (idempotent)
    if (!spinner_state.started) {
        pthread_mutex_unlock(&spinner_state.mutex);
        return;
    }
    
    // Stop the spinner
    spinner_state.running = 0;
    spinner_state.started = 0;
    
    // Clean up message
    if (spinner_state.message) {
        free(spinner_state.message); // Use free, no gc here.
        spinner_state.message = NULL;
    }
    
    pthread_mutex_unlock(&spinner_state.mutex);
    
    // Wait for thread to finish
    pthread_join(spinner_state.thread, NULL);
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
        
        char *full_path = gc_asprintf("%s/%s", path, entry->d_name);
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