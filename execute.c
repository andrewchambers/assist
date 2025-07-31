#include "agent.h"
#include "util.h"
#include "gc.h"
#include "string.h"
#include "execute.h"
#include "agent_commands.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <cJSON.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

extern gc_state gc;

// Escape single quotes in a string for safe inclusion in shell single-quoted strings
// Returns a new string with ' replaced by '\''
static char* shell_escape_single_quotes(const char *str) {
    if (!str) return NULL;
    
    // Count single quotes to determine output size
    int quote_count = 0;
    for (const char *p = str; *p; p++) {
        if (*p == '\'') quote_count++;
    }
    
    // If no quotes, return copy of original
    if (quote_count == 0) {
        return gc_strdup(&gc, str);
    }
    
    // Allocate space: original length + (3 extra chars per quote) + null terminator
    int new_len = strlen(str) + (quote_count * 3) + 1;
    char *result = gc_malloc(&gc, new_len);
    
    char *out = result;
    for (const char *p = str; *p; p++) {
        if (*p == '\'') {
            // Replace ' with '\''
            *out++ = '\'';
            *out++ = '\\';
            *out++ = '\'';
            *out++ = '\'';
        } else {
            *out++ = *p;
        }
    }
    *out = '\0';
    
    return result;
}


// Write the current state to a JSON file
static int write_state_json(const char *path, AgentCommandState *cmd_state) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }
    
    // Add working directory
    if (cmd_state->working_dir) {
        if (!cJSON_AddStringToObject(root, "working_dir", cmd_state->working_dir)) {
            cJSON_Delete(root);
            return -1;
        }
    }
    
    // Add focused files array
    cJSON *focused = cJSON_CreateArray();
    if (!focused) {
        cJSON_Delete(root);
        return -1;
    }
    
    for (int i = 0; i < cmd_state->focused_files_count; i++) {
        cJSON *str = cJSON_CreateString(cmd_state->focused_files[i]);
        if (!str || !cJSON_AddItemToArray(focused, str)) {
            cJSON_Delete(root);
            return -1;
        }
    }
    
    if (!cJSON_AddItemToObject(root, "focused_files", focused)) {
        cJSON_Delete(root);
        return -1;
    }
    
    // Write to file
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        return -1;
    }
    
    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    
    int ret = 0;
    if (fprintf(f, "%s", json_str) < 0) {
        ret = -1;
    }
    
    if (fclose(f) != 0) {
        ret = -1;
    }
    
    return ret;
}

// Read state back from JSON file and update the state
static void read_state_json(const char *path, AgentState *state, AgentCommandState *cmd_state) {
    char *error = NULL;
    char *content = file_to_string(path, &error);
    if (!content) {
        return;
    }
    
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        return;
    }
    
    // Check for special fields that indicate command results
    cJSON *done = cJSON_GetObjectItem(root, "done");
    if (done && cJSON_IsTrue(done)) {
        state->done = true;
        cJSON *done_msg = cJSON_GetObjectItem(root, "done_message");
        if (done_msg && cJSON_IsString(done_msg)) {
            state->done_message = gc_strdup(&gc, cJSON_GetStringValue(done_msg));
        }
    }
    
    cJSON *aborted = cJSON_GetObjectItem(root, "aborted");
    if (aborted && cJSON_IsTrue(aborted)) {
        state->aborted = true;
        cJSON *abort_msg = cJSON_GetObjectItem(root, "abort_message");
        if (abort_msg && cJSON_IsString(abort_msg)) {
            state->abort_message = gc_strdup(&gc, cJSON_GetStringValue(abort_msg));
        }
    }
    
    // Update working directory
    cJSON *wd = cJSON_GetObjectItem(root, "working_dir");
    if (wd && cJSON_IsString(wd)) {
        // gc doesn't have explicit free
        state->working_dir = gc_strdup(&gc, cJSON_GetStringValue(wd));
        cmd_state->working_dir = state->working_dir;
    }
    
    // Update focused files
    cJSON *focused = cJSON_GetObjectItem(root, "focused_files");
    if (focused && cJSON_IsArray(focused)) {
        int new_count = cJSON_GetArraySize(focused);
        char **new_files = gc_malloc(&gc, new_count * sizeof(char*));
        
        for (int i = 0; i < new_count; i++) {
            cJSON *item = cJSON_GetArrayItem(focused, i);
            if (item && cJSON_IsString(item)) {
                new_files[i] = gc_strdup(&gc, cJSON_GetStringValue(item));
            }
        }
        
        state->focused_files = new_files;
        state->focused_files_count = new_count;
        cmd_state->focused_files = new_files;
        cmd_state->focused_files_count = new_count;
    }
}

