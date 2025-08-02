// Minimal spinner (POSIX)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "spinner.h"

#define SPIN_CHARS "|/-\\"
#define SPIN_DELAY_MS 100

static struct {
    pthread_t th;
    pthread_mutex_t mutex;
    int running;   // 0/1
    int started;   // 0/1
} sp = {0, PTHREAD_MUTEX_INITIALIZER, 0, 0};

static void *spin_loop(void *arg) {
    char *msg = (char *)arg;  // owned by this thread
    const char *m = msg ? msg : "";
    int n = (int)strlen(SPIN_CHARS), i = 0;
    int running;
    
    fprintf(stderr, "\033[?25l"); fflush(stderr); // hide cursor
    
    pthread_mutex_lock(&sp.mutex);
    running = sp.running;
    pthread_mutex_unlock(&sp.mutex);
    
    while (running) {
        fprintf(stderr, "\r%c %s", SPIN_CHARS[i], m); fflush(stderr);
        i = (i + 1) % n;
        usleep(SPIN_DELAY_MS * 1000);
        
        pthread_mutex_lock(&sp.mutex);
        running = sp.running;
        pthread_mutex_unlock(&sp.mutex);
    }
    int clear = 2 + (int)strlen(m);          // spinner + space + msg
    fprintf(stderr, "\r%*s\r\033[?25h", clear, ""); // clear line, show cursor
    fflush(stderr);
    free(msg);  // free the duplicated message
    return NULL;
}

void start_spinner(const char *message) {
    if (!isatty(STDERR_FILENO)) {
        return;
    }

    pthread_mutex_lock(&sp.mutex);
    if (sp.started) {
        pthread_mutex_unlock(&sp.mutex);
        return;  // already started
    }
    
    // Duplicate message for thread ownership
    char *msg_copy = message ? strdup(message) : NULL;
    
    sp.running = 1;
    sp.started = 1;
    
    if (pthread_create(&sp.th, NULL, spin_loop, msg_copy) != 0) {
        sp.running = 0;
        sp.started = 0;
        free(msg_copy);  // free on error
    }
    pthread_mutex_unlock(&sp.mutex);
}

void stop_spinner(void) {
    pthread_mutex_lock(&sp.mutex);
    if (!sp.started) {
        pthread_mutex_unlock(&sp.mutex);
        return;  // not started
    }
    sp.started = 0;
    sp.running = 0;
    pthread_mutex_unlock(&sp.mutex);
    
    pthread_join(sp.th, NULL);
}
