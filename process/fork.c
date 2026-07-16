#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHELL_SIGNAL_BASE 128

static void child_routine(void)
{
    int return_code = 23;

    printf("child_routine(): [%d] running as child, parent's pid is %d\n", getpid(), getppid());
    exit(return_code);
}

static void parent_routine(void)
{
    printf("parent_routine(): [%d] running as parent\n", getpid());
}

int main(void)
{
    pid_t pid = fork();
    int exit_code = 0;

    if (pid > 0) {
        int status;

        parent_routine();
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("main(): return code is %d\n", WEXITSTATUS(status));
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            printf("main(): terminated by signal %d\n", WTERMSIG(status));
            exit_code = SHELL_SIGNAL_BASE + WTERMSIG(status);
        } else {
            fprintf(stderr, "main(): abnormal termination\n");
            exit_code = 1;
        }
    } else if (pid == 0) {
        child_routine();
    } else {
        fprintf(stderr, "main(): fork failed\n");
        exit_code = 1;
    }

    return exit_code;
}