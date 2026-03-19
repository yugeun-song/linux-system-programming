#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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

    if (pid < 0) {
        fprintf(stderr, "fork failed\n");
        return 1;
    } else if (pid == 0) {
        child_routine();
    } else {
        int status;

        parent_routine();
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("return code is %d\n", WEXITSTATUS(status));
        }
    }

    return 0;
}