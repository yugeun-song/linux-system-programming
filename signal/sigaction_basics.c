#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* NOTICE: Use 'volatile sig_atomic_t' instead of 'int' for the signal flag to ensure atomic access. */
static volatile sig_atomic_t g_is_running = 1;

/* NOTICE: Only SIGKILL and SIGSTOP cannot be caught or blocked; all others can.
 * Returning from a SIGFPE/SIGILL/SIGSEGV/SIGBUS handler is undefined behavior. */
static void signal_handler(__attribute__((unused)) int signum, siginfo_t *info,
                           __attribute__((unused)) void *ucontext)
{
    if (info->si_pid == getpid()) {
        const char msg[] = "signal_handler(): self-raised, ignoring\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        return;
    }

    const char msg[] = "signal_handler(): external signal, exiting\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    g_is_running = 0;
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    int counter = 0;

    if (sigemptyset(&sa.sa_mask) == -1) {
        perror("main(): failed to initialize signal set with sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("main(): failed to register SIGINT handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGKILL, &sa, NULL) == -1) {
        perror("main(): failed to register SIGKILL handler (expected; cannot be caught)");
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("main(): failed to register SIGTERM handler");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGSTOP, &sa, NULL) == -1) {
        perror("main(): failed to register SIGSTOP handler (expected; cannot be caught)");
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
                perror("main(): failed to raise SIGINT");
                return EXIT_FAILURE;
            }
        }
    }

    printf("main(): finished\n");
    return EXIT_SUCCESS;
}