#include "assist.h"
#include "util.h"
#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cJSON.h>


char* extract_exec_script(const char *text) {
    gc_string_builder_t sb;
    gc_string_builder_init(&sb, 1024);
    bool found_any = false;
    
    const char *p = text;
    
    while (*p) {
        // Look for "exec"
        const char *exec_pos = strstr(p, "exec");
        if (!exec_pos) {
            break;
        }
        
        // Move past "exec"
        const char *after_exec = exec_pos + 4;
        
        // Skip whitespace (spaces and tabs)
        while (*after_exec == ' ' || *after_exec == '\t') {
            after_exec++;
        }
        
        // Check for newline
        if (*after_exec != '\n') {
            p = exec_pos + 1;
            continue;
        }
        after_exec++; // Move past the newline
        
        // Look for delimiter (backticks or tildes)
        char delimiter_char = '\0';
        int delimiter_count = 0;
        const char *delimiter_start = after_exec;
        
        // Count consecutive backticks or tildes
        if (*delimiter_start == '`' || *delimiter_start == '~') {
            delimiter_char = *delimiter_start;
            while (*delimiter_start == delimiter_char) {
                delimiter_count++;
                delimiter_start++;
            }
        }
        
        // Need at least 3 delimiter characters
        if (delimiter_count < 3) {
            p = exec_pos + 1;
            continue;
        }
        
        // Check for newline after delimiter
        if (*delimiter_start != '\n') {
            p = exec_pos + 1;
            continue;
        }
        delimiter_start++; // Move past the newline
        
        // Find the closing delimiter (must be same char type, same count or more, on its own line)
        const char *start_content = delimiter_start;
        const char *end_content = NULL;
        const char *search_pos = start_content;
        
        while (*search_pos) {
            // Look for newline followed by delimiter
            if (*search_pos == '\n') {
                const char *next_line = search_pos + 1;
                // Check if line starts with the same delimiter character
                int closing_count = 0;
                while (next_line[closing_count] == delimiter_char) {
                    closing_count++;
                }
                // Must have at least as many delimiter chars as opening
                if (closing_count >= delimiter_count && 
                    (next_line[closing_count] == '\n' || next_line[closing_count] == '\0')) {
                    end_content = search_pos;
                    break;
                }
            }
            search_pos++;
        }
        
        if (!end_content) {
            p = exec_pos + 1;
            continue;
        }
        
        // Extract the content
        size_t content_len = end_content - start_content;
        
        // Add newline between blocks if not the first one
        if (found_any) {
            gc_string_builder_append_str(&sb, "\n");
        }
        
        // Add the content to our result
        char *content = gc_malloc(content_len + 1);
        memcpy(content, start_content, content_len);
        content[content_len] = '\0';
        
        gc_string_builder_append_str(&sb, content);
        gc_free(content);
        
        found_any = true;
        
        // Move past the closing delimiter for next iteration
        p = end_content + 1;
        while (*p && *p != '\n') p++;
    }
    
    if (!found_any) {
        return NULL;
    }
    
    return gc_string_builder_finalize(&sb);
}

char* truncate_history(const char *history, int max_size) {
    if (!history) return gc_strdup("(none)");
    
    int len = strlen(history);
    if (len <= max_size) {
        return gc_strdup(history);
    }
    
    // Find a good starting point after truncation
    const char *start = history + (len - max_size);
    const char *newline = strchr(start, '\n');
    if (newline) {
        start = newline + 1;
    }
    
    return gc_asprintf("[... older iterations truncated ...]\n\n%s", start);
}


