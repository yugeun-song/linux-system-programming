#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

static __thread int gs_tls_value;

static void report_child_tls(void)
{
    PRINT_INFO("child tls=%d", gs_tls_value);
}

int main(void)
{
    gs_tls_value = 1000;
    PRINT_INFO("forking with tls=%d", gs_tls_value);

    pid_t pid = fork();
    if (pid < 0) {
        PRINT_PERROR(errno, "fork failed");
        return EXIT_FAILURE;
    } else if (pid == 0) {
        report_child_tls();
        _exit(0);
    }

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            PRINT_PERROR(errno, "waitpid failed");
            return EXIT_FAILURE;
        }
    }

    PRINT_INFO("child exited");

    return 0;
}
