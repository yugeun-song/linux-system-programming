#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helper/log.h"

#define SHELL_SIGNAL_BASE 128

extern char **environ;

static void parent_routine(void)
{
    printf("parent_routine(): [%d] running as parent\n", getpid());
}

int main(void)
{
    pid_t pid;
    char *child_argv[] = { "ls", NULL };
    int exit_code = 0;
    int spawn_ret = posix_spawnp(&pid, "ls", NULL, NULL, child_argv, environ);

    if (spawn_ret == 0) {
        int status;

        parent_routine();

        while (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR) {
                LOG_PERROR(errno, "waitpid failed");
                return 1;
            }
        }

        if (WIFEXITED(status)) {
            printf("main(): return code is %d\n", WEXITSTATUS(status));
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            printf("main(): terminated by signal %d\n", WTERMSIG(status));
            exit_code = SHELL_SIGNAL_BASE + WTERMSIG(status);
        } else {
            LOG_ERR("abnormal termination");
            exit_code = 1;
        }
    } else {
        LOG_PERROR(spawn_ret, "posix_spawnp failed");
        exit_code = 1;
    }

    return exit_code;
}