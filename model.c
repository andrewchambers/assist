#include "model.h"
#include "util.h"
#include "gc.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <cJSON.h>
#include <curl/curl.h>

extern gc_state gc;

// N.B. Because we initialized cJSON elsewhere to use our gc, we don't
// need to call cJSON free functions manually.

static model_config_t *create_default_models(void) {
    // Check which API keys are available
    const char *openrouter_key = getenv("OPENROUTER_API_KEY");
    const char *openai_key = getenv("OPENAI_API_KEY");
    const char *gemini_key = getenv("GEMINI_API_KEY");
    const char *xai_key = getenv("XAI_API_KEY");
    
    // Initialize config
    model_config_t *config = gc_malloc(&gc, sizeof(model_config_t));
    config->count = 0;
    size_t capacity = 4;  // Start with small capacity
    config->models = gc_malloc(&gc, sizeof(model_t) * capacity);
    
    // Macro to add a model with automatic capacity management
    // token_limit: the advertised max context length of the model in tokens (input + output)
    #define ADD_MODEL(name_str, endpoint_str, model_str, api_key_str, params_str, token_limit) \
        do { \
            if (config->count >= capacity) { \
                capacity *= 2; \
                model_t *new_models = gc_malloc(&gc, sizeof(model_t) * capacity); \
                memcpy(new_models, config->models, sizeof(model_t) * config->count); \
                config->models = new_models; \
            } \
            config->models[config->count].name = gc_strdup(&gc, name_str); \
            config->models[config->count].type = MODEL_TYPE_OPENAI; \
            config->models[config->count].max_tokens = (size_t)(token_limit); \
            config->models[config->count].config.openai.endpoint = gc_strdup(&gc, endpoint_str); \
            config->models[config->count].config.openai.model = gc_strdup(&gc, model_str); \
            config->models[config->count].config.openai.api_key = gc_strdup(&gc, api_key_str); \
            config->models[config->count].config.openai.params = gc_strdup(&gc, params_str); \
            config->count++; \
        } while(0)
    
    // Add models based on available API keys, preferring OpenRouter

    // OpenAI models - prefer OpenRouter if available
    if (openrouter_key) {
        ADD_MODEL("o3", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "openai/o3",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"OpenAI\"]}"
                  "}",
                  200000);

                // o3-pro is OpenRouter exclusive for now due to the endpoint support.
        ADD_MODEL("o3-pro", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "openai/o3-pro",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"OpenAI\"]}"
                  "}",
                  200000);
        ADD_MODEL("o4-mini", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "openai/o4-mini",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"OpenAI\"]}"
                  "}",
                  200000);
        

    } else if (openai_key) {
        // Fall back to direct OpenAI API
        ADD_MODEL("o4-mini", 
                  "https://api.openai.com/v1/chat/completions",
                  "o4-mini",
                  openai_key,
                  "{"
                    "\"reasoning_effort\":\"high\","
                    "\"stream\":true"
                  "}",
                  200000);
        
        ADD_MODEL("o3", 
                  "https://api.openai.com/v1/chat/completions",
                  "o3",
                  openai_key,
                  "{"
                    "\"reasoning_effort\":\"high\","
                    "\"stream\":true"
                  "}",
                  200000);
    }
    
    // Gemini models - prefer OpenRouter if available
    if (openrouter_key) {
        ADD_MODEL("gemini", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "google/gemini-2.5-pro",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"Google\"]}"
                  "}",
                  1000000);
    } else if (gemini_key) {
        // Fall back to direct Gemini API
        ADD_MODEL("gemini", 
                  "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions",
                  "google/gemini-2.5-pro",
                  gemini_key,
                  "{"
                    "\"reasoning_effort\":\"high\","
                    "\"stream\":true"
                  "}",
                  1000000);
    }
    
    // X.AI models - prefer OpenRouter if available
    if (openrouter_key) {
        ADD_MODEL("grok-4", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "x-ai/grok-4",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"X.AI\"]}"
                  "}",
                  131072);
    } else if (xai_key) {
        // Fall back to direct X.AI API
        ADD_MODEL("grok-4", 
                  "https://api.x.ai/v1/chat/completions",
                  "grok-4",
                  xai_key,
                  "{"
                    "\"reasoning_effort\":\"high\","
                    "\"stream\":true"
                  "}",
                  131072);
    }
    
    // OpenRouter-exclusive models
    if (openrouter_key) {

         ADD_MODEL("deepseek-r1", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "deepseek/deepseek-r1-0528",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"DeepSeek\"]}"
                  "}",
                  163840);

        ADD_MODEL("glm-4.5", 
                  "https://openrouter.ai/api/v1/chat/completions",
                  "z-ai/glm-4.5",
                  openrouter_key,
                  "{"
                    "\"reasoning\":{\"effort\":\"high\"},"
                    "\"stream\":true,"
                    "\"provider\":{\"only\":[\"Z.AI\"]}"
                  "}",
                  131072);
        
    }
    
    ADD_MODEL("local/qwen3-30b", 
              "http://localhost:11434/v1/chat/completions",
              "qwen3:30b",
              "ollama",
              "{"
                "\"stream\":true"
              "}",
              256000); 
    
    #undef ADD_MODEL
    
    return config;
}

static model_config_t *load_models_from_file(const char *path, char **error) {
    char *content = file_to_string(path, error);
    if (!content) {
        return NULL;
    }
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        const char *json_error = cJSON_GetErrorPtr();
        if (error) {
            if (json_error) {
                *error = gc_asprintf(&gc, "JSON parse error: %s", json_error);
            } else {
                *error = gc_strdup(&gc, "Invalid JSON in config file");
            }
        }
        return NULL;
    }
    
    if (!cJSON_IsObject(json)) {
        if (error) {
            *error = gc_strdup(&gc, "Config file must contain a JSON object");
        }
        return NULL;
    }
    
    // Count the number of models (keys in the object)
    // Note: cJSON preserves the order of object keys as they appear in the JSON file.
    // The first model defined in the JSON will become the default model (index 0).
    int count = cJSON_GetArraySize(json);
    if (count <= 0) {
        if (error) {
            *error = gc_strdup(&gc, "Config file contains no models");
        }
        return NULL;
    }
    
    model_config_t *config = gc_malloc(&gc, sizeof(model_config_t));
    config->models = gc_malloc(&gc, sizeof(model_t) * count);
    config->count = count;
    
    int index = 0;
    cJSON *model_obj = NULL;
    cJSON_ArrayForEach(model_obj, json) {
        const char *model_name = model_obj->string;
        
        if (!model_name || strlen(model_name) == 0) {
            if (error) {
                *error = gc_strdup(&gc, "Model name cannot be empty");
            }
            return NULL;
        }
        
        if (!cJSON_IsObject(model_obj)) {
            if (error) {
                *error = gc_asprintf(&gc, "Model '%s' definition must be an object", model_name);
            }
            return NULL;
        }
        
        cJSON *type = cJSON_GetObjectItem(model_obj, "type");
        cJSON *endpoint = cJSON_GetObjectItem(model_obj, "endpoint");
        cJSON *api_key = cJSON_GetObjectItem(model_obj, "api_key");
        cJSON *api_key_env = cJSON_GetObjectItem(model_obj, "api_key_env");
        cJSON *params = cJSON_GetObjectItem(model_obj, "params");
        cJSON *model = cJSON_GetObjectItem(model_obj, "model");
        cJSON *max_tokens = cJSON_GetObjectItem(model_obj, "max_tokens");
        
        if (!type || !cJSON_IsString(type)) {
            if (error) {
                *error = gc_asprintf(&gc, "Model '%s' missing required 'type' field", model_name);
            }
            return NULL;
        }
        
        // Store model name
        config->models[index].name = gc_strdup(&gc, model_name);
        
        // Store max tokens if provided, otherwise use default
        if (max_tokens && cJSON_IsNumber(max_tokens)) {
            config->models[index].max_tokens = (size_t)max_tokens->valuedouble;
        } else {
            config->models[index].max_tokens = 128000;
        }
        
        // Handle different model types
        if (strcmp(type->valuestring, "openai") == 0) {
            // OpenAI-compatible model
            config->models[index].type = MODEL_TYPE_OPENAI;
            
            if (!endpoint || !cJSON_IsString(endpoint)) {
                if (error) {
                    *error = gc_asprintf(&gc, "OpenAI model '%s' missing required 'endpoint' field", model_name);
                }
                return NULL;
            }
            
            config->models[index].config.openai.endpoint = gc_strdup(&gc, endpoint->valuestring);
            
            // Store model name if provided
            if (model && cJSON_IsString(model)) {
                config->models[index].config.openai.model = gc_strdup(&gc, model->valuestring);
            } else {
                config->models[index].config.openai.model = NULL;
            }
            
            // Handle API key (either direct or via environment variable)
            if (api_key && cJSON_IsString(api_key)) {
                config->models[index].config.openai.api_key = gc_strdup(&gc, api_key->valuestring);
            } else if (api_key_env && cJSON_IsString(api_key_env)) {
                const char *env_value = getenv(api_key_env->valuestring);
                config->models[index].config.openai.api_key = env_value ? gc_strdup(&gc, env_value) : NULL;
            } else {
                config->models[index].config.openai.api_key = NULL;
            }
            
            // Store additional params as JSON string
            if (params && cJSON_IsObject(params)) {
                char *params_str = cJSON_PrintUnformatted(params);
                config->models[index].config.openai.params = params_str;
            } else {
                config->models[index].config.openai.params = NULL;
            }
            
        } else {
            if (error) {
                *error = gc_asprintf(&gc, "Model '%s' has invalid type '%s' (must be 'openai')", 
                                   model_name, type->valuestring);
            }
            return NULL;
        }
        
        index++;
    }
    
    return config;
}

static char *get_config_path(void) {
    char *config_home = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    char *path = NULL;
    
    if (config_home) {
        size_t len = strlen(config_home) + strlen("/minicoder/models.json") + 1;
        path = gc_malloc(&gc, len);
        if (!path) return NULL;
        snprintf(path, len, "%s/minicoder/models.json", config_home);
    } else if (home) {
        size_t len = strlen(home) + strlen("/.config/minicoder/models.json") + 1;
        path = gc_malloc(&gc, len);
        if (!path) return NULL;
        snprintf(path, len, "%s/.config/minicoder/models.json", home);
    }
    
    return path;
}

model_config_t *init_models(char **error) {
    model_config_t *models = NULL;
    char *load_error = NULL;
    
    // First, check MINICODER_MODEL_CONFIG environment variable
    const char *env_config = getenv("MINICODER_MODEL_CONFIG");
    if (env_config && strlen(env_config) > 0) {
        int exists = file_exists(env_config);
        if (exists == 1) {
            models = load_models_from_file(env_config, &load_error);
            if (models) {
                return models;
            }
            // Continue to other locations if this fails
        }
    }
    
    // Second, check XDG_CONFIG_HOME or ~/.config
    char *config_path = get_config_path();
    if (config_path) {
        int exists = file_exists(config_path);
        if (exists == 1) {
            models = load_models_from_file(config_path, &load_error);
            if (models) {
                return models;
            }
        }
    }
    
    // Third, check /etc/minicoder/models.json
    const char *etc_config = "/etc/minicoder/models.json";
    int exists = file_exists(etc_config);
    if (exists == 1) {
        models = load_models_from_file(etc_config, &load_error);
        if (models) {
            return models;
        }
    }
    
    // If no config files found anywhere, create defaults
    models = create_default_models();
    if (!models) {
        if (error) {
            *error = gc_strdup(&gc, "Failed to create default models");
        }
        return NULL;
    }
    return models;
}

model_t *get_model(model_config_t *config, const char *name) {
    if (!config || !name) {
        return NULL;
    }
    
    for (size_t i = 0; i < config->count; i++) {
        if (config->models[i].name && strcmp(config->models[i].name, name) == 0) {
            return &config->models[i];
        }
    }
    
    return NULL;
}

model_t *get_default_model(model_config_t *config) {
    if (!config || config->count == 0) {
        return NULL;
    }
    
    return &config->models[0];
}

void list_models(model_config_t *config, FILE *stream) {
    if (!stream) {
        stream = stdout;
    }
    
    if (!config || config->count == 0) {
        fprintf(stream, "No models configured.\n");
        return;
    }
    
    fprintf(stream, "Available models:\n");
    for (size_t i = 0; i < config->count; i++) {
        fprintf(stream, "- %s%s\n", config->models[i].name,
                (i == 0) ? " (default)" : "");
    }
}

struct curl_response {
    char *data;
    size_t size;
    const model_completion_options_t *options;
    char **error;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_response *mem = (struct curl_response *)userp;
    
    // Check for cancellation
    if (mem->options && mem->options->cancellation_callback) {
        if (mem->options->cancellation_callback(mem->options->cancellation_user_data)) {
            // Set error and return 0 to abort the transfer
            if (mem->error && !*mem->error) {
                *mem->error = gc_strdup(&gc, "Operation cancelled by user");
            }
            return 0;
        }
    }
    
    char *ptr = gc_realloc(&gc, mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0;  // Out of memory
    }
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

// Structure to hold streaming state
struct streaming_state {
    string_builder_t response_buffer;      // Complete response
    string_builder_t line_buffer;          // Partial line buffer
    const model_completion_options_t *options;
    char **error;
    int done;
};

// Callback for streaming data from CURL
static size_t streaming_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct streaming_state *state = (struct streaming_state *)userp;
    
    // Check for cancellation
    if (state->options && state->options->cancellation_callback) {
        if (state->options->cancellation_callback(state->options->cancellation_user_data)) {
            // Set error and return 0 to abort the transfer
            if (state->error && !*state->error) {
                *state->error = gc_strdup(&gc, "Operation cancelled by user");
            }
            return 0;
        }
    }
    
    // Append new data to line buffer
    string_builder_append(&state->line_buffer, contents, realsize);
    
    // Process complete lines
    char *buffer = state->line_buffer.data;
    size_t buffer_len = state->line_buffer.size;
    size_t line_start = 0;
    
    for (size_t i = 0; i < buffer_len; i++) {
        if (buffer[i] == '\n') {
            // Found end of line
            size_t line_len = i - line_start;
            
            // Skip empty lines
            if (line_len > 0) {
                // Process the line
                char *line = gc_malloc(&gc, line_len + 1);
                memcpy(line, buffer + line_start, line_len);
                line[line_len] = '\0';
                
                // Remove carriage return if present
                if (line_len > 0 && line[line_len - 1] == '\r') {
                    line[line_len - 1] = '\0';
                }
                
                // Check for SSE data line
                if (strncmp(line, "data: ", 6) == 0) {

                    // We must dup the string for gc root reasons.
                    const char *data = gc_strdup(&gc, line + 6);
                    
                    // Check for [DONE] message
                    if (strcmp(data, "[DONE]") == 0) {
                        state->done = 1;
                    } else {
                        // Parse JSON
                        cJSON *chunk_json = cJSON_Parse(data);
                        if (chunk_json) {
                            // Extract content from choices[0].delta.content
                            cJSON *choices = cJSON_GetObjectItem(chunk_json, "choices");
                            if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
                                cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
                                if (first_choice) {
                                    cJSON *delta = cJSON_GetObjectItem(first_choice, "delta");
                                    if (delta) {
                                        // Check for regular content
                                        cJSON *content = cJSON_GetObjectItem(delta, "content");
                                        if (content && cJSON_IsString(content)) {
                                            const char *text = content->valuestring;
                                            size_t text_len = strlen(text);
                                            
                                            // Append to response buffer
                                            string_builder_append(&state->response_buffer, text, text_len);
                                            
                                            // Call output callback if provided
                                            if (state->options && state->options->output_callback && text_len) {
                                                state->options->output_callback(text, text_len, 
                                                    CHUNK_TYPE_CONTENT, state->options->callback_user_data);
                                            }
                                        }
                                        
                                        // Check for reasoning content (OpenRouter style)
                                        cJSON *reasoning = cJSON_GetObjectItem(delta, "reasoning");
                                        if (!reasoning) {
                                            // Deep seek style.
                                            reasoning = cJSON_GetObjectItem(delta, "reasoning_content");
                                        }
                                        if (reasoning && cJSON_IsString(reasoning)) {
                                            const char *reasoning_text = reasoning->valuestring;
                                            size_t reasoning_len = strlen(reasoning_text);
                                            
                                            // Call output callback for reasoning if provided
                                            if (state->options && state->options->output_callback && reasoning_len) {
                                                state->options->output_callback(reasoning_text, reasoning_len,
                                                    CHUNK_TYPE_REASONING, state->options->callback_user_data);
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Check for errors in the response
                            cJSON *error_obj = cJSON_GetObjectItem(chunk_json, "error");
                            if (error_obj) {
                                cJSON *error_msg = cJSON_GetObjectItem(error_obj, "message");
                                if (error_msg && cJSON_IsString(error_msg)) {
                                    if (state->error && !*state->error) {
                                        *state->error = gc_asprintf(&gc, "API error: %s", error_msg->valuestring);
                                    }
                                } else {
                                    if (state->error && !*state->error) {
                                        *state->error = gc_strdup(&gc, "API returned an error");
                                    }
                                }
                                return 0; // Stop processing on error
                            }
                        }
                    }
                }
            }
            
            line_start = i + 1;
        }
    }
    
    // Keep any incomplete line in the buffer
    if (line_start < buffer_len) {
        memmove(buffer, buffer + line_start, buffer_len - line_start);
        state->line_buffer.size = buffer_len - line_start;
    } else {
        state->line_buffer.size = 0;
    }
    
    return realsize;
}

// Progress callback for CURL - used to check for cancellation
static int model_curl_xferinfo_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow; // Unused parameters
    
    const model_completion_options_t *options = (const model_completion_options_t *)clientp;
    if (options && options->cancellation_callback) {
        if (options->cancellation_callback(options->cancellation_user_data)) {
            return 1; // Non-zero return aborts the transfer
        }
    }
    return 0; // Continue
}

static char *openai_completion_streaming(model_t *model, const char *prompt, cJSON *request_json, const model_completion_options_t *options, char **error) {
    (void)prompt; // Unused parameter
    
    // Initialize streaming state
    struct streaming_state state = {0};
    string_builder_init(&state.response_buffer, &gc, 1024);
    string_builder_init(&state.line_buffer, &gc, 1024);
    state.options = options;
    state.error = error;
    state.done = 0;
    
    // Initialize cURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = gc_strdup(&gc, "Failed to initialize cURL");
        }
        return NULL;
    }
    
    char *request_body = cJSON_PrintUnformatted(request_json);
    if (!request_body) {
        curl_easy_cleanup(curl);
        if (error) {
            *error = gc_strdup(&gc, "Failed to serialize JSON request");
        }
        return NULL;
    }
    
    // Set up cURL options for streaming
    curl_easy_setopt(curl, CURLOPT_URL, model->config.openai.endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streaming_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&state);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    
    char *auth_header = gc_asprintf(&gc, "Authorization: Bearer %s", model->config.openai.api_key);
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set up progress callback for cancellation checking
    if (options && options->cancellation_callback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, model_curl_xferinfo_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)options);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); // Enable progress meter
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up cURL resources
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        if (error) {
            if (res == CURLE_ABORTED_BY_CALLBACK) {
                *error = gc_strdup(&gc, "Operation cancelled by user");
            } else {
                *error = gc_asprintf(&gc, "cURL error: %s", curl_easy_strerror(res));
            }
        }
        return NULL;
    }
    
    // Check if we got an error during streaming
    if (error && *error) {
        return NULL;
    }
    
    // Process any remaining data in line buffer
    if (state.line_buffer.size > 0 && !state.done) {
        // Check if the remaining data is a JSON error response
        char *buffer_copy = gc_malloc(&gc, state.line_buffer.size + 1);
        memcpy(buffer_copy, state.line_buffer.data, state.line_buffer.size);
        buffer_copy[state.line_buffer.size] = '\0';
        
        cJSON *error_json = cJSON_Parse(buffer_copy);
        if (error_json) {
            // It's a JSON response, likely an error
            cJSON *error_obj = cJSON_GetObjectItem(error_json, "error");
            if (error_obj) {
                cJSON *error_msg = cJSON_GetObjectItem(error_obj, "message");
                if (error_msg && cJSON_IsString(error_msg)) {
                    if (error) {
                        *error = gc_asprintf(&gc, "API error: %s", error_msg->valuestring);
                    }
                } else {
                    if (error) {
                        *error = gc_strdup(&gc, "API returned an error");
                    }
                }
            } else {
                if (error) {
                    *error = gc_strdup(&gc, "Unexpected JSON response instead of SSE stream");
                }
            }
            return NULL;
        }
        
        // Not JSON, so it's incomplete SSE data
        if (error) {
            *error = gc_strdup(&gc, "Incomplete SSE data received");
        }
        return NULL;
    }
    
    // Release the response buffer and return the complete response
    char *complete_response = string_builder_finalize(&state.response_buffer);
    
    if (!complete_response || strlen(complete_response) == 0) {
        if (error) {
            *error = gc_strdup(&gc, "No content received from streaming API");
        }
        return NULL;
    }
    
    return complete_response;
}

static char *openai_completion_non_streaming(model_t *model, const char *prompt, cJSON *request_json, const model_completion_options_t *options, char **error) {
    (void)prompt; // Unused parameter
    
    // Initialize cURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = gc_strdup(&gc, "Failed to initialize cURL");
        }
        return NULL;
    }
    
    // Prepare response buffer
    struct curl_response response = {0};
    response.data = gc_malloc(&gc, 1);  // Will be grown as needed by gc_realloc
    response.size = 0;
    response.options = options;
    response.error = error;
    
    char *request_body = cJSON_PrintUnformatted(request_json);
    
    if (!request_body) {
        curl_easy_cleanup(curl);
        if (error) {
            *error = gc_strdup(&gc, "Failed to serialize JSON request");
        }
        return NULL;
    }
    
    // Set up cURL options
    curl_easy_setopt(curl, CURLOPT_URL, model->config.openai.endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    char *auth_header = gc_asprintf(&gc, "Authorization: Bearer %s", model->config.openai.api_key);
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set up progress callback for cancellation checking
    if (options && options->cancellation_callback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, model_curl_xferinfo_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)options);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); // Enable progress meter
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up cURL resources
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        if (error) {
            if (res == CURLE_ABORTED_BY_CALLBACK) {
                *error = gc_strdup(&gc, "Operation cancelled by user");
            } else {
                *error = gc_asprintf(&gc, "cURL error: %s", curl_easy_strerror(res));
            }
        }
        return NULL;
    }
    
    // Parse the response
    cJSON *response_json = cJSON_Parse(response.data);
    if (!response_json) {
        if (error) {
            *error = gc_strdup(&gc, "Failed to parse API response");
        }
        return NULL;
    }
    
    // Check for API errors
    cJSON *error_obj = cJSON_GetObjectItem(response_json, "error");
    if (error_obj) {
        cJSON *error_msg = cJSON_GetObjectItem(error_obj, "message");
        if (error_msg && cJSON_IsString(error_msg)) {
            if (error) {
                *error = gc_asprintf(&gc, "API error: %s", error_msg->valuestring);
            }
        } else {
            if (error) {
                *error = gc_strdup(&gc, "API returned an error");
            }
        }
        return NULL;
    }
    
    // Extract the completion text
    char *content_text = NULL;
    
    // Try chat completions format first
    cJSON *choices = cJSON_GetObjectItem(response_json, "choices");
    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
        if (first_choice) {
            // Get content from chat format (only format we support)
            cJSON *message = cJSON_GetObjectItem(first_choice, "message");
            if (message) {
                // Check for regular content
                cJSON *content = cJSON_GetObjectItem(message, "content");
                if (content && cJSON_IsString(content)) {
                    content_text = gc_strdup(&gc, content->valuestring);
                    
                    // Call the output callback for content if provided
                    if (options && options->output_callback && *content_text) {
                        options->output_callback(content_text, strlen(content_text), 
                            CHUNK_TYPE_CONTENT, options->callback_user_data);
                    }
                }
                
                // Check for reasoning content (OpenRouter style)
                cJSON *reasoning = cJSON_GetObjectItem(message, "reasoning");
                if (!reasoning) {
                    // Deep seek style.
                    reasoning = cJSON_GetObjectItem(message, "reasoning_content");
                }
                if (reasoning && cJSON_IsString(reasoning)) {
                    const char *reasoning_text = reasoning->valuestring;
                    
                    // Call the output callback for reasoning if provided
                    if (options && options->output_callback && *reasoning_text) {
                        options->output_callback(reasoning_text, strlen(reasoning_text),
                            CHUNK_TYPE_REASONING, options->callback_user_data);
                    }
                }
            }
        }
    }
    
    if (!content_text) {
        if (error) {
            *error = gc_strdup(&gc, "No content text found in API response");
        }
        return NULL;
    }
    
    return content_text;
}

static char *openai_completion(model_t *model, const char *prompt, const model_completion_options_t *options, char **error) {

    // Check if API key is available
    if (!model->config.openai.api_key) {
        if (error) {
            *error = gc_asprintf(&gc, "No API key configured for model '%s'", model->name);
        }
        return NULL;
    }
    
    // Build JSON request
    cJSON *request_json = cJSON_CreateObject();
    if (!request_json) {
        if (error) {
            *error = gc_strdup(&gc, "Failed to create JSON request");
        }
        return NULL;
    }
    
    // Add model if specified
    if (model->config.openai.model) {
        cJSON_AddStringToObject(request_json, "model", model->config.openai.model);
    }
    
    // Verify endpoint is /chat/completions
    if (!strstr(model->config.openai.endpoint, "/chat/completions")) {
        if (error) {
            *error = gc_asprintf(&gc, "Model '%s' endpoint must be a /chat/completions endpoint", model->name);
        }
        return NULL;
    }
    
    // Add messages for chat completions
    cJSON *messages = cJSON_CreateArray();
    cJSON *message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", "user");
    cJSON_AddStringToObject(message, "content", prompt);
    cJSON_AddItemToArray(messages, message);
    cJSON_AddItemToObject(request_json, "messages", messages);
    
    // Don't add max_tokens to the request - it's not uniformly supported across providers
    // and can cause issues. Let each provider handle their own limits.
    
    // Add additional parameters if provided
    if (model->config.openai.params) {
        cJSON *params = cJSON_Parse(model->config.openai.params);
        if (params) {
            cJSON *item = params->child;
            while (item) {
                cJSON *copy = cJSON_Duplicate(item, 1);
                if (copy) {
                    cJSON_AddItemToObject(request_json, item->string, copy);
                }
                item = item->next;
            }
        }
    }
    
    // Check if streaming is requested
    cJSON *stream_param = cJSON_GetObjectItem(request_json, "stream");
    int is_streaming = stream_param && cJSON_IsBool(stream_param) && cJSON_IsTrue(stream_param);
    
    char *result = NULL;
    if (is_streaming) {
        result = openai_completion_streaming(model, prompt, request_json, options, error);
    } else {
        result = openai_completion_non_streaming(model, prompt, request_json, options, error);
    }
    
    return result;
}


char *model_completion(model_t *model, const char *prompt, const model_completion_options_t *options, char **error) {
    if (!model || !prompt) {
        if (error) {
            *error = gc_strdup(&gc, "Invalid parameters: model and prompt are required");
        }
        return NULL;
    }
    
    // Dispatch to appropriate handler based on model type
    switch (model->type) {
        case MODEL_TYPE_OPENAI:
            return openai_completion(model, prompt, options, error);
        default:
            if (error) {
                *error = gc_asprintf(&gc, "Unknown model type for model '%s'", model->name);
            }
            return NULL;
    }
}