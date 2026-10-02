#include "ipc/signal_handler.h"
#include <string.h>

static volatile sig_atomic_t g_shutdown = 0;

static void on_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        g_shutdown = 1;
    }
}

void signals_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

bool signals_is_shutdown_requested(void) {
    return (g_shutdown != 0);
}

void signals_request_shutdown(void) {
    g_shutdown = 1;
}
