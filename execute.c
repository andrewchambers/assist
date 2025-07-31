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

static int exec_command(const char *command, char **out_output, int forward_output) {
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
            *out_output = gc_asprintf(&gc, "Failed to create pipe: %s", strerror(errno));
        }
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        if (out_output) {
            *out_output = gc_asprintf(&gc, "Failed to fork: %s", strerror(errno));
        }
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
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
        
        // Execute command
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
        
        // Forward output to parent terminal if requested
        if (forward_output) {
            if (write(STDOUT_FILENO, buffer, bytes_read) == -1) {
                // Ignore write errors for output forwarding
            }
        }
    }
    close(pipefd[0]);
    
    // Wait for child process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        if (out_output) {
            *out_output = gc_asprintf(&gc, "Failed to wait for child process: %s", strerror(errno));
        }
        return -1;
    }
    
    if (out_output) {
        *out_output = string_builder_finalize(&sb);
    }
    
    // Check exit status
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    
    return -1;  // Process terminated abnormally
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

char* execute_script(const char *script, AgentState *state, AgentCommandState *cmd_state) {
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
    
    // Write script with proper shebang and settings
    if (fprintf(f, "#!/bin/sh\n") < 0 ||
        fprintf(f, "export MINICODER_STATE_FILE='%s'\n", state_path) < 0 ||
        fprintf(f, "export PATH='%s/bin:%s'\n", temp_dir, current_path) < 0 || 
        fprintf(f, "set -ex\n") < 0) {
        fclose(f);
        remove_directory(temp_dir);
        return gc_strdup(&gc, "Error: Failed to write script header");
    }
    
    if (cmd_state->working_dir) {
        if (fprintf(f, "cd '%s'\n", cmd_state->working_dir) < 0) {
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
    char *output = NULL;
    int exit_code = exec_command(script_path, &output, 1);
    if (exit_code < 0) {
        remove_directory(temp_dir);
        return gc_asprintf(&gc, "Error: Failed to execute script: %s", 
                          output ? output : "Unknown error");
    }
    
    // Read state back
    read_state_json(state_path, state, cmd_state);
    
    // Clean up temp directory
    if (remove_directory(temp_dir) != 0) {
        // Log warning but don't fail - the script already executed
        fprintf(stderr, "Warning: Failed to clean up temporary directory: %s\n", temp_dir);
    }
    
    // Build result
    string_builder_t sb;
    string_builder_init(&sb, &gc, 1024);
    
    if (output) {
        string_builder_append_str(&sb, output);
    }
    
    if (exit_code != 0 && !state->done && !state->aborted) {
        string_builder_append_fmt(&sb, "\n[Script exited with code %d]\n", exit_code);
    }
    
    return string_builder_finalize(&sb);
}