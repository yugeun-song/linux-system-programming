#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "utils/log.h"

static pthread_mutex_t g_mutex;

/* No pthread_mutex_* call is async-signal-safe (signal-safety(7)). trylock and timedlock only
 * bound the wait; that they work here is a glibc detail, not a portable guarantee. */
static void handler_lock(int signum)
{
    int saved_errno = errno;

    PRINT_INFO("locking mutex...");

    pthread_mutex_lock(&g_mutex);

    PRINT_INFO("acquired mutex");

    pthread_mutex_unlock(&g_mutex);

    errno = saved_errno;
}

static void handler_trylock(int signum)
{
    int saved_errno = errno;

    PRINT_INFO("trying mutex...");

    int return_code = pthread_mutex_trylock(&g_mutex);

    if (return_code == EBUSY) {
        PRINT_INFO("mutex busy (EBUSY)");
    } else if (return_code == 0) {
        PRINT_INFO("acquired mutex");
        pthread_mutex_unlock(&g_mutex);
    }

    errno = saved_errno;
}

static void handler_timedlock(int signum)
{
    int saved_errno = errno;
    struct timespec deadline;

    PRINT_INFO("timed-locking mutex (1s timeout)...");

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 1;

    int return_code = pthread_mutex_timedlock(&g_mutex, &deadline);

    if (return_code == ETIMEDOUT) {
        PRINT_INFO("timed out (ETIMEDOUT)");
    } else if (return_code == 0) {
        PRINT_INFO("acquired mutex");
        pthread_mutex_unlock(&g_mutex);
    }

    errno = saved_errno;
}

static void *thread_routine(void *arg)
{
    PRINT_INFO("locking mutex...");

    pthread_mutex_lock(&g_mutex);
    PRINT_INFO("acquired mutex");
    pthread_mutex_unlock(&g_mutex);

    return NULL;
}

static int set_handler(void (*handler)(int))
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
    pthread_t thread;
    int return_code;

    return_code = pthread_mutex_init(&g_mutex, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_mutex_init failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("--- normal thread ---");

    pthread_mutex_lock(&g_mutex);
    PRINT_INFO("mutex locked, creating thread");

    return_code = pthread_create(&thread, NULL, thread_routine, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_create failed");
        return EXIT_FAILURE;
    }

    sleep(1);
    PRINT_INFO("releasing mutex");
    pthread_mutex_unlock(&g_mutex);

    return_code = pthread_join(thread, NULL);
    if (return_code != 0) {
        PRINT_PERROR(return_code, "pthread_join failed");
        return EXIT_FAILURE;
    }

    PRINT_INFO("thread finished");
    PRINT_INFO("--- signal handler (trylock) ---");

    if (set_handler(handler_trylock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&g_mutex);

    PRINT_INFO("resumed");
    PRINT_INFO("--- signal handler (timedlock, 1s timeout) ---");

    if (set_handler(handler_timedlock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&g_mutex);

    PRINT_INFO("resumed");
    PRINT_INFO("--- signal handler (lock) ---");

    if (set_handler(handler_lock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    PRINT_INFO("mutex locked, SIGALRM in 1 second");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&g_mutex);

    PRINT_INFO("done");

    pthread_mutex_destroy(&g_mutex);
    return 0;
}