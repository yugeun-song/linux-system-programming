#define _GNU_SOURCE

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

static __thread int g_tls_value;

static void child_report(int caller_tls_value)
{
    LOG_INFO("parent tls=%d\n"
             "child tls=%d\n"
             "tls preserved across fork=%s",
             caller_tls_value,
             g_tls_value,
             g_tls_value == caller_tls_value ? "yes" : "no");
}

int main(void)
{
    int caller_tls_value = 1000;

    g_tls_value = caller_tls_value;
    LOG_INFO("forking with tls=%d", g_tls_value);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_PERROR(errno, "fork failed");
        return 1;
    } else if (pid == 0) {
        child_report(caller_tls_value);
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