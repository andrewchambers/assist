#include "agent.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <cJSON.h>
#include <errno.h>


// Write the current state to a JSON file
static int write_state_json(const char *path, AssistantCommandState *cmd_state) {
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
static void read_state_json(const char *path, AssistantState *state, AssistantCommandState *cmd_state) {
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
            state->done_message = gc_strdup(cJSON_GetStringValue(done_msg));
        }
    }
    
    cJSON *aborted = cJSON_GetObjectItem(root, "aborted");
    if (aborted && cJSON_IsTrue(aborted)) {
        state->aborted = true;
        cJSON *abort_msg = cJSON_GetObjectItem(root, "abort_message");
        if (abort_msg && cJSON_IsString(abort_msg)) {
            state->abort_message = gc_strdup(cJSON_GetStringValue(abort_msg));
        }
    }
    
    // Update working directory
    cJSON *wd = cJSON_GetObjectItem(root, "working_dir");
    if (wd && cJSON_IsString(wd)) {
        gc_free(state->working_dir);
        state->working_dir = gc_strdup(cJSON_GetStringValue(wd));
        cmd_state->working_dir = state->working_dir;
    }
    
    // Update focused files
    cJSON *focused = cJSON_GetObjectItem(root, "focused_files");
    if (focused && cJSON_IsArray(focused)) {
        int new_count = cJSON_GetArraySize(focused);
        char **new_files = gc_malloc(new_count * sizeof(char*));
        
        for (int i = 0; i < new_count; i++) {
            cJSON *item = cJSON_GetArrayItem(focused, i);
            if (item && cJSON_IsString(item)) {
                new_files[i] = gc_strdup(cJSON_GetStringValue(item));
            }
        }
        
        state->focused_files = new_files;
        state->focused_files_count = new_count;
        cmd_state->focused_files = new_files;
        cmd_state->focused_files_count = new_count;
    }
}

char* execute_script(const char *script, AssistantState *state, AssistantCommandState *cmd_state) {
    // Create temporary directory
    char temp_template[] = "/tmp/minicoder-XXXXXX";
    char *temp_dir = mkdtemp(temp_template);
    if (!temp_dir) {
        return gc_strdup("Error: Failed to create temporary directory");
    }
    // Make a gc copy since temp_template is on stack
    temp_dir = gc_strdup(temp_dir);
    
    // Get current executable path
    if (!g_executable_path[0]) {
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to get executable path");
    }
    
    // Create bin directory
    char *bin_dir = gc_asprintf("%s/bin", temp_dir);
    if (mkdir(bin_dir, 0755) != 0) {
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to create bin directory");
    }
    
    // Create symlinks for agent commands
    const char *commands[] = {
        "agent-focus", "agent-unfocus", "agent-cd", "agent-abort", "agent-done"
    };
    
    for (int i = 0; i < 5; i++) {
        char *link_path = gc_asprintf("%s/bin/%s", temp_dir, commands[i]);
        if (symlink(g_executable_path, link_path) != 0) {
            remove_directory(temp_dir);
            return gc_asprintf("Error: Failed to create symlink for %s", commands[i]);
        }
    }
    
    // Write initial state JSON
    char *state_path = gc_asprintf("%s/model_state.json", temp_dir);
    if (write_state_json(state_path, cmd_state) != 0) {
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to write initial state JSON");
    }
    
    // Create script file
    char *script_path = gc_asprintf("%s/script.sh", temp_dir);
    FILE *f = fopen(script_path, "w");
    if (!f) {
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to create script file");
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
        return gc_strdup("Error: Failed to write script header");
    }
    
    if (cmd_state->working_dir) {
        if (fprintf(f, "cd '%s'\n", cmd_state->working_dir) < 0) {
            fclose(f);
            remove_directory(temp_dir);
            return gc_strdup("Error: Failed to write working directory change");
        }
    }
    
    if (fprintf(f, "%s\n", script) < 0) {
        fclose(f);
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to write script body");
    }
    
    if (fclose(f) != 0) {
        remove_directory(temp_dir);
        return gc_strdup("Error: Failed to close script file");
    }
    
    // Make script executable
    if (chmod(script_path, 0755) != 0) {
        remove_directory(temp_dir);
        return gc_asprintf("Error: Failed to make script executable: %s", strerror(errno));
    }
    
    // Execute the script
    char *output = NULL;
    int exit_code = exec_command(script_path, &output, 1);
    if (exit_code < 0) {
        remove_directory(temp_dir);
        return gc_asprintf("Error: Failed to execute script: %s", 
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
    gc_string_builder_t sb;
    gc_string_builder_init(&sb, 1024);
    
    if (output) {
        gc_string_builder_append_str(&sb, output);
    }
    
    if (exit_code != 0 && !state->done && !state->aborted) {
        gc_string_builder_append_fmt(&sb, "\n[Script exited with code %d]\n", exit_code);
    }
    
    return gc_string_builder_finalize(&sb);
}