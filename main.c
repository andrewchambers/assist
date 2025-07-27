#define _GNU_SOURCE
#include "gc.h"
#include <cJSON.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include "util.h"
#include "string.h"
#include "agent.h"
#include "model.h"

gc_state gc;

// Version information
#ifndef MINICODER_VERSION
#define MINICODER_VERSION "dev"
#endif

// Wrapper functions for cJSON hooks
static void *cjson_malloc_wrapper(size_t size) {
    return gc_malloc(&gc, size);
}

static void cjson_free_wrapper(void *ptr) {
    // gc doesn't have explicit free, do nothing
}

// Default files to focus on if none specified

// Global flag for signal handling
static volatile sig_atomic_t interrupt_received = 0;

// Global to store argv[0] for executable path resolution

// Signal handler
static void signal_handler(int sig) {
    interrupt_received = 1;
    fprintf(stderr, "\nReceived interrupt signal. Shutting down...\n");
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

// Cancellation check callback for model
static int cancellation_callback(void *user_data) {
    return interrupt_received;
}

// Handle agent commands (agent-focus, agent-cd, etc.)
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
    if (strcmp(cmd, "agent-focus") == 0) {
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

static void print_usage(const char *prog_name, model_config_t *model_config) {
    fprintf(stderr, "Usage: %s [options] <request>\n", prog_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --debug                    Show debug output including prompts\n");
    fprintf(stderr, "  --max-iterations NUM       Maximum number of iterations (default: 50)\n");
    
    // Show default model if available
    if (model_config && model_config->count > 0) {
        model_t *default_model = get_default_model(model_config);
        if (default_model) {
            fprintf(stderr, "  --model MODEL              Model to use (default: %s)\n", default_model->name);
        } else {
            fprintf(stderr, "  --model MODEL              Model to use\n");
        }
    } else {
        fprintf(stderr, "  --model MODEL              Model to use\n");
    }
    
    fprintf(stderr, "  --focus FILES              Files or globs to focus on initially (space-separated)\n");
    fprintf(stderr, "  --help                     Show this help message\n");
    fprintf(stderr, "  --version                  Show version information\n");
    fprintf(stderr, "\n");
}

int assist_main(int argc, char *argv[]) {

    // Initialize model configuration early to show in usage
    char *error = NULL;
    model_config_t *model_config = init_models(&error);
    if (!model_config) {
        fprintf(stderr, "Error initializing models: %s\n", error ? error : "Unknown error");
        return 1;
    }

    // Default values
    bool debug = false;
    int max_iterations = 50;
    char *model = NULL;
    char *focus_arg = NULL;  // Store the --focus argument
    
    // Parse command line arguments
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--debug") == 0) {
            debug = true;
            i++;
        } else if (strcmp(argv[i], "--max-iterations") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --max-iterations requires an argument\n");
                return 1;
            }
            max_iterations = atoi(argv[i + 1]);
            if (max_iterations <= 0) {
                fprintf(stderr, "Error: Invalid max iterations: %s\n", argv[i + 1]);
                return 1;
            }
            i += 2;
        } else if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --model requires an argument\n");
                return 1;
            }
            model = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--focus") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --focus requires an argument\n");
                return 1;
            }
            focus_arg = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0], model_config);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("minicoder %s\n", MINICODER_VERSION);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0], model_config);
            return 1;
        }
    }
    
    // Check if we have any models configured
    if (model_config->count == 0) {
        fprintf(stderr, "Error: No models configured. Please check your config file or environment variables.\n");
        return 1;
    }
    
    // Validate model if specified
    if (model) {
        model_t *m = get_model(model_config, model);
        if (!m) {
            fprintf(stderr, "Error: Unknown model: %s\n", model);
            list_models(model_config, stderr);
            return 1;
        }
    }
    
    // Check for user request
    if (i >= argc) {
        fprintf(stderr, "Error: No request provided\n");
        print_usage(argv[0], model_config);
        return 1;
    }
    
    // Concatenate remaining arguments as the user request
    string_builder_t sb;
    string_builder_init(&sb, &gc, 256);
    for (int j = i; j < argc; j++) {
        if (j > i) string_builder_append_str(&sb, " ");
        string_builder_append_str(&sb, argv[j]);
    }
    char *user_request = string_builder_finalize(&sb);
    
    // Process focus files
    char **focus_files = NULL;
    int focus_count = 0;
    
    // Only use focus if explicitly provided
    if (focus_arg) {
        expand_globs_t exp_result;
        int ret = expand_globs(focus_arg, &exp_result);
        if (ret == 0) {
            // Allocate array for expanded files
            focus_files = gc_malloc(&gc, exp_result.we_wordc * sizeof(char*));
            focus_count = 0;
            
            // Copy expanded files that exist
            for (size_t j = 0; j < exp_result.we_wordc; j++) {
                struct stat st;
                if (stat(exp_result.we_wordv[j], &st) != 0) {
                    // Silently skip non-existent files
                    continue;
                }
                focus_files[focus_count++] = gc_strdup(exp_result.we_wordv[j]);
            }
            
            // No need to free - memory managed by gc
        }
    }
    
    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Warning: Failed to set up signal handler\n");
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "Warning: Failed to set up SIGTERM handler\n");
    }
    
    // Create agent arguments
    AgentArgs args = {
        .user_request = user_request,
        .debug = debug,
        .max_iterations = max_iterations,
        .model = model,
        .initial_focus = focus_files,
        .initial_focus_count = focus_count,
        .output = stdout,
        .working_dir = NULL,  // Use current directory
        .model_config = model_config,
        .should_cancel = cancellation_callback
    };
    
    // Run the agent
    AgentResult result = run_agent(&args);
    
    // Map agent result to exit code
    switch (result) {
        case AGENT_RESULT_SUCCESS:
            return 0;
        default:
            return 1;
    }
}

int main(int argc, char *argv[]) {
    // Initialize the garbage collector
    gc_init(&gc, &argc);
    
    // Initialize cJSON to use gc memory management
    cJSON_Hooks hooks;
    hooks.malloc_fn = cjson_malloc_wrapper;
    hooks.free_fn = cjson_free_wrapper;
    cJSON_InitHooks(&hooks);
    
    // Initialize self_exec_path with argv[0]
    self_exec_path_init(argv[0]);

        // Check if called as an agent command
    char *cmd = basename(argv[0]);
    
    // Extract just the command name after the last slash
    char *last_slash = strrchr(argv[0], '/');
    if (last_slash) {
        cmd = last_slash + 1;
    }

    int result;

    if (strncmp(cmd, "agent-", 6) == 0) {
        result = agent_command_main(cmd, argc, argv);
    } else {
        result = assist_main(argc, argv);
    }
    
    // Clean up the garbage collector
    gc_cleanup(&gc);
    
    return result;
}