char* get_focused_content(char **files, int file_count, int max_size) {
    gc_string_builder_t sb;
    gc_string_builder_init(&sb, 1024);
    
    int total_size = 0;
    
    // First pass: check total size
    for (int i = 0; i < file_count; i++) {
        struct stat st;
        if (stat(files[i], &st) == 0) {
            total_size += st.st_size;
        }
    }
    
    if (total_size > max_size) {
        gc_string_builder_append_str(&sb, "[WARNING: Focused content size limit exceeded!]\n\n");
        gc_string_builder_append_fmt(&sb, "Total size of focused files: %d bytes (limit: %d bytes)\n", total_size, max_size);
        gc_string_builder_append_str(&sb, "You need to unfocus some files to stay within the limit.\n\n");
        gc_string_builder_append_str(&sb, "Currently focused files:\n");
        
        for (int i = 0; i < file_count; i++) {
            struct stat st;
            if (stat(files[i], &st) == 0) {
                gc_string_builder_append_fmt(&sb, "- %s (%ld bytes)\n", files[i], st.st_size);
            }
        }
    } else {
        // Read file contents
        for (int i = 0; i < file_count; i++) {
            if (i > 0) {
                gc_string_builder_append_str(&sb, "\n\n");
            }
            
            gc_string_builder_append_fmt(&sb, "--- %s ---\n", files[i]);
            
            if (is_binary_file(files[i])) {
                struct stat st;
                if (stat(files[i], &st) == 0) {
                    gc_string_builder_append_fmt(&sb, "[Binary data (%ld bytes)]", st.st_size);
                } else {
                    gc_string_builder_append_str(&sb, "[Binary data]");
                }
            } else {
                char *error = NULL;
                char *content = file_to_string(files[i], &error);
                if (content) {
                    gc_string_builder_append_str(&sb, content);
                } else {
                    gc_string_builder_append_fmt(&sb, "[Error reading file: %s]", error ? error : "Unknown error");
                }
            }
        }
    }
    
    return gc_string_builder_finalize(&sb);
}


// Context structure for model output callback
typedef struct {
    FILE *output;
    bool reasoning_header_shown;
    bool response_header_shown;
    bool spinner_stopped;
} OutputContext;

// Arguments for prompt building
typedef struct {
    const char *user_request;
    const AssistantState *state;
    const char *focused_files;
    const char *history;
} PromptBuildArgs;

// Build the prompt for the LLM
static char* build_prompt(const PromptBuildArgs *args) {
    gc_string_builder_t sb;
    gc_string_builder_init(&sb, 4096);
    
    // Add the prompt template content
    gc_string_builder_append_str(&sb, "You are part of an agentic ai system that iterates while trying to perform a task.\n");
    gc_string_builder_append_str(&sb, "You interact with the system by outputting scripts to be run in a specific format.\n\n");
    
    gc_string_builder_append_str(&sb, "You may output a shell script in the following format for execution:\n\n");
    gc_string_builder_append_str(&sb, "exec\n```\n");
    gc_string_builder_append_str(&sb, "Your posix shell script here\n");
    gc_string_builder_append_str(&sb, "```\n\n");
    
    gc_string_builder_append_str(&sb, "The code blocks support markdown-style delimiters (3+ ` or ~).\n");
    gc_string_builder_append_str(&sb, "You *must* adjust your delimiters if your script contains backticks.\n\n");
    
    gc_string_builder_append_str(&sb, "Example:\n\n");
    gc_string_builder_append_str(&sb, "exec\n```\n");
    gc_string_builder_append_str(&sb, "echo \"Task completed successfully!\" | agent-done\n");
    gc_string_builder_append_str(&sb, "```\n\n");
    
    gc_string_builder_append_str(&sb, "The output of this script execution will be given back to you at the next iteration of this loop.\n");
    gc_string_builder_append_str(&sb, "The script starts with -e set and therefore exits immediately if any command fails.\n");
    gc_string_builder_append_str(&sb, "The script starts with -x set by default so you can debug any issues more easily.\n\n");
    
    gc_string_builder_append_str(&sb, "The following shell builtin commands have been added to help you control the agent loop.\n\n");
    gc_string_builder_append_str(&sb, "- agent-focus PATH...\n");
    gc_string_builder_append_str(&sb, "  The focus command will add files to the current context such that the contents appear in future iterations automatically.\n");
    gc_string_builder_append_str(&sb, "- agent-unfocus PATH...\n");
    gc_string_builder_append_str(&sb, "  The unfocus commands removes files from the current focus.\n");
    gc_string_builder_append_str(&sb, "- agent-cd PATH\n");
    gc_string_builder_append_str(&sb, "  Change the agent's working directory.\n");
    gc_string_builder_append_str(&sb, "- agent-abort\n");
    gc_string_builder_append_str(&sb, "   Abort the agent loop as failed. Reads an optional message from stdin or answer for the user.\n");
    gc_string_builder_append_str(&sb, "- agent-done\n");
    gc_string_builder_append_str(&sb, "   Signal that the goal is complete and stop iteration. Reads an optional message from stdin for the user.\n\n");
    
    gc_string_builder_append_str(&sb, "You should use these agent commands from within exec blocks.\n\n");
    
    gc_string_builder_append_str(&sb, "You have limited memory. You should explain your reasoning and plans\n");
    gc_string_builder_append_str(&sb, "clearly so that future iterations can understand the context and continue the work.\n\n");
    gc_string_builder_append_str(&sb, "In your output you should clearly explain:\n");
    gc_string_builder_append_str(&sb, "- What you have accomplished so far\n");
    gc_string_builder_append_str(&sb, "- What the script you're about to run will do\n");
    gc_string_builder_append_str(&sb, "- What still needs to be done in future iterations\n\n");
    gc_string_builder_append_str(&sb, "A nested and bulleted todo list with [done] markers would be a good way to track your progress.\n");
    
    gc_string_builder_append_str(&sb, "A history of previous iterations (including your full responses and command outputs) will be preserved below,\n");
    gc_string_builder_append_str(&sb, "though older iterations will be phased out of context gradually.\n\n");
    
    gc_string_builder_append_str(&sb, "You must exec the `agent-done` command to end iteration. Use the message to answer questions or explain what was achieved.\n\n");
    
    gc_string_builder_append_str(&sb, "--- CURRENT STATE ---\n\n");
    
    gc_string_builder_append_str(&sb, "User query/request:\n\n");
    gc_string_builder_append_fmt(&sb, "%s\n\n", args->user_request);
    
    gc_string_builder_append_fmt(&sb, "Working directory: %s\n\n", args->state->working_dir);
    
    gc_string_builder_append_str(&sb, "Focused files:\n\n");
    gc_string_builder_append_fmt(&sb, "%s\n\n", args->focused_files);
    
    gc_string_builder_append_str(&sb, "History of previous iterations:\n\n");
    gc_string_builder_append_fmt(&sb, "%s", args->history);
    
    return gc_string_builder_finalize(&sb);
}

// Callback function to handle model output streaming
static void output_callback(const char *chunk, size_t chunk_len, model_chunk_type_t chunk_type, void *user_data) {
    OutputContext *ctx = (OutputContext *)user_data;
    
    // Stop spinner on first output of any kind
    if (!ctx->spinner_stopped && chunk_len > 0) {
        stop_spinner();
        ctx->spinner_stopped = true;
    }
    
    if (chunk_type == CHUNK_TYPE_REASONING && chunk_len > 0) {
        // Add newline before first reasoning chunk
        if (!ctx->reasoning_header_shown) {
            fprintf(ctx->output, "\n");
            ctx->reasoning_header_shown = true;
        }
        
        // Write the reasoning chunk
        fwrite(chunk, 1, chunk_len, ctx->output);
        fflush(ctx->output);
    } else if (chunk_type == CHUNK_TYPE_CONTENT && chunk_len > 0) {
        // Add newline when transitioning from reasoning to content
        if (!ctx->response_header_shown) {
            if (ctx->reasoning_header_shown) {
                fprintf(ctx->output, "\n");
            }
            fprintf(ctx->output, "\n");
            ctx->response_header_shown = true;
        }
        
        // Write the content chunk to output
        fwrite(chunk, 1, chunk_len, ctx->output);
        fflush(ctx->output);
    }
}

AssistantResult run_assistant(AssistantArgs *args) {
    AssistantState state = {0};
    AssistantCommandState cmd_state = {0};
    
    // Initialize working directory
    if (args->working_dir) {
        state.working_dir = gc_strdup(args->working_dir);
        cmd_state.working_dir = gc_strdup(args->working_dir);
    } else {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            state.working_dir = gc_strdup(cwd);
            cmd_state.working_dir = gc_strdup(cwd);
        }
    }
    
    // Process initial focus files
    if (args->initial_focus_count > 0) {
        // Copy the already-expanded paths from args
        state.focused_files = gc_malloc(args->initial_focus_count * sizeof(char*));
        for (int i = 0; i < args->initial_focus_count; i++) {
            state.focused_files[i] = gc_strdup(args->initial_focus[i]);
        }
        state.focused_files_count = args->initial_focus_count;
        
        cmd_state.focused_files = state.focused_files;
        cmd_state.focused_files_count = state.focused_files_count;
    }
    
    // Get model once before the loop to know its context limits
    model_t *model = NULL;
    if (args->model) {
        model = get_model(args->model_config, args->model);
    } else {
        model = get_default_model(args->model_config);
    }
    
    if (!model) {
        fprintf(args->output, "Error: Unknown model: %s\n", args->model ? args->model : "(default)");
        return ASSISTANT_RESULT_ERROR;
    }
    
    // Generate a dummy prompt once to get exact system prompt size
    PromptBuildArgs dummy_args = {
        .user_request = args->user_request,
        .state = &state,
        .focused_files = "(none)",
        .history = ""
    };
    char *dummy_prompt = build_prompt(&dummy_args);
    size_t system_prompt_size = strlen(dummy_prompt);
    
    while (!state.done && !state.aborted && state.iteration < args->max_iterations) {
        // Check for cancellation
        if (args->should_cancel && args->should_cancel(NULL)) {
            fprintf(args->output, "\n=== Cancelled ===\n");
            return ASSISTANT_RESULT_CANCELLED;
        }
        
        state.iteration++;
        
        // String builder for current iteration's entry
        gc_string_builder_t iteration_sb;
        gc_string_builder_init(&iteration_sb, 1024);
        
        // Calculate proportional context allocation based on model limits
        // (system_prompt_size was calculated once before the loop and already includes user_request)
        
        size_t safety_margin = 1000; // Buffer for formatting, etc.
        
        // Calculate available context for focused files and history
        size_t model_limit = model->max_context_bytes;
        size_t available_context = 0;
        if (model_limit > (system_prompt_size + safety_margin)) {
            available_context = model_limit - system_prompt_size - safety_margin;
        }
        
        size_t dynamic_focused_limit = (size_t)(available_context * 0.7);
        size_t dynamic_history_limit = (size_t)(available_context * 0.3);
        
        // Use the dynamically calculated limits
        size_t effective_focused_limit = dynamic_focused_limit;
        size_t effective_history_limit = dynamic_history_limit;
        
        if (args->debug) {
            fprintf(args->output, "\n--- DEBUG: Context Allocation ---\n");
            fprintf(args->output, "Model: %s (max context: %zu bytes)\n", model->name, model->max_context_bytes);
            fprintf(args->output, "Available context: %zu bytes\n", available_context);
            fprintf(args->output, "Focused files limit: %zu bytes\n", effective_focused_limit);
            fprintf(args->output, "History limit: %zu bytes\n", effective_history_limit);
            fprintf(args->output, "--- END DEBUG ---\n\n");
        }
        
        char *focused_files = "(none)";
        if (state.focused_files_count > 0) {
            focused_files = get_focused_content(state.focused_files, 
                                              state.focused_files_count, 
                                              effective_focused_limit);
        }

        // Even though we might have some unused focus space left for history, we still don't want history
        // to grow too big. Current models degrade with large contexts (Though this might change).
        char *history = truncate_history(state.iteration_history, effective_history_limit);
        
        // Build prompt using the dedicated function
        PromptBuildArgs prompt_args = {
            .user_request = args->user_request,
            .state = &state,
            .focused_files = focused_files,
            .history = history
        };
        char *prompt = build_prompt(&prompt_args);
        
        fprintf(args->output, "\n=== Iteration %d ===\n", state.iteration);
        
        if (args->debug) {
            fprintf(args->output, "\n--- DEBUG: Prompt sent to LLM ---\n");
            fprintf(args->output, "%s\n", prompt);
            fprintf(args->output, "--- END DEBUG ---\n");
        }
        
        // Model already retrieved above for context calculation
        char *error = NULL;
        model_completion_options_t options = {0};
        
        // Setup output callback context
        OutputContext output_ctx = {0};
        output_ctx.output = args->output;
        
        // Always use the output callback for streaming
        options.output_callback = output_callback;
        options.callback_user_data = &output_ctx;
        
        // Setup cancellation callback if provided
        if (args->should_cancel) {
            options.cancellation_callback = args->should_cancel;
        }
        
        // Start spinner while waiting for model
        start_spinner("Thinking...");
        
        char *response = model_completion(model, prompt, &options, &error);
        
        // Ensure spinner is stopped (in case no output was received)
        stop_spinner();
        
        if (!response) {
            fprintf(args->output, "Error: Failed to get model response: %s\n", error ? error : "Unknown error");
            return ASSISTANT_RESULT_ERROR;
        }
        
        // Add newline after streamed output
        fprintf(args->output, "\n");
        
        // Build iteration entry header
        gc_string_builder_append_fmt(&iteration_sb, "=== Iteration %d ===\n\n", state.iteration);
        
        // Add the full model response
        gc_string_builder_append_str(&iteration_sb, "Assistant:\n");
        gc_string_builder_append_str(&iteration_sb, response);
        gc_string_builder_append_str(&iteration_sb, "\n");
        
        // Extract and execute script if present
        char *exec_script = extract_exec_script(response);
        if (exec_script) {
            fprintf(args->output, "\nExecuting...\n\n");
            
            // Ensure cmd_state has the latest working directory
            cmd_state.working_dir = state.working_dir;
            
            char *output = execute_script(exec_script, &state, &cmd_state);
            if (output) {
                gc_string_builder_append_fmt(&iteration_sb, "\nScript output:\n%s\n", output);
            }
        }
        
        // Append this iteration to history
        char *iteration_entry = gc_string_builder_finalize(&iteration_sb);
        if (state.iteration_history) {
            char *new_history = gc_asprintf("%s\n%s", state.iteration_history, iteration_entry);
            gc_free(state.iteration_history);
            state.iteration_history = new_history;
        } else {
            state.iteration_history = iteration_entry;
        }
    }
    
    if (state.done) {
        fprintf(args->output, "\n=== Success ===\n\n");
        if (state.done_message && strlen(state.done_message) > 0) {
            fprintf(args->output, "%s\n", state.done_message);
        }
        return ASSISTANT_RESULT_SUCCESS;
    } else if (state.aborted) {
        fprintf(args->output, "\n=== Abort ===\n\n");
        if (state.abort_message && strlen(state.abort_message) > 0) {
            fprintf(args->output, "%s\n", state.abort_message);
        }
        return ASSISTANT_RESULT_ABORTED;
    } else {
        fprintf(args->output, "\n=== Iteration Limit Exceeded ===\n\n[Stopped after %d iterations]\n", args->max_iterations);
        return ASSISTANT_RESULT_MAX_ITERATIONS;
    }
    
}