#ifndef AGENT_H
#define AGENT_H

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
    char *iteration_history;  // Last iteration only
    int iteration;
    bool done;
    char *done_message;
    bool aborted;
    char *abort_message;
    char *working_dir;
} AgentState;


typedef struct {
    char *user_request;
    bool debug;
    int max_iterations;
    char *model;
    char **initial_focus;
    int initial_focus_count;
    FILE *output;
    char *working_dir;
    model_config_t *model_config;
    model_cancellation_callback should_cancel;  // Optional cancellation check callback
} AgentArgs;

typedef struct {
    char **focused_files;
    int focused_files_count;
    char *working_dir;
} AgentCommandState;

typedef enum {
    AGENT_RESULT_SUCCESS,      // Task completed successfully
    AGENT_RESULT_ABORTED,      // Task was aborted by agent
    AGENT_RESULT_CANCELLED,    // Task was cancelled by user
    AGENT_RESULT_MAX_ITERATIONS, // Hit iteration limit
    AGENT_RESULT_ERROR         // An error occurred
} AgentResult;

// Main function to run the agent
AgentResult run_agent(AgentArgs *args);

// Helper functions
char* extract_exec_script(const char *text);
char* get_focused_content(char **files, int file_count);

// From shell.c
char* execute_script(const char *script, AgentState *state, AgentCommandState *cmd_state);

#endif // AGENT_H