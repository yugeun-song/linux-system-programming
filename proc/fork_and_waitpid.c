#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHELL_SIGNAL_BASE 128

void child_routine(void)
{
    int return_code = 23;

    printf("[%d] I am child process, parent's pid is %d\n", getpid(),
           getppid());
    exit(return_code);
}

void parent_routine(void)
{
    printf("[%d] I am parent process\n", getpid());
}

int main(void)
{
    pid_t pid = fork();
    int exit_code = 0;

    if (pid < 0) {
        fprintf(stderr, "fork failed\n");
        exit_code = 1;
    } else if (pid == 0) {
        child_routine();
    } else {
        int status;

        parent_routine();
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("return code is %d\n", WEXITSTATUS(status));
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            printf("terminated by signal %d\n", WTERMSIG(status));
            exit_code = SHELL_SIGNAL_BASE + WTERMSIG(status);
        } else {
            fprintf(stderr, "abnormal termination\n");
            exit_code = 1;
        }
    }

    return exit_code;
}