#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils/log.h"

/* volatile forces a re-read each iteration; sig_atomic_t makes the handler's store indivisible. */
static volatile sig_atomic_t gs_is_running = 1;

/*
 * Returning from a handler for a kernel-generated SIGFPE/SIGILL/SIGSEGV/SIGBUS is undefined;
 * for one sent by kill(), raise() or sigqueue() it is defined.
 */
static void handle_signal(int signum, siginfo_t *const mut_info, void *const mut_ucontext)
{
    int saved_errno = errno;

    if (mut_info->si_pid == getpid()) {
        PRINT_INFO("self-raised, ignoring");
    } else {
        PRINT_INFO("external signal, exiting");
        gs_is_running = 0;
    }

    errno = saved_errno;
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = handle_signal;
    sa.sa_flags = SA_SIGINFO;

    if (sigemptyset(&sa.sa_mask) == -1) {
        PRINT_PERROR(errno, "failed to initialize signal set with sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        PRINT_PERROR(errno, "failed to register SIGINT handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGKILL, &sa, NULL) == -1) {
        PRINT_WARN_ERRNO(errno, "failed to register SIGKILL handler (expected; cannot be caught)");
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        PRINT_PERROR(errno, "failed to register SIGTERM handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGSTOP, &sa, NULL) == -1) {
        PRINT_WARN_ERRNO(errno, "failed to register SIGSTOP handler (expected; cannot be caught)");
    }

    PRINT_INFO("loop is running (press Ctrl+C or run 'kill %d' command)", getpid());

    size_t loop_count = 0;

    while (gs_is_running != 0) {
        PRINT_INFO("looping...");
        sleep(1);

        ++loop_count;
        if (loop_count >= 5) {
            loop_count = 0;
            PRINT_INFO("raising SIGINT (self-raised, expected to be ignored)...");
            if (raise(SIGINT) != 0) {
                PRINT_PERROR(errno, "failed to raise SIGINT");
                return EXIT_FAILURE;
            }
        }
    }

    PRINT_INFO("finished");
    return 0;
}
