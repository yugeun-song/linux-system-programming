#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>

#include "helper/log.h"

void *joinable_thread_routine(void *arg)
{
    pid_t my_tid = gettid();
    uintptr_t thread_exit_code = 13;
    char *msg = (char *)arg;

    printf("joinable_thread_routine(): [%d] received message %s\n", my_tid, msg);

    return (void *)thread_exit_code;
}

void *detached_thread_routine(void *arg)
{
    pid_t my_tid = gettid();

    printf("detached_thread_routine(): [%d] waiting for 10 seconds\n", my_tid);
    sleep(10);
    printf("detached_thread_routine(): [%d] finished\n", my_tid);

    return NULL;
}

int main(void)
{
    pthread_t joinable_thread;
    pthread_t detached_thread;
    pthread_attr_t detached_thread_attr;

    pid_t my_tid = gettid();

    uintptr_t return_code = 0;
    void *thread_return_code;
    char *arg_msg = "Hello, POSIX Thread!";
    int rc;

    rc = pthread_attr_init(&detached_thread_attr);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_attr_init failed");
        return 1;
    }

    rc = pthread_attr_setdetachstate(&detached_thread_attr, PTHREAD_CREATE_DETACHED);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_attr_setdetachstate failed");
        return 1;
    }

    rc = pthread_create(&detached_thread, &detached_thread_attr, detached_thread_routine, NULL);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_create failed");
        return 1;
    }

    rc = pthread_create(&joinable_thread, NULL, joinable_thread_routine, (void *)arg_msg);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_create failed");
        return 1;
    }

    printf("main(): [%d] created the joinable and detached threads\n", my_tid);

    rc = pthread_join(joinable_thread, &thread_return_code);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_join failed");
        return 1;
    }

    return_code = (uintptr_t)thread_return_code;
    printf("main(): [%d] joinable thread returned %lu\n", my_tid, (unsigned long)return_code);

    pthread_attr_destroy(&detached_thread_attr);
    pthread_exit(NULL);

    return 0;
}