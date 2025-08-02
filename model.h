#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    MODEL_TYPE_OPENAI
} model_type_t;

typedef struct {
    char *endpoint;
    char *model;
    char *api_key;
    char *params;  // JSON string of additional parameters
} openai_model_t;

typedef struct {
    char *name;
    model_type_t type;
    size_t max_tokens;         // Maximum context window in tokens (input + output combined)
    char *provider;            // Provider name (e.g., "OpenAI", "Anthropic", "Google")
    char *description;         // Brief description of the model
    union {
        openai_model_t openai;
    } config;
} model_t;

typedef struct {
    model_t *models;
    size_t count;
} model_config_t;

/**
 * Chunk types for model output callbacks
 */
typedef enum {
    CHUNK_TYPE_CONTENT,    // Normal model output
    CHUNK_TYPE_REASONING   // Reasoning tokens (e.g. from OpenRouter)
} model_chunk_type_t;

/**
 * Callback function type for streaming output.
 * @param chunk The chunk of output text
 * @param chunk_len Length of the chunk
 * @param chunk_type Type of the chunk (content or reasoning)
 * @param user_data User-provided data passed through from options
 */
typedef void (*model_output_callback)(const char *chunk, size_t chunk_len, model_chunk_type_t chunk_type, void *user_data);

/**
 * Callback function type for checking if an operation should be cancelled.
 * @param user_data User-provided data passed through from options
 * @return 1 if the operation should be cancelled, 0 otherwise
 */
typedef int (*model_cancellation_callback)(void *user_data);

/**
 * Options for model completion.
 */
typedef struct {
    model_output_callback output_callback;  // Callback for output chunks (can be NULL)
    void *callback_user_data;               // User data passed to callback
    model_cancellation_callback cancellation_callback;  // Callback to check if cancelled (can be NULL)
    void *cancellation_user_data;           // User data passed to cancellation callback
} model_completion_options_t;

/**
 * Initialize model configuration from config file or use defaults.
 * @param config_path Path to the model config file (can be NULL for defaults)
 * Returns pointer to model_config_t on success, NULL on failure.
 * On failure, *error is set to allocated error message (caller must free).
 * Caller is responsible for storing the returned config.
 */
model_config_t *init_models(const char *config_path, char **error);

/**
 * Get a model by name from the configuration.
 * Returns pointer to model or NULL if not found.
 * config must be initialized via init_models().
 */
model_t *get_model(model_config_t *config, const char *name);

/**
 * Get the first model from the configuration (default model).
 * Returns pointer to model or NULL if no models are configured.
 * config must be initialized via init_models().
 */
model_t *get_default_model(model_config_t *config);

/**
 * Free all memory associated with model configuration.
 * Since we use the garbage collector, this function is not needed.
 */
void cleanup_models(model_config_t *config);

/**
 * List all available models to the specified stream.
 * config must be initialized via init_models().
 * If stream is NULL, outputs to stdout.
 */
void list_models(model_config_t *config, FILE *stream);

/**
 * Get a completion from the specified model.
 * Returns an allocated string containing the completion response, or NULL on failure.
 * Caller must free the returned string.
 * On failure, *error is set to allocated error message (caller must free).
 * @param model The model to use for completion
 * @param prompt The prompt to send to the model
 * @param options Options for the completion (can be NULL for defaults)
 * @param error Pointer to store error message on failure
 */
char *model_completion(model_t *model, const char *prompt, const model_completion_options_t *options, char **error);

#endif /* MODEL_H */