#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils/log.h"

static void *run_joinable_thread(void *const mut_arg)
{
    uintptr_t thread_exit_code = 13;
    const char *arg_msg = mut_arg;

    PRINT_INFO("received message %s", arg_msg);

    return (void *)thread_exit_code;
}

static void *run_detached_thread(void *const mut_arg)
{
    PRINT_INFO("waiting for 10 seconds");
    sleep(10);
    PRINT_INFO("finished");

    return NULL;
}

int main(void)
{
    pthread_attr_t detached_thread_attr;
    int return_code = pthread_attr_init(&detached_thread_attr);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_attr_init failed");
        return EXIT_FAILURE;
    }

    return_code = pthread_attr_setdetachstate(&detached_thread_attr, PTHREAD_CREATE_DETACHED);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_attr_setdetachstate failed");
        return EXIT_FAILURE;
    }

    pthread_t detached_thread;

    return_code = pthread_create(&detached_thread, &detached_thread_attr, run_detached_thread,
                                 NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_create failed");
        return EXIT_FAILURE;
    }

    pthread_t joinable_thread;
    const char *arg_msg = "Hello, POSIX Thread!";

    return_code = pthread_create(&joinable_thread, NULL, run_joinable_thread, (void *)arg_msg);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_create failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("created the joinable and detached threads");

    void *joinable_thread_exit_code = NULL;

    return_code = pthread_join(joinable_thread, &joinable_thread_exit_code);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_join failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("joinable thread returned %lu",
               (unsigned long)((uintptr_t)joinable_thread_exit_code));

    pthread_attr_destroy(&detached_thread_attr);
    pthread_exit(NULL);

    return 0;
}
