#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "helper/log.h"

/* NOTICE: Use 'volatile sig_atomic_t' instead of 'int' for the signal flag to ensure atomic access. */
static volatile sig_atomic_t g_is_running = 1;

/* NOTICE: Only SIGKILL and SIGSTOP cannot be caught or blocked; all others can.
 * Returning from a SIGFPE/SIGILL/SIGSEGV/SIGBUS handler is undefined behavior. */
static void signal_handler(int signum, siginfo_t *info, void *ucontext)
{
    int saved_errno = errno;

    if (info->si_pid == getpid()) {
        const char msg[] = "signal_handler(): self-raised, ignoring\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    } else {
        const char msg[] = "signal_handler(): external signal, exiting\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        g_is_running = 0;
    }

    errno = saved_errno;
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    int counter = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (sigemptyset(&sa.sa_mask) == -1) {
        LOG_PERROR(errno, "failed to initialize signal set with sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_PERROR(errno, "failed to register SIGINT handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGKILL, &sa, NULL) == -1) {
        LOG_PWARN(errno, "failed to register SIGKILL handler (expected; cannot be caught)");
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_PERROR(errno, "failed to register SIGTERM handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGSTOP, &sa, NULL) == -1) {
        LOG_PWARN(errno, "failed to register SIGSTOP handler (expected; cannot be caught)");
    }

    printf("main(): loop is running (press Ctrl+C or run 'kill %d' command)\n", getpid());

    while (g_is_running) {
        printf("main(): looping...\n");
        sleep(1);

        ++counter;
        if (counter >= 5) {
            counter = 0;
            printf("main(): raising SIGINT (self-raised, expected to be ignored)...\n");
            if (raise(SIGINT) != 0) {
                LOG_PERROR(errno, "failed to raise SIGINT");
                return EXIT_FAILURE;
            }
        }
    }

    printf("main(): finished\n");
    return EXIT_SUCCESS;
}