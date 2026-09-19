#define _GNU_SOURCE

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

static __thread int g_tls_value;

static void child_report(void)
{
    PRINT_INFO("child tls=%d", g_tls_value);
}

int main(void)
{
    g_tls_value = 1000;
    PRINT_INFO("forking with tls=%d", g_tls_value);

    pid_t pid = fork();
    if (pid < 0) {
        PRINT_PERROR(errno, "fork failed");
        return 1;
    } else if (pid == 0) {
        child_report();
        _exit(0);
    }

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            PRINT_PERROR(errno, "waitpid failed");
            return 1;
        }
    }

    PRINT_INFO("child exited");

    return 0;
}