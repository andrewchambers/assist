#include "model.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <cJSON.h>
#include <curl/curl.h>

// N.B. Because we initialized cJSON elsewhere to use our gc, we don't
// need to call cJSON free functions manually.

static model_config_t *create_default_models(void) {
    // Check which API keys are available
    const char *openrouter_key = getenv("OPENROUTER_API_KEY");
    const char *openai_key = getenv("OPENAI_API_KEY");
    
    if (!openrouter_key && !openai_key) {
        // No API keys available - set up default Ollama model
        model_config_t *config = gc_malloc(sizeof(model_config_t));
        config->count = 1;
        config->models = gc_malloc(sizeof(model_t) * config->count);
        
        // Default to qwen3:32b over Ollama
        config->models[0].name = gc_strdup("qwen3-32b");
        config->models[0].type = MODEL_TYPE_OPENAI;
        config->models[0].max_context_bytes = 131072;  // ~32K tokens * 4 bytes/token
        config->models[0].config.openai.endpoint = gc_strdup("http://localhost:11434/v1/chat/completions");
        config->models[0].config.openai.model = gc_strdup("qwen3:32b");
        config->models[0].config.openai.api_key = gc_strdup("ollama");  // Ollama doesn't require API key, but field is needed
        config->models[0].config.openai.params = gc_strdup("{\"stream\":true}");
        
        return config;
    }
    
    model_config_t *config = gc_malloc(sizeof(model_config_t));
    
    if (openrouter_key) {
        // Use OpenRouter models
        config->count = 6;
        config->models = gc_malloc(sizeof(model_t) * config->count);
        
        // Default model is first
        config->models[0].name = gc_strdup("o3");
        config->models[0].type = MODEL_TYPE_OPENAI;
        config->models[0].max_context_bytes = 512000;  // ~128K tokens * 4 bytes/token
        config->models[0].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[0].config.openai.model = gc_strdup("openai/o3");
        config->models[0].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[0].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");
        
        config->models[1].name = gc_strdup("o3-pro");
        config->models[1].type = MODEL_TYPE_OPENAI;
        config->models[1].max_context_bytes = 512000;  // ~128K tokens * 4 bytes/token
        config->models[1].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[1].config.openai.model = gc_strdup("openai/o3-pro");
        config->models[1].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[1].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

        config->models[2].name = gc_strdup("o4-mini");
        config->models[2].type = MODEL_TYPE_OPENAI;
        config->models[2].max_context_bytes = 512000;  // ~128K tokens * 4 bytes/token
        config->models[2].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[2].config.openai.model = gc_strdup("openai/o4-mini");
        config->models[2].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[2].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

        config->models[3].name = gc_strdup("grok-4");
        config->models[3].type = MODEL_TYPE_OPENAI;
        config->models[3].max_context_bytes = 524288;  // ~131K tokens * 4 bytes/token
        config->models[3].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[3].config.openai.model = gc_strdup("x-ai/grok-4");
        config->models[3].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[3].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

        config->models[4].name = gc_strdup("gemini");
        config->models[4].type = MODEL_TYPE_OPENAI;
        config->models[4].max_context_bytes = 2097152;  // ~524K tokens * 4 bytes/token
        config->models[4].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[4].config.openai.model = gc_strdup("google/gemini-2.5-pro");
        config->models[4].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[4].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

        config->models[5].name = gc_strdup("deepseek");
        config->models[5].type = MODEL_TYPE_OPENAI;
        config->models[5].max_context_bytes = 524288;  // ~131K tokens * 4 bytes/token
        config->models[5].config.openai.endpoint = gc_strdup("https://openrouter.ai/api/v1/chat/completions");
        config->models[5].config.openai.model = gc_strdup("deepseek/deepseek-r1-0528");
        config->models[5].config.openai.api_key = gc_strdup(openrouter_key);
        config->models[5].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");


    } else {
        // Use OpenAI models as fallback
        config->count = 2;
        config->models = gc_malloc(sizeof(model_t) * config->count);
        
        // o4-mini
        config->models[0].name = gc_strdup("o4-mini");
        config->models[0].type = MODEL_TYPE_OPENAI;
        config->models[0].max_context_bytes = 512000;  // ~128K tokens * 4 bytes/token
        config->models[0].config.openai.endpoint = gc_strdup("https://api.openai.com/v1/chat/completions");
        config->models[0].config.openai.model = gc_strdup("o4-mini");
        config->models[0].config.openai.api_key = gc_strdup(openai_key);
        config->models[0].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

        // o3 as default
        config->models[1].name = gc_strdup("o3");
        config->models[1].type = MODEL_TYPE_OPENAI;
        config->models[1].max_context_bytes = 512000;  // ~128K tokens * 4 bytes/token
        config->models[1].config.openai.endpoint = gc_strdup("https://api.openai.com/v1/chat/completions");
        config->models[1].config.openai.model = gc_strdup("o3");
        config->models[1].config.openai.api_key = gc_strdup(openai_key);
        config->models[1].config.openai.params = gc_strdup("{\"reasoning\":{\"effort\":\"high\"},\"stream\":true}");

    }

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
                *error = gc_asprintf("JSON parse error: %s", json_error);
            } else {
                *error = gc_strdup("Invalid JSON in config file");
            }
        }
        return NULL;
    }
    
    if (!cJSON_IsObject(json)) {
        if (error) {
            *error = gc_strdup("Config file must contain a JSON object");
        }
        return NULL;
    }
    
    // Count the number of models (keys in the object)
    // Note: cJSON preserves the order of object keys as they appear in the JSON file.
    // The first model defined in the JSON will become the default model (index 0).
    int count = cJSON_GetArraySize(json);
    if (count <= 0) {
        if (error) {
            *error = gc_strdup("Config file contains no models");
        }
        return NULL;
    }
    
    model_config_t *config = gc_malloc(sizeof(model_config_t));
    config->models = gc_malloc(sizeof(model_t) * count);
    config->count = count;
    
    int index = 0;
    cJSON *model_obj = NULL;
    cJSON_ArrayForEach(model_obj, json) {
        const char *model_name = model_obj->string;
        
        if (!model_name || strlen(model_name) == 0) {
            if (error) {
                *error = gc_strdup("Model name cannot be empty");
            }
            return NULL;
        }
        
        if (!cJSON_IsObject(model_obj)) {
            if (error) {
                *error = gc_asprintf("Model '%s' definition must be an object", model_name);
            }
            return NULL;
        }
        
        cJSON *type = cJSON_GetObjectItem(model_obj, "type");
        cJSON *endpoint = cJSON_GetObjectItem(model_obj, "endpoint");
        cJSON *api_key = cJSON_GetObjectItem(model_obj, "api_key");
        cJSON *api_key_env = cJSON_GetObjectItem(model_obj, "api_key_env");
        cJSON *params = cJSON_GetObjectItem(model_obj, "params");
        cJSON *model = cJSON_GetObjectItem(model_obj, "model");
        cJSON *max_context_bytes = cJSON_GetObjectItem(model_obj, "max_context_bytes");
        
        if (!type || !cJSON_IsString(type)) {
            if (error) {
                *error = gc_asprintf("Model '%s' missing required 'type' field", model_name);
            }
            return NULL;
        }
        
        // Store model name
        config->models[index].name = gc_strdup(model_name);
        
        // Store max context bytes if provided, otherwise use default
        if (max_context_bytes && cJSON_IsNumber(max_context_bytes)) {
            config->models[index].max_context_bytes = (size_t)max_context_bytes->valuedouble;
        } else {
            config->models[index].max_context_bytes = 512000; // Default ~128K tokens * 4 bytes/token
        }
        
        // Handle different model types
        if (strcmp(type->valuestring, "openai") == 0) {
            // OpenAI-compatible model
            config->models[index].type = MODEL_TYPE_OPENAI;
            
            if (!endpoint || !cJSON_IsString(endpoint)) {
                if (error) {
                    *error = gc_asprintf("OpenAI model '%s' missing required 'endpoint' field", model_name);
                }
                return NULL;
            }
            
            config->models[index].config.openai.endpoint = gc_strdup(endpoint->valuestring);
            
            // Store model name if provided
            if (model && cJSON_IsString(model)) {
                config->models[index].config.openai.model = gc_strdup(model->valuestring);
            } else {
                config->models[index].config.openai.model = NULL;
            }
            
            // Handle API key (either direct or via environment variable)
            if (api_key && cJSON_IsString(api_key)) {
                config->models[index].config.openai.api_key = gc_strdup(api_key->valuestring);
            } else if (api_key_env && cJSON_IsString(api_key_env)) {
                const char *env_value = getenv(api_key_env->valuestring);
                config->models[index].config.openai.api_key = env_value ? gc_strdup(env_value) : NULL;
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
                *error = gc_asprintf("Model '%s' has invalid type '%s' (must be 'openai')", 
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
        path = gc_malloc(len);
        if (!path) return NULL;
        snprintf(path, len, "%s/minicoder/models.json", config_home);
    } else if (home) {
        size_t len = strlen(home) + strlen("/.config/minicoder/models.json") + 1;
        path = gc_malloc(len);
        if (!path) return NULL;
        snprintf(path, len, "%s/.config/minicoder/models.json", home);
    }
    
    return path;
}

model_config_t *init_models(char **error) {
    char *config_path = get_config_path();
    
    if (!config_path) {
        if (error) {
            *error = gc_strdup("Failed to determine config path");
        }
        return NULL;
    }
    
    model_config_t *models = NULL;
    
    int exists = file_exists(config_path);
    if (exists == -1) {
        // Error accessing file
        if (error) {
            *error = gc_asprintf("Failed to access config file %s: %s", config_path, strerror(errno));
        }
        return NULL;
    } else if (exists == 0) {
        // File doesn't exist, create defaults
        models = create_default_models();
        if (!models) {
            if (error) {
                *error = gc_strdup("Failed to create default models");
            }
            return NULL;
        }
        return models;
    }
    
    char *load_error = NULL;
    models = load_models_from_file(config_path, &load_error);
    if (!models) {
        if (error) {
            if (load_error) {
                *error = load_error;
            } else {
                *error = gc_strdup("Failed to parse JSON config file");
            }
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
                *mem->error = gc_strdup("Operation cancelled by user");
            }
            return 0;
        }
    }
    
    char *ptr = gc_realloc(mem->data, mem->size + realsize + 1);
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
    gc_string_builder_t response_buffer;      // Complete response
    gc_string_builder_t line_buffer;          // Partial line buffer
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
                *state->error = gc_strdup("Operation cancelled by user");
            }
            return 0;
        }
    }
    
    // Append new data to line buffer
    gc_string_builder_append(&state->line_buffer, contents, realsize);
    
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
                char *line = gc_malloc(line_len + 1);
                memcpy(line, buffer + line_start, line_len);
                line[line_len] = '\0';
                
                // Remove carriage return if present
                if (line_len > 0 && line[line_len - 1] == '\r') {
                    line[line_len - 1] = '\0';
                }
                
                // Check for SSE data line
                if (strncmp(line, "data: ", 6) == 0) {
                    const char *data = line + 6;
                    
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
                                            gc_string_builder_append(&state->response_buffer, text, text_len);
                                            
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
                                        *state->error = gc_asprintf("API error: %s", error_msg->valuestring);
                                    }
                                } else {
                                    if (state->error && !*state->error) {
                                        *state->error = gc_strdup("API returned an error");
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
    gc_string_builder_init(&state.response_buffer, 1024);
    gc_string_builder_init(&state.line_buffer, 1024);
    state.options = options;
    state.error = error;
    state.done = 0;
    
    // Initialize cURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = gc_strdup("Failed to initialize cURL");
        }
        return NULL;
    }
    
    char *request_body = cJSON_PrintUnformatted(request_json);
    if (!request_body) {
        curl_easy_cleanup(curl);
        if (error) {
            *error = gc_strdup("Failed to serialize JSON request");
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
    
    char *auth_header = gc_asprintf("Authorization: Bearer %s", model->config.openai.api_key);
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
                *error = gc_strdup("Operation cancelled by user");
            } else {
                *error = gc_asprintf("cURL error: %s", curl_easy_strerror(res));
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
        // This shouldn't happen with properly formatted SSE, but handle it gracefully
        if (error) {
            *error = gc_strdup("Incomplete SSE data received");
        }
        return NULL;
    }
    
    // Release the response buffer and return the complete response
    char *complete_response = gc_string_builder_finalize(&state.response_buffer);
    
    if (!complete_response || strlen(complete_response) == 0) {
        if (error) {
            *error = gc_strdup("No content received from streaming API");
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
            *error = gc_strdup("Failed to initialize cURL");
        }
        return NULL;
    }
    
    // Prepare response buffer
    struct curl_response response = {0};
    response.data = gc_malloc(1);  // Will be grown as needed by gc_realloc
    response.size = 0;
    response.options = options;
    response.error = error;
    
    char *request_body = cJSON_PrintUnformatted(request_json);
    
    if (!request_body) {
        curl_easy_cleanup(curl);
        if (error) {
            *error = gc_strdup("Failed to serialize JSON request");
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
    
    char *auth_header = gc_asprintf("Authorization: Bearer %s", model->config.openai.api_key);
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
                *error = gc_strdup("Operation cancelled by user");
            } else {
                *error = gc_asprintf("cURL error: %s", curl_easy_strerror(res));
            }
        }
        return NULL;
    }
    
    // Parse the response
    cJSON *response_json = cJSON_Parse(response.data);
    if (!response_json) {
        if (error) {
            *error = gc_strdup("Failed to parse API response");
        }
        return NULL;
    }
    
    // Check for API errors
    cJSON *error_obj = cJSON_GetObjectItem(response_json, "error");
    if (error_obj) {
        cJSON *error_msg = cJSON_GetObjectItem(error_obj, "message");
        if (error_msg && cJSON_IsString(error_msg)) {
            if (error) {
                *error = gc_asprintf("API error: %s", error_msg->valuestring);
            }
        } else {
            if (error) {
                *error = gc_strdup("API returned an error");
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
                    content_text = gc_strdup(content->valuestring);
                    
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
            *error = gc_strdup("No content text found in API response");
        }
        return NULL;
    }
    
    return content_text;
}

static char *openai_completion(model_t *model, const char *prompt, const model_completion_options_t *options, char **error) {

    // Check if API key is available
    if (!model->config.openai.api_key) {
        if (error) {
            *error = gc_asprintf("No API key configured for model '%s'", model->name);
        }
        return NULL;
    }
    
    // Build JSON request
    cJSON *request_json = cJSON_CreateObject();
    if (!request_json) {
        if (error) {
            *error = gc_strdup("Failed to create JSON request");
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
            *error = gc_asprintf("Model '%s' endpoint must be a /chat/completions endpoint", model->name);
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
            *error = gc_strdup("Invalid parameters: model and prompt are required");
        }
        return NULL;
    }
    
    // Dispatch to appropriate handler based on model type
    switch (model->type) {
        case MODEL_TYPE_OPENAI:
            return openai_completion(model, prompt, options, error);
        default:
            if (error) {
                *error = gc_asprintf("Unknown model type for model '%s'", model->name);
            }
            return NULL;
    }
}