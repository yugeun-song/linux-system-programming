#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

#define SHELL_SIGNAL_BASE 128

static void child_routine(void)
{
    LOG_INFO("running as child");
    _exit(23);
}

static void parent_routine(pid_t child_pid)
{
    LOG_INFO("running as parent, child pid is %d", child_pid);
}

int main(void)
{
    pid_t pid = fork();
    int exit_code = 0;

    if (pid > 0) {
        int child_status;

        parent_routine(pid);

        while (waitpid(pid, &child_status, 0) == -1) {
            if (errno != EINTR) {
                LOG_PERROR(errno, "waitpid failed");
                return 1;
            }
        }

        if (WIFEXITED(child_status)) {
            LOG_INFO("child return code is %d", WEXITSTATUS(child_status));
            exit_code = WEXITSTATUS(child_status);
        } else if (WIFSIGNALED(child_status)) {
            LOG_INFO("child terminated by signal %d", WTERMSIG(child_status));
            exit_code = SHELL_SIGNAL_BASE + WTERMSIG(child_status);
        } else {
            LOG_ERR("child terminated abnormally");
            exit_code = 1;
        }
    } else if (pid == 0) {
        child_routine();
    } else {
        LOG_PERROR(errno, "fork failed");
        exit_code = 1;
    }

    return exit_code;
}