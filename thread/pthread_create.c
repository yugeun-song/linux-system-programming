#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void *joinable_thread_routine(void *arg)
{
    pid_t my_tid = gettid();
    uint64_t thread_exit_code = 13;
    char *msg = (char *)arg;

    printf("[%d] Joinable Child thread: %s\n", my_tid, msg);

    return (void *)((uintptr_t)thread_exit_code);
}

void *detached_thread_routine(__attribute__((unused)) void *arg)
{
    pid_t my_tid = gettid();

    printf("[%d] Detached Child thread: Waiting for 10 seconds...\n", my_tid);
    sleep(10);
    printf("[%d] Detached Child thread: Finished\n", my_tid);

    return NULL;
}

int main(void)
{
    pthread_t joinable_thread;
    pthread_t detached_thread;
    pthread_attr_t detached_thread_attr;

    pid_t my_tid = gettid();

    uint64_t return_code = -1;
    void *thread_return_code;
    char *arg_msg = "Hello, POSIX Thread!";

    if (pthread_attr_init(&detached_thread_attr) != 0) {
        perror("pthread_attr_init failed");
        return 1;
    }

    if (pthread_attr_setdetachstate(&detached_thread_attr,
                                    PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate failed");
        return 1;
    }

    if (pthread_create(&detached_thread, &detached_thread_attr,
                       detached_thread_routine, NULL) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    if (pthread_create(&joinable_thread, NULL, joinable_thread_routine,
                       (void *)arg_msg) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    printf("[%d] Main thread: Thread created\n", my_tid);

    if (pthread_join(joinable_thread, &thread_return_code) != 0) {
        perror("pthread_join failed");
        return 1;
    }

    return_code = (uint64_t)thread_return_code;
    printf("[%d] Main thread: Finished with return code %lu\n",
           my_tid, (unsigned long) return_code);

    pthread_attr_destroy(&detached_thread_attr);
    pthread_exit(NULL);

    return 0;
}