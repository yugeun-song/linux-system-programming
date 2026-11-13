#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "utils/log.h"

static pthread_mutex_t gs_mutex;

/*
 * No pthread_mutex_* call is async-signal-safe (signal-safety(7)). trylock and timedlock only
 * bound the wait; that they work here is a glibc detail, not a portable guarantee.
 */
static void handle_alarm_with_lock(int signum)
{
    int saved_errno = errno;

    PRINT_INFO("locking mutex...");

    pthread_mutex_lock(&gs_mutex);

    PRINT_INFO("acquired mutex");

    pthread_mutex_unlock(&gs_mutex);

    errno = saved_errno;
}

static void handle_alarm_with_trylock(int signum)
{
    int saved_errno = errno;

    PRINT_INFO("trying mutex...");

    int return_code = pthread_mutex_trylock(&gs_mutex);

    if (return_code == EBUSY) {
        PRINT_INFO("mutex busy (EBUSY)");
    } else if (return_code == 0) {
        PRINT_INFO("acquired mutex");
        pthread_mutex_unlock(&gs_mutex);
    }

    errno = saved_errno;
}

static void handle_alarm_with_timedlock(int signum)
{
    int saved_errno = errno;
    struct timespec deadline;

    PRINT_INFO("timed-locking mutex (1s timeout)...");

    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        PRINT_PERROR(errno, "clock_gettime failed");
        errno = saved_errno;
        return;
    }
    deadline.tv_sec += 1;

    int return_code = pthread_mutex_timedlock(&gs_mutex, &deadline);

    if (return_code == ETIMEDOUT) {
        PRINT_INFO("timed out (ETIMEDOUT)");
    } else if (return_code == 0) {
        PRINT_INFO("acquired mutex");
        pthread_mutex_unlock(&gs_mutex);
    }

    errno = saved_errno;
}

static void *run_thread(void *const mut_arg)
{
    PRINT_INFO("locking mutex...");

    pthread_mutex_lock(&gs_mutex);
    PRINT_INFO("acquired mutex");
    pthread_mutex_unlock(&gs_mutex);

    return NULL;
}

static int set_handler(void (*const handler)(int))
{
    struct sigaction sa = { 0 };

    sa.sa_handler = handler;

    if (sigemptyset(&sa.sa_mask) == -1) {
        PRINT_PERROR(errno, "sigemptyset failed");
        return -1;
    }

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        PRINT_PERROR(errno, "sigaction failed");
        return -1;
    }

    return 0;
}

int main(void)
{
    int return_code = pthread_mutex_init(&gs_mutex, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_mutex_init failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("--- normal thread ---");

    pthread_mutex_lock(&gs_mutex);
    PRINT_INFO("mutex locked, creating thread");

    pthread_t thread;

    return_code = pthread_create(&thread, NULL, run_thread, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_create failed");
        return EXIT_FAILURE;
    }

    sleep(1);
    PRINT_INFO("releasing mutex");
    pthread_mutex_unlock(&gs_mutex);

    return_code = pthread_join(thread, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_join failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("thread finished");
    PRINT_INFO("--- signal handler (trylock) ---");

    if (set_handler(handle_alarm_with_trylock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&gs_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&gs_mutex);

    PRINT_INFO("resumed");
    PRINT_INFO("--- signal handler (timedlock, 1s timeout) ---");

    if (set_handler(handle_alarm_with_timedlock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&gs_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&gs_mutex);

    PRINT_INFO("resumed");
    PRINT_INFO("--- signal handler (lock) ---");

    if (set_handler(handle_alarm_with_lock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&gs_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&gs_mutex);

    PRINT_INFO("done");

    pthread_mutex_destroy(&gs_mutex);
    return 0;
}
