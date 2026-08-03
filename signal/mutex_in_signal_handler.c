#define _GNU_SOURCE

/* NOTICE: pthread_mutex_trylock() and pthread_mutex_timedlock() avoid deadlock
 * but are still NOT async-signal-safe (see signal-safety(7)). They work on
 * Linux/glibc as an implementation detail, not a portable guarantee. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "helper/log.h"

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static void handler_lock(int signum)
{
    int saved_errno = errno;

    const char m1[] = "handler_lock(): locking mutex...\n";
    write(STDOUT_FILENO, m1, sizeof(m1) - 1);

    pthread_mutex_lock(&g_mutex);

    const char m2[] = "handler_lock(): acquired mutex\n";
    write(STDOUT_FILENO, m2, sizeof(m2) - 1);

    pthread_mutex_unlock(&g_mutex);

    errno = saved_errno;
}

static void handler_trylock(int signum)
{
    int saved_errno = errno;

    const char m1[] = "handler_trylock(): trying mutex...\n";
    write(STDOUT_FILENO, m1, sizeof(m1) - 1);

    int rc = pthread_mutex_trylock(&g_mutex);

    if (rc == EBUSY) {
        const char m2[] = "handler_trylock(): mutex busy (EBUSY)\n";
        write(STDOUT_FILENO, m2, sizeof(m2) - 1);
    } else if (rc == 0) {
        const char m2[] = "handler_trylock(): acquired mutex\n";
        write(STDOUT_FILENO, m2, sizeof(m2) - 1);
        pthread_mutex_unlock(&g_mutex);
    }

    errno = saved_errno;
}

static void handler_timedlock(int signum)
{
    int saved_errno = errno;
    struct timespec ts;

    const char m1[] = "handler_timedlock(): timed-locking mutex (1s timeout)...\n";
    write(STDOUT_FILENO, m1, sizeof(m1) - 1);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;

    int rc = pthread_mutex_timedlock(&g_mutex, &ts);

    if (rc == ETIMEDOUT) {
        const char m2[] = "handler_timedlock(): timed out (ETIMEDOUT)\n";
        write(STDOUT_FILENO, m2, sizeof(m2) - 1);
    } else if (rc == 0) {
        const char m2[] = "handler_timedlock(): acquired mutex\n";
        write(STDOUT_FILENO, m2, sizeof(m2) - 1);
        pthread_mutex_unlock(&g_mutex);
    }

    errno = saved_errno;
}

static void *thread_routine(void *arg)
{
    printf("thread_routine(): locking mutex...\n");

    pthread_mutex_lock(&g_mutex);
    printf("thread_routine(): acquired mutex\n");
    pthread_mutex_unlock(&g_mutex);

    return NULL;
}

static int set_handler(void (*fn)(int))
{
    struct sigaction sa = { 0 };

    sa.sa_handler = fn;

    if (sigemptyset(&sa.sa_mask) == -1) {
        LOG_PERROR(errno, "sigemptyset failed");
        return -1;
    }

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        LOG_PERROR(errno, "sigaction failed");
        return -1;
    }

    return 0;
}

int main(void)
{
    pthread_t thread;
    int rc;

    setvbuf(stdout, NULL, _IOLBF, 0);

    printf("main(): --- normal thread ---\n");

    pthread_mutex_lock(&g_mutex);
    printf("main(): mutex locked, creating thread\n");

    rc = pthread_create(&thread, NULL, thread_routine, NULL);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_create failed");
        return EXIT_FAILURE;
    }

    sleep(1);
    printf("main(): releasing mutex\n");
    pthread_mutex_unlock(&g_mutex);

    rc = pthread_join(thread, NULL);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_join failed");
        return EXIT_FAILURE;
    }

    printf("main(): thread finished\n\n"
           "main(): --- signal handler (trylock) ---\n");

    if (set_handler(handler_trylock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    printf("main(): mutex locked, SIGALRM in 1 second\n");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&g_mutex);

    printf("main(): resumed\n\n"
           "main(): --- signal handler (timedlock, 1s timeout) ---\n");

    if (set_handler(handler_timedlock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    printf("main(): mutex locked, SIGALRM in 1 second\n");
    alarm(1);
    sleep(3);
    pthread_mutex_unlock(&g_mutex);

    printf("main(): resumed\n\n"
           "main(): --- signal handler (lock) ---\n");

    if (set_handler(handler_lock) != 0) {
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&g_mutex);
    printf("main(): mutex locked, SIGALRM in 1 second\n");
    alarm(1);
    sleep(3);

    pthread_mutex_unlock(&g_mutex);
    printf("main(): done\n");

    pthread_mutex_destroy(&g_mutex);
    return 0;
}