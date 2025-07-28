#define _GNU_SOURCE
#include "agent_commands.h"
#include "util.h"
#include "gc.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <cJSON.h>

extern gc_state gc;

// Global buffer to store the executable path
char g_executable_path[PATH_MAX * 2] = {0};

void self_exec_path_init(const char *argv0) {
    if (!argv0) {
        die("argv0 is required!\n");
    }
    
    // First try /proc/self/exe (Linux)
    char proc_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", proc_path, sizeof(proc_path) - 1);
    if (len > 0) {
        proc_path[len] = '\0';
        strncpy(g_executable_path, proc_path, PATH_MAX * 2 - 1);
        g_executable_path[PATH_MAX * 2 - 1] = '\0';
        return;
    }
   
    // Otherwise, resolve argv[0] to absolute path
    if (argv0[0] == '/') {
        // Already absolute
        strncpy(g_executable_path, argv0, PATH_MAX * 2 - 1);
        g_executable_path[PATH_MAX * 2 - 1] = '\0';
    } else if (strchr(argv0, '/')) {
        // Relative path with directory component
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            size_t cwd_len = strlen(cwd);
            size_t argv0_len = strlen(argv0);
            if (cwd_len + argv0_len + 1 < PATH_MAX * 2) {
                snprintf(g_executable_path, PATH_MAX * 2, "%s/%s", cwd, argv0);
            } else {
                // Path would be too long, just use argv0 as-is
                strncpy(g_executable_path, argv0, PATH_MAX - 1);
                g_executable_path[PATH_MAX - 1] = '\0';
            }
        }
    } else {
        // Just a command name, search PATH
        const char *path_env = getenv("PATH");
        if (!path_env) {
            // Fallback to default PATH if not set
            path_env = "/usr/local/bin:/usr/bin:/bin";
        }
        
        // Make a copy of PATH since strtok modifies the string
        char *path_copy = gc_strdup(path_env);
        
        char *dir = strtok(path_copy, ":");
        bool found = false;
        
        while (dir != NULL) {
            char candidate[PATH_MAX];
            snprintf(candidate, PATH_MAX, "%s/%s", dir, argv0);
            
            // Check if file exists and is executable
            if (access(candidate, X_OK) == 0) {
                strncpy(g_executable_path, candidate, PATH_MAX * 2 - 1);
                g_executable_path[PATH_MAX * 2 - 1] = '\0';
                found = true;
                break;
            }
            
            dir = strtok(NULL, ":");
        }
        
        if (!found) {
            die("Failed to find executable '%s' in PATH\n", argv0);
        }
    }
}

// Read message from stdin (for agent-done and agent-abort)
static char* read_message_from_stdin() {
    string_builder_t sb;
    string_builder_init(&sb, &gc, 1024);
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        string_builder_append_str(&sb, buffer);
    }
    
    // Trim trailing newline if present
    char *msg = string_builder_finalize(&sb);
    int len = strlen(msg);
    if (len > 0 && msg[len-1] == '\n') {
        msg[len-1] = '\0';
    }
    
    return msg;
}

// Handle agent commands (agent-files, agent-cd, etc.)
int agent_command_main(const char *cmd, int argc, char *argv[]) {
    const char *state_file = getenv("MINICODER_STATE_FILE");
    if (!state_file) {
        fprintf(stderr, "Error: MINICODER_STATE_FILE environment variable not set\n");
        return 1;
    }
    
    // Read current state
    char *error = NULL;
    char *content = file_to_string(state_file, &error);
    if (!content) {
        fprintf(stderr, "Error reading state file: %s\n", error ? error : "Unknown error");
        return 1;
    }
    
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        fprintf(stderr, "Error parsing state JSON\n");
        return 1;
    }
    
    // Handle different commands
    if (strcmp(cmd, "agent-files") == 0) {
        // Replace entire focused list
        cJSON *focused = cJSON_GetObjectItem(root, "focused_files");
        if (focused) {
            cJSON_DeleteItemFromObject(root, "focused_files");
        }
        
        // Create new array with the specified files
        focused = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "focused_files", focused);
        
        for (int i = 1; i < argc; i++) {
            // Get absolute path
            char *abs_path = realpath(argv[i], NULL);
            if (!abs_path) {
                abs_path = gc_strdup(argv[i]);
            } else {
                char *gc_abs = gc_strdup(abs_path);
                free(abs_path);
                abs_path = gc_abs;
            }
            
            cJSON_AddItemToArray(focused, cJSON_CreateString(abs_path));
            printf("Focused on: %s\n", abs_path);
        }
        
        if (argc == 1) {
            // No files specified, clear the focus
            printf("Cleared all focused files\n");
        }
    }
    else if (strcmp(cmd, "agent-cd") == 0) {
        // Change working directory
        if (argc < 2) {
            fprintf(stderr, "Usage: agent-cd PATH\n");
            return 1;
        }
        
        char *abs_path = realpath(argv[1], NULL);
        if (!abs_path) {
            fprintf(stderr, "Error: Invalid directory path: %s\n", argv[1]);
            return 1;
        }
        char *gc_abs = gc_strdup(abs_path);
        free(abs_path);
        
        cJSON_DeleteItemFromObject(root, "working_dir");
        cJSON_AddStringToObject(root, "working_dir", gc_abs);
        printf("Changed directory to: %s\n", gc_abs);
    }
    else if (strcmp(cmd, "agent-done") == 0) {
        // Mark as done
        cJSON_AddBoolToObject(root, "done", cJSON_True);
        
        // Read message from stdin
        char *message = read_message_from_stdin();
        if (message && strlen(message) > 0) {
            cJSON_AddStringToObject(root, "done_message", message);
        }
    }
    else if (strcmp(cmd, "agent-abort") == 0) {
        // Mark as aborted
        cJSON_AddBoolToObject(root, "aborted", cJSON_True);
        
        // Read message from stdin
        char *message = read_message_from_stdin();
        if (message && strlen(message) > 0) {
            cJSON_AddStringToObject(root, "abort_message", message);
        }
    }
    else {
        fprintf(stderr, "Unknown agent command: %s\n", cmd);
        return 1;
    }
    
    // Write updated state back
    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(state_file, "w");
    if (!f) {
        fprintf(stderr, "Error writing state file\n");
        return 1;
    }
    fprintf(f, "%s", json_str);
    fclose(f);
    
    return 0;
}