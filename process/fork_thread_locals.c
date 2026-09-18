#define _GNU_SOURCE

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

static __thread int g_tls_value;

static void child_report(void)
{
    LOG_INFO("child tls=%d", g_tls_value);
}

int main(void)
{
    g_tls_value = 1000;
    LOG_INFO("forking with tls=%d", g_tls_value);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_PERROR(errno, "fork failed");
        return 1;
    } else if (pid == 0) {
        child_report();
        _exit(0);
    }

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            LOG_PERROR(errno, "waitpid failed");
            return 1;
        }
    }

    LOG_INFO("child exited");

    return 0;
}