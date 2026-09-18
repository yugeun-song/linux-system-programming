#include <pthread.h>
#include <unistd.h>

#include "utils/log.h"

void *joinable_thread_routine(void *arg)
{
    unsigned long *thread_exit_code = (unsigned long *)13;
    char *arg_msg = (char *)arg;

    LOG_INFO("received message %s", arg_msg);

    return (void *)thread_exit_code;
}

void *detached_thread_routine(void *arg)
{
    LOG_INFO("waiting for 10 seconds");
    sleep(10);
    LOG_INFO("finished");

    return NULL;
}

int main(void)
{
    pthread_t joinable_thread;
    pthread_t detached_thread;
    pthread_attr_t detached_thread_attr;

    void *joinable_thread_exit_code = NULL;
    char *arg_msg = "Hello, POSIX Thread!";
    int return_code;

    return_code = pthread_attr_init(&detached_thread_attr);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_attr_init failed");
        return 1;
    }

    return_code = pthread_attr_setdetachstate(&detached_thread_attr, PTHREAD_CREATE_DETACHED);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_attr_setdetachstate failed");
        return 1;
    }

    return_code = pthread_create(&detached_thread, &detached_thread_attr, detached_thread_routine, NULL);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_create failed");
        return 1;
    }

    return_code = pthread_create(&joinable_thread, NULL, joinable_thread_routine, (void *)arg_msg);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_create failed");
        return 1;
    }

    LOG_INFO("created the joinable and detached threads");

    return_code = pthread_join(joinable_thread, &joinable_thread_exit_code);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_join failed");
        return 1;
    }

    LOG_INFO("joinable thread returned %lu", (unsigned long)joinable_thread_exit_code);

    pthread_attr_destroy(&detached_thread_attr);
    pthread_exit(NULL);

    return 0;
}