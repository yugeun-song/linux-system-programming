#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

#define SHELL_SIGNAL_BASE 128

int main(void)
{
    pid_t pid;
    char *child_argv[] = { "ls", "-al", NULL };
    int exit_code = 0;
    int spawn_result;

    spawn_result = posix_spawnp(&pid, "ls", NULL, NULL, child_argv, NULL);

    if (spawn_result == 0) {
        int child_status;

        LOG_INFO("running as parent");

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
    } else {
        LOG_PERROR(spawn_result, "posix_spawnp failed");
        exit_code = 1;
    }

    return exit_code;
}