#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helper/log.h"

#define SHELL_SIGNAL_BASE 128

void child_routine(void)
{
    static const char msg[] = "child_routine(): running as child\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    _exit(23);
}

void parent_routine(pid_t child_pid)
{
    printf("parent_routine(): running as parent, child pid is %d\n", child_pid);
}

int main(void)
{
    pid_t pid;
    int exit_code = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);

    pid = fork();

    if (pid < 0) {
        LOG_PERROR(errno, "fork failed");
        exit_code = 1;
    } else if (pid == 0) {
        child_routine();
    } else {
        int status;

        parent_routine(pid);

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
    }

    return exit_code;
}