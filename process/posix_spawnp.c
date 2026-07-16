#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
    } else {
        fprintf(stderr, "main(): posix_spawn failed: %s\n", strerror(spawn_ret));
        exit_code = 1;
    }

    return exit_code;
}