char* execute_agent_script(const char *script, AgentState *state, AgentCommandState *cmd_state) {
    // Create temporary directory
    char temp_template[] = "/tmp/minicoder-XXXXXX";
    char *temp_dir = mkdtemp(temp_template);
    if (!temp_dir) {
        return gc_strdup(&gc, "Error: Failed to create temporary directory");
    }
    // Make a gc copy since temp_template is on stack
    temp_dir = gc_strdup(&gc, temp_dir);
    
    // Get current executable path
    if (!g_executable_path || !g_executable_path[0]) {
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to get executable path");
    }
    
    // Create bin directory
    char *bin_dir = gc_asprintf(&gc, "%s/bin", temp_dir);
    if (mkdir(bin_dir, 0755) != 0) {
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to create bin directory");
    }
    
    // Create symlinks for agent commands
    const char *commands[] = {
        "agent-files", "agent-cd", "agent-abort", "agent-done"
    };
    
    for (int i = 0; i < 4; i++) {
        char *link_path = gc_asprintf(&gc, "%s/bin/%s", temp_dir, commands[i]);
        if (symlink(g_executable_path, link_path) != 0) {
            remove_directory(temp_dir);
            return gc_asprintf(&gc, "Error: Failed to create symlink for %s", commands[i]);
        }
    }
    
    // Write initial state JSON
    char *state_path = gc_asprintf(&gc, "%s/model_state.json", temp_dir);
    if (write_state_json(state_path, cmd_state) != 0) {
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to write initial state JSON");
    }
    
    // Create script file
    char *script_path = gc_asprintf(&gc, "%s/script.sh", temp_dir);
    FILE *f = fopen(script_path, "w");
    if (!f) {
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to create script file");
    }
    
    // Get current PATH to preserve it
    const char *current_path = getenv("PATH");
    if (!current_path) {
        current_path = "/usr/local/bin:/usr/bin:/bin";  // Fallback default
    }
    
    // Escape paths for safe shell inclusion
    char *escaped_state_path = shell_escape_single_quotes(state_path);
    char *escaped_temp_dir = shell_escape_single_quotes(temp_dir);
    char *escaped_current_path = shell_escape_single_quotes(current_path);
    
    // Write script with proper shebang and settings
    if (fprintf(f, "export MINICODER_STATE_FILE='%s'\n", escaped_state_path) < 0 ||
        fprintf(f, "export PATH='%s/bin:%s'\n", escaped_temp_dir, escaped_current_path) < 0 || 
        fprintf(f, "set -ex\n") < 0) {
        fclose(f);
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to write script header");
    }
    
    if (cmd_state->working_dir) {
        char *escaped_working_dir = shell_escape_single_quotes(cmd_state->working_dir);
        if (fprintf(f, "cd '%s'\n", escaped_working_dir) < 0) {
            fclose(f);
            remove_directory(temp_dir);
            return gc_strdup(&gc, "Error: Failed to write working directory change");
        }
    }
    
    if (fprintf(f, "%s\n", script) < 0) {
        fclose(f);
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to write script body");
    }
    
    if (fclose(f) != 0) {
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to close script file");
    }
    
    // Make script executable
    if (chmod(script_path, 0755) != 0) {
        remove_directory(temp_dir);
        return gc_asprintf(&gc, "Error: Failed to make script executable: %s", strerror(errno));
    }
    
    // Execute the script
    // Get shell from environment or use default
    const char *shell = getenv("MINICODER_SHELL");
    if (!shell) {
        shell = "/bin/sh";
    }
    
    // Set up shell arguments to execute the script file
    char *shell_args[3];
    shell_args[0] = (char *)shell;
    shell_args[1] = script_path;
    shell_args[2] = NULL;
    
    // Create pipe for capturing output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        remove_directory(temp_dir);
        return gc_asprintf(&gc, "Error: Failed to create pipe: %s", strerror(errno));
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        remove_directory(temp_dir);
        return gc_asprintf(&gc, "Error: Failed to fork: %s", strerror(errno));
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close read end
        
        // Redirect stdin to /dev/null to prevent reading from terminal
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        
        // Redirect stdout and stderr to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            _exit(127);
        }
        close(pipefd[1]);  // Close original write end
        
        // Execute script
        execvp(shell, shell_args);
        // If we get here, execvp failed
        _exit(127);
    }
    
    // Parent process
    close(pipefd[1]);  // Close write end
    
    // Read output from pipe
    string_builder_t sb;
    string_builder_init(&sb, &gc, 4096);
    
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        string_builder_append_str(&sb, buffer);
        
        // Forward output to parent terminal
        if (write(STDOUT_FILENO, buffer, bytes_read) == -1) {
            // Ignore write errors for output forwarding
        }
    }
    close(pipefd[0]);
    
    // Wait for child process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        remove_directory(temp_dir);
        return gc_asprintf(&gc, "Error: Failed to wait for child process: %s", strerror(errno));
    }
    
    char *output = string_builder_finalize(&sb);
    int exit_code = -1;
    
    // Check exit status
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    }
    
    // Read state back
    read_state_json(state_path, state, cmd_state);
    
    // Clean up temp directory
    if (remove_directory(temp_dir) != 0) {
        // Log warning but don't fail - the script already executed
        fprintf(stderr, "Warning: Failed to clean up temporary directory: %s\n", temp_dir);
    }
    
    // Build result
    string_builder_t result_sb;
    string_builder_init(&result_sb, &gc, 1024);
    
    if (output) {
        string_builder_append_str(&result_sb, output);
    }
    
    if (exit_code != 0 && !state->done && !state->aborted) {
        string_builder_append_fmt(&result_sb, "\n[Script exited with code %d]\n", exit_code);
    }
    
    return string_builder_finalize(&result_sb);
}