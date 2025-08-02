#include "agent.h"
#include "util.h"
#include "spinner.h"
#include "model.h"
#include "execute.h"
#include "gc.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cJSON.h>

extern gc_state gc;


static char* extract_exec_script(const char *text) {
    string_builder_t sb;
    string_builder_init(&sb, &gc, 1024);
    bool found_any = false;
    
    const char *p = text;
    
    while (*p) {
        // Look for "exec" at the beginning of a line
        const char *exec_pos = strstr(p, "exec");
        if (!exec_pos) {
            break;
        }
        
        // Check if "exec" is at the beginning of a line
        bool at_line_start = (exec_pos == text) || (exec_pos > text && *(exec_pos - 1) == '\n');
        if (!at_line_start) {
            p = exec_pos + 1;
            continue;
        }
        
        // Move past "exec"
        const char *after_exec = exec_pos + 4;
        
        // Check for newline immediately after "exec"
        if (*after_exec != '\n') {
            p = exec_pos + 1;
            continue;
        }
        after_exec++; // Move past the newline
        
        // Look for delimiter (backticks or tildes)
        char delimiter_char = '\0';
        int delimiter_count = 0;
        const char *fence_start = after_exec;
        
        // Count consecutive backticks or tildes
        if (*fence_start == '`' || *fence_start == '~') {
            delimiter_char = *fence_start;
            const char *p_delim = fence_start;
            while (*p_delim == delimiter_char) {
                delimiter_count++;
                p_delim++;
            }
        }
        
        // Need at least 3 delimiter characters
        if (delimiter_count < 3) {
            p = exec_pos + 1;
            continue;
        }
        
        // Find the end of the opening fence line (skip past delimiter and any language specifier)
        const char *line_end = fence_start + delimiter_count;
        while (*line_end && *line_end != '\n') {
            line_end++;
        }
        
        // Check for newline after fence
        if (*line_end != '\n') {
            p = exec_pos + 1;
            continue;
        }
        
        // Content starts after the newline
        const char *start_content = line_end + 1;
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
            string_builder_append_str(&sb, "\n");
        }
        
        // Add the content to our result
        char *content = gc_malloc(&gc, content_len + 1);
        memcpy(content, start_content, content_len);
        content[content_len] = '\0';
        
        string_builder_append_str(&sb, content);
        
        found_any = true;
        
        // Move past the closing delimiter for next iteration
        p = end_content + 1;
        while (*p && *p != '\n') p++;
    }
    
    if (!found_any) {
        return NULL;
    }
    
    return string_builder_finalize(&sb);
}

// Helper function to truncate text to a maximum byte length
static char* truncate_text(const char *text, size_t max_bytes, const char *truncation_note) {
    if (!text || strlen(text) <= max_bytes) {
        return gc_strdup(&gc, text ? text : "");
    }
    
    // Find a good truncation point (not in the middle of a line)
    size_t truncate_at = max_bytes;
    while (truncate_at > 0 && text[truncate_at] != '\n') {
        truncate_at--;
    }
    
    // If we couldn't find a newline, just use the max
    if (truncate_at == 0) {
        truncate_at = max_bytes;
    }
    
    string_builder_t sb;
    string_builder_init(&sb, &gc, truncate_at + strlen(truncation_note) + 50);
    
    // Add the truncated text
    char *truncated = gc_malloc(&gc, truncate_at + 1);
    memcpy(truncated, text, truncate_at);
    truncated[truncate_at] = '\0';
    
    string_builder_append_str(&sb, truncated);
    string_builder_append_str(&sb, "\n\n");
    string_builder_append_str(&sb, truncation_note);
    
    return string_builder_finalize(&sb);
}

// Helper function to truncate history if needed
static char* truncate_history_if_needed(const char *history, size_t max_bytes) {
    if (!history || strlen(history) <= max_bytes) {
        return gc_strdup(&gc, history ? history : "(none)");
    }
    
    // For history, we want to keep the end (most recent output)
    size_t history_len = strlen(history);
    const char *start = history + (history_len - max_bytes);
    
    // Find a good starting point (beginning of a line)
    // But don't search too far - limit to 1KB to avoid discarding everything
    const char *search_limit = start + 1024;
    if (search_limit > history + history_len) {
        search_limit = history + history_len;
    }
    
    while (start < search_limit && *start != '\n') {
        start++;
    }
    
    // If we didn't find a newline within the search limit, just use the original start
    if (start >= search_limit) {
        start = history + (history_len - max_bytes);
    } else if (*start == '\n') {
        start++;
    }
    
    string_builder_t sb;
    string_builder_init(&sb, &gc, max_bytes + 100);
    
    string_builder_append_str(&sb, "[... previous iteration truncated to fit context limits ...]\n\n");
    string_builder_append_str(&sb, start);
    
    return string_builder_finalize(&sb);
}

