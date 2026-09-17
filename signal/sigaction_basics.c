#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "utils/log.h"

/* volatile forces a re-read each iteration; sig_atomic_t makes the handler's store indivisible. */
static volatile sig_atomic_t g_is_running = 1;

/* Returning from a handler for a kernel-generated SIGFPE/SIGILL/SIGSEGV/SIGBUS is undefined;
 * for one sent by kill(), raise() or sigqueue() it is defined. */
static void signal_handler(int signum, siginfo_t *info, void *ucontext)
{
    int saved_errno = errno;

    if (info->si_pid == getpid()) {
        LOG_INFO("self-raised, ignoring");
    } else {
        LOG_INFO("external signal, exiting");
        g_is_running = 0;
    }

    errno = saved_errno;
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    int loop_count = 0;

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

    LOG_INFO("loop is running (press Ctrl+C or run 'kill %d' command)", getpid());

    while (g_is_running) {
        LOG_INFO("looping...");
        sleep(1);

        ++loop_count;
        if (loop_count >= 5) {
            loop_count = 0;
            LOG_INFO("raising SIGINT (self-raised, expected to be ignored)...");
            if (raise(SIGINT) != 0) {
                LOG_PERROR(errno, "failed to raise SIGINT");
                return EXIT_FAILURE;
            }
        }
    }

    LOG_INFO("finished");
    return EXIT_SUCCESS;
}