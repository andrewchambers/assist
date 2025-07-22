#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    printf("Received %zu bytes\n", total);
    return total;
}

int main(void) {
    CURL *curl;
    CURLcode res;
    
    printf("Testing libcurl with mbedTLS...\n");
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        
        printf("Fetching https://www.google.com...\n");
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            printf("Response code: %ld\n", response_code);
            
            char *content_type;
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
            if(!res && content_type) {
                printf("Content-Type: %s\n", content_type);
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    
    printf("Test complete.\n");
    return 0;
}