static char* get_focused_content(char **files, int file_count) {
    string_builder_t sb;
    string_builder_init(&sb, &gc, 1024);
    
    // Read file contents
    for (int i = 0; i < file_count; i++) {
        if (i > 0) {
            string_builder_append_str(&sb, "\n\n");
        }
        
        string_builder_append_fmt(&sb, "--- %s ---\n", files[i]);
        
        char *bin_error = NULL;
        int is_binary = is_binary_file(files[i], &bin_error);
        
        if (is_binary == -1) {
            // Error checking if file is binary
            string_builder_append_fmt(&sb, "[Error: %s]", bin_error ? bin_error : "Failed to check file");
        } else if (is_binary == 1) {
            // File is binary
            struct stat st;
            if (stat(files[i], &st) == 0) {
                string_builder_append_fmt(&sb, "[Binary data (%ld bytes)]", st.st_size);
            } else {
                string_builder_append_str(&sb, "[Binary data]");
            }
        } else {
            char *error = NULL;
            char *content = file_to_string(files[i], &error);
            if (content) {
                string_builder_append_str(&sb, content);
            } else {
                string_builder_append_fmt(&sb, "[Error reading file: %s]", error ? error : "Unknown error");
            }
        }
    }
    
    return string_builder_finalize(&sb);
}


// Context structure for model output callback
typedef struct {
    FILE *output;
    bool reasoning_header_shown;
    bool response_header_shown;
    bool spinner_stopped;
    char last_char;  // Track last character output
} OutputContext;

// Arguments for prompt building
typedef struct {
    const char *user_request;
    const AgentState *state;
    const char *focused_files;
    const char *history;
    const char *extra_instructions;
} PromptBuildArgs;

// Build the prompt for the LLM
static char* build_prompt(const PromptBuildArgs *args) {
    string_builder_t sb;
    string_builder_init(&sb, &gc, 4096);
    
    // Add the prompt template content
    string_builder_append_str(&sb, "You are an AI agent that is part of an outer execution loop.\n");
    string_builder_append_str(&sb, "Your goal is to execute one shell script per iteration in order to accomplish a user task, or answer a user question.\n\n");
    
    string_builder_append_str(&sb, "# HOW TO EXECUTE SCRIPTS\n\n");

    string_builder_append_str(&sb, "Output a single shell script in this format:\n\n");

    string_builder_append_str(&sb, "exec\n```\n");
    string_builder_append_str(&sb, "# Your POSIX shell script here\n");
    string_builder_append_str(&sb, "```\n\n");

    string_builder_append_str(&sb, "Your script will be run automatically at the end of your turn, and the output will be returned in the next iteration.\n");
    string_builder_append_str(&sb, "Scripts run with -e (exit on error) and -x (debug trace) flags set.\n");
    string_builder_append_str(&sb, "The exec code blocks support markdown delimiters (3+ ` or ~). Adjust the delimiters if your script contains backticks.\n\n");

    string_builder_append_str(&sb, "# AGENT COMMANDS\n\n");

    string_builder_append_str(&sb, "Special commands that control the agent loop are available in your scripts PATH (use them within exec blocks):\n\n");
    string_builder_append_str(&sb, "- agent-files [FILES...] # Replace currently focused files (shown in every iteration, empty to clear)\n");
    string_builder_append_str(&sb, "- agent-cd PATH          # Change working directory permanently (persists across iterations)\n");
    string_builder_append_str(&sb, "- agent-abort            # Stop with failure (pipe message: echo \"reason\" | agent-abort)\n");
    string_builder_append_str(&sb, "- agent-done             # Complete successfully (pipe message: echo \"summary\" | agent-done)\n\n");
    
    string_builder_append_str(&sb, "# STATE MANAGEMENT\n\n");

    string_builder_append_str(&sb, "What persists between iterations:\n");
    string_builder_append_str(&sb, "- Working directory (via agent-cd)\n");
    string_builder_append_str(&sb, "- Focused files list (via agent-files)\n");
    string_builder_append_str(&sb, "- Your own output and the script execution from the previous iteration\n\n");
    string_builder_append_str(&sb, "What does NOT persist:\n");
    string_builder_append_str(&sb, "- Shell variables\n");
    string_builder_append_str(&sb, "- Current directory from 'cd' command\n");
    string_builder_append_str(&sb, "- Output from older iteration\n\n");
    
    string_builder_append_str(&sb, "# PROGRESS TRACKING\n\n");

    string_builder_append_str(&sb, "Maintain a structured task list with clear status markers:\n\n");

    string_builder_append_str(&sb, "- [ ] Main task\n");
    string_builder_append_str(&sb, "  - [✓] Completed subtask (verified in previous iteration)\n");
    string_builder_append_str(&sb, "  - [→] Current subtask (what this script will do)\n");
    string_builder_append_str(&sb, "  - [ ] Pending subtask (for future iterations)\n");
    string_builder_append_str(&sb, "  - [✗] Failed subtask (needs retry or different approach)\n\n");
    string_builder_append_str(&sb, "Only mark tasks [✓] complete AFTER seeing successful output, you shouldn't assume success.\n\n");

    string_builder_append_str(&sb, "# TASK COMPLETION\n\n");

    string_builder_append_str(&sb, "- You should only run the `agent-done` command when the original user request is satisfied\n");
    string_builder_append_str(&sb, "- Supply a message agent-done to answer the user questions or explain what was achieved\n");
    string_builder_append_str(&sb, "- It is easier for the user to read the agent-done message than any execution output\n\n");

    string_builder_append_str(&sb, "# ERROR HANDLING\n\n");

    string_builder_append_str(&sb, "When your exec script fails:\n");
    string_builder_append_str(&sb, "- Examine the -x trace output to identify the failing command\n");
    string_builder_append_str(&sb, "- Check exit codes and error messages\n");
    string_builder_append_str(&sb, "- Consider aborting with agent-abort if the task cannot proceed\n\n");
    
    string_builder_append_str(&sb, "# BEST PRACTICES\n\n");

    string_builder_append_str(&sb, "- State clearly what your script will attempt\n");
    string_builder_append_str(&sb, "- Focus files you'll need to reference in future iterations\n");
    string_builder_append_str(&sb, "- Mention important information for use in the next iteration\n");
    string_builder_append_str(&sb, "- Break complex tasks into smaller, verifiable steps\n");
    string_builder_append_str(&sb, "- Try to accomplish steps each iteration in logical chunks\n");
    string_builder_append_str(&sb, "- Verify outputs before proceeding (verify success in the next iteration)\n");
    string_builder_append_str(&sb, "- Track your own progress via notes (you can only see the output of the last iteration)\n\n");
    
    // Add custom instructions if provided
    if (args->extra_instructions && strlen(args->extra_instructions) > 0) {
        string_builder_append_str(&sb, "# CUSTOM INSTRUCTIONS\n\n");
        string_builder_append_str(&sb, args->extra_instructions);
        
        // Ensure there's at least one newline after instructions
        size_t len = strlen(args->extra_instructions);
        if (len > 0 && args->extra_instructions[len - 1] != '\n') {
            string_builder_append_str(&sb, "\n");
        }
        // Always add an extra newline for spacing
        string_builder_append_str(&sb, "\n");
    }
    
    string_builder_append_str(&sb, "--- CURRENT STATE ---\n\n");
    
    string_builder_append_fmt(&sb, "User query/request:\n\n%s\n\n", args->user_request);
    
    string_builder_append_fmt(&sb, "Working directory:\n\n%s\n\n", args->state->working_dir);
    
    string_builder_append_fmt(&sb, "Focused files:\n\n%s\n\n", args->focused_files);
    
    string_builder_append_fmt(&sb, "Last iteration:\n\n%s", args->history);
    
    return string_builder_finalize(&sb);
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
            ctx->last_char = '\n';
        }
        
        // Write the reasoning chunk
        fwrite(chunk, 1, chunk_len, ctx->output);
        fflush(ctx->output);
        
        // Track last character
        if (chunk_len > 0) {
            ctx->last_char = chunk[chunk_len - 1];
        }
    } else if (chunk_type == CHUNK_TYPE_CONTENT && chunk_len > 0) {
        // Add newline when transitioning from reasoning to content only if reasoning didn't end with one
        if (!ctx->response_header_shown) {
            if (ctx->reasoning_header_shown && ctx->last_char != '\n') {
                fprintf(ctx->output, "\n");
                ctx->last_char = '\n';
            }
            ctx->response_header_shown = true;
        }
        
        // Write the content chunk to output
        fwrite(chunk, 1, chunk_len, ctx->output);
        fflush(ctx->output);
        
        // Track last character
        if (chunk_len > 0) {
            ctx->last_char = chunk[chunk_len - 1];
        }
    }
}

AgentResult run_agent(AgentArgs *args) {
    AgentState state = {0};
    AgentCommandState cmd_state = {0};
    
    // Initialize working directory
    if (args->working_dir) {
        state.working_dir = gc_strdup(&gc, args->working_dir);
        cmd_state.working_dir = gc_strdup(&gc, args->working_dir);
    } else {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) {
            state.working_dir = gc_strdup(&gc, cwd);
            cmd_state.working_dir = gc_strdup(&gc, cwd);
        }
    }
    
    // Process initial focus files
    if (args->initial_focus_count > 0) {
        // Copy the already-expanded paths from args
        state.focused_files = gc_malloc(&gc, args->initial_focus_count * sizeof(char*));
        for (int i = 0; i < args->initial_focus_count; i++) {
            state.focused_files[i] = gc_strdup(&gc, args->initial_focus[i]);
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
        return AGENT_RESULT_ERROR;
    }
    
    // Generate a dummy prompt once to get exact system prompt size
    PromptBuildArgs dummy_args = {
        .user_request = args->user_request,
        .state = &state,
        .focused_files = "(none)",
        .history = "",
        .extra_instructions = args->extra_instructions
    };
    char *dummy_prompt = build_prompt(&dummy_args);
    size_t system_prompt_size = strlen(dummy_prompt);
    
    while (!state.done && !state.aborted && state.iteration < args->max_iterations) {
        // Check for cancellation
        if (args->should_cancel && args->should_cancel(NULL)) {
            fprintf(args->output, "\n=== Cancelled ===\n");
            return AGENT_RESULT_CANCELLED;
        }
        
        state.iteration++;
        
        // String builder for current iteration's entry
        string_builder_t iteration_sb;
        string_builder_init(&iteration_sb, &gc, 1024);
        
        // Calculate available space for variable content
        // Compute max_context_bytes from max_tokens
        // Formula: tokens * 4 bytes/token * 0.9 safety margin / 2 for input/output split
        if (model->max_tokens == 0) {
            fprintf(args->output, "Error: Model '%s' does not specify max_tokens\n", model->name);
            return AGENT_RESULT_ERROR;
        }
        size_t max_context_bytes = (size_t)(model->max_tokens * 4 * 0.9 / 2);
        
        // Safety margin: Reserve 20% for token estimation variance
        size_t safety_margin = max_context_bytes * 20 / 100;
        size_t available_bytes = max_context_bytes - system_prompt_size - safety_margin;
        
        // Allocate space for each component (focused files, history)
        // Give 40% to focused files, 60% to history
        size_t focused_files_budget = available_bytes * 40 / 100;
        size_t initial_history_budget = available_bytes * 60 / 100;
        
        // Get focused files content
        char *focused_files_full = "(none)";
        char *focused_files = "(none)";
        size_t focused_files_actual_size = 0;
        
        if (state.focused_files_count > 0) {
            focused_files_full = get_focused_content(state.focused_files, 
                                                    state.focused_files_count);
            focused_files_actual_size = strlen(focused_files_full);
            
            if (focused_files_actual_size > focused_files_budget) {
                focused_files = truncate_text(focused_files_full, focused_files_budget,
                    "[NOTE: Focused files were truncated to fit context limits. Consider focusing on fewer or smaller files.]");
                // Update actual size to reflect the truncated content + note
                focused_files_actual_size = strlen(focused_files);
            } else {
                focused_files = focused_files_full;
            }
        } else {
            focused_files_actual_size = strlen("(none)");
        }

        // Extend history budget with unused focused files space
        size_t unused_files_budget = focused_files_budget - focused_files_actual_size;
        size_t history_budget = initial_history_budget + unused_files_budget;
        
        // Get history from previous iteration with truncation if needed
        char *history = truncate_history_if_needed(state.prev_iteration, history_budget);
        
        // Build prompt using the dedicated function
        PromptBuildArgs prompt_args = {
            .user_request = args->user_request,
            .state = &state,
            .focused_files = focused_files,
            .history = history,
            .extra_instructions = args->extra_instructions
        };
        char *prompt = build_prompt(&prompt_args);
        
        // Print and build the iteration header
        // Only add newline before header if it's not the first iteration
        const char *iteration_header;
        if (state.iteration > 1) {
            iteration_header = gc_asprintf(&gc, "\n=== Iteration %d ===\n", state.iteration);
        } else {
            iteration_header = gc_asprintf(&gc, "=== Iteration %d ===\n", state.iteration);
        }
        fprintf(args->output, "%s", iteration_header);
        string_builder_append_str(&iteration_sb, iteration_header);
        
        if (args->debug) {
            fprintf(args->output, "\n--- DEBUG: Context management ---\n");
            fprintf(args->output, "Model context limit: %zu bytes\n", max_context_bytes);
            fprintf(args->output, "Base prompt size: %zu bytes\n", system_prompt_size);
            fprintf(args->output, "Available for content: %zu bytes\n", available_bytes);
            fprintf(args->output, "Focused files size: %zu bytes (budget: %zu, used: %zu)\n", 
                    strlen(focused_files_full), focused_files_budget, focused_files_actual_size);
            
            // Calculate previous iteration size
            size_t prev_iteration_size = state.prev_iteration ? strlen(state.prev_iteration) : 0;
            fprintf(args->output, "Previous iteration size: %zu bytes (initial budget: %zu, extended budget: %zu)\n", 
                    prev_iteration_size, initial_history_budget, history_budget);
            
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

        fprintf(args->output, "Agent:\n");
        string_builder_append_str(&iteration_sb, "Agent:\n");
        
        char *response = model_completion(model, prompt, &options, &error);
        
        // Ensure spinner is stopped (in case no output was received)
        stop_spinner();
        
        if (!response) {
            fprintf(args->output, "Error: Failed to get model response: %s\n", error ? error : "Unknown error");
            return AGENT_RESULT_ERROR;
        }
        
        // Ensure model output ends with a newline
        if (output_ctx.last_char != '\n' && output_ctx.last_char != '\0') {
            fprintf(args->output, "\n");
        }
        
        // Add to history (the response was already streamed to user)
        string_builder_append_str(&iteration_sb, response);
        
        // Ensure history also ends with newline
        size_t response_len = strlen(response);
        if (response_len > 0 && response[response_len - 1] != '\n') {
            string_builder_append_str(&iteration_sb, "\n");
        }
        
        // Extract and execute script if present
        char *exec_script = extract_exec_script(response);
        if (exec_script) {
            const char *executing_message = "Executing agent script...\n";
            fprintf(args->output, "%s", executing_message);
            string_builder_append_str(&iteration_sb, executing_message);
            
            // Ensure cmd_state has the latest working directory
            cmd_state.working_dir = state.working_dir;
            
            char *script_output = execute_agent_script(exec_script, &state, &cmd_state);
            if (script_output) {
                string_builder_append_str(&iteration_sb, script_output);
            }
        }
        
        // Store this iteration for the next iteration to see
        state.prev_iteration = string_builder_finalize(&iteration_sb);
    }
    
    if (state.done) {
        fprintf(args->output, "\n=== Success ===\n");
        if (state.done_message && strlen(state.done_message) > 0) {
            fprintf(args->output, "\n%s\n", state.done_message);
        }
        return AGENT_RESULT_SUCCESS;
    } else if (state.aborted) {
        fprintf(args->output, "\n=== Abort ===\n");
        if (state.abort_message && strlen(state.abort_message) > 0) {
            fprintf(args->output, "\n%s\n", state.abort_message);
        }
        return AGENT_RESULT_ABORTED;
    } else {
        fprintf(args->output, "\n=== Iteration Limit Exceeded ===\n\n[Stopped after %d iterations]\n", args->max_iterations);
        return AGENT_RESULT_MAX_ITERATIONS;
    }
    
}