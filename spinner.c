// Minimal spinner (POSIX)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include "spinner.h"

#define SPIN_CHARS "|/-\\"
#define SPIN_DELAY_MS 100

static struct {
    pthread_t th;
    atomic_int running;   // 0/1
    atomic_int started;   // 0/1
    char msg[256];        // copied once at start
} sp = {0};

static void *spin_loop(void *_) {
    int n = (int)strlen(SPIN_CHARS), i = 0;
    fprintf(stderr, "\033[?25l"); fflush(stderr); // hide cursor
    while (atomic_load(&sp.running)) {
        const char *m = sp.msg[0] ? sp.msg : "";
        fprintf(stderr, "\r%c %s", SPIN_CHARS[i], m); fflush(stderr);
        i = (i + 1) % n;
        usleep(SPIN_DELAY_MS * 1000);
    }
    int clear = 2 + (int)strlen(sp.msg);          // spinner + space + msg
    fprintf(stderr, "\r%*s\r\033[?25h", clear, ""); // clear line, show cursor
    fflush(stderr);
    return NULL;
}

void start_spinner(const char *message) {
    if (atomic_exchange(&sp.started, 1)) return;      // already started
    if (!isatty(STDERR_FILENO)) { atomic_store(&sp.started, 0); return; }
    sp.msg[0] = '\0';
    if (message) {
        strncpy(sp.msg, message, sizeof(sp.msg) - 1);
        sp.msg[sizeof(sp.msg) - 1] = '\0';
    }
    atomic_store(&sp.running, 1);
    if (pthread_create(&sp.th, NULL, spin_loop, NULL) != 0) {
        atomic_store(&sp.running, 0);
        atomic_store(&sp.started, 0);
    }
}

void stop_spinner(void) {
    if (!atomic_exchange(&sp.started, 0)) return;     // not started
    atomic_store(&sp.running, 0);
    pthread_join(sp.th, NULL);
}
