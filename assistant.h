#ifndef ASSISTANT_H
#define ASSISTANT_H

#include <stdbool.h>
#include <stdio.h>
#include "model.h"

typedef struct {
    char *user_request;
    int iteration;
    char *focused_files;
    char *iteration_history;
    char *working_dir;
} PromptData;

typedef struct {
    char **focused_files;
    int focused_files_count;
    char *iteration_history;  // Combined history of all iterations
    int iteration;
    bool done;
    char *done_message;
    bool aborted;
    char *abort_message;
    char *working_dir;
} AssistantState;


typedef struct {
    char *user_request;
    bool debug;
    int max_iterations;
    char *model;
    char *reasoning;
    char **initial_focus;
    int initial_focus_count;
    FILE *output;
    char *working_dir;
    model_config_t *model_config;
    model_cancellation_callback should_cancel;  // Optional cancellation check callback
} AssistantArgs;

typedef struct {
    char **focused_files;
    int focused_files_count;
    char *working_dir;
} AssistantCommandState;

typedef enum {
    ASSISTANT_RESULT_SUCCESS,      // Task completed successfully
    ASSISTANT_RESULT_ABORTED,      // Task was aborted by assistant
    ASSISTANT_RESULT_CANCELLED,    // Task was cancelled by user
    ASSISTANT_RESULT_MAX_ITERATIONS, // Hit iteration limit
    ASSISTANT_RESULT_ERROR         // An error occurred
} AssistantResult;

// Main function to run the assistant
AssistantResult run_assistant(AssistantArgs *args);

// Helper functions
char* extract_exec_script(const char *text);
char* truncate_history(const char *history, int max_size);
char* get_focused_content(char **files, int file_count, int max_size);

// From shell.c
char* execute_script(const char *script, AssistantState *state, AssistantCommandState *cmd_state);

#endif // ASSISTANT